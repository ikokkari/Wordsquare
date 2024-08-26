# Wordsquare
This repository contains C and Python programs to find [double word squares](https://en.wikipedia.org/wiki/Word_square#Double_word_squares) of given size. The Python program was written first as a proof of concept, and then translated to C for faster execution, with additional improvements.

The C source code file `wordsquare.c` is self-contained and has no dependencies outside the standard library. It has been hardcoded to use the file `words_sorted.txt` as the wordlist, easy enough to change.

You can compile `wordsquare.c` with your favourite C compiler. You should first edit the constants `N` to determine the size of the grid. It has currently been set to 6 for demonstration purposes. However, the largest known double word squares in English have the length 8, so you can make it 9 to look for double word squares of that size and become famous for being the first person to discover one. Once compiled, you just start the run the program with the command

```
./wordsquare prefix_start prefix_end
```

where `prefix_start` is the prefix of the word in the first row of the grid where the program will start searching for double word squares, and `prefix_end` is the prefix where the search stops. If you have extra processor cycles available, set this prefix to something and let it run. Perhaps you will become the first person to discover a valid 9-by-9 double word square.

Both programs use backtracking search that fills the grid alternating between filling the next word in the next row and column, aided with forward checking for the unfilled cells in the remaining grid, plus some other small optimizations. They will iterate through all possible word squares in alphabetical order of the word in the first row. Since double word squares are symmetric anyway, the word in the first column is constrained to be lexicographically smaller than the word in the first row, even as it starts with the same letter.

The source code for `wordsquare.c` is released under the GPL v3 licence. Wordlist `words_sorted.txt` adapted from [dwyl/english-words](https://github.com/dwyl/english-words).
