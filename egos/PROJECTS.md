Here are some potential student projects:

- Write a file system.  Students could write a FAT or UNIX file system and
  keep state on disk.
- Write a file system cache.
- Add security features to the file system: file access bits
- Implement user-level threading and semaphores.  (Currently implemented in mt.c.)
- Implement a Multi-Level Feedback Queue.  The current scheduler is round-robin,
  and every 10ms the clock simply picks another process to run, which is
  highly inefficient.  (With this, it may not be necessary to have them
  implement their "own" context switching.)
  (Note, I put a solution in the code with #ifdef HW_MLFQ.  Should not be
  distributed to students.)
- Write a pager.  Currently pages are only fetched from the file system,
  but there is no paging device and nothing is ever paged out.  Should also
  include a page replacement algorithm such as the clock algorithm.
  (I put in a solution for this as well, with #ifdef HW_PAGING.)
- Make additions to the shell.  For example, input/output redirection, or
  even pipes.
- Add measurements: load avg, paging stats, cache hit ratio, TLB flushes,
  context switches, etc.
