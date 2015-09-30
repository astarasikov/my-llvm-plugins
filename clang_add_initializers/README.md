Intro
=====

This is a clang rewriter that looks for uninitialized local and global
non-static variables in the given file and the files it includes
and adds the corresponding initializers:
zero (=0) for primitive types (ints, floats)
empty curly braces list (={}) for record types

It is loosely based on some clang tutorials.

Usage
============

I've only tested with LLVM 3.7 on Debian Jessie.
I've installed LLVM to /opt/my\_clang/.
You will also need libc++-dev and libc++abi-dev

Just invoke the CIrewriter with the same options that you would invoke clang.
Also looks like the source code file name must be the last argument now.

TODO
====

There is a lot of room for improvement (as it was my first ever attempt
at using LLVM API).
    * Ensure the same file is not processed multiple times.
    * Look for a way to get rid of explicitly specifying header paths
    * Convert to a plugin format so that one invokes the original clang binary
    * Learn how to use LLVM build system
    * Add blacklist for types (such as va\_list)
