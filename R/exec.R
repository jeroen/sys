#' Running System Commands
#'
#' Powerful replacements for [system2] with support for interruptions, background
#' tasks and fine grained control over `STDOUT` / `STDERR` binary or text streams.
#'
#' Each value within the `args` vector will automatically be quoted when needed;
#' you should not quote arguments yourself. Doing so anyway could lead to the value
#' being quoted twice on some platforms.
#'
#' The `exec_wait` function runs a system command and waits for the child process
#' to exit. When the child process completes normally (either success or error) it
#' returns with the program exit code. Otherwise (if the child process gets aborted)
#' R raises an error. The R user can interrupt the program by sending SIGINT (press
#' ESC or CTRL+C) in which case the child process tree is properly terminated.
#' Output streams `STDOUT` and `STDERR` are piped back to the parent process and can
#' be sent to a connection or callback function. See the section on *Output Streams*
#' below for details.
#'
#' The `exec_background` function starts the program and immediately returns the
#' PID of the child process. This is useful for running a server daemon or background
#' process.
#' Because this is non-blocking, `std_out` and `std_out` can only be `TRUE`/`FALSE` or
#' a file path. The state of the process can be checked with `exec_status` which
#' returns the exit status, or `NA` if the process is still running. If `wait = TRUE`
#' then `exec_status` blocks until the process completes (but can be interrupted).
#' The child can be killed with [tools::pskill].
#'
#' The `exec_internal` function is a convenience wrapper around `exec_wait` which
#' automatically captures output streams and raises an error if execution fails.
#' Upon success it returns a list with status code, and raw vectors containing
#' stdout and stderr data (use [as_text] for converting to text).
#'
#' @section Output Streams:
#'
#' The `std_out` and `std_err` parameters are used to control how output streams
#' of the child are processed. Possible values for both foreground and background
#' processes are:
#'
#'  - `TRUE`: print child output in R console
#'  - `FALSE`: suppress output stream
#'  - *string*: name or path of file to redirect output
#'
#' In addition the `exec_wait` function also supports the following `std_out` and `std_err`
#' types:
#'
#'  - *connection* a writable R [connection] object such as [stdout] or [stderr]
#'  - *function*: callback function with one argument accepting a raw vector (use
#'  [as_text] to convert to text).
#'
#' When using `exec_background` with `std_out = TRUE` or `std_err = TRUE` on Windows,
#' separate threads are used to print output. This works in RStudio and RTerm but
#' not in RGui because the latter has a custom I/O mechanism. Directing output to a
#' file is usually the safest option.
#'
#' @export
#' @return `exec_background` returns a pid. `exec_wait` returns an exit code.
#' `exec_internal` returns a list with exit code, stdout and stderr strings.
#' @name exec
#' @aliases sys
#' @seealso Base [system2] and [pipe] provide other methods for running a system
#' command with output.
#' @family sys
#' @rdname exec
#' @param cmd the command to run. Either a full path or the name of a program on
#' the `PATH`. On Windows this is automatically converted to a short path using
#' [Sys.which], unless wrapped in [I()].
#' @param args character vector of arguments to pass. On Windows these automatically
#' get quoted using [windows_quote], unless the value is wrapped in [I()].
#' @param std_out if and where to direct child process `STDOUT`. Must be one of
#' `TRUE`, `FALSE`, filename, connection object or callback function. See section
#' on *Output Streams* below for details.
#' @param std_err if and where to direct child process `STDERR`. Must be one of
#' `TRUE`, `FALSE`, filename, connection object or callback function. See section
#' on *Output Streams* below for details.
#' @param std_in file path to map std_in
#' @param timeout maximum time in seconds
#' @examples # Run a command (interrupt with CTRL+C)
#' status <- exec_wait("date")
#'
#' # Capture std/out
#' out <- exec_internal("date")
#' print(out$status)
#' cat(as_text(out$stdout))
#'
#' if(nchar(Sys.which("ping"))){
#'
#' # Run a background process (daemon)
#' pid <- exec_background("ping", "localhost")
#'
#' # Kill it after a while
#' Sys.sleep(2)
#' tools::pskill(pid)
#'
#' # Cleans up the zombie proc
#' exec_status(pid)
#' rm(pid)
#' }
exec_wait <- function(cmd, args = NULL, std_out = stdout(), std_err = stderr(), std_in = NULL, timeout = 0){
  # Convert TRUE or filepath into connection objects
  std_out <- if(isTRUE(std_out) || identical(std_out, "")){
    stdout()
  } else if(is.character(std_out)){
    file(normalizePath(std_out, mustWork = FALSE))
  } else std_out

  std_err <- if(isTRUE(std_err) || identical(std_err, "")){
    stderr()
  } else if(is.character(std_err)){
    std_err <- file(normalizePath(std_err, mustWork = FALSE))
  } else std_err

  # Define the callbacks
  outfun <- if(inherits(std_out, "connection")){
    if(!isOpen(std_out)){
      open(std_out, "wb")
      on.exit(close(std_out), add = TRUE)
    }
    if(identical(summary(std_out)$text, "text")){
      function(x){
        cat(rawToChar(x), file = std_out)
        flush(std_out)
      }
    } else {
      function(x){
        writeBin(x, con = std_out)
        flush(std_out)
      }
    }
  } else if(is.function(std_out)){
    if(!length(formals(std_out)))
      stop("Function std_out must take at least one argument")
    std_out
  }

  errfun <- if(inherits(std_err, "connection")){
    if(!isOpen(std_err)){
      open(std_err, "wb")
      on.exit(close(std_err), add = TRUE)
    }
    if(identical(summary(std_err)$text, "text")){
      function(x){
        cat(rawToChar(x), file = std_err)
        flush(std_err)
      }
    } else {
      function(x){
        writeBin(x, con = std_err)
        flush(std_err)
      }
    }
  } else if(is.function(std_err)){
    if(!length(formals(std_err)))
      stop("Function std_err must take at least one argument")
    std_err
  }
  execute(cmd = cmd, args = args, std_out = outfun, std_err = errfun,
          std_in = std_in, wait = TRUE, timeout = timeout)
}

