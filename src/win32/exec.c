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

/* copy from R source */

const char *formatError(DWORD res){
  static char buf[1000], *p;
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, res,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buf, 1000, NULL);
  p = buf+strlen(buf) -1;
  if(*p == '\n') *p = '\0';
  p = buf+strlen(buf) -1;
  if(*p == '\r') *p = '\0';
  p = buf+strlen(buf) -1;
  if(*p == '.') *p = '\0';
  return buf;
}


/* check for system errors */
void bail_if(int err, const char * what){
  if(err)
    Rf_errorcall(R_NilValue, "System failure for: %s (%s)", what, formatError(GetLastError()));
}

void warn_if(int err, const char * what){
  if(err)
    Rf_warningcall(R_NilValue, "System failure for: %s (%s)", what, formatError(GetLastError()));
}

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
        Rprintf("ReadFile(pipe) failed (%d)\n", err);
      CloseHandle(pipe);
      ExitThread(0);
      return(0);
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
    bail_if(!PeekNamedPipe(pipe, NULL, 0, NULL, &len, NULL), "PeekNamedPipe");
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
  HANDLE out = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    &sa, CREATE_ALWAYS, dwFlags, NULL);
  bail_if(out == INVALID_HANDLE_VALUE, "CreateFile");
  return out;
}

BOOL CALLBACK closeWindows(HWND hWnd, LPARAM lpid) {
  DWORD pid = (DWORD)lpid;
  DWORD win;
  GetWindowThreadProcessId(hWnd, &win);
  if(pid == win)
    CloseWindow(hWnd);
  return TRUE;
}

void fin_proc(SEXP ptr){
  if(!R_ExternalPtrAddr(ptr)) return;
  CloseHandle(R_ExternalPtrAddr(ptr));
  R_ClearExternalPtr(ptr);
}

// Keeps one process handle open to let exec_status() read exit code
SEXP make_handle_ptr(HANDLE proc){
  SEXP ptr = PROTECT(R_MakeExternalPtr(proc, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ptr, fin_proc, 1);
  setAttrib(ptr, R_ClassSymbol, mkString("handle_ptr"));
  UNPROTECT(1);
  return ptr;
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
    bail_if(!CreatePipe(&pipe_out, &si.hStdOutput, &sa, 0), "CreatePipe stdout");
    bail_if(!SetHandleInformation(pipe_out, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdout");
  } else if(IS_STRING(outfun)){
    si.hStdOutput = fd(CHAR(STRING_ELT(outfun, 0)));
  }

  //set STDERR
  if(block || IS_TRUE(errfun)){
    bail_if(!CreatePipe(&pipe_err, &si.hStdError, &sa, 0), "CreatePipe stderr");
    bail_if(!SetHandleInformation(pipe_err, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdout");
  } else if(IS_STRING(errfun)){
    si.hStdError = fd(CHAR(STRING_ELT(errfun, 0)));
  }

  //append args into full command line
  size_t max_len = 32768;
  char argv[32769] = "";
  for(int i = 0; i < Rf_length(args); i++){
    size_t len = Rf_length(STRING_ELT(args, i));
    if(len > max_len)
      Rf_error("Command too long (max 32768)");
    strcat(argv, CHAR(STRING_ELT(args, i)));
    if(i < Rf_length(args) - 1)
      strcat(argv, " ");
    max_len = max_len - (len + 1);
  }
  PROCESS_INFORMATION pi = {0};
  const char * cmd = CHAR(STRING_ELT(command, 0));
  DWORD dwCreationFlags =  CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED;
  /* This will cause orphans unless we install a SIGBREAK handler on the child
  if(!block)
    dwCreationFlags |= CREATE_NEW_PROCESS_GROUP; //allows sending CTRL+BREAK
  */
  if(!CreateProcess(NULL, argv, &sa, &sa, TRUE, dwCreationFlags, NULL, NULL, &si, &pi))
    Rf_errorcall(R_NilValue, "Failed to execute '%s' (%s)", cmd, formatError(GetLastError()));

  //CloseHandle(pi.hThread);
  DWORD pid = GetProcessId(pi.hProcess);
  HANDLE proc = pi.hProcess;
  HANDLE thread = pi.hThread;

  //A 'job' is some sort of process container
  HANDLE job = CreateJobObject(NULL, NULL);
  bail_if(!AssignProcessToJobObject(job, proc), "AssignProcessToJobObject");
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
    warn_if(!CloseHandle(pipe_out), "CloseHandle pipe_out");
    warn_if(!CloseHandle(pipe_err), "CloseHandle pipe_err");
    warn_if(GetExitCodeProcess(proc, &exit_code) == 0, "GetExitCodeProcess");
    warn_if(!CloseHandle(proc), "CloseHandle proc");
    res = exit_code; //if wait=TRUE, return exit code
  } else {
    //create background threads to print stdout/stderr
    if(IS_TRUE(outfun))
      bail_if(!CreateThread(NULL, 0, PrintOut, pipe_out, 0, 0), "CreateThread stdout");
    if(IS_TRUE(errfun))
      bail_if(!CreateThread(NULL, 0, PrintErr, pipe_err, 0, 0), "CreateThread stderr");
  }
  CloseHandle(job);
  CloseHandle(si.hStdError);
  CloseHandle(si.hStdOutput);
  SEXP out = PROTECT(ScalarInteger(res));
  if(!block)
    setAttrib(out, install("handle"), make_handle_ptr(proc));
  UNPROTECT(1);
  return out;
}

SEXP R_exec_status(SEXP rpid, SEXP wait){
  DWORD exit_code = NA_INTEGER;
  int pid = asInteger(rpid);
  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
  bail_if(!proc, "OpenProcess()");
  do {
    DWORD res = WaitForSingleObject(proc, 200);
    bail_if(res == WAIT_FAILED, "WaitForSingleObject()");
    if(res != WAIT_TIMEOUT)
      break;
  } while(asLogical(wait) && !pending_interrupt());
  warn_if(GetExitCodeProcess(proc, &exit_code) == 0, "GetExitCodeProcess");
  CloseHandle(proc);
  return ScalarInteger(exit_code == STILL_ACTIVE ? NA_INTEGER : exit_code);
}

SEXP R_eval_fork(SEXP x, ...){
  Rf_error("eval_fork not available on windows");
}
