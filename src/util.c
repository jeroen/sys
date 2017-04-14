#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <Rembedded.h>
#include <string.h>

void R_init_sys(DllInfo *info) {
  R_registerRoutines(info, NULL, NULL, NULL, NULL);
  R_useDynamicSymbols(info, TRUE);
}

SEXP R_safe_build(){
#ifdef SYS_BUILD_SAFE
  return ScalarLogical(TRUE);
#else
  return ScalarLogical(FALSE);
#endif
}


SEXP R_set_tempdir(SEXP path){
  const char * tmpdir = CHAR(STRING_ELT(path, 0));
#ifdef SYS_BUILD_SAFE
  R_TempDir = strdup(tmpdir);
#else
  Rf_error("Cannot set tempdir(), sys has been built without SYS_BUILD_SAFE");
#endif
  return path;
}
