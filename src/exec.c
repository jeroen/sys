#include <Rinternals.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#define IS_STRING(x) (Rf_isString(x) && Rf_length(x))
#define IS_TRUE(x) (Rf_isLogical(x) && Rf_length(x) && asLogical(x))
#define IS_FALSE(x) (Rf_isLogical(x) && Rf_length(x) && !asLogical(x))

/* prevent potential handlers from cleaning up exit codes */
void block_sigchld(){
  sigset_t block_sigchld;
  sigemptyset(&block_sigchld);
  sigaddset(&block_sigchld, SIGCHLD);
  sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
}

void resume_sigchild(){
  sigset_t block_sigchld;
  sigemptyset(&block_sigchld);
  sigaddset(&block_sigchld, SIGCHLD);
  sigprocmask(SIG_UNBLOCK, &block_sigchld, NULL);
}

/* check for system errors */
void bail_if(int err, const char * what){
  if(err)
    Rf_errorcall(R_NilValue, "System failure for: %s (%s)", what, strerror(errno));
}

void warn_if(int err, const char * what){
  if(err)
    Rf_warningcall(R_NilValue, "System failure for: %s (%s)", what, strerror(errno));
}

void check_child_success(int fd, const char * cmd){
  int child_errno;
  int n = read(fd, &child_errno, sizeof(child_errno));
  close(fd);
  if (n) {
    Rf_errorcall(R_NilValue, "Failed to execute '%s' (%s)", cmd, strerror(child_errno));
  }
}

/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

int wait_for_action(int fd1, int fd2){
  int waitms = 500;
  short events = POLLIN | POLLERR | POLLHUP;
  struct pollfd ufds[2] = {
    {fd1, events, events},
    {fd2, events, events}
  };
  return poll(ufds, 2, waitms);
}

void R_callback(SEXP fun, const char * buf, ssize_t len){
  if(!isFunction(fun)) return;
  int ok;
  SEXP str = PROTECT(allocVector(RAWSXP, len));
  memcpy(RAW(str), buf, len);
  SEXP call = PROTECT(LCONS(fun, LCONS(str, R_NilValue)));
  R_tryEval(call, R_GlobalEnv, &ok);
  UNPROTECT(2);
}

SEXP C_execute(SEXP command, SEXP args, SEXP outfun, SEXP errfun, SEXP wait){
  //split process
  int block = asLogical(wait);
  int pipe_out[2];
  int pipe_err[2];
  int failure[2];

  //setup execvp errno pipe
  bail_if(pipe(failure), "pipe(failure)");

  //create io pipes only in blocking mode
  if(block){
    bail_if(pipe(pipe_out) || pipe(pipe_err), "create pipe");
    block_sigchld();
  }

  //fork the main process
  pid_t pid = fork();
  bail_if(pid < 0, "fork()");

  //CHILD PROCESS
  if(pid == 0){
    if(block){
      //undo blocking in child (is this needed at all?)
      resume_sigchild();

      // send stdout to the pipe
      bail_if(dup2(pipe_out[1], STDOUT_FILENO) < 0, "dup2() stdout");
      close(pipe_out[0]);
      close(pipe_out[1]);

      //send stderr to the pipe
      bail_if(dup2(pipe_err[1], STDERR_FILENO) < 0, "dup2() stderr");
      close(pipe_err[0]);
      close(pipe_err[1]);
    } else {
      if(IS_STRING(outfun)){
        const char * file = CHAR(STRING_ELT(outfun, 0));
        int fd = open(file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        bail_if(dup2(fd, STDOUT_FILENO) < 0, "dup2() stdout");
        close(fd);
      } else if(!IS_TRUE(outfun)){
        close(STDOUT_FILENO);
      }
      if(IS_STRING(errfun)){
        const char * file = CHAR(STRING_ELT(errfun, 0));
        int fd = open(file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        bail_if(dup2(fd, STDERR_FILENO) < 0, "dup2() stderr");
        close(fd);
      } else if(!IS_TRUE(errfun)){
        close(STDERR_FILENO);
      }
    }

    //prevents signals from being propagated to fork
    setpgid(0, 0);

    // close STDIN for fork
    close(STDIN_FILENO);

    //close all file descriptors before exit, otherwise they can segfault
    for (int i = 3; i < sysconf(_SC_OPEN_MAX); i++) {
      if(i != failure[1])
        close(i);
    }

    //prepare execv
    int len = Rf_length(args);
    const char * argv[len + 1];
    argv[len] = NULL;
    for(int i = 0; i < len; i++){
      argv[i] = Rf_translateCharUTF8(STRING_ELT(args, i));
    }

    //execvp never returns if successful
    fcntl(failure[1], F_SETFD, FD_CLOEXEC);
    execvp(CHAR(STRING_ELT(command, 0)), (char **) argv);

    //execvp failed! Send errno to parent
    warn_if(write(failure[1], &errno, sizeof(errno)) < 0, "write to failure pipe");
    close(failure[1]);

    //exit() not allowed by CRAN. raise() should suffice
    //exit(EXIT_FAILURE);
    raise(SIGKILL);
  }

  //PARENT PROCESS:
  close(failure[1]);
  if (!block){
    check_child_success(failure[0], CHAR(STRING_ELT(command, 0)));
    return ScalarInteger(pid);
  }

  //blocking: close write end of IO pipes
  close(pipe_out[1]);
  close(pipe_err[1]);

  //use async IO
  fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);

  //status -1 means error, 0 means running
  int status = 0;
  int killcount = 0;
  char buffer[65336];
  while (waitpid(pid, &status, WNOHANG) >= 0){
    if(pending_interrupt()){
      //pass interrupt to child. On second try we SIGKILL.
      warn_if(kill(-pid, killcount ? SIGKILL : SIGINT), "kill child");
      killcount++;
    }
    //make sure to empty the pipes, even if fun == NULL
    ssize_t len;
    wait_for_action(pipe_out[0], pipe_err[0]);
    while ((len = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
      R_callback(outfun, buffer, len);
    while ((len = read(pipe_err[0], buffer, sizeof(buffer))) > 0)
      R_callback(errfun, buffer, len);
  }
  warn_if(close(pipe_out[0]), "close stdout");
  warn_if(close(pipe_err[0]), "close stderr");

  // check for execvp() error *after* closing pipes and zombie
  resume_sigchild();
  check_child_success(failure[0], CHAR(STRING_ELT(command, 0)));

  if(WIFEXITED(status)){
    return ScalarInteger(WEXITSTATUS(status));
  } else {
    int signal = WTERMSIG(status);
    if(signal != 0)
      Rf_errorcall(R_NilValue, "Program '%s' terminated by SIGNAL (%s)", CHAR(STRING_ELT(command, 0)), strsignal(signal));
    Rf_errorcall(R_NilValue, "Program terminated abnormally");
  }
}
