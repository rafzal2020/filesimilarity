# filesimilarity
CS214 Project 2

Rayaan Afzal - ra965
Anthony Rahner - arr234

Description
-----------
`compare` reads a set of files (specified directly or by traversing
directories) and prints the pairwise Jensen-Shannon Distance (JSD) for
every pair of files, sorted in decreasing order of combined word count.

Directory Traversal
  Directories are traversed recursively using opendir/readdir/stat via
  handle_filepath(). Any entry whose name begins with '.' is skipped,
  including hidden files and the . and .. entries. Regular files are only
  added if their name ends with the configured suffix (default ".txt").
  Subdirectories are always recursed into. Explicitly provided file
  arguments are added regardless of suffix.

Error Handling
  File/directory open failures are reported with perror() and skipped;
  processing continues. malloc failures terminate immediately via the
  CHECK_ALLOC macro. If fewer than two files are collected, an error is
  printed and the program exits with EXIT_FAILURE. 


Testing Plan
------------

Our test suite used both individual file comparisons and a directory
(testdir) with the following structure:

  testdir/
    c.txt          ("apple banana apple orange")
    c_copy.txt     ("apple banana apple orange")
    d.txt          ("apple banana banana orange")
    subdir1/
      blah.txt     (12 words: blah, fruit, mango, etc.)
      crashouthill.txt (10 words: gonna, call, cops, etc.)
    subdir2/
      e.txt        ("purdue sucks apple mango")
      subsubdir/
        bus.txt    ("bus i'm gonna hop on this bus")

We also used standalone files: test1.txt (13 words), test2.txt (10 words),
baz.txt (3 words), and a.txt (2 words).

Test 1 - Known JSD value (fruit-themed files)
  Ran: ./compare test1.txt test2.txt
  test1.txt (13 words) and test2.txt (10 words) share several words
  (apple, banana, orange, fruit, delicious) but differ in others
  (grape, salad vs mango, tropical). Expected JSD: 0.55851.

Test 2 - Identical files produce JSD of 0
  Ran: ./compare testdir/c.txt testdir/c_copy.txt
  Both files contain "apple banana apple orange" — identical content.
  Expected JSD: 0.00000.

Test 3 - Similar but not identical files
  Ran: ./compare testdir/c.txt testdir/d.txt
  c.txt has apple:0.5, banana:0.25; d.txt has apple:0.25, banana:0.5.
  Same vocabulary, different frequencies. Expected JSD: 0.24754.

Test 4 - Nearly identical short files
  Ran: ./compare baz.txt a.txt
  baz.txt ("hello hi hi") and a.txt ("hello hi") share the same words
  but differ in frequency. Expected JSD: 0.14395.

Test 5 - Completely disjoint vocabularies produce JSD of 1
  Several pairs in testdir share no words at all (e.g. c.txt vs
  crashouthill.txt, c.txt vs bus.txt). Expected JSD: 1.00000.

Test 6 - Recursive traversal and file discovery
  Ran: ./compare testdir
  Verified all 7 .txt files were discovered across all nesting levels
  (root, subdir1, subdir2, subsubdir) and all 21 pairs were printed.

Test 7 - Correct output ordering
  Using the same testdir run, verified output was sorted in decreasing
  order of combined word count. The first row was:
    blah.txt vs crashouthill.txt (combined=22, JSD=0.90430)
  The last rows were pairs of 4-word files (combined=8).

Test 8 - Hidden file exclusion
  Placed a .hidden.txt file inside testdir. Confirmed it did not appear
  in any comparison pair in the output.

Test 9 - Non-.txt file exclusion
  Placed a .c file inside subdir1. Confirmed it was ignored and did
  not appear in the output.

Test 10 - Mixed explicit file and directory arguments
  Ran: ./compare test1.txt testdir
  Confirmed test1.txt was included alongside all 7 testdir files,
  producing 28 total pairs, and was not double-counted.

Test 11 - Fewer than two files exits with error
  Ran: ./compare testdir/c.txt
  Confirmed an error message was printed and the program exited with
  EXIT_FAILURE.

Test 12 - Memory and undefined behavior check
  Recompiled with -fsanitize=address,undefined and re-ran all of the
  above tests. No memory errors, leaks, or undefined behavior were
  reported by AddressSanitizer or UBSan.
