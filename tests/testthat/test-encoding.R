context("test-encoding")

test_that("multiplication works", {
  txt <- readLines(system.file('utf8.txt', package = 'sys', mustWork = TRUE), encoding = 'UTF-8')
  res <- sys::exec_internal('echo', txt)
  expect_equal(res$status, 0)
  con <- rawConnection(res$stdout)
  output <- readLines(con, encoding = 'UTF-8')
  close(con)
  expect_equal(txt, output)
})
