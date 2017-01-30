#' Evaluate in fork
#'
#' Replacement for [eval] or [mcparallel][parallel::mcparallel] which evaluates an
#' expression in a temporary fork so that it has no side effects on the main R session.
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

#' @importFrom parallel mcparallel
set_interactive <- function(set){
  C_mc_interactive <- utils::getFromNamespace('C_mc_interactive', "parallel")
  .Call(C_mc_interactive, set)
}

