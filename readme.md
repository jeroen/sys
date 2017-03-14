# sys

> Portable System Utilities

[![Build Status](https://travis-ci.org/jeroen/sys.svg?branch=master)](https://travis-ci.org/jeroen/sys)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/jeroen/sys?branch=master&svg=true)](https://ci.appveyor.com/project/jeroen/sys)
[![Coverage Status](https://codecov.io/github/jeroen/sys/coverage.svg?branch=master)](https://codecov.io/github/jeroen/sys?branch=master)
[![CRAN_Status_Badge](http://www.r-pkg.org/badges/version/sys)](https://cran.r-project.org/package=sys)
[![CRAN RStudio mirror downloads](http://cranlogs.r-pkg.org/badges/sys)](https://cran.r-project.org/package=sys)
[![Github Stars](https://img.shields.io/github/stars/jeroen/sys.svg?style=social&label=Github)](https://github.com/jeroen/sys)

Powerful replacements for base system2 with consistent behavior across
platforms. Supports interruption, background tasks, and full control over
STDOUT / STDERR binary or text streams.

## Hello World

Run a blocking process

```r
# Blocks until done, interrupt with ESC or CTRL+C
res <- exec_wait("ping", "google.com")
```

To run as a background process:

```r
# Run as a background process
pid <- exec_background("ping", "google.com")

# Kill it after a while
sleep(4)
tools::pskill(pid)
```

See the `?sys` manual page for details.

## Details

The `exec_wait` function runs a system command and waits for the child process
to exit. When the child process completes normally (either success or error) it
returns with the program exit code. Otherwise (if the child process gets aborted)
R raises an error. The R user can interrupt the program by sending SIGINT (press
ESC or CTRL+C) in which case the child process tree is properly terminated.
Output streams `STDOUT` and `STDERR` are piped back to the parent process and can
be sent to a connection or callback function. See the section on *Output Streams*
below for details.

The `exec_background` function starts the program and immediately returns the
PID of the child process. Because this is non-blocking, `std_out` and `std_out`
can only be `TRUE`/`FALSE` or a file path. The state of the process is not
controlled by R but the child can be killed manually with [tools::pskill]. This
is useful for running a server daemon or background process.

The `exec_internal` function is a convenience wrapper around `exec_wait` which
automatically captures output streams and raises an error if execution fails.
Upon success it returns a list with status code, and raw vectors containing
stdout and stderr data (use [rawToChar] for converting to text).

## Output Streams:

The `std_out` and `std_err` parameters are used to control how output streams
of the child are processed. Possible values for both foreground and background
processes are:

 - `TRUE`: print child output in R console
 - `FALSE`: suppress output stream
 - *string*: name or path of file to redirect output

In addition the `exec_wait` function also supports the following `std_out` and `std_err`
types:

 - *connection* a writeable R [connection] object such as [stdout] or [stderr]
 - *function*: callback function with one argument accepting a raw vector (use
 [rawToChar] to convert to text).

When using `exec_background` with `std_out = TRUE` or `std_err = TRUE` on Windows,
separate threads are used to print output. This works in RStudio and RTerm but
not in RGui because the latter has a custom I/O mechanism. Directing output to a
file is usually the safest option.
