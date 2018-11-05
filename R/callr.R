#' Execute R from R
#'
#' Convenience wrappers for [exec_wait] and [exec_internal] that shell out to
#' R itself: `R.home("bin/R")`.
#'
#' This is a simple but robust way to invoke R commands in a separate process.
#' Use the [callr](https://cran.r-project.org/package=callr) package if you
#' need more sophisticated control over (multiple) R process jobs.
#'
#' @export
#' @rdname exec_r
#' @name exec_r
#' @family sys
#' @inheritParams exec
#' @param args command line arguments for R
#' @param std_in a file to send to stdin, usually an R script (see examples).
#' @examples # Hello world
#' r_wait("--version")
#'
#' # Run some code
#' r_wait(c('--vanilla', '-q', '-e', 'sessionInfo()'))
#'
#' # Run a script via stdin
#' tmp <- tempfile()
#' writeLines(c("x <- rnorm(100)", "mean(x)"), con = tmp)
#' r_wait(std_in = tmp)
r_wait <- function(args = '--vanilla', std_out = stdout(), std_err = stderr(), std_in = NULL){
  exec_wait(rbin(), args = args, std_out = std_out, std_err = std_err, std_in = std_in)
}

#' @export
#' @rdname exec_r
r_internal <- function(args = '--vanilla', std_in = NULL, error = TRUE){
  exec_internal(rbin(), args = args, std_in = std_in, error = error)
}

#' @export
#' @rdname exec_r
r_background <- function(args = '--vanilla', std_out = TRUE, std_err = TRUE, std_in = NULL){
  exec_background(rbin(), args = args, std_out = std_out, std_err = std_err, std_in = std_in)
}

rbin <- function(){
  cmd <- ifelse(.Platform$OS.type == 'windows', 'Rterm', 'R')
  file.path(R.home('bin'), cmd)
}
