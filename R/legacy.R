#' Legacy functions
#'
#' These functions have moved into the `unix` package. Please update
#' your references.
#'
#' @export
#' @rdname legacy
#' @param ... see respective functions in the unix package
eval_safe <- function(...){
  unix::eval_safe(...)
}

#' @export
#' @rdname legacy
aa_config <- function(...){
  unix::aa_config(...)
}

set_tempdir <- function(...){
  unix:::set_tempdir(...)
}
