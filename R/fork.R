#' Evaluate in fork
#'
#' Version of [eval] which evaluates expression in a temporary fork so
#' that it has no side effects on the main R session.
#' Basically a robust version of `mccollect(mcparallel(expr))`.
#' Not available on Windows because it required `fork()`.
#'
#' @export
#' @param expr expression to evaluate
#' @param envir the [environment] in which expr is to be evaluated
#' @param tmp the value of [tempdir] inside the forked process
#' @param timeout maximum time in seconds to allow for call to return
eval_fork <- function(expr, envir = parent.frame(), tmp = tempfile("fork"), timeout = 60){
  if(!file.exists(tmp))
    dir.create(tmp)
  clexpr <- call("eval", substitute(expr), envir)
  trexpr <- call('tryCatch', expr = clexpr,
    interrupt = function(e){
      stop(simpleError("process interrupted by user", call = clexpr))
    },
    error = function(e){
      structure(e, class = "eval_fork_error")
    }
  )
 out <- eval_fork_internal(trexpr, envir, tmp, timeout)
 if(inherits(out, "eval_fork_error")){
   if(is.numeric(attr(out, "timeout"))){
     stop(simpleError(sprintf("timeout reached (%dms)", timeout*1000), out$call[[2]]))
   }
   stop(simpleError(out$message, out$call[[2]]))
 }
 return(out)
}

#' @useDynLib sys R_eval_fork
eval_fork_internal <- function(expr, envir, tmp, timeout){
  .Call(R_eval_fork, expr, envir, tmp, timeout)
}
