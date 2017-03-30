#include <Rinternals.h>

extern int pending_interrupt();

SEXP R_freeze(SEXP interrupt) {
  int loop = 1;
  while(loop){
    if(asLogical(interrupt) && pending_interrupt())
      break;
    loop = 1+1;
  }
  return R_NilValue;
}
