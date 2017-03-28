#' Evaluate in fork
#'
#' Version of [eval] which evaluates expression in a temporary fork so
#' that it has no side effects on the main R session.
#' Basically a robust version of `mccollect(mcparallel(expr))`.
#' Not available on Windows because it required `fork()`.
#'
#' @export
#' @inheritParams exec
#' @param expr expression to evaluate
#' @param envir the [environment] in which expr is to be evaluated
#' @param tmp the value of [tempdir] inside the forked process
#' @param timeout maximum time in seconds to allow for call to return
eval_fork <- function(expr, envir = parent.frame(), tmp = tempfile("fork"), timeout = 60,
                      std_out = stdout(), std_err = stderr()){
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
  if(!file.exists(tmp))
    dir.create(tmp)
  clenv <- force(envir)
  clexpr <- substitute(expr)
  trexpr <- call('tryCatch', expr = clexpr,
    interrupt = function(e){
      stop(simpleError("process interrupted by user", call = trexpr))
    }, error = function(e){
      structure(e, class = "eval_fork_error")
    }
  )
 out <- eval_fork_internal(trexpr, clenv, tmp, timeout, outfun, errfun)
 if(inherits(out, "eval_fork_error")){
   stop(simpleError(out$message, out$call[[2]]))
 }
 return(out)
}

#' @useDynLib sys R_eval_fork
eval_fork_internal <- function(expr, envir, tmp, timeout, outfun, errfun){
  .Call(R_eval_fork, expr, envir, tmp, timeout, outfun, errfun)
}
