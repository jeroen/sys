#include <Rinternals.h>
#include <R_ext/Rdynload.h>

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
