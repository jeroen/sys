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
  expect_error(eval_fork(tools::pskill(Sys.getpid())), "child process")
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

  fib_fork <- function(n){
    eval_fork({
      #print(Sys.getpid())
      if(n < 2) n else fib_fork(n-1) + fib_fork(n-2)
    })
  }

  #forks 10 deep :o
  expect_equal(fib_fork(10), 55)

  fib_safe <- function(n){
    eval_safe({
      #print(Sys.getpid())
      if(n < 2) n else fib_safe(n-1) + fib_safe(n-2)
    })
  }

  #forks 10 deep :o
  expect_equal(fib_safe(10), 55)
})

test_that("compatibility with parallel package", {
  skip_on_os("windows")
  square_fork <- function(x){
    parallel::mccollect(parallel::mcparallel(x^2))[[1]]
  }

  # Run mcparallel inside sys
  expect_equal(square_fork(5), 25)
  expect_equal(eval_fork(square_fork(6)), 36)
  expect_equal(eval_safe(square_fork(7)), 49)

})

test_that("frozen children get killed",{
  skip_on_os("windows")

  expect_before <- function(expr, time){
    elapsed <- system.time(try(expr, silent = TRUE))["elapsed"]
    expect_lt(elapsed, time)
  }

  # test timers
  expect_before(eval_fork(freeze(FALSE), timeout = 1), 2)
  expect_before(eval_fork(freeze(TRUE), timeout = 1), 2)
})

test_that("condition class gets preserved", {
  skip_on_os("windows")

  test <- function(){
    e <- structure(
      list(message = "some message", call = NULL),
      class = c("error", "condition", "my_custom_class")
    )
    base::stop(e)
  }


  err <- tryCatch(eval_safe(test()), error = function(e){e})
  expect_s3_class(err, "error")
  expect_s3_class(err, "my_custom_class")

})
