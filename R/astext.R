#' Convert Raw to Text
#'
#' Parses a raw vector as lines of text. This is similar to [charToRaw] but
#' splits output by (platform specific) linebreaks and allows for marking
#' output with a given encoding.
#'
#'
#' @export
#' @seealso [base::charToRaw]
#' @param x vector to be converted to text
#' @param ... parameters passed to [readLines] such as `encoding` or `n`
as_text <- function(x, ...){
  if(length(x)){
    con <- rawConnection(x)
    on.exit(close(con))
    readLines(con, ...)
  } else {
    character(0)
  }
}
