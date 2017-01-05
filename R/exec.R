#' Execute a program
#'
#' The `exec_with_wait` function runs a program and returns the status code
#' of the child program. Negative codes indicates the program was terminated
#' by a SIGNAL. The `exec_with_pid` function returns immediately with the PID
#' of the child program. In this case the state of the child is unknown.
#'
#' @export
#' @rdname exec
#' @param cmd the command to run
#' @param args character vector of arguments to pass
#' @useDynLib sys C_run_with_pid
exec_with_wait <- function(cmd, args = NULL){
  argv <- c(cmd, as.character(args))
  .Call(C_run_with_pid, cmd, argv, TRUE)
}

#' @export
#' @rdname exec
exec_with_pid <- function(cmd, args = NULL){
  argv <- c(cmd, as.character(args))
  .Call(C_run_with_pid, cmd, argv, FALSE)
}
