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


## Feature 3: Column Display (v1.2.0)

**Branch:** `feature-column-display-v1.2.0`

### Implementation

The default directory listing now gathers visible filenames into a
dynamically allocated array before printing them. While collecting
entries, the program tracks the longest filename.

The terminal width is obtained using `ioctl()` with `TIOCGWINSZ`.
When terminal-width detection is unavailable, the program falls back
to 80 columns. The number of columns and rows is calculated from the
terminal width and the maximum filename length, with spacing between
columns.

The output is printed down each column and then across the row. The
`-l` long-listing mode from Feature 2 remains available.

### Report Questions

**1. Explain down-then-across printing. Why is one simple loop
insufficient?**

The program first collects the filenames and calculates the number
of rows. It then prints an item from each column for a row. The
index for a column is calculated as row + column * number_of_rows.
A single loop in directory-reading order would print entries one
after another and would not arrange them in the required column order.

**2. What is ioctl() used for, and what are the limitations of a
fixed-width fallback?**

The `ioctl()` call with `TIOCGWINSZ` obtains the current terminal
width so the program can choose a suitable number of columns.
A fixed width such as 80 characters may waste space on a wider
terminal or cause wrapping and poor alignment on a narrower one.
The fallback is useful when terminal-size detection is unavailable,
but it cannot adapt to the actual terminal dimensions.


## Feature 4: Horizontal Column Display (v1.3.0)

**Branch:** `feature-horizontal-display-v1.3.0`

**Option:** `-x`

### Implementation

The program now supports the `-x` option through `getopt()`.
The horizontal display function calculates a column width based on
the longest filename and determines how many entries can fit across
the terminal. It prints filenames in their original collection order
from left to right, wrapping to a new line when the row is full.

The default mode continues to display entries in down-then-across
columns, and the `-l` mode continues to print the long listing.

### Report Questions

**1. Compare down-then-across and across printing. Which needs
more pre-calculation, and why?**

Down-then-across printing requires more pre-calculation because the
program must determine the row count and map each row and column to
the correct filename index. Horizontal printing is simpler because
it can process filenames sequentially and move to the next line after
the current row reaches its available width.

**2. How are the -l, -x and default display modes managed?**

The program uses two state flags, `long_format` and
`horizontal_format`. The `getopt()` loop sets the corresponding flag
when it encounters `-l` or `-x` and clears the competing flag.
After collecting filenames, the program calls the long-listing
function logic for `-l`, the horizontal printer for `-x`, or the
down-then-across printer when neither option is selected.


## Feature 5: Alphabetical Sort (v1.4.0)

**Branch:** `feature-alphabetical-sort-v1.4.0`

### Implementation

The program now sorts collected directory entries using the standard
C library function `qsort()` before displaying them. The comparison
function receives two `const void *` arguments, treats them as
pointers to filename strings, and uses `strcmp()` to determine their
alphabetical order.

Sorting is performed before selecting the display logic. Therefore,
the default down-then-across mode, the `-x` horizontal mode, and the
`-l` long-listing mode all use the sorted array.

### Report Questions

**1. Why must directory entries be collected before sorting? What are
the drawbacks for directories containing millions of files?**

Sorting requires access to the collection of filenames being sorted.
The program first reads all visible directory entries into a dynamic
array and then sorts that array. For a directory containing millions
of files, this approach can consume substantial memory, require
additional allocation time, and make sorting slower. Allocation can
also fail if sufficient memory is unavailable.

**2. Explain the purpose and signature of the qsort() comparison
function. Why does it take const void * arguments?**

The comparison function tells `qsort()` how to compare two array
elements. Its signature is `int compare_names(const void *a,
const void *b)`. The generic `const void *` parameters allow `qsort()`
to work with arrays of different data types. The comparison function
casts or interprets these pointers according to the actual element
type, compares the strings using `strcmp()`, and returns a negative
value, zero, or a positive value depending on their order. The
`const` qualifier means the comparison function does not modify the
elements being compared.

