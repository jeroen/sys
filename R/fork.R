#' Evaluate in fork
#'
#' Version of [eval] which evaluates expression in a temporary fork so
#' that it has no side effects on the main R session.
#' Basically a robust version of `mccollect(mcparallel(expr))`.
#' Not available on Windows because it required `fork()`.
#'
#' @export
#' @useDynLib sys R_eval_fork
#' @param expr expression to evaluate
#' @param envir the [environment] in which expr is to be evaluated
#' @param tmp the value of [tempdir] inside the forked process
eval_fork <- function(expr, envir = parent.frame(), tmp = tempfile("fork")){
  if(!file.exists(tmp))
    dir.create(tmp)
  .Call(R_eval_fork, substitute(expr), envir, tmp)
}


