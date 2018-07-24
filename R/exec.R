#' Running System Commands
#'
#' Powerful replacements for [system2] with support for interruptions, background
#' tasks and fine grained control over `STDOUT` / `STDERR` binary or text streams.
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
#' stdout and stderr data (use [rawToChar] for converting to text).
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
#'  [rawToChar] to convert to text).
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
#' @rdname exec
#' @param cmd the command to run. Either a full path or the name of a program
#' which exists in the `PATH`.
#' @param args character vector of arguments to pass
#' @param std_out if and where to direct child process `STDOUT`. Must be one of
#' `TRUE`, `FALSE`, filename, connection object or callback function. See section
#' on *Output Streams* below for details.
#' @param std_err if and where to direct child process `STDERR`. Must be one of
#' `TRUE`, `FALSE`, filename, connection object or callback function. See section
#' on *Output Streams* below for details.
#' @examples # Run a command (interrupt with CTRL+C)
#' status <- exec_wait("date")
#'
#' # Capture std/out
#' out <- exec_internal("date")
#' print(out$status)
#' cat(rawToChar(out$stdout))
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
exec_wait <- function(cmd, args = NULL, std_out = stdout(), std_err = stderr()){
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
  }
  execute(cmd, args, outfun, errfun, wait = TRUE)
}

#' @export
#' @rdname exec
exec_background <- function(cmd, args = NULL, std_out = TRUE, std_err = TRUE){
  if(!is.character(std_out) && !is.logical(std_out))
    stop("argument 'std_out' must be TRUE / FALSE or a filename")
  if(!is.character(std_err) && !is.logical(std_err))
    stop("argument 'std_err' must be TRUE / FALSE or a filename")
  execute(cmd, args, std_out, std_err, wait = FALSE)
}

#' @export
#' @rdname exec
#' @param error automatically raise an error if the exit status is non-zero.
exec_internal <- function(cmd, args = NULL, error = TRUE){
  outcon <- rawConnection(raw(0), "r+")
  on.exit(close(outcon), add = TRUE)
  errcon <- rawConnection(raw(0), "r+")
  on.exit(close(errcon), add = TRUE)
  status <- exec_wait(cmd, args, std_out = outcon, std_err = errcon)
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
execute <- function(cmd, args, std_out, std_err, wait){
  stopifnot(is.character(cmd))
  stopifnot(is.logical(wait))
  argv <- c(cmd, as.character(args))
  .Call(C_execute, cmd, argv, std_out, std_err, wait)
}
