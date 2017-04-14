#' @useDynLib sys R_set_tempdir
set_tempdir <- function(path){
  path <- normalizePath(path)
  if(!file.exists(path))
    dir.create(path)
  .Call(R_set_tempdir, path)
}
