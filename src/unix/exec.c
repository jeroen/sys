#include <Rinternals.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

void R_callback(SEXP fun, const char * buf, ssize_t len){
  if(!isFunction(fun)) return;
  int ok;
  SEXP str = PROTECT(ScalarString(mkCharLen(buf, len)));
  SEXP call = PROTECT(LCONS(fun, LCONS(str, R_NilValue)));
  R_tryEval(call, R_GlobalEnv, &ok);
  UNPROTECT(2);
}

SEXP C_exec_internal(SEXP command, SEXP args, SEXP outfun, SEXP errfun, SEXP wait){
  //split process
  int pipe_out[2];
  int pipe_err[2];
  pipe(pipe_out);
  pipe(pipe_err);
  pid_t pid = fork();

  //this happens in the child
  if(pid == 0){
    // send stdout to the pipe
    dup2(pipe_out[1], STDOUT_FILENO);
    close(pipe_out[0]);
    close(pipe_out[1]);

    //send stderr to the pipe
    dup2(pipe_err[1], STDERR_FILENO);
    close(pipe_err[0]);
    close(pipe_err[1]);

    //prevents signals from being propagated to fork
    setpgid(0, 0);

    // close STDIN for fork
    close(STDIN_FILENO);

    //prepare execv
    int len = Rf_length(args);
    const char * argv[len + 1];
    argv[len] = NULL;
    for(int i = 0; i < len; i++){
      argv[i] = Rf_translateCharUTF8(STRING_ELT(args, i));
    }

    //execvp never returns if successful
    execvp(CHAR(STRING_ELT(command, 0)), (char **) argv);

    //close all file descriptors before exit, otherwise they can segfault
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) close(i);

    // picked up by WTERMSIG() below in parent proc
    raise(SIGILL);
    //exit(0); //now allowed by CRAN. raise() should suffice anyway
  }

  //something went wrong
  if(pid < 0)
    Rf_errorcall(R_NilValue, "Failed to fork");

  //close write end of pipe in parent
  char buffer[1024];
  fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
  fcntl(pipe_err[0], F_SETFL, O_NONBLOCK);
  close(pipe_out[1]);
  close(pipe_err[1]);

  //remaining program
  if(asLogical(wait)){
    int status;
    //-1 means error, 0 means running
    while (waitpid(pid, &status, WNOHANG) >= 0){
      if(pending_interrupt()){
        kill(pid, SIGINT); //pass interrupt to child
        //picked up below
      }
      //make sure to empty the pipes, even if fun == NULL
      ssize_t len;
      while ((len = read(pipe_out[0], buffer, sizeof(buffer))) > 0)
        R_callback(outfun, buffer, len);
      while ((len = read(pipe_err[0], buffer, sizeof(buffer))) > 0)
        R_callback(errfun, buffer, len);
    }
    close(pipe_out[0]);
    close(pipe_err[0]);
    if(WIFEXITED(status)){
      return ScalarInteger(WEXITSTATUS(status));
    } else {
      int signal = WTERMSIG(status);
      if(signal == SIGILL)
        Rf_errorcall(R_NilValue, "Failed to execute '%s'! Invalid path?", CHAR(STRING_ELT(command, 0)));
      if(signal != 0)
        Rf_errorcall(R_NilValue, "Program '%s' terminated by SIGNAL (%s)", CHAR(STRING_ELT(command, 0)), strsignal(signal));
      Rf_errorcall(R_NilValue, "Program terminated abnormally");
    }
  }
  return ScalarInteger(pid);
}
