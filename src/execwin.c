#ifdef _WIN32
#include <Rinternals.h>
#include <windows.h>


/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

SEXP C_run_with_pid(SEXP command, SEXP args, SEXP wait){
  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {0};
  si.cb = sizeof(STARTUPINFO);
  const char * cmd = CHAR(STRING_ELT(command, 0));
  char argv[MAX_PATH];
  argv[0] = '\0';
  for(int i = 0; i < Rf_length(args); i++){
    strcat(argv, CHAR(STRING_ELT(args, i)));
    strcat(argv, " ");
  }
  if(!CreateProcess(cmd, argv, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    Rf_errorcall(R_NilValue, "CreateProcess failed for %s", cmd);

  CloseHandle(pi.hThread);
  DWORD dwPid = GetProcessId(pi.hProcess);
  HANDLE ff = pi.hProcess;
  if(asLogical(wait)){
    while (WAIT_TIMEOUT == WaitForSingleObject(ff, 500)) {
      if(pending_interrupt()){
        if(!TerminateProcess(ff, -2))
          Rf_errorcall(R_NilValue, "TerminateProcess failed: %d", GetLastError());
      }
    }
    DWORD exit_code;
    GetExitCodeProcess(ff, &exit_code);
    return ScalarInteger(exit_code);
  }
  CloseHandle(ff);
  return ScalarInteger(dwPid);

}
#endif
