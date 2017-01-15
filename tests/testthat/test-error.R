context("error messages")

test_that("error is raised for invalid executable",{
  expect_error(exec_wait("doesnotexist"), "Failed to execute")
  expect_error(exec_background("doesnotexist"), "Failed to execute")
})
