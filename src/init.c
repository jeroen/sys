#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <signal.h>
#include <sys/wait.h>

void sys_sigchld_callback(int sig, siginfo_t *info, void *ctx) {
  if (sig != SIGCHLD) return;
  pid_t pid = info->si_pid;
  int wstat;
  waitpid(pid, &wstat, WNOHANG);
}

/* TODO: use oldact as well... */
static void setup_signal_handler() {
  struct sigaction action;
  action.sa_sigaction = sys_sigchld_callback;
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGCHLD, &action, /* oldact= */ NULL);
}

static void remove_signal_handler() {
  struct sigaction action;
  action.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &action, /* oldact= */ NULL);
}

void R_init_sys(DllInfo *info) {
  //setup_signal_handler();
  R_registerRoutines(info, NULL, NULL, NULL, NULL);
  R_useDynamicSymbols(info, TRUE);
}

void R_unload_sys(DllInfo *dll) {
  remove_signal_handler();
}
