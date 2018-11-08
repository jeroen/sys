context("test-timeout")

test_that("exec timeout works", {
  times <- system.time({
    expect_error(exec_wait('sleep', '10', timeout = 1.5))
  })
  expect_lt(times[['elapsed']], 1.99)

  # Also try with exec_internal
  times <- system.time({
    expect_error(exec_internal('sleep', '10', timeout = 0.5))
  })
  expect_lt(times[['elapsed']], 0.99)
})
