#' Quote arguments on Windows
#'
#' Properly escapes spaces and double quotes in shell arguments so that they get
#' properly parsed by most Windows programs.
#'
#' @export
#' @rdname quote
#' @name quote
#' @param args character vector with arguments
windows_quote <- function(args){
  if(is.null(args))
    return(args)
  stopifnot(is.character(args))
  args <- enc2utf8(args)
  vapply(args, windows_quote_one, character(1), USE.NAMES = FALSE)
}

windows_quote_one <- function(str){
  if(!nchar(str)){
    return('""')
  }
  if(!grepl('[ \t"]', str)){
    return(str)
  }
  if(!grepl('["\\]', str)){
    return(paste0('"', str, '"'))
  }
  str <- gsub('([\\]*)"', '\\1\\1\\\\"', str, useBytes = TRUE)
  str <- gsub('([\\]+)$', '\\1\\1', str, useBytes = TRUE)
  paste0('"', str, '"')
}
