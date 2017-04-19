#' Package config
#'
#' Shows which features are enabled in the package configuration.
#'
#' @export
#' @examples sys_config()
sys_config <- function(){
  list(
    safe = safe_build(),
    apparmor = have_apparmor()
  )
}

#' @useDynLib sys R_freeze
freeze <- function(interrupt = TRUE){
  .Call(R_freeze, as.logical(interrupt))
}

#' @useDynLib sys R_safe_build
safe_build <- function(){
  .Call(R_safe_build)
}

#' @useDynLib sys R_have_apparmor
have_apparmor <- function(){
  .Call(R_have_apparmor)
}

#' @useDynLib sys R_set_tempdir
set_tempdir <- function(path){
  path <- normalizePath(path)
  if(!file.exists(path))
    dir.create(path)
  .Call(R_set_tempdir, path)
}

#' @useDynLib sys R_set_interactive
set_interactive <- function(set){
  stopifnot(is.logical(set))
  .Call(R_set_interactive, set)
}

#' @useDynLib sys R_aa_change_profile
aa_change_profile <- function(profile){
  stopifnot(is.character(profile))
  .Call(R_aa_change_profile, profile)
}

#' @useDynLib sys R_setuid
setuid <- function(uid){
  stopifnot(is.numeric(uid))
  .Call(R_setuid, as.integer(uid))
}

#' @useDynLib sys R_setgid
setgid <- function(gid){
  stopifnot(is.numeric(gid))
  .Call(R_setgid, as.integer(gid))
}

#' @useDynLib sys R_set_priority
set_priority <- function(priority){
  stopifnot(is.numeric(priority))
  .Call(R_set_priority, as.integer(priority))
}

#' @useDynLib sys R_set_rlimits
set_rlimits <- function(rlimits){
  rlimits <- do.call(parse_limits, as.list(rlimits))
  .Call(R_set_rlimits, rlimits)
}
