#include <Rinternals.h>
#include <windows.h>
#include <sys/time.h>

/* NOTES
 * On Windows, when wait = FALSE and std_out = TRUE or std_err = TRUE
 * then stdout / stderr get piped to background threads to simulate
 * the unix behavior of inheriting stdout/stderr in by child.
 */

#define IS_STRING(x) (Rf_isString(x) && Rf_length(x))
#define IS_TRUE(x) (Rf_isLogical(x) && Rf_length(x) && asLogical(x))
#define IS_FALSE(x) (Rf_isLogical(x) && Rf_length(x) && !asLogical(x))

/* copy from R source */

static const char *formatError(DWORD res){
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
static void bail_if(int err, const char * what){
  if(err)
    Rf_errorcall(R_NilValue, "System failure for: %s (%s)", what, formatError(GetLastError()));
}

static void warn_if(int err, const char * what){
  if(err)
    Rf_warningcall(R_NilValue, "System failure for: %s (%s)", what, formatError(GetLastError()));
}

/* Check for interrupt without long jumping */
static void check_interrupt_fn(void *dummy) {
  R_CheckUserInterrupt();
}

int pending_interrupt() {
  return !(R_ToplevelExec(check_interrupt_fn, NULL));
}

static int str_to_wchar(const char * str, wchar_t **wstr){
  int len = MultiByteToWideChar( CP_UTF8 , 0 , str , -1, NULL , 0 );
  *wstr = calloc(len, sizeof(*wstr));
  MultiByteToWideChar( CP_UTF8 , 0 , str , -1, *wstr , len );
  return len;
}

static wchar_t* sexp_to_wchar(SEXP args){
  int total = 1;
  wchar_t *out = calloc(total, sizeof(*out));
  wchar_t *space = NULL;
  int spacelen = str_to_wchar(" ", &space);
  for(int i = 0; i < Rf_length(args); i++){
    wchar_t *arg = NULL;
    const char *str = CHAR(STRING_ELT(args, i));
    int len = str_to_wchar(str, &arg);
    total = total + len;
    out = realloc(out, (total + spacelen) * sizeof(*out));
    if(wcsncat(out, arg, len) == NULL)
      Rf_error("Failure in wcsncat");
    if(i < Rf_length(args) - 1 && wcsncat(out, space, spacelen) == NULL)
      Rf_error("Failure in wcsncat");
    free(arg);
  }
  return out;
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

static void ReadFromPipe(SEXP fun, HANDLE pipe){
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

static HANDLE fd_read(const char *path){
  SECURITY_ATTRIBUTES sa = {0};
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  DWORD dwFlags = FILE_ATTRIBUTE_NORMAL;
  wchar_t *wpath;
  str_to_wchar(path, &wpath);
  HANDLE out = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ,
                          &sa, OPEN_EXISTING, dwFlags, NULL);
  free(wpath);
  bail_if(out == INVALID_HANDLE_VALUE, "CreateFile");
  return out;
}

/* Create FD in Windows */
static HANDLE fd_write(const char * path){
  SECURITY_ATTRIBUTES sa = {0};
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;
  DWORD dwFlags = FILE_ATTRIBUTE_NORMAL;
  wchar_t *wpath;
  str_to_wchar(path, &wpath);
  HANDLE out = CreateFileW(wpath, GENERIC_WRITE, FILE_SHARE_WRITE,
                    &sa, CREATE_ALWAYS, dwFlags, NULL);
  free(wpath);
  bail_if(out == INVALID_HANDLE_VALUE, "CreateFile");
  return out;
}

static BOOL CALLBACK closeWindows(HWND hWnd, LPARAM lpid) {
  DWORD pid = (DWORD)lpid;
  DWORD win;
  GetWindowThreadProcessId(hWnd, &win);
  if(pid == win)
    CloseWindow(hWnd);
  return TRUE;
}

static void fin_proc(SEXP ptr){
  if(!R_ExternalPtrAddr(ptr)) return;
  CloseHandle(R_ExternalPtrAddr(ptr));
  R_ClearExternalPtr(ptr);
}

// Keeps one process handle open to let exec_status() read exit code
static SEXP make_handle_ptr(HANDLE proc){
  SEXP ptr = PROTECT(R_MakeExternalPtr(proc, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ptr, fin_proc, 1);
  setAttrib(ptr, R_ClassSymbol, mkString("handle_ptr"));
  UNPROTECT(1);
  return ptr;
}

SEXP C_execute(SEXP command, SEXP args, SEXP outfun, SEXP errfun, SEXP wait, SEXP input, SEXP timeout){
  int block = asLogical(wait);
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle = TRUE;

  STARTUPINFOW si = {0};
  si.cb = sizeof(STARTUPINFOW);
  si.dwFlags |= STARTF_USESTDHANDLES;
  HANDLE pipe_out = NULL;
  HANDLE pipe_err = NULL;

  //set STDOUT pipe
  if(block || IS_TRUE(outfun)){
    bail_if(!CreatePipe(&pipe_out, &si.hStdOutput, &sa, 0), "CreatePipe stdout");
    bail_if(!SetHandleInformation(pipe_out, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdout");
  } else if(IS_STRING(outfun)){
    si.hStdOutput = fd_write(CHAR(STRING_ELT(outfun, 0)));
  }

  //set STDERR
  if(block || IS_TRUE(errfun)){
    bail_if(!CreatePipe(&pipe_err, &si.hStdError, &sa, 0), "CreatePipe stderr");
    bail_if(!SetHandleInformation(pipe_err, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation stdout");
  } else if(IS_STRING(errfun)){
    si.hStdError = fd_write(CHAR(STRING_ELT(errfun, 0)));
  }

  if(IS_STRING(input)){
    si.hStdInput = fd_read(CHAR(STRING_ELT(input, 0)));
  }

  //append args into full command line
  wchar_t *argv = sexp_to_wchar(args);
  if(wcslen(argv) >= 32768)
    Rf_error("Windows commands cannot be longer than 32,768 characters");
  PROCESS_INFORMATION pi = {0};
  const char * cmd = CHAR(STRING_ELT(command, 0));
  DWORD dwCreationFlags =  CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED;
  /* This will cause orphans unless we install a SIGBREAK handler on the child
  if(!block)
    dwCreationFlags |= CREATE_NEW_PROCESS_GROUP; //allows sending CTRL+BREAK
  */

  //printf("ARGV: %S\n", argv); //NOTE capital %S for formatting wchar_t str
  if(!CreateProcessW(NULL, argv, &sa, &sa, TRUE, dwCreationFlags, NULL, NULL, &si, &pi)){
    //Failure to start, probably non existing program. Cleanup.
    CloseHandle(pipe_out); CloseHandle(pipe_err);
    CloseHandle(si.hStdInput); CloseHandle(si.hStdOutput); CloseHandle(si.hStdInput);
    Rf_errorcall(R_NilValue, "Failed to execute '%s' (%s)", cmd, formatError(GetLastError()));
  }

  //CloseHandle(pi.hThread);
  DWORD pid = GetProcessId(pi.hProcess);
  HANDLE proc = pi.hProcess;
  HANDLE thread = pi.hThread;

  //A 'job' is some sort of process container
  HANDLE job = CreateJobObject(NULL, NULL);
  bail_if(!AssignProcessToJobObject(job, proc), "AssignProcessToJobObject");
  ResumeThread(thread);
  CloseHandle(thread);
  free(argv);

  //start timer
  int timeout_reached = 0;
  struct timeval start, end;
  double totaltime = REAL(timeout)[0];
  gettimeofday(&start, NULL);

  int res = pid;
  const HANDLE all_handles[3] = {proc, pipe_out, pipe_err};
  if(block){
    int running = 1;
    while(running){
      int num_handles = (pipe_out && pipe_err) ? 3 : 1;
      running = WaitForMultipleObjects(num_handles, all_handles, 0, 200);
      ReadFromPipe(outfun, pipe_out);
      ReadFromPipe(errfun, pipe_err);

      //check for timeout
      if(totaltime > 0){
        gettimeofday(&end, NULL);
        timeout_reached = ((end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6) > totaltime;
      }
      if(pending_interrupt() || timeout_reached){
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
  CloseHandle(si.hStdInput);
  if(timeout_reached && res){
    Rf_errorcall(R_NilValue, "Program '%s' terminated (timeout reached: %.2fsec)",
                 CHAR(STRING_ELT(command, 0)), totaltime);
  }
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