#' @export
#' @rdname exec
exec_background <- function(cmd, args = NULL, std_out = TRUE, std_err = TRUE, std_in = NULL){
  if(!is.character(std_out) && !is.logical(std_out))
    stop("argument 'std_out' must be TRUE / FALSE or a filename")
  if(!is.character(std_err) && !is.logical(std_err))
    stop("argument 'std_err' must be TRUE / FALSE or a filename")
  execute(cmd = cmd, args = args, std_out = std_out, std_err = std_err,
          wait = FALSE, std_in = std_in, timeout = 0)
}

#' @export
#' @rdname exec
#' @param error automatically raise an error if the exit status is non-zero.
exec_internal <- function(cmd, args = NULL, std_in = NULL, error = TRUE, timeout = 0){
  outcon <- rawConnection(raw(0), "r+")
  on.exit(close(outcon), add = TRUE)
  errcon <- rawConnection(raw(0), "r+")
  on.exit(close(errcon), add = TRUE)
  status <- exec_wait(cmd, args, std_out = outcon,
                      std_err = errcon, std_in = std_in, timeout = timeout)
  if(isTRUE(error) && !identical(status, 0L))
    stop(sprintf("Executing '%s' failed with status %d", cmd, status))
  list(
    status = status,
    stdout = rawConnectionValue(outcon),
    stderr = rawConnectionValue(errcon)
  )
}

#' @export
#' @rdname exec
#' @useDynLib sys R_exec_status
#' @param pid integer with a process ID
#' @param wait block until the process completes
exec_status <- function(pid, wait = TRUE){
  .Call(R_exec_status, pid, wait)
}

#' @useDynLib sys C_execute
execute <- function(cmd, args, std_out, std_err, std_in, wait, timeout){
  stopifnot(is.character(cmd))
  if(.Platform$OS.type == 'windows'){
    if(!inherits(cmd, 'AsIs'))
      cmd <- utils::shortPathName(path.expand(cmd))
    if(!inherits(args, 'AsIs'))
      args <- windows_quote(args)
  }
  stopifnot(is.logical(wait))
  argv <- enc2utf8(c(cmd, args))
  if(length(std_in) && !is.logical(std_in)) # Only files supported for stdin
    std_in <- enc2utf8(normalizePath(std_in, mustWork = TRUE))
  .Call(C_execute, cmd, argv, std_out, std_err, std_in, wait, timeout)
}
