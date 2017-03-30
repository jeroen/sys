context("eval_fork")

test_that("eval_fork works", {
  skip_on_os("windows")

  # PID must be different
  expect_true(Sys.getpid() != eval_fork(Sys.getpid()))

  # State is inherited
  n <- 10
  x <- eval_fork(rnorm(n))
  expect_identical(x, rnorm(n))

  # Test cleanups
  for(i in 1:300){
    expect_equal(pi, eval_fork(pi))
  }
})

test_that("eval_fork gives errors", {
  skip_on_os("windows")

  # Test regular errors
  expect_error(eval_safe(stop("uhoh")), "uhoh")
  expect_error(eval_safe(blablabla()), "could not find function")

  # Test that proc dies properly
  expect_error(eval_fork(tools::pskill(Sys.getpid())), "child process died")
  expect_error(eval_fork(Sys.sleep(10), timeout = 2), "timeout")

  # Test that tryCatch works
  expect_equal(eval_fork(try(pi, silent = TRUE)), pi)
  expect_is(eval_fork(try(blabla(), silent = TRUE)), "try-error")
  expect_is(eval_fork(tryCatch(blabla(), error = identity)), "simpleError")
})

test_that("eval_fork works recursively", {
  skip_on_os("windows")
  expect_equal(eval_fork(eval_fork(1+1)), 2)
  expect_equal(eval_fork(eval_fork(1+1) + eval_fork(1+1)), 4)

  expect_error(eval_safe(eval_safe(stop("uhoh"))), "uhoh")
  expect_error(eval_safe(eval_safe(blablabla())), "could not find function")

  fib <- function(n){
    eval_fork({
      #print(Sys.getpid())
      if(n < 2)
        n
      else
        fib(n-1) + fib(n-2)
    })
  }
  #forks 10 deep :o
  expect_equal(fib(10), 55)
})

