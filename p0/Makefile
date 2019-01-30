# Instructions/Makefile for the queue
# To compile run: 'make osx' (if on OSX) or 'make linux' (if on Linux).
# 'make clean' will clean out compiled files. Try doing this if you have any
# 'strange' issues with building.

# Note: This makefile is really written as 'gcc vs. clang'. For this code
# it should not matter which OS you are on, just make sure you have a compiler
# installed that is either gcc or clang. Clang is usually pre-installed on Macs
# with XCode. If you have build/compiling issues, please investigate them yourself
# before asking on Piazza or coming to office hours. Google is your friend!

# The point of this project is to teach you some basic C that you will need to complete
# the real OS project. You are implementing a simple, singly linked list in C. The 
# specifications are included in the queue.h file and are fairly self explanatory.
# This project will be *completely* autograded, so correctness is key! In addition to 
# turning in your queue.c file, you will also be turning in your tests, which will be
# graded by hand. Depending on how you package these tests you should be able to test the
# majority of the functionality in around 30 tests, so if you go on writing more than that
# than you may have duplicates.

# Put your tests in the test_queue.c file. It is compiled automatically when you run make 
# and includes a single basic test and framework. Testing is important, and since it is hand
# graded you should give some thought to these tests. Poor planning or organization will 
# be penalized. Your tests should be thorough and organized so that someone who hasn't seen
# your interface can figure out what's going on.

# Resources:
# You can read about how to install clang here: https://clang.llvm.org/get_started.html
# You can read about how to install gcc here: https://gcc.gnu.org/install/
# You can read about the C debugger, GDB, here: https://www.gnu.org/software/gdb/

# Basic C Reference : https://www.geeksforgeeks.org/c-language-set-1-introduction/

# Have fun!

osx:
	clang -Werror queue.c queue.h test_queue.c 
linux:
	gcc -Werror queue.h queue.c test_queue.c

clean:
	rm *.gch
	rm *.out
