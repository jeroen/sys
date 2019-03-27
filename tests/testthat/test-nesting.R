context("nested jobs")

test_that("Jobs can be nested", {
  skip_if_not(nchar(Sys.which('whoami')) > 0)
  res1 <- sys::exec_internal("whoami")
  expect_equal(res1$status, 0)
  user <- as_text(res1$stdout)
  res2 <- sys::r_internal(c('--silent', '-e', 'sys::exec_wait("whoami")'))
  expect_equal(res2$status, 0)
  output <- as_text(res2$stdout)
  expect_equal(output[2], user)
})
