#' Legacy functions
#'
#' These functions have moved into the `unix` package.
#'
#' @export
#' @param ... see `?eval_safe` in unix
eval_safe <- function(...){
  unix::eval_safe(...)
}
