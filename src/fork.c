#define R_INTERFACE_PTRS
#include <Rinterface.h>
#include <Rembedded.h>
#include <Rconfig.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

static const int R_DefaultSerializeVersion = 2;

#define r 0
#define w 1

#define waitms 200

extern Rboolean R_isForkedChild;
extern char * Sys_TempDir;
extern void bail_if(int err, const char * what);
extern void warn_if(int err, const char * what);
extern void safe_close(int fd);
extern void set_pipe(int input, int output[2]);
extern void pipe_set_read(int pipe[2]);
extern void check_interrupt_fn(void *dummy);
extern int pending_interrupt();
extern int wait_for_action2(int fd1, int fd2);
extern void print_output(int pipe_out[2], SEXP fun);
extern void kill_process_group(int signum);

static int wait_with_timeout(int fd, int ms){
  short events = POLLIN | POLLERR | POLLHUP;
  struct pollfd ufds = {fd, events, 0};
  if(poll(&ufds, 1, ms) > 0)
    return ufds.revents;
  return 0;
}

/* Callback functions to serialize/unserialize via the pipe */
static void OutBytesCB(R_outpstream_t stream, void * raw, int size){
  int * results = stream->data;
  char * buf = raw;
  ssize_t remaining = size;
  while(remaining > 0){
    ssize_t written = write(results[w], buf, remaining);
    bail_if(written < 0, "write to pipe");
    remaining -= written;
    buf += written;
  }
}

static void InBytesCB(R_inpstream_t stream, void *buf, int length){
  R_CheckUserInterrupt();
  int * results = stream->data;
  bail_if(read(results[r], buf, length) < 0, "read from pipe");
}

/* Not sure if these are ever needed */
static void OutCharCB(R_outpstream_t stream, int c){
  OutBytesCB(stream, &c, sizeof(c));
}

static int InCharCB(R_inpstream_t stream){
  int val;
  InBytesCB(stream, &val, sizeof(val));
  return val;
}

static SEXP unserialize_from_pipe(int results[2]){
  //unserialize stream
  struct R_inpstream_st stream;
  R_InitInPStream(&stream, results, R_pstream_xdr_format, InCharCB, InBytesCB, NULL,  R_NilValue);
  return R_Unserialize(&stream);
}

static void serialize_to_pipe(SEXP object, int results[2]){
  //serialize output
  struct R_outpstream_st stream;
  R_InitOutPStream(&stream, results, R_pstream_xdr_format, R_DefaultSerializeVersion, OutCharCB, OutBytesCB, NULL, R_NilValue);
  R_Serialize(object, &stream);
}

static void raw_to_pipe(SEXP object, int results[2]){
  R_xlen_t len = Rf_length(object);
  bail_if(write(results[w], &len, sizeof(len)) < sizeof(len), "raw_to_pipe: send size-byte");
  bail_if(write(results[w], RAW(object), len) < len, "raw_to_pipe: send raw data");
}

SEXP raw_from_pipe(int results[2]){
  R_xlen_t len = 0;
  bail_if(read(results[r], &len, sizeof(len)) < sizeof(len), "raw_from_pipe: read size-byte");
  SEXP out = Rf_allocVector(RAWSXP, len);
  unsigned char * ptr = RAW(out);
  while(len > 0){
    int bufsize = read(results[r], ptr, len);
    bail_if(bufsize <= 0, "failed to read from buffer");
    ptr += bufsize;
    len -= bufsize;
  }
  return out;
}

int Fake_ReadConsole(const char * a, unsigned char * b, int c, int d){
  return 0;
}

void My_R_Flush(){

}

/* do not wipe (shared) tempdir or run finalizers in forked process */
void My_R_CleanUp (SA_TYPE saveact, int status, int RunLast){
#ifdef SYS_BUILD_SAFE
  //R_RunExitFinalizers();
  Rf_KillAllDevices();
#endif
}

/* disables console, finalizers, interactivity inside forked procs */
void prepare_fork(const char * tmpdir, int fd_out, int fd_err){
#ifdef SYS_BUILD_SAFE
  //either set R_Outputfile+R_Consolefile OR ptr_R_WriteConsoleEx()
  R_Outputfile = fdopen(fd_out, "wb");
  R_Consolefile = fdopen(fd_err, "wb");
  ptr_R_WriteConsole = NULL;
  ptr_R_WriteConsoleEx = NULL;
  ptr_R_ResetConsole = My_R_Flush;
  ptr_R_FlushConsole = My_R_Flush;
  ptr_R_ReadConsole = Fake_ReadConsole;
  ptr_R_CleanUp = My_R_CleanUp;
  R_isForkedChild = 1;
  R_Interactive = 0;
  R_TempDir = strdup(tmpdir);
#ifndef HAVE_VISIBILITY_ATTRIBUTE
  Sys_TempDir = R_TempDir;
#endif
#endif
}

