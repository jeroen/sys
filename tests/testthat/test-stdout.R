context("stdout and stderr")

test_that("test output for std_out equals TRUE/FALSE", {
  string <- "hello world!"
  output1 <- capture.output(res <- exec_wait('echo', string))
  output2 <- capture.output(res <- exec_wait('echo', string, std_out = FALSE))
  expect_equal(output1, string)
  expect_equal(output2, character())

  output3 <- capture.output(res <- exec_wait('ping', 'asfdsafdsfasdfasdf'), type = 'message')
  output4 <- capture.output(res <- exec_wait('ping', 'asfdsafdsfasdfasdf', std_err = FALSE), type = 'message')
  expect_length(output3, 1)
  expect_length(output4, 0)
})
