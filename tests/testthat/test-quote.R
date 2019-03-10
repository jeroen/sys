context("test-quote")

# Test cases: https://github.com/libuv/libuv/blob/v1.23.0/src/win/process.c#L486-L502
test_that("windows quoting arguments", {
  input <- c('hello"world', 'hello""world', 'hello\\world', 'hello\\\\world',
             'hello\\"world', 'hello\\\\"world', 'hello world\\', '')
  output <- c('"hello\\"world"', '"hello\\"\\"world"', 'hello\\world', 'hello\\\\world',
             '"hello\\\\\\"world"', '"hello\\\\\\\\\\"world"', '"hello world\\\\"', '""')
  expect_equal(windows_quote(input), output)

  if(.Platform$OS.type == 'windows'){
    args <- c('/C', 'echo', 'foo bar')
    out1 <- exec_internal('cmd', args)
    out2 <- exec_internal('cmd', I(args))
    expect_equal(as_text(out1$stdout), '"foo bar"')
    expect_equal(as_text(out2$stdout), 'foo bar')
  }
})

