context("test-timeout")

test_that("exec timeout works", {
  if(.Platform$OS.type == "windows"){
    command = "ping"
    args = c("-n", "5", "localhost")
  } else {
    command = 'sleep'
    args = '5'
  }
  times <- system.time({
    expect_error(exec_wait(command, args, timeout = 1.5, std_out = FALSE), "timeout")
  })
  expect_gte(times[['elapsed']], 1.5)
  expect_lt(times[['elapsed']], 2.5)

  # Also try with exec_internal
  times <- system.time({
    expect_error(exec_internal(command, args, timeout = 0.5), "timeout")
  })
  expect_gte(times[['elapsed']], 0.5)
  expect_lt(times[['elapsed']], 1.5)
})
