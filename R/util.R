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