SEXP R_eval_fork(SEXP call, SEXP env, SEXP subtmp, SEXP timeout, SEXP outfun, SEXP errfun){
  int results[2];
  int pipe_out[2];
  int pipe_err[2];
  bail_if(pipe(results), "create results pipe");
  bail_if(pipe(pipe_out) || pipe(pipe_err), "create output pipes");

  //fork the main process
  int fail = -1;
  pid_t pid = fork();
  bail_if(pid < 0, "fork()");

  if(pid == 0){
    //prevents signals from being propagated to fork
    setpgid(0, 0);

    //close read pipe
    close(results[r]);

    //This breaks parallel! See issue #11
    safe_close(STDIN_FILENO);

    //Linux only: try to kill proccess group when parent dies
#ifdef PR_SET_PDEATHSIG
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    signal(SIGTERM, kill_process_group);
#endif

    //this is the hacky stuff
    prepare_fork(CHAR(STRING_ELT(subtmp, 0)), pipe_out[w], pipe_err[w]);

    //execute
    fail = 99; //not using this yet
    SEXP object = R_tryEval(call, env, &fail);

    //special case of raw vector
    if(fail == 0 && object != NULL && TYPEOF(object) == RAWSXP)
      fail = 1985;

    //try to send the 'success byte' and then output
    if(write(results[w], &fail, sizeof(fail)) > 0){
      if(fail == 1985){
        raw_to_pipe(object, results);
      } else if(fail == 0 && object){
        serialize_to_pipe(object, results);
      } else {
        const char * errbuf = R_curErrorBuf();
        serialize_to_pipe(mkString(errbuf ? errbuf : "unknown error in child"), results);
      }
    }

    //suicide
    close(results[w]);
    close(pipe_out[w]);
    close(pipe_err[w]);
    raise(SIGKILL);
  }

  //start timer
  struct timeval start, end;
  gettimeofday(&start, NULL);

  //start listening to child
  close(results[w]);
  pipe_set_read(pipe_out);
  pipe_set_read(pipe_err);
  int status = 0;
  int killcount = 0;
  double elapsed = 0;
  int is_timeout = 0;
  double totaltime = REAL(timeout)[0];
  while(status == 0){ //mabye test for: is_alive(pid) ?
    //wait for pipe to hear from child
    if(is_timeout || pending_interrupt()){
      //looks like rstudio always does SIGKILL, regardless
      warn_if(kill(pid, killcount == 0 ? SIGINT : killcount == 1 ? SIGTERM : SIGKILL), "kill child");
      status = wait_with_timeout(results[r], 500);
      killcount++;
    } else {
      wait_for_action2(pipe_out[r], pipe_err[r]);
      status = wait_with_timeout(results[r], 0);

      //empty pipes
      print_output(pipe_out, outfun);
      print_output(pipe_err, errfun);
      gettimeofday(&end, NULL);
      elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
      is_timeout = (totaltime > 0) && (elapsed > totaltime);
    }
  }
  warn_if(close(pipe_out[r]), "close stdout");
  warn_if(close(pipe_err[r]), "close stderr");
  bail_if(status < 0, "poll() on failure pipe");

  //read the 'success byte'
  SEXP res = R_NilValue;
  if(status > 0){
    int child_is_alive = read(results[r], &fail, sizeof(fail));
    bail_if(child_is_alive < 0, "read pipe");
    if(child_is_alive > 0){
      if(fail == 0){
        res = unserialize_from_pipe(results);
      } else if(fail == 1985){
        res = raw_from_pipe(results);
        fail = 0;
      }
    }
  }

  //cleanup
  close(results[r]);
  kill(-pid, SIGKILL); //kills entire process group
  waitpid(pid, NULL, 0); //wait for zombie(s) to die

  //actual R error
  if(status == 0 || fail){
    if(killcount && is_timeout){
      Rf_errorcall(call, "timeout reached (%f sec)", totaltime);
    } else if(killcount) {
      Rf_errorcall(call, "process interrupted by parent");
    } else if(isString(res) && Rf_length(res) && Rf_length(STRING_ELT(res, 0)) > 8){
      Rf_errorcall(R_NilValue, CHAR(STRING_ELT(res, 0)));
    }
    Rf_errorcall(call, "child process has died");
  }

  //add timeout attribute
  return res;
}
