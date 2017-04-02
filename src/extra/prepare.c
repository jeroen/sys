#define R_INTERFACE_PTRS
#include <Rinterface.h>
#include <Rembedded.h>
#include <R_ext/Visibility.h>
#include <string.h>

extern char * Sys_TempDir;
extern Rboolean R_isForkedChild;
void write_out_ex(const char * buf, int size, int otype);

attribute_hidden static void prepare_fork_internal(const char * tmpdir){
  ptr_R_WriteConsole = NULL;
  ptr_R_WriteConsoleEx = write_out_ex;
  R_isForkedChild = 1;
  R_Interactive = 0;
  R_TempDir = strdup(tmpdir);
#ifndef HAVE_VISIBILITY_ATTRIBUTE
  Sys_TempDir = R_TempDir;
#endif
}

attribute_visible void prepare_fork(const char * tmpdir){
  prepare_fork_internal(tmpdir);
}
