#' Safe Evaluation
#'
#' Evaluates an expression in a temporary fork so that it has no side effects on the main
#' R session. For [eval_safe()] the expression is wrapped in additional R code to deal with
#' errors and graphics devices (recommended).
#'
#' @export
#' @rdname eval_fork
#' @inheritParams exec
#' @param expr expression to evaluate
#' @param tmp the value of [tempdir()] inside the forked process
#' @param timeout maximum time in seconds to allow for call to return
#' @examples # works like regular eval:
#' eval_safe(rnorm(5))
#'
#' # Exceptions get propagated
#' test <- function() { doesnotexit() }
#' tryCatch(eval_safe(test()), error = function(e){
#'   cat("oh no!", e$message, "\n")
#' })
#'
#' # Honor interrupt and timeout, even inside C evaluations
#' try(eval_safe(svd(matrix(rnorm(1e8), 1e4)), timeout = 2))
#'
#' # Capture output
#' outcon <- rawConnection(raw(0), "r+")
#' eval_safe(print(sessionInfo()), std_out = outcon)
#' cat(rawToChar(rawConnectionValue(outcon)))
eval_fork <- function(expr, tmp = tempfile("fork"), timeout = 60, std_out = stdout(),
                      std_err = stderr(), priority = NULL, uid = NULL, gid = NULL, rlimits = NULL){
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
  clenv <- force(parent.frame())
  clexpr <- substitute(expr)
  eval_fork_internal(clexpr, clenv, tmp, timeout, outfun, errfun, priority, uid, gid, rlimits)
}

#' @rdname eval_fork
#' @export
#' @importFrom grDevices pdf dev.cur dev.off
#' @param device graphics device to use in the fork, see [dev.new()]
#' @param rlimits named vector/list with rlimit values, for example: `c(cpu = 60, fsize = 1e6)`.
#' @param uid evaluate as given user (uid or name). See [unix::setuid()], only for root.
#' @param gid evaluate as given group (gid or name). See [unix::setgid()] only for root.
#' @param priority (integer) priority of the child process. High value is low priority.
#' Non root user may only raise this value (decrease priority)
#' @param profile AppArmor profile, see `RAppArmor::aa_change_profile()`.
#' Requires the `RAppArmor` package (Debian/Ubuntu only)
eval_safe <- function(expr, tmp = tempfile("fork"), timeout = 60, std_out = stdout(),
                      std_err = stderr(), device = pdf, rlimits = list(), uid = NULL,
                      gid = NULL, priority = NULL, profile = NULL){
  orig_expr <- substitute(expr)
  out <- eval_fork(expr = tryCatch({
    if(length(device))
      options(device = device)
    if(length(profile)){
      tryCatch(check_apparmor(), error = function(e){
        warning("You can only use the 'profile' parameter when RAppArmor is installed")
        stop(e)
      })
      RAppArmor::aa_change_profile(profile)
    }
    while(dev.cur() > 1) dev.off()
    options(menu.graphics = FALSE)
    withVisible(eval(orig_expr, parent.frame()))
  }, error = function(e){
    old_class <- attr(e, "class")
    structure(e, class = c(old_class, "eval_fork_error"))
  }, finally = substitute(while(dev.cur() > 1) dev.off())),
  tmp = tmp, timeout = timeout, std_out = std_out, std_err = std_err, priority = priority,
  uid = uid, gid = gid, rlimits = rlimits)
  if(inherits(out, "eval_fork_error"))
    base::stop(out)
  if(out$visible)
    out$value
  else
    invisible(out$value)
}

#' @useDynLib sys R_eval_fork
eval_fork_internal <- function(expr, envir, tmp, timeout, outfun, errfun, priority,
                               uid, gid, rlimits){
  if(!file.exists(tmp))
    dir.create(tmp)
  if(length(uid))
    stopifnot(is.numeric(uid))
  if(length(gid))
    stopifnot(is.numeric(gid))
  if(length(priority))
    stopifnot(is.numeric(priority))
  if(length(timeout))
    stopifnot(is.numeric(timeout))
  uid <- as.integer(uid)
  gid <- as.integer(gid)
  priority <- as.integer(priority)
  rlimits <- do.call(parse_limits, as.list(rlimits))
  timeout <- as.numeric(timeout)
  tmp <- normalizePath(tmp)
  .Call(R_eval_fork, expr, envir, tmp, timeout, outfun, errfun, priority, uid, gid, rlimits)
}

# Limits MUST be named
parse_limits <- function(..., as = NA, core = NA, cpu = NA, data = NA, fsize = NA,
                         memlock = NA, nofile = NA, nproc = NA, stack = NA){
  unknown <- list(...)
  if(length(unknown))
    stop("Unsupported rlimits: ", paste(names(unknown), collapse = ", "))
  out <- as.numeric(c(as, core, cpu, data, fsize, memlock, nofile, nproc, stack))
  structure(out, names = names(formals(sys.function()))[-1])
}

#' @useDynLib sys R_freeze
freeze <- function(interrupt = TRUE){
  .Call(R_freeze, as.logical(interrupt))
}
