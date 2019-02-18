# os4411

This is the os4411 operating system development environment.  It
runs in user space, but paging is more-or-less real.  In the 'earth'
directory are the sources for a simulated hardware environment that
includes a software-loaded TLB and interrupt handling.  The 'grass'
directory implements the operating system on top of this simulated
hardware.

Running this
============

Simply run:

    make run

Hopefully a login prompt will come up, and you can log in to the guest
account "guest" using password "guest".
Then you can run commands like 'ls', 'echo', and 'cat', or even run a
nested 'shell'.  Do "ls /bin" to see a list of all commands.

'make run' puts the terminal temporarily in 'raw' mode, meaning that input
characters are passed as is, and then runs "./a.out", the executable.
You can run "./a.out" directly yourself, but remember that the terminal
will then be in "cooked" mode and input/output will feel unnatural.

Debugging
=========

You can run the operating system in gdb, to do this, do:

    $ gdb ./a.out
    (gdb) handle SIGSEGV nostop noprint
    (gdb) handle SIGILL nostop noprint
    (gdb) r

The reason for the two handles is that we don't want gdb to intercept
the SIGSEGV signals that are used for page faulting, or the SIGILL
signals that are used for system calls.  (We use instruction '.long 6',
which is not an x86 instruction, as a system call instruction.) On
Mac OS X you probably also need to do the same for SIGBUS, as page
faults manifest themselves there that way.

You can set breakpoints, single step through code, print the values
of global and stack variables, see the stack trace, and so on.
Unfortunately, it is not possible to catch illegal accesses by the
kernel to memory, because those generate SIGSEGV, which we told the
debugger to ignore.  Instead, the 'earth' subsystem will print a report.
Also unfortunately, it is not really possible to run valgrind with the
operating system (to find uninitialized variables and/or memory leaks)
because valgrind gets confused by all the stack operations.

Directory Organization
======================
    apps        The sources for grass apps
    docs        Documents
    earth       The hardware simulator
    grass       The actual operating system
    lib         The library for grass apps
    shared      Code used by both the kernel and apps
    tools       Tools to convert Linux/MacOS binaries to grass binaries
