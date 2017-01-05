#include <Rinternals.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

SEXP C_run_with_pid(SEXP command, SEXP args, SEXP wait){
  pid_t pid = fork();
  int len = Rf_length(args);
  const char * argv[len + 1];
  argv[len] = NULL;
  for(int i = 0; i < len; i++){
    argv[i] = Rf_translateCharUTF8(STRING_ELT(args, i));
  }

  //this happens in the child
  if(pid == 0){
    execvp(CHAR(STRING_ELT(command, 0)), (char **) argv);

    //close all file descriptors before exit, otherwise they can segfault
    for (int i = 0; i < 4096; i++) close(i);

    // picked up by WTERMSIG() below in parent proc
    raise(SIGILL);
    //exit(0); //now allowed by CRAN. raise() should suffice anyway
  }

  //something went wrong
  if(pid < 0)
    Rf_errorcall(R_NilValue, "Failed to fork");

  //remaining program
  if(asLogical(wait)){
    int status;
    //-1 means error, 0 means running
    while (waitpid(pid, &status, WNOHANG) >= 0){
      if(pending_interrupt()){
        kill(pid, SIGINT); //pass interrupt to child
        //picked up below
      }
    }
    if(WIFEXITED(status)){
      return ScalarInteger(WEXITSTATUS(status));
    } else {
      int signal = WTERMSIG(status);
      if(signal == SIGILL)
        Rf_errorcall(R_NilValue, "Failed to execute '%s'! Invalid path?", CHAR(STRING_ELT(command, 0)));
      if(signal != 0)
        Rf_errorcall(R_NilValue, "Program '%s' terminated prematurely with SIGNAL %s", CHAR(STRING_ELT(command, 0)), strsignal(signal));
      return ScalarInteger(-1 * WTERMSIG(status));
    }
  }
  return ScalarInteger(pid);
}
