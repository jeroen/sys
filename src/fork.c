#include <Rinternals.h>
#include <Rinterface.h>
#include <Rembedded.h>
#include <Rconfig.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>

extern Rboolean R_isForkedChild;
extern void warn_if(int err, const char * what);
extern void bail_if(int err, const char * what);
extern int pending_interrupt();
extern char * Sys_TempDir;

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define waitms 200
static const int R_DefaultSerializeVersion = 2;

static int wait_for_action1(int fd){
  short events = POLLIN | POLLERR | POLLHUP;
  struct pollfd ufds = {fd, events, events};
  return poll(&ufds, 1, waitms);
}

/* Callback functions to serialize/unserialize via the pipe */
static void OutBytesCB(R_outpstream_t stream, void * raw, int size){
  int * results = stream->data;
  char * buf = raw;
  ssize_t remaining = size;
  while(remaining > 0){
    ssize_t written = write(results[1], buf, remaining);
    bail_if(written < 0, "write to pipe");
    remaining -= written;
    buf += written;
  }
}

static void InBytesCB(R_inpstream_t stream, void *buf, int length){
  R_CheckUserInterrupt();
  int * results = stream->data;
  bail_if(read(results[0], buf, length) < 0, "read from pipe");
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

static void serialize_to_pipe(SEXP object, int results[2]){
  //serialize output
  struct R_outpstream_st stream;
  stream.data = results;
  stream.type = R_pstream_xdr_format;
  stream.version = R_DefaultSerializeVersion;
  stream.OutChar = OutCharCB;
  stream.OutBytes = OutBytesCB;
  stream.OutPersistHookFunc = NULL;
  stream.OutPersistHookData = R_NilValue;

  //TODO: this can raise an error so that the process never dies!
  R_Serialize(object, &stream);
}

void prepare_fork(const char * tmpdir){
#ifndef R_SYS_BUILD_CLEAN
  R_isForkedChild = 1;
  R_Interactive = 0;
  R_TempDir = strdup(tmpdir);
#ifndef HAVE_VISIBILITY_ATTRIBUTE
  Sys_TempDir = R_TempDir;
#endif
#endif
}

static SEXP unserialize_from_pipe(int results[2]){
  //unserialize stream
  struct R_inpstream_st stream;
  stream.data = results;
  stream.type = R_pstream_xdr_format;
  stream.InPersistHookFunc = NULL;
  stream.InPersistHookData = R_NilValue;
  stream.InBytes = InBytesCB;
  stream.InChar = InCharCB;

  //TODO: this can raise an error!
  return R_Unserialize(&stream);
}

SEXP R_eval_fork(SEXP call, SEXP env, SEXP subtmp, SEXP timeout, SEXP silent){
  int results[2];
  bail_if(pipe(results), "create pipe");

  //fork the main process
  pid_t pid = fork();
  bail_if(pid < 0, "fork()");

  int fail = 99;
  if(pid == 0){
    //prevents signals from being propagated to fork
    setpgid(0, 0);

    //this is the hacky stuff
    prepare_fork(CHAR(STRING_ELT(subtmp, 0)));

    //close stdout
    if(asLogical(silent)){
      int fdnull = open("/dev/null", O_WRONLY);
      dup2(fdnull, STDOUT_FILENO);
      close(fdnull);
    }

    //close read pipe
    close(results[0]);

    //execute
    SEXP object = R_tryEval(call, env, &fail);

    //try to send the 'success byte' and then output
    if(write(results[1], &fail, sizeof(fail)) > 0){
      const char * errbuf = R_curErrorBuf();
      serialize_to_pipe(fail || object == NULL ? mkString(errbuf ? errbuf : "unknown error in child") : object, results);
    }

    //suicide
    close(results[1]);
    raise(SIGKILL);
  }

  //wait for pipe to hear from child
  close(results[1]);
  int status = 0;
  int killcount = 0;
  int timeoutms = REAL(timeout)[0] * 1000;
  int elapsedms = 0;
  while(status < 1){
    if(pending_interrupt() || elapsedms >= timeoutms){
      warn_if(kill(pid, killcount ? SIGKILL : SIGINT), "kill child");
      killcount++;
    }
    status = wait_for_action1(results[0]);
    elapsedms += waitms;
  }
  bail_if(status < 0, "poll() on failure pipe");

  //read the 'success byte'
  int bytes = read(results[0], &fail, sizeof(fail));
  bail_if(bytes < 0, "read pipe");

  //still alive: reading data
  SEXP res = (bytes > 0) ? unserialize_from_pipe(results) : R_NilValue;

  //cleanup
  close(results[0]);
  kill(-pid, SIGKILL); //kills entire process group

  //collect exit code which cleans up the zombie process
  waitpid(pid, NULL, 0);

  //check for process error
  if(bytes == 0)
    Rf_errorcall(call, killcount ? "process died after interrupt" : "child process died");

  //Check for failures. Rarely happens, R errors should get caucht by tryCatch.
  if(fail){
    if(killcount && elapsedms >= timeoutms)
      Rf_errorcall(call, "timeout reached (%d ms)", timeoutms);
    if(killcount)
      Rf_errorcall(call, "process interrupted by parent");
    if(isString(res) && Rf_length(res) && Rf_length(STRING_ELT(res, 0)) > 8)
      Rf_errorcall(call, CHAR(STRING_ELT(res, 0)) + 7);
    Rf_errorcall(call, "unknown error");
  }

  //add timeout attribute
  if(inherits(res, "eval_fork_error") && killcount && elapsedms >= timeoutms)
    SET_VECTOR_ELT(res, 0, mkString("timeout reached"));
  return res;
}
