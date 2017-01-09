#' Execute a program
#'
#' Flexible and robust replacements for [base::system2].
#'
#' The `exec_with_wait` function runs a program and waits for it to complete.
#' If the process completes (either with success or error) it returns the status
#' code, but an error is raised when the process is terminated by a SIGNAL.
#' The R user can interrupt the program by sending SIGINT (press ESC or CTRL+C)
#' in which case the process tree is properly terminated.
#'
#' The `exec_with_pid` function returns immediately with the PID
#' of the child program. In this case the state of the child is unknown.
#' You should kill it manually with [tools::pskill]. This is useful for running
#' a server daemon or background process.
#'
#' @export
#' @rdname exec
#' @param cmd the command to run. Eiter a full path or the name of a program
#' which exists in the `PATH`.
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
