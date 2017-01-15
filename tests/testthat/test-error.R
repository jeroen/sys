context("error messages")

test_that("error is raised for invalid executable",{
  expect_error(exec_wait("doesnotexist"), "Failed to execute")
  expect_error(exec_background("doesnotexist"), "Failed to execute")

  # without stdout
  expect_error(exec_wait("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")
  expect_error(exec_background("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")
})


test_that("no error is raised for system error", {
  expect_is(exec_wait("ping", "asfdsafdsfasdfasdf", std_err = FALSE), "integer")
  expect_is(exec_background("ping", "asfdsafdsfasdfasdf", std_err = FALSE), "integer")
})
