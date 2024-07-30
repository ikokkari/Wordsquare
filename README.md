# Wordsquare
This repository contains C and Python programs to find double word squares of given size. The Python program was written first as a proof of concept, and then translated to C for faster execution.

The source code file `wordsquare.c` is self-contained and has no dependencies outside the standard library. You can compile it with your favourite C compiler. You should first edit the constant N to choose the size of the grid, and EXIT_AFTER_FIRST to make the program print the first double square that it finds. Once compiled, you can run it with the command

```
wordsquare l
```

where `l` is the first letter of the word square that you are looking for.

The programs use backtracking search with forward checking for the unfilled grid, plus some other small optimizations.

