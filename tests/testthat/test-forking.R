context("eval_fork")

test_that("eval_fork works", {
  skip_on_os("windows")

  # PID must be different
  expect_false(Sys.getpid() == eval_fork(Sys.getpid()))
  expect_equal(unix::getpid(), eval_fork(unix::getppid()))

  # Priority (requires unix > 1.2)
  prio <- unix::getpriority()
  expect_equal(eval_safe(unix::getpriority(), priority = prio + 1), prio + 1)

  # initiates RNG with a seed (needed below)
  rnorm(1)

  # Test that state is inherited
  x <- eval_fork(rnorm(10))
  y <- eval_fork(rnorm(10))
  z <- rnorm(10)
  expect_identical(x, y)
  expect_identical(x, z)

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

  # Test that proc dies
  expect_error(eval_fork(tools::pskill(Sys.getpid())), "child process")
  expect_error(eval_fork(Sys.sleep(10), timeout = 2), "timeout")

  # Test that tryCatch works
  expect_equal(eval_fork(try(pi, silent = TRUE)), pi)
  expect_is(eval_fork(try(blabla(), silent = TRUE)), "try-error")
  expect_is(eval_fork(tryCatch(blabla(), error = identity)), "simpleError")
})

test_that("Fork does not clean tmpdir", {
  skip_on_os("windows")
  skip_if_not(safe_build())
  expect_error(eval_fork(q()), "died")
  expect_true(file.exists(tempdir()))
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

test_that("frozen children get killed", {
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

test_that("scope environment is correct", {
  skip_on_os("windows")
  (test <- function(){
    mydev <- grDevices::pdf
    timer <- 60
    x <- 42
    blabla <- function(){
      return(x)
    }
    testfun <- function(){
      blabla()
    }
    testerr <- function(){
      doesnotexit()
    }
    expect_equal(42, eval_safe(testfun(), dev = mydev, timeout = timer))
    expect_equal(42, eval_fork(testfun(), timeout = timer))
    expect_error(eval_safe(testerr()), "doesnotexit")
  })()
})

test_that("rlimits apply in eval_safe", {
  skip_on_os("windows")
  if(unix::rlimit_cpu()$max > 100)
    expect_equal(eval_safe(unix::rlimit_cpu()$max, rlimits = c(cpu = 100, data = 1e7)), 100)
  if(unix::rlimit_data()$max > 1e7)
    expect_equal(eval_safe(unix::rlimit_data()$max, rlimits = c(cpu = 100, data = 1e7)), 1e7)

  # unsupported rlimit
  expect_error(eval_safe(123, rlimits = c(foo = 123)), "foo")

  # unnamed rlimits
  expect_error(eval_safe(123, rlimits = list(123)), "rlimit")
})

test_that("stdout gets redirected to parent",{
  skip_on_os("windows")
  skip_if_not(safe_build())

  outcon <- rawConnection(raw(0), "r+")
  errcon <- rawConnection(raw(0), "r+")

  out <- eval_fork({
    cat("foo")
    cat("bar", file = stderr())
    42
  }, std_out = outcon, std_err = errcon)

  expect_identical(out, 42)

  expect_identical(rawConnectionValue(outcon), charToRaw("foo"))
  expect_identical(rawConnectionValue(errcon), charToRaw("bar"))

  close(outcon)
  close(errcon)
})

test_that("tempdir/interactivity", {
  skip_on_os("windows")
  skip_if_not(safe_build())

  subtmp <- eval_fork(tempdir())
  expect_equal(normalizePath(tempdir()), normalizePath(dirname(subtmp)))
  expect_true(grepl("^fork", basename(subtmp)))
  expect_false(eval_fork(interactive()))
  expect_equal(eval_safe(readline(), std_out = FALSE, std_err = FALSE), "")
})
