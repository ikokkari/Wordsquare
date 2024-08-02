# Wordsquare
This repository contains C and Python programs to find double word squares of given size. The Python program was written first as a proof of concept, and then translated to C for faster execution.

The source code file `wordsquare.c` is self-contained and has no dependencies outside the standard library. It has been hardcoded to use the file `words_sorted.txt` as the wordlist, but this should be easy enough to change.

You can compile `wordsquare.c` with your favourite C compiler. You should first edit the constants `N` to determine the size of the grid, and `EXIT_AFTER_FIRST` to make the program print either only the first double square that it finds, or iterate them all in lexicographic order. Once compiled, you can run it with the command

```
./wordsquare prefix
```

where `prefix` is the prefix of the word in the first row of the grid.

Both programs use backtracking search that fills the grid alternating between filling the next word in the next row and column, aided with forward checking for the unfilled cells in the remaining grid, plus some other small optimizations. They will iterate through all possible word squares in alphabetical order of the word in the first row. Since double word squares are symmetric anyway, the word in the first column is constrained to be lexicographically smaller than the word in the first row, even as it starts with the same letter.

