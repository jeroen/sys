check_apparmor <- function(){
  getNamespace("RAppArmor")
  if(!RAppArmor::aa_is_compiled())
    stop("AppArmor is not available on your Linux distribution")
  
  # Try aa-getcon
  confinement <- tryCatch({
    RAppArmor::aa_getcon()
  }, error = function(e){
    stop("Failed to lookup process confinement. Have a look at: sudo aa-status", e$message)
  })
  
  if(identical(confinement$con, "unconfined")){
    if(!RAppArmor::aa_is_enabled())
      stop("AppArmor seems disabled in your system")
  }
}
