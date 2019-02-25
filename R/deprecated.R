#' Deprecated functions
#'
#' These functions have moved into the `unix` package. Please update
#' your references.
#'
#' @export
#' @name sys-deprecated
#' @rdname deprecated
#' @param ... see respective functions in the unix package
eval_safe <- function(...){
  .Deprecated('unix::eval_safe', 'sys')
  unix::eval_safe(...)
}

#' @export
#' @rdname deprecated
eval_fork <- function(...){
  .Deprecated('unix::eval_fork', 'sys')
  unix::eval_fork(...)
}
