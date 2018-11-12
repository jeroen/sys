#include <Rinternals.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#define r 0
#define w 1
#define waitms 200
#define IS_STRING(x) (Rf_isString(x) && Rf_length(x))
#define IS_TRUE(x) (Rf_isLogical(x) && Rf_length(x) && asLogical(x))
#define IS_FALSE(x) (Rf_isLogical(x) && Rf_length(x) && !asLogical(x))

void kill_process_group(int signum) {
  kill(0, SIGKILL); // kills process group
  raise(SIGKILL); // just to be sure
}

/* prevent potential handlers from cleaning up exit codes */
static void block_sigchld(){
  sigset_t block_sigchld;
  sigemptyset(&block_sigchld);
  sigaddset(&block_sigchld, SIGCHLD);
  sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
}

static void resume_sigchild(){
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

/* In the fork we don't want to use the R API anymore */
void print_if(int err, const char * what){
  if(err){
    FILE *stream = fdopen(STDERR_FILENO, "w");
    if(stream){
      fprintf(stream, "System failure for: %s (%s)\n", what, strerror(errno));
      fclose(stream);
    }
  }
}

void warn_if(int err, const char * what){
  if(err)
    Rf_warningcall(R_NilValue, "System failure for: %s (%s)", what, strerror(errno));
}

void set_pipe(int input, int output[2]){
  print_if(dup2(output[w], input) < 0, "dup2() stdout/stderr");
  close(output[r]);
  close(output[w]);
}

void pipe_set_read(int pipe[2]){
  close(pipe[w]);
  bail_if(fcntl(pipe[r], F_SETFL, O_NONBLOCK) < 0, "fcntl() in pipe_set_read");
}

void set_input(const char * file){
  close(STDIN_FILENO);
  int fd = open(file, O_RDONLY); //lowest numbered FD should be 0
  print_if(fd != 0, "open() set_input not equal to STDIN_FILENO");
}

void set_output(int target, const char * file){
  close(target);
  int fd = open(file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  print_if(fd < 0, "open() set_output");
  if(fd == target)
    return;
  print_if(fcntl(fd, F_DUPFD, target) < 0, "fcntl() set_output");
  close(fd);
}

void safe_close(int target){
  set_output(target, "/dev/null");
}

static void check_child_success(int fd, const char * cmd){
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

int wait_for_action2(int fd1, int fd2){
  short events = POLLIN | POLLERR | POLLHUP;
  struct pollfd ufds[2] = {
    {fd1, events, events},
    {fd2, events, events}
  };
  return poll(ufds, 2, waitms);
}

static void R_callback(SEXP fun, const char * buf, ssize_t len){
  if(!isFunction(fun)) return;
  int ok;
  SEXP str = PROTECT(allocVector(RAWSXP, len));
  memcpy(RAW(str), buf, len);
  SEXP call = PROTECT(LCONS(fun, LCONS(str, R_NilValue)));
  R_tryEval(call, R_GlobalEnv, &ok);
  UNPROTECT(2);
}

void print_output(int pipe_out[2], SEXP fun){
  static ssize_t len;
  static char buffer[65336];
  while ((len = read(pipe_out[r], buffer, sizeof(buffer))) > 0)
    R_callback(fun, buffer, len);
}

SEXP C_execute(SEXP command, SEXP args, SEXP outfun, SEXP errfun, SEXP input, SEXP wait, SEXP timeout){
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

      // send stdout/stderr to pipes
      set_pipe(STDOUT_FILENO, pipe_out);
      set_pipe(STDERR_FILENO, pipe_err);
    } else {
      //redirect stdout in background process
      if(IS_STRING(outfun)){
        set_output(STDOUT_FILENO, CHAR(STRING_ELT(outfun, 0)));
      } else if(!IS_TRUE(outfun)){
        safe_close(STDOUT_FILENO);
      }
      //redirect stderr in background process
      if(IS_STRING(errfun)){
        set_output(STDERR_FILENO, CHAR(STRING_ELT(errfun, 0)));
      } else if(!IS_TRUE(errfun)){
        safe_close(STDERR_FILENO);
      }
    }

    //Linux only: set pgid and commit suicide when parent dies
#ifdef PR_SET_PDEATHSIG
    setpgid(0, 0);
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    signal(SIGTERM, kill_process_group);
#endif
    //OSX: do NOT change pgid, so we receive signals from parent group

    // Set STDIN for child (default is /dev/null)
    if(IS_FALSE(input)){
      //set stdin to unreadable /dev/null (O_WRONLY)
      safe_close(STDIN_FILENO);
    } else if(!IS_TRUE(input)){
      set_input(IS_STRING(input) ? CHAR(STRING_ELT(input, 0)) : "/dev/null");
    }

    //close all file descriptors before exit, otherwise they can segfault
    for (int i = 3; i < sysconf(_SC_OPEN_MAX); i++) {
      if(i != failure[w])
        close(i);
    }

    //prepare execv
    int len = Rf_length(args);
    char * argv[len + 1];
    argv[len] = NULL;
    for(int i = 0; i < len; i++){
      argv[i] = strdup(CHAR(STRING_ELT(args, i)));
    }

    //execvp never returns if successful
    fcntl(failure[w], F_SETFD, FD_CLOEXEC);
    execvp(CHAR(STRING_ELT(command, 0)), argv);

    //execvp failed! Send errno to parent
    print_if(write(failure[w], &errno, sizeof(errno)) < 0, "write to failure pipe");
    close(failure[w]);

    //exit() not allowed by CRAN. raise() should suffice
    //exit(EXIT_FAILURE);
    raise(SIGKILL);
  }

  //PARENT PROCESS:
  close(failure[w]);
  if (!block){
    check_child_success(failure[r], CHAR(STRING_ELT(command, 0)));
    return ScalarInteger(pid);
  }

  //blocking: close write end of IO pipes
  pipe_set_read(pipe_out);
  pipe_set_read(pipe_err);

  //start timer
  struct timeval start, end;
  double elapsed, totaltime = REAL(timeout)[0];
  gettimeofday(&start, NULL);

  //status -1 means error, 0 means running
  int status = 0;
  int killcount = 0;
  while (waitpid(pid, &status, WNOHANG) >= 0){
    //check for timeout
    if(totaltime > 0){
      gettimeofday(&end, NULL);
      elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
      if(killcount == 0 && elapsed > totaltime){
        warn_if(kill(pid, SIGINT), "interrupt child");
        killcount++;
      } else if(killcount == 1 && elapsed > (totaltime + 1)){
        warn_if(kill(pid, SIGKILL), "force kill child");
        killcount++;
      }
    }

    //for well behaved programs, SIGINT is automatically forwarded
    if(pending_interrupt()){
      //pass interrupt to child. On second try we SIGKILL.
      warn_if(kill(pid, killcount ? SIGKILL : SIGINT), "kill child");
      killcount++;
    }
    //make sure to empty the pipes, even if fun == NULL
    wait_for_action2(pipe_out[r], pipe_err[r]);

    //print stdout/stderr buffers
    print_output(pipe_out, outfun);
    print_output(pipe_err, errfun);
  }
  warn_if(close(pipe_out[r]), "close stdout");
  warn_if(close(pipe_err[r]), "close stderr");

  // check for execvp() error *after* closing pipes and zombie
  resume_sigchild();
  check_child_success(failure[r], CHAR(STRING_ELT(command, 0)));

  if(WIFEXITED(status)){
    return ScalarInteger(WEXITSTATUS(status));
  } else {
    int signal = WTERMSIG(status);
    if(signal != 0){
      if(killcount && elapsed > totaltime){
        Rf_errorcall(R_NilValue, "Program '%s' terminated (timeout reached: %.2fsec)",
                     CHAR(STRING_ELT(command, 0)), totaltime);
      } else {
        Rf_errorcall(R_NilValue, "Program '%s' terminated by SIGNAL (%s)",
                     CHAR(STRING_ELT(command, 0)), strsignal(signal));
      }
    }
    Rf_errorcall(R_NilValue, "Program terminated abnormally");
  }
}

SEXP R_exec_status(SEXP rpid, SEXP wait){
  int wstat = NA_INTEGER;
  pid_t pid = asInteger(rpid);
  do {
    int res = waitpid(pid, &wstat, WNOHANG);
    bail_if(res < 0, "waitpid()");
    if(res)
      break;
    usleep(100*1000);
  } while (asLogical(wait) && !pending_interrupt());
  return ScalarInteger(wstat);
}
