#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>

/* .Call calls */
extern SEXP C_execute(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
extern SEXP R_exec_status(SEXP, SEXP);

static const R_CallMethodDef CallEntries[] = {
    {"C_execute",     (DL_FUNC) &C_execute,     7},
    {"R_exec_status", (DL_FUNC) &R_exec_status, 2},
    {NULL, NULL, 0}
};

void R_init_sys(DllInfo *dll){
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
