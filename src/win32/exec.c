#include <Rinternals.h>
#include <windows.h>


/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

BOOL CALLBACK closeWindows(HWND hWnd, LPARAM lpid) {
  DWORD pid = (DWORD)lpid;
  DWORD win;
  GetWindowThreadProcessId(hWnd, &win);
  if(pid == win)
    CloseWindow(hWnd);
  return TRUE;
}

SEXP C_run_with_pid(SEXP command, SEXP args, SEXP wait){
  SECURITY_ATTRIBUTES sa;
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
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  if(!CreateProcess(NULL, argv, &sa, &sa, TRUE, CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    Rf_errorcall(R_NilValue, "CreateProcess failed for %s", cmd);

  //CloseHandle(pi.hThread);
  DWORD pid = GetProcessId(pi.hProcess);
  HANDLE proc = pi.hProcess;
  HANDLE thread = pi.hThread;

  //A 'job' is some sort of process container
  HANDLE job = CreateJobObject(NULL, NULL);
  if(!AssignProcessToJobObject(job, proc))
    Rf_errorcall(R_NilValue, "AssignProcessToJobObject failed: %d", GetLastError());
  ResumeThread(thread);
  CloseHandle(thread);

  if(asLogical(wait)){
    while (WAIT_TIMEOUT == WaitForSingleObject(proc, 500)) {
      if(pending_interrupt()){
        EnumWindows(closeWindows, pid);
        if(!TerminateJobObject(job, -2))
          Rf_errorcall(R_NilValue, "TerminateJobObject failed: %d", GetLastError());
        /*** TerminateJobObject kills all procs and threads
        if(!TerminateThread(thread, 99))
          Rf_errorcall(R_NilValue, "TerminateThread failed %d", GetLastError());
        if(!TerminateProcess(proc, 99))
          Rf_errorcall(R_NilValue, "TerminateProcess failed: %d", GetLastError());
        */
      }
    }
    DWORD exit_code;
    GetExitCodeProcess(proc, &exit_code);
    CloseHandle(proc);
    CloseHandle(job);
    return ScalarInteger(exit_code);
  }
  CloseHandle(proc);
  CloseHandle(job);
  return ScalarInteger(pid);

}
