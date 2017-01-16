context("error handling")

test_that("run ping is available", {
  sysname <- tolower(Sys.info()[["sysname"]])
  args <- switch(sysname,
     windows = c("-n 2", "localhost"),
     darwin = c("-t2", "localhost"),
     linux = c("-c2", "localhost"),
     sunos = c("-s", "localhost", "64", "2")
  )
  expect_equal(exec_wait("ping", args, std_out = FALSE), 0)
})

test_that("error is raised for invalid executable",{
  expect_error(exec_wait("doesnotexist"), "Failed to execute")
  expect_error(exec_background("doesnotexist"), "Failed to execute")

  # without stdout
  expect_error(exec_wait("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")
  expect_error(exec_background("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")
})


test_that("no error is raised for program error", {
  expect_is(exec_wait("ping", "999.999.999.999.999", std_err = FALSE, std_out = FALSE), "integer")
  expect_is(exec_background("ping", "999.999.999.999.999", std_err = FALSE, std_out = FALSE), "integer")
})

test_that("exec_internal automatically raises error", {
  expect_error(exec_internal('ping', "999.999.999.999.999"))
  out <- exec_internal('ping', "999.999.999.999.999", error = FALSE)
  expect_gt(out$status, 0)
})
