context("test-quote")

# Tests from: https://github.com/cran/processx/blob/3.2.0/src/win/processx.c#L132-L148
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
    expect_equal(trimws(rawToChar(out1$stdout)), '"foo bar"')
    expect_equal(trimws(rawToChar(out2$stdout)), 'foo bar')
  }
})

