set_hard_limits <- function(as = NULL, core = NULL, cpu = NULL, data = NULL, fsize = NULL,
                            memlock = NULL, nofile = NULL, nproc = NULL, stack = NULL){
  unix::rlimit_as(as, as)
  unix::rlimit_core(core, core)
  unix::rlimit_cpu(cpu, cpu)
  unix::rlimit_data(data, data)
  unix::rlimit_fsize(fsize, fsize)
  unix::rlimit_memlock(memlock, memlock)
  unix::rlimit_nofile(nofile, nofile)
  unix::rlimit_nproc(nproc, nproc)
  unix::rlimit_stack(stack, stack)
}
