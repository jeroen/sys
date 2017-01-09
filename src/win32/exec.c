#include <Rinternals.h>
#include <windows.h>


/* Check for interrupt without long jumping */
void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

/* Create FD in Windows */
HANDLE fd(const char * path){
  SECURITY_ATTRIBUTES sa;
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  DWORD dwFlags = FILE_ATTRIBUTE_NORMAL;
  return CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    &sa, CREATE_ALWAYS, dwFlags, NULL);
}

BOOL CALLBACK closeWindows(HWND hWnd, LPARAM lpid) {
  DWORD pid = (DWORD)lpid;
  DWORD win;
  GetWindowThreadProcessId(hWnd, &win);
  if(pid == win)
    CloseWindow(hWnd);
  return TRUE;
}

SEXP C_exec_internal(SEXP command, SEXP args, SEXP outfile, SEXP errfile, SEXP wait){
  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {0};
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags |= STARTF_USESTDHANDLES;

  // make STDOUT go to file
  int has_outfile = 0;
  if(!Rf_length(outfile)){
    si.hStdOutput = NULL;
  } else if(Rf_isString(outfile)){
    const char * path = CHAR(STRING_ELT(outfile, 0));
    if(strlen(path)){
      has_outfile = 1;
      si.hStdOutput = fd(path);
    } else {
      si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
  } else if(Rf_isLogical(outfile)){
    if(asLogical(outfile)){
      //TODO: outfile = TRUE
    } else {
      si.hStdOutput = NULL;
    }
  }

  // make STDERR go to file
  int has_errfile = 0;
  if(!Rf_length(errfile)){
    si.hStdError = NULL;
  } else if(Rf_isString(errfile)){
    const char * path = CHAR(STRING_ELT(errfile, 0));
    if(strlen(path)){
      has_errfile = 1;
      si.hStdError = fd(path);
    } else {
      si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
  } else if(Rf_isLogical(errfile)){
    if(asLogical(errfile)){
      //TODO: errfile = TRUE
    } else {
      si.hStdError = NULL;
    }
  }

  const char * cmd = CHAR(STRING_ELT(command, 0));
  char argv[MAX_PATH] = "";
  for(int i = 0; i < Rf_length(args); i++){
    strcat(argv, CHAR(STRING_ELT(args, i)));
    strcat(argv, " ");
  }
  SECURITY_ATTRIBUTES sa;
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

  int res = pid;
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
    res = exit_code; //if wait=TRUE, return exit code
  }
  CloseHandle(proc);
  CloseHandle(job);
  if(has_errfile)
    CloseHandle(si.hStdError);
  if(has_outfile)
    CloseHandle(si.hStdOutput);
  return ScalarInteger(res);
}
