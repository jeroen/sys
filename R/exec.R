#' Execute a program
#'
#' Flexible and robust replacements for [base::system2] / [base::pipe]. Output
#' from the child process is piped back to R and can be used via `stdout` and
#' `stderr` callback functions.
#'
#' The `exec_wait` function runs a system command and waits for it to complete.
#' If the process completes (either with success or error) it returns the status
#' code, but an error is raised when the process is terminated by a SIGNAL.
#' The program can be interrupted by sending SIGINT (press ESC or CTRL+C) in
#' which case the process tree is properly terminated.
#'
#' The `exec_background` function started the program and immediately returns the
#' PID of the child process. In this case the state of the child is unknown.
#' You should kill it manually with [tools::pskill]. This is useful for running
#' a server daemon or background process.
#'
#' @export
#' @seealso Base [system2] and [pipe] provide other methods for running a system
#' command with output.
#' @rdname exec
#' @param cmd the command to run. Eiter a full path or the name of a program
#' which exists in the `PATH`.
#' @param args character vector of arguments to pass
#' @param stdout callback function to process `STDOUT` text, or a file path to
#' pipe `STDOUT` to, or `NULL` to silence.
#' @param stderr callback function to process `STDERR` text, or a file path to
#' pipe `STDERR` to, or `NULL` to silence.
exec_wait <- function(cmd, args = NULL, stdout = cat, stderr = cat){
  exec_internal(cmd, args, stdout, stderr, wait = TRUE)
}

#' @export
#' @rdname exec
exec_background <- function(cmd, args = NULL, stdout = cat, stderr = cat){
  exec_internal(cmd, args, stdout, stderr, wait = FALSE)
}

#' @useDynLib sys C_exec_internal
exec_internal <- function(cmd, args, stdout, stderr, wait){
  stopifnot(is.character(cmd))
  argv <- c(cmd, as.character(args))
  if(is.character(stdout)){
    outfile <- file(normalizePath(stdout, mustWork = FALSE), open = "w+")
    on.exit(close(outfile))
    stdout <- function(x){
      writeLines(x, outfile)
      flush(outfile)
    }
  }
  if(is.character(stderr)){
    errfile <- file(normalizePath(stderr, mustWork = FALSE), open = "w+")
    on.exit(close(errfile))
    stderr <- function(x){
      writeLines(x, errfile)
      flush(outfile)
    }
  }
  if(is.character(stdout))
    stdout <- normalizePath(stdout, mustWork = FALSE)
  if(is.character(stderr))
    stderr <- normalizePath(stderr, mustWork = FALSE)
  .Call(C_exec_internal, cmd, argv, stdout, stderr, wait)
}

make_callback <- function(x){
  if(is.function(x))
    return(x)
  if(is.character(x)){
    x <- normalizePath(x, mustWork = FALSE)
    out <- file(x, open = "wt")
    return(function(str){
      writeLines(str, out)
    })
  }

}
