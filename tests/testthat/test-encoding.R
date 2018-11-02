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

test_that("UTF-8 filename", {
  txt <- readLines(system.file('utf8.txt', package = 'sys', mustWork = TRUE), encoding = 'UTF-8')
  tmp <- tempfile(txt)
  f <- file(tmp, 'wb')
  serialize(iris, f)
  close(f)
  expect_true(file.exists(tmp))

  # As a file path
  res <- sys::exec_internal('cat', tmp)
  expect_equal(res$status, 0)
  expect_equal(unserialize(res$stdout), iris)

  # As stdin file
  res <- sys::exec_internal('cat', std_in = tmp)
  expect_equal(res$status, 0)
  expect_equal(unserialize(res$stdout), iris)
})
