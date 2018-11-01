context("test-stdin")

test_that("streaming from stdin works", {
  tmp <- tempfile()
  input <- c("foo", "bar", "baz")
  writeLines(input, con = tmp)
  res <- exec_internal('sort', std_in = tmp)
  expect_equal(res$status, 0)
  con <- rawConnection(res$stdout)
  output <- readLines(con)
  close(con)
  expect_equal(output, sort(input))
})
