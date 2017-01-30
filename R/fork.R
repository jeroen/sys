#' Evaluate in fork
#'
#' Replacement for [eval] or [mcparallel][parallel::mcparallel] which evaluates an
#' expression in a temporary fork so that it has no side effects on the main R session.
#'
#' @export
#' @useDynLib sys R_eval_fork
#' @param expr expression to evaluate
#' @param envir the [environment] in which expr is to be evaluated.
eval_fork <- function(expr, envir = parent.frame()){
  .Call(R_eval_fork, substitute(expr), envir)
}

#' @importFrom parallel mcparallel
set_interactive <- function(set){
  C_mc_interactive <- utils::getFromNamespace('C_mc_interactive', "parallel")
  .Call(C_mc_interactive, set)
}

