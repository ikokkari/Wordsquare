# Wordsquare
This repository contains a program to generate [double word squares](https://en.wikipedia.org/wiki/Word_square#Double_word_squares) of given size. The author used it to perform an exhaustive search to verify that no double word squares of order 9 exist in English.

The C source code file `wordsquare.c` is self-contained and has no dependencies outside the standard library. It has been hardcoded to use the file `words_sorted.txt` as the wordlist, easy enough to change.

You can compile `wordsquare.c` with your favourite C compiler. You should first edit the constant `N` to determine the size of the grid. It has currently been set to 6 for demonstration purposes. Once compiled, you just start the run the program with the command

```
./wordsquare prefix_start prefix_end
```

where `prefix_start` is the prefix of the word in the first row of the grid where the program will start searching for double word squares, and `prefix_end` is the prefix where the search stops. For example,

```
./wordsquare p q
```

would generate all double word squares whose first word starts with the letter `p`. When searching for larger squares, you can turn on `VERBOSE` to make the program output the current word and some cutoff statistics.

The program uses backtracking search that fills the grid alternating between filling rows and columns, aided with constraint propagation for the unfilled cells in the remaining grid, plus some other small optimizations. The program generates all possible word squares in alphabetical order of the word in the first row. Since double word squares are symmetric anyway, the word in the first column is constrained to be lexicographically smaller than the word in the first row, even as it starts with the same letter.

By setting the variable `DOUBLE` to 0 before compilation, the program will look for regular word squares whose rows and columns contain the same words. For order 9, exactly one regular word square was found:

```
necessism
existence
circumfer
escarping
sturnidae
sempitern
infidelic
scenarize
mergences
```

The source code for `wordsquare.c` is released under the GPL v3 licence. Wordlist `words_sorted.txt` adapted from [dwyl/english-words](https://github.com/dwyl/english-words).
