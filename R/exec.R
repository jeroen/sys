#' Run a system command
#'
#' Flexible replacements for [system2] / [pipe] with support for interruptions,
#' background tasks and proper control over `stdout` / `stderr` streams.
#'
#' The `exec_wait` function runs a system command and waits for the child process
#' to exit. The `STDOUT` and `STDERR` streams are piped back to the parent process
#' and available as a connection or callback funtion. If the child process completes
#' normally (either success or error) `exec_wait` returns the program exit code.
#' On the other hand, when the child process is terminated by a SIGNAL, an error is
#' raised in R. The R user can interrupt the program by sending SIGINT (press
#' ESC or CTRL+C) in which case the child process tree is properly terminated.
#'
#' The `exec_background` function starts the program and immediately returns the
#' PID of the child process. Because this is non-blocking, `std_out` and `std_out`
#' can only be `NULL` or a file path. The state of the process is not controlled by
#' R but the child can be killed manually with [tools::pskill]. This is useful for
#' running a server daemon or background process.
#'
#' @export
#' @seealso Base [system2] and [pipe] provide other methods for running a system
#' command with output.
#' @rdname exec
#' @param cmd the command to run. Eiter a full path or the name of a program
#' which exists in the `PATH`.
#' @param args character vector of arguments to pass
#' @param std_out filename to redirect program `STDOUT` stream. For `exec_wait` this may
#' also be a connection or callback function.
#' @param std_err filename to redirect program `STDERR` stream. For `exec_wait` this may
#' also be a connection or callback function.
exec_wait <- function(cmd, args = NULL, std_out = stdout(), std_err = stderr()){
  if(is.character(std_out)){
    std_out <- file(normalizePath(std_out, mustWork = FALSE), open = "w+")
    on.exit(close(std_out), add = TRUE)
  }
  if(is.character(std_err)){
    std_err <- file(normalizePath(std_err, mustWork = FALSE), open = "w+")
    on.exit(close(std_err), add = TRUE)
  }
  outfun <- if(inherits(std_out, "connection")){
    function(x){
      cat(x, file = std_out)
      flush(std_out)
    }
  }
  errfun <- if(inherits(std_err, "connection")){
    function(x){
      cat(x, file = std_err)
      flush(std_err)
    }
  }
  exec_internal(cmd, args, outfun, errfun, wait = TRUE)
}

#' @export
#' @rdname exec
exec_background <- function(cmd, args = NULL, std_out = NULL, std_err = NULL){
  if(length(std_out))
    stopifnot(is.character(std_out))
  if(length(std_err))
    stopifnot(is.character(std_err))
  exec_internal(cmd, args, std_out, std_err, wait = FALSE)
}

#' @useDynLib sys C_exec_internal
exec_internal <- function(cmd, args, std_out, std_err, wait){
  stopifnot(is.character(cmd))
  stopifnot(is.logical(wait))
  argv <- c(cmd, as.character(args))
  .Call(C_exec_internal, cmd, argv, std_out, std_err, wait)
}
