context("test-encoding")

test_that("UTF-8 encoded text arguments", {
  txt <- readLines(system.file('utf8.txt', package = 'sys', mustWork = TRUE), encoding = 'UTF-8')
  res <- sys::exec_internal('echo', txt)
  expect_equal(res$status, 0)
  con <- rawConnection(res$stdout)
  output <- readLines(con, encoding = 'UTF-8')
  close(con)
  expect_equal(txt, output)
})

test_that("UTF-8 filenames, binary data", {
  skip_if(getRversion() < "3.5")
  tmp <- paste(tempdir(), "\u0420\u0423\u0421\u0421\u041a\u0418\u0419.txt", sep = "/")
  tmp <- normalizePath(tmp, mustWork = FALSE)
  f <- file(tmp, 'wb')
  serialize(iris, f)
  close(f)
  expect_true(file.exists(tmp))

  # As a file path
  res <- if(.Platform$OS.type == "windows"){
    sys::exec_internal('cmd', c("/C", "type", tmp))
  } else {
    sys::exec_internal('cat', tmp)
  }
  expect_equal(res$status, 0)
  expect_equal(unserialize(res$stdout), iris)
})

test_that("UTF-8 filename as std_in", {
  skip_if(getRversion() < "3.5")
  input <- c("foo", "bar", "baz")
  txt <- readLines(system.file('utf8.txt', package = 'sys', mustWork = TRUE), encoding = 'UTF-8')
  tmp <- normalizePath(paste(tempdir(), txt, sep = "/"), mustWork = FALSE)
  f <- file(tmp, 'wb')
  writeBin(charToRaw(paste(input, collapse = "\n")), con = f, useBytes = TRUE)
  close(f)
  expect_true(file.exists(tmp))
  res <- exec_internal('sort', std_in = tmp)
  expect_equal(res$status, 0)
  con <- rawConnection(res$stdout)
  output <- readLines(con)
  close(con)
  expect_equal(output, sort(input))
})
