# Wordsquare
This repository contains C and Python programs to find double word squares of given size. The Python program was written first as a proof of concept, and then translated to C for faster execution.

The source code file `wordsquare.c` is self-contained and has no dependencies outside the standard library. It has been hardcoded to use the file `words_sorted.txt` as the wordlist, but this should be easy enough to change.

You can compile `wordsquare.c` with your favourite C compiler. You should first edit the constants `N` to determine the size of the grid, and `EXIT_AFTER_FIRST` to make the program print either only the first double square that it finds, or iterate them all in lexicographic order. Once compiled, you can run it with the command

```
wordsquare x
```

where `x` is the letter at the top left corner of the word square.

Both programs use backtracking search with forward checking for the unfilled grid, plus some other small optimizations. They will iterate through all possible word squares in alphabetical order of the word in the first row. Since double word squares are symmetric anyway, the word in the first column is lexicographically greater than the word in the first row.

