#include <Rinternals.h>
#include <windows.h>

/* NOTES
 * On Windows, when wait = FALSE and std_out = TRUE or std_err = TRUE
 * then stdout / stderr get piped to background threads to simulate
 * the unix behavior of inheriting stdout/stderr in by child.
 */

#define IS_STRING(x) (Rf_isString(x) && Rf_length(x))
#define IS_TRUE(x) (Rf_isLogical(x) && Rf_length(x) && asLogical(x))
#define IS_FALSE(x) (Rf_isLogical(x) && Rf_length(x) && !asLogical(x))

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
  SEXP str = PROTECT(allocVector(RAWSXP, len));
  memcpy(RAW(str), buf, len);
  SEXP call = PROTECT(LCONS(fun, LCONS(str, R_NilValue)));
  R_tryEval(call, R_GlobalEnv, &ok);
  UNPROTECT(2);
}

//ReadFile blocks so no need to sleep()
//Do NOT call RPrintf here because R is not thread safe!
static DWORD WINAPI PrintPipe(HANDLE pipe, FILE *stream){
  while(1){
    unsigned long len;
    char buffer[65336];
    if(!ReadFile(pipe, buffer, 65337, &len, NULL)){
      int err = GetLastError();
      if(err != ERROR_BROKEN_PIPE)
        printf("ReadFile(pipe) failed (%d)\n", err);
      CloseHandle(pipe);
      ExitThread(0);
    }
    fprintf(stream, "%.*s", (int) len, buffer);
  }
}

static DWORD WINAPI PrintOut(HANDLE pipe){
  return PrintPipe(pipe, stdout);
}

static DWORD WINAPI PrintErr(HANDLE pipe){
  return PrintPipe(pipe, stderr);
}

void ReadFromPipe(SEXP fun, HANDLE pipe){
  unsigned long len = 1;
  while(1){
    if(!PeekNamedPipe(pipe, NULL, 0, NULL, &len, NULL))
      Rf_errorcall(R_NilValue, "PeekNamedPipe failed");
    if(!len)
      break;
    char buffer[len];
    unsigned long outlen;
    if(ReadFile(pipe, buffer, len, &outlen, NULL))
      R_callback(fun, buffer, outlen);
  }
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

SEXP C_execute(SEXP command, SEXP args, SEXP outfun, SEXP errfun, SEXP wait){
  int block = asLogical(wait);
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  STARTUPINFO si = {0};
  si.cb = sizeof(STARTUPINFO);
  si.dwFlags |= STARTF_USESTDHANDLES;
  HANDLE pipe_out = NULL;
  HANDLE pipe_err = NULL;

  //set STDOUT pipe
  if(block || IS_TRUE(outfun)){
    if (!CreatePipe(&pipe_out, &si.hStdOutput, &sa, 0))
      Rf_errorcall(R_NilValue, "Failed to creat stdout pipe");
    if (!SetHandleInformation(pipe_out, HANDLE_FLAG_INHERIT, 0))
      Rf_errorcall(R_NilValue, "SetHandleInformation failed");
  } else if(IS_STRING(outfun)){
    si.hStdOutput = fd(CHAR(STRING_ELT(outfun, 0)));
  }

  //set STDERR
  if(block || IS_TRUE(errfun)){
    if (!CreatePipe(&pipe_err, &si.hStdError, &sa, 0))
      Rf_errorcall(R_NilValue, "Failed to creat stdout pipe");
    if (!SetHandleInformation(pipe_err, HANDLE_FLAG_INHERIT, 0))
      Rf_errorcall(R_NilValue, "SetHandleInformation failed");
  } else if(IS_STRING(errfun)){
    si.hStdError = fd(CHAR(STRING_ELT(errfun, 0)));
  }

  //make command
  const char * cmd = CHAR(STRING_ELT(command, 0));
  char argv[MAX_PATH] = "";
  for(int i = 0; i < Rf_length(args); i++){
    strcat(argv, CHAR(STRING_ELT(args, i)));
    strcat(argv, " ");
  }
  PROCESS_INFORMATION pi = {0};
  if(!CreateProcess(NULL, argv, &sa, &sa, TRUE, CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    Rf_errorcall(R_NilValue, "Failed to execute '%s'", cmd);

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
  if(block){
    int running = 1;
    while(running){
      running = WaitForSingleObject(proc, 200);
      ReadFromPipe(outfun, pipe_out);
      ReadFromPipe(errfun, pipe_err);
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
    CloseHandle(pipe_out);
    CloseHandle(pipe_err);
    GetExitCodeProcess(proc, &exit_code);
    res = exit_code; //if wait=TRUE, return exit code
  } else {
    //create background threads to print stdout/stderr
    if(IS_TRUE(outfun))
      CreateThread(NULL, 0, PrintOut, pipe_out, 0, 0);
    if(IS_TRUE(errfun))
      CreateThread(NULL, 0, PrintErr, pipe_err, 0, 0);
  }
  CloseHandle(proc);
  CloseHandle(job);
  CloseHandle(si.hStdError);
  CloseHandle(si.hStdOutput);
  return ScalarInteger(res);
}
