#' Quote arguments on Windows
#'
#' Quotes and escapes shell arguments when needed so that they get properly parsed
#' by most Windows programs. This function is used internally to automatically quote
#' system commands, the user should normally not quote arguments manually.
#'
#' Algorithm is ported to R from
#' [libuv](https://github.com/libuv/libuv/blob/v1.23.0/src/win/process.c#L454-L524).
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
