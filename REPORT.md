# Operating Systems Programming Assignment 02

## Student Information
- Roll Number: BSDSF24A019
- Assignment: Re-engineering the ls Utility

## Feature 1: Project Setup
- Created the required project directories: src, bin, obj, and man.
- Added the starter C source file.
- Created a Makefile to compile the program.
- Compiled and tested the starter program.

## Build
Run `make` to build the program.

## Run
Run `./bin/ls` to execute the program.

## Features Implemented
Feature 1: Project setup and build system.

## Feature 2: Long Listing Format (v1.1.0)

**Branch:** `feature-long-listing-v1.1.0`

**Option:** `-l`

### Implementation

The program now supports the `-l` command-line option through `getopt()`.

The long listing displays file type and permissions, link count, owner,
group, file size, modification time, and file name.

The implementation uses `lstat()` to retrieve metadata for directory
entries, `getpwuid()` to resolve owner IDs, `getgrgid()` to resolve group
IDs, and `ctime()` to format the modification timestamp. The custom
permission formatter handles regular files, directories, symbolic links,
and special permission bits.

The program also displays the target of a symbolic link when available.
The normal listing mode remains available without the `-l` option.

### Report Questions

**1. What is the difference between stat() and lstat()? When is lstat()
more appropriate for ls?**

`stat()` follows a symbolic link and returns information about the file
or directory that the link points to. `lstat()` returns information
about the symbolic link itself. In a long listing, `lstat()` is useful
when we want to show the link's own file type and permissions rather
than the target's metadata. It also helps us identify symbolic links
and display their targets.

**2. How can bitwise operators and predefined macros extract file type
and permission information from st_mode?**

The `st_mode` field stores file-type bits and permission bits together.
Macros such as `S_ISDIR(mode)` can test whether a mode represents a
directory, while `(mode & S_IFMT) == S_IFDIR` tests the file-type bits
using a mask. Permission masks such as `S_IRUSR`, `S_IWUSR`, and
`S_IXUSR` test the owner's read, write, and execute permissions.
Similar masks exist for the group and other users. Special bits such
as `S_ISUID`, `S_ISGID`, and `S_ISVTX` are checked separately so the
permission string can display `s`, `S`, `t`, or `T` where appropriate.

