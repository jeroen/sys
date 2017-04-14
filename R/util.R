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
