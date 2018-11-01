#include <Rinternals.h>
#include <R_ext/Rdynload.h>

/* .Call calls */
extern SEXP C_execute(SEXP, SEXP, SEXP, SEXP, SEXP);
extern SEXP R_aa_change_profile(SEXP);
extern SEXP R_aa_getcon();
extern SEXP R_aa_is_enabled();
extern SEXP R_eval_fork(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
extern SEXP R_exec_status(SEXP, SEXP);
extern SEXP R_freeze(SEXP);
extern SEXP R_have_apparmor();
extern SEXP R_safe_build();
extern SEXP R_set_interactive(SEXP);
extern SEXP R_set_priority(SEXP);
extern SEXP R_set_rlimits(SEXP);
extern SEXP R_set_tempdir(SEXP);
extern SEXP R_setgid(SEXP);
extern SEXP R_setuid(SEXP);

static const R_CallMethodDef CallEntries[] = {
    {"C_execute",           (DL_FUNC) &C_execute,           6},
    {"R_aa_change_profile", (DL_FUNC) &R_aa_change_profile, 1},
    {"R_aa_getcon",         (DL_FUNC) &R_aa_getcon,         0},
    {"R_aa_is_enabled",     (DL_FUNC) &R_aa_is_enabled,     0},
    {"R_eval_fork",         (DL_FUNC) &R_eval_fork,         6},
    {"R_exec_status",       (DL_FUNC) &R_exec_status,       2},
    {"R_freeze",            (DL_FUNC) &R_freeze,            1},
    {"R_have_apparmor",     (DL_FUNC) &R_have_apparmor,     0},
    {"R_safe_build",        (DL_FUNC) &R_safe_build,        0},
    {"R_set_interactive",   (DL_FUNC) &R_set_interactive,   1},
    {"R_set_priority",      (DL_FUNC) &R_set_priority,      1},
    {"R_set_rlimits",       (DL_FUNC) &R_set_rlimits,       1},
    {"R_set_tempdir",       (DL_FUNC) &R_set_tempdir,       1},
    {"R_setgid",            (DL_FUNC) &R_setgid,            1},
    {"R_setuid",            (DL_FUNC) &R_setuid,            1},
    {NULL, NULL, 0}
};

void R_init_sys(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}
