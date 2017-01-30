#include <Rinternals.h>
#include <unistd.h>
#include <signal.h>

static const int R_DefaultSerializeVersion = 2;
void bail_if(int err, const char * what);

/* Callback functions to serialize/unserialize via the pipe */
static void OutBytesCB(R_outpstream_t stream, void * buf, int size){
  int * results = stream->data;
  ssize_t remaining = size;
  while(remaining > 0){
    ssize_t written = write(results[1], buf, remaining);
    bail_if(written < 0, "write to pipe");
    remaining -= written;
    buf+= written;
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

SEXP R_eval_fork(SEXP call, SEXP env){
  int results[2];
  pipe(results);

  pid_t pid = fork();
  int fail = 99;
  if(pid == 0){
    //close read pipe
    close(results[0]);

    //execute
    SEXP object = R_tryEval(call, env, &fail);
    bail_if(write(results[1], &fail, sizeof(fail)) < 0, "write pipe");

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
    R_Serialize(fail ? mkString(R_curErrorBuf()) : object, &stream);

    //suicide
    close(results[1]);
    raise(SIGKILL);
  }

  //check if raised error
  close(results[1]);
  int bytes = read(results[0], &fail, sizeof(fail));
  bail_if(bytes < 0, "read pipe");
  if(bytes == 0)
    Rf_errorcall(call, "child process died");

  //unserialize stream
  struct R_inpstream_st stream;
  stream.data = results;
  stream.type = R_pstream_xdr_format;
  stream.InPersistHookFunc = NULL;
  stream.InPersistHookData = R_NilValue;
  stream.InBytes = InBytesCB;
  stream.InChar = InCharCB;

  //TODO: this can raise an error!
  SEXP res = R_Unserialize(&stream);

  //Check for error
  if(fail == 1){
    const char * err = "unknown error in forked process";
    if(isString(res) && Rf_length(res))
      err = CHAR(STRING_ELT(res, 0)) + 7;
    Rf_errorcall(call, err);
  }

  //cleanup and return
  kill(pid, SIGKILL);
  close(results[0]);
  return res;
}
