context("error handling")

test_that("catching execution errors", {
  # Test that 'ping' is on the path
  skip_if_not(as.logical(nchar(Sys.which('ping'))), "ping utility is not available")

  # Ping has different args for each platform
  sysname <- tolower(Sys.info()[["sysname"]])
  args <- switch(sysname,
     windows = c("-n", "2", "localhost"),
     darwin = c("-t2", "localhost"),
     sunos = c("-s", "localhost", "64", "2"),
     c("-c2", "localhost") #linux/default
  )

  # Run ping
  expect_equal(exec_wait("ping", args, std_out = FALSE), 0)

  # Error for non existing program (win-builder gives a german error)
  expect_error(exec_wait("doesnotexist"), "Failed to execute.*(file|Datei)")
  expect_error(exec_background("doesnotexist"), "Failed to execute.*(file|Datei)")

  # Same without stdout
  expect_error(exec_wait("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")
  expect_error(exec_background("doesnotexist", std_out = FALSE, std_err = FALSE), "Failed to execute")

  # Program error
  expect_is(exec_wait("ping", "999.999.999.999.999", std_err = FALSE, std_out = FALSE), "integer")
  expect_is(exec_background("ping", "999.999.999.999.999", std_err = FALSE, std_out = FALSE), "integer")

  # Program error with exec_internal
  expect_error(exec_internal('ping', "999.999.999.999.999"))
  out <- exec_internal('ping', "999.999.999.999.999", error = FALSE)
  expect_gt(out$status, 0)
})
