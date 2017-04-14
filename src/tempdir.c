#include <Rinternals.h>
#include <Rembedded.h>
#include <string.h>

SEXP R_set_tempdir(SEXP path){
  const char * tmpdir = CHAR(STRING_ELT(path, 0));
#ifdef SYS_BUILD_SAFE
  R_TempDir = strdup(tmpdir);
#else
  Rf_error("Cannot set tempdir(), sys has been built without SYS_BUILD_SAFE");
#endif
  return path;
}
