/*
Author: Ilkka Kokkarinen, ilkka.kokkarinen@gmail.com
Date: Sep 4, 2024
GitHub: https://github.com/ikokkari/Wordsquare
*/

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

/* Big number to mean "none" */
#define M 1000000

/* Output progress report about what is happening. */
#define VERBOSE 0

/* Size of grid square */
#define N 6

/* Maximum size of wordlist. */
#define MAXWORDS 100000

/* Opcodes for operations in undo stack. */
#define UNDO_DONE 0
#define UNDO_PLACE 1
#define UNDO_REMAIN 2

/* We don't need negative numbers for anything in this program. */
typedef unsigned int uint;
typedef unsigned long ulong;

/* Two-letter prefixes that exist in words. */
uint prefixes[26][26];

/* Starting positions of each one character prefix in the wordlist. */
uint one_start[26];

/* Starting position of each two character prefix in the wordlist. */
uint two_start[26][26];

/* How many words start with given two character prefix. */
uint two_count[26][26];

/* For each cell of the square, the characters that are still possible for that cell,
   encoded as a bit vector in the lowest 26 bits of the unsigned integer. */
uint remain[N][N];

/* The contents of the square. Each element is either the character placed in that cell,
   or the period '.' to indicate an empty cell. */
char square[N][N];

/* The words chosen so far. */
char word_buffer[2 * N][N + 1];

/* The undo stack used to store the operations to restore previous state in backtracking. */
uint* undo;
uint undo_top = 0;
uint undo_capacity = 100;

/* Rows and columns that need to be checked in consistency propagation. */
uint* to_check;

/* A vector to keep track which words have already been taken into the double word square. */
uint* taken;

/* The possible words used to fill in the double word square. */
uint word_count = 0;
char wordlist[MAXWORDS][N + 1];

/* The start of the words to try in the first row. */
char* first = NULL;
uint first_idx = 0;

/* The end of the words to try in the first row. */
char* last = NULL;
uint last_idx = 0;

/* Index to wordlist of the word on the first row. */
uint first_row_idx = 0;

/* Bookkeeping statistics for effectiveness of remain pruning. */
ulong remain_cutoffs = 0;
ulong prefix_cutoffs = 0;
uint max_level = 0;

/* Read the list of words from the file words_sorted.txt, keeping the words of length N. */
void read_wordlist() {
  for(uint i = 0; i < 26; i++) {
    one_start[i] = M;
    for(uint j = 0; j < 26; j++) {
      two_start[i][j] = M;
      two_count[i][j] = 0;
    }
  }
  char buffer[100];
  FILE* file = fopen("words_sorted.txt", "r");
  while(fgets(buffer, sizeof buffer, file) != NULL) {
    uint j = 0;
    while('a' <= buffer[j] && buffer[j] <= 'z') {
      j++;
    }
    buffer[j] = '\0';
    if(j == N) {
      strcpy(wordlist[word_count], buffer);
      if(one_start[buffer[0] - 'a'] == M) {
	one_start[buffer[0] - 'a'] = word_count;
      }
      if(two_start[buffer[0] - 'a'][buffer[1] - 'a'] == M) {
	two_start[buffer[0] - 'a'][buffer[1] - 'a'] = word_count;
	two_count[buffer[0] - 'a'][buffer[1] - 'a'] = 1;
      }
      else {
	two_count[buffer[0] - 'a'][buffer[1] - 'a']++;
      }
      word_count++;
    }
  }
  fclose(file);
  taken = calloc(word_count, sizeof(uint));
  last_idx = word_count;
}

/* Print the contents of the current square. */
void print_square() {
  for(uint i = 0; i < N; i++) {
    for(uint j = 0; j < N; j++) {
      printf("%c", square[i][j]);
    }
    printf("\n");
  }
  printf("\n\n");
}

/* Push an item into the undo stack, expanding the stack space if needed. */
void undo_push(uint item) {
  if(undo_top == undo_capacity) {
    undo_capacity *= 2;
    undo = realloc(undo, undo_capacity * sizeof(uint));
  }
  undo[undo_top++] = item;
}

/* Place the word into the square starting at cell (x, y) into direction (dx, dy). */
void place_word(char* word, uint x, uint y, uint dx, uint dy) {
  for(uint j = 0; j < N; j++) { 
    if(square[x][y] == '.') {
      square[x][y] = word[j];
      undo_push(x);
      undo_push(y);
      undo_push(UNDO_PLACE);
      if(remain[x][y] != 1 << (word[j] - 'a')) {
	if(dx == 0) {
	  to_check[2 * y + 1] = 1;
	}
	else {
	  to_check[2 * x] = 1;
	}
      }
    }
    x += dx;
    y += dy;
  }
}

/* Determine if the given word would fit into the square starting at cell (x, y)
   into direction (dx, dy). */
uint word_fits(char* word, uint x, uint y, uint dx, uint dy) {
  for(uint i = 0; i < N; i++) {
    char ch = word[i];
    if(square[x][y] != '.' && square[x][y] != ch) {
      return 0;
    }
    if(square[x][y] == '.' && (remain[x][y] & (1 << (ch - 'a'))) == 0) {
      return 0;
    }
    x += dx;
    y += dy;
  }
  return 1;
}

/* Find the longest contiguous prefix in the square starting from position (x, y)
   into direction (dx, dy). */
void find_prefix(char* buffer, uint x, uint y, uint dx, uint dy) {
  uint i = 0;
  while(i < N) {
    if(square[x][y] != '.') {
      buffer[i++] = square[x][y];
      x += dx;
      y += dy;
    }
    else {
      break;
    }
  }
  buffer[i] = '\0';
  return;
}

/* Binary search algorithm to find the position of the first word that starts with 
   the given prefix. */ 
uint bisect_left(char* prefix) {
  /* Look up the precomputed results for one- and two-character prefixes. */
  if(prefix[1] == '\0') {
    return one_start[prefix[0] - 'a'];
  }
  if(prefix[2] == '\0') {
    return two_start[prefix[0] - 'a'][prefix[1] - 'a'];
  }
  
  /* Prefix is after the last word in the wordlist. */
  if(strcmp(wordlist[word_count - 1], prefix) < 0) {
    return word_count;
  }

  /* Use binary search to find the first word that starts with the given prefix. */
  uint lo = 0, hi = word_count - 1;
  while(lo < hi) {
    uint mid = (lo + hi) / 2;
    if(strcmp(wordlist[mid], prefix) < 0) {
      lo = mid + 1;
    }
    else {
      hi = mid;
    }
  }
  return lo;
}

/* Determines if the first word is a prefix of the second word. */
uint starts_with(char* first, char* second) {
  uint i = 0;
  while(1) {
    if(first[i] == '\0') { return 1; }
    if(first[i] != second[i]) { return 0; }
    i++;
  }
}

/* Update the remaining characters table for the row or column that starts at (x, y)
   and goes in direction (dx, dy). Returns 1 if dead end reached, 0 otherwise. */
uint update_one_remain(uint x, uint y, uint dx, uint dy) {
  uint possible[N];
  char buffer[N + 1];
  for(uint j = 0; j < N; j++) { possible[j] = 0; }
  find_prefix(buffer, x, y, dx, dy);
  uint i = bisect_left(buffer);
  if(i >= word_count || !starts_with(buffer, wordlist[i])) {
    return 1;
  }
  while(i < word_count && starts_with(buffer, wordlist[i])) {
    if(word_fits(wordlist[i], x, y, dx, dy)) {
      for(uint j = 0; j < N; j++) {
	if(square[x + dx * j][y + dy * j] == '.') {
	  possible[j] |= 1 << (wordlist[i][j] - 'a');
	}
      }
    }
    i++;
  }
  for(uint j = 0; j < N; j++) {
    uint xx = x + dx * j, yy = y + dy * j;
    if(square[xx][yy] == '.') {
      uint new_remain = possible[j] & remain[xx][yy];
      if(new_remain == 0) {
        remain_cutoffs++;
	return 1;
      }
      if(new_remain != remain[xx][yy]) {
	undo_push(remain[xx][yy]);
	remain[xx][yy] = new_remain;
	undo_push(xx);
	undo_push(yy);
	undo_push(UNDO_REMAIN);
	if(dx == 0) {
	  to_check[2 * yy + 1] = 1;
	}
	else {
	  to_check[2 * xx] = 1;
	}
      }
    }
  }
  return 0;
}

/* Perform consistency propagation throughout the entire square until convergence. */
uint update_all_remains(uint level) {
  while(1) {
    /* Find the tightest level that needs checking. */
    uint v = level;
    uint best_v = M;
    uint bv = 0;
    while(v < 2 * N) {
      if(to_check[v]) {
	if(best_v == M) {
	  best_v = v;
	  if(best_v & 1) {
	    bv = two_count[square[0][best_v / 2] - 'a'][square[1][best_v / 2] - 'a'];
	  }
	  else {
	    bv = two_count[square[best_v / 2][0] - 'a'][square[best_v / 2][1] - 'a'];
	  }
	}
	else {
	  uint cv;
	  if(v & 1) {
	    cv = two_count[square[0][v / 2] - 'a'][square[1][v / 2] - 'a'];
	  }
	  else {
	    cv = two_count[square[v / 2][0] - 'a'][square[v / 2][1] - 'a'];
	  }
	  if(cv < bv) {
	    best_v = v;
	  }
	}
      }
      v++;
    }
    if(best_v == M) { /* All done with checking. */
      return 1;
    } 
    to_check[best_v] = 0;
    if(best_v & 1 ? update_one_remain(0, best_v / 2, 1, 0) : update_one_remain(best_v / 2, 0, 0, 1)) {
      return 0;
    }
  }
  return 1;
}

/* Verify that the two words on the first two rows are possible to complete into column words. */  
uint verify_col_prefixes() {
  for(uint j = 1; j < N; j++) {
    if(two_start[square[0][j] - 'a'][square[1][j] - 'a'] == M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Verify that the two words on the first two columns are possible to complete into row words. */
uint verify_row_prefixes() {
  for(uint i = 2; i < N; i++) {
    if(two_start[square[i][0] - 'a'][square[i][1] - 'a'] == M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Pop and execute instructions from the undo stack to restore previous state in backtracking. */
void unroll_choices() {
  uint item = undo[--undo_top];
  while(item != UNDO_DONE) {
    if(item == UNDO_PLACE) {
      uint y = undo[--undo_top];
      uint x = undo[--undo_top];
      square[x][y] = '.';
    }
    else if(item == UNDO_REMAIN) {
      uint y = undo[--undo_top];
      uint x = undo[--undo_top];
      remain[x][y] = undo[--undo_top];
    }
    item = undo[--undo_top];
  }
}

/* Recursive backtracking algorithm to fill in the double word square. */
void fill_square(uint level) {
  if(level > max_level) { max_level = level; }
  if(level == 2 * N) { /* The grid is complete and ready to be printed out. */
    print_square();
    return;
  }
  uint x, y, dx, dy;
  if(level & 1) { /* Fill in a vertical column at this level of recursion. */
    x = 0; y = level / 2; dx = 1; dy = 0;
  }
  else { /* Fill in a horizontal row at this level of recursion. */
    x = level / 2; y = 0; dx = 0; dy = 1;
  }
  find_prefix(word_buffer[level], x, y, dx, dy);
  uint i = level == 0 ? first_idx : bisect_left(word_buffer[level]);
  while(i < (level == 0 ? last_idx: word_count) && (level != 1 || i < first_row_idx) && starts_with(word_buffer[level], wordlist[i])) {
    if(!taken[i] && word_fits(wordlist[i], x, y, dx, dy)) {
      if(level == 0) {
        first_row_idx = i;
        if(VERBOSE) {
          printf("\x1b[AMoving to word #%d '%s' (%ldMR and %ldMP), max level %d.\n",
		   i - first_idx, wordlist[i], remain_cutoffs / 1000000, prefix_cutoffs / 1000000, max_level);
        }
      }
      for(uint j = 0; j < 2 * N; j++) { to_check[j] = 0; }
      undo_push(UNDO_DONE);
      place_word(wordlist[i], x, y, dx, dy);
      if(
         (level != 2 || verify_col_prefixes()) &&
	 (level != 3 || verify_row_prefixes()) &&
	 (level < 4 || update_all_remains(level + 1))
      ) {
	taken[i] = 1;
	fill_square(level + 1);
	taken[i] = 0;
      }
      unroll_choices();
    }
    i++;
  }
}

char* get_word(uint idx) {
  return idx < word_count ? wordlist[idx] : "";
}

/* Main driver of the backtracking algorithm. */
int main(int argc, char** argv) {
  read_wordlist();
  undo = malloc(sizeof(uint) * undo_capacity);
  to_check = calloc(3 * N, sizeof(uint));
  for(uint x = 0; x < N; x++) {
    for(uint y = 0; y < N; y++) {
      remain[x][y] = 0x3FFFFFF; /* Integer with the lowest 26 bits on. */
      square[x][y] = '.'; /* Grid is initially all empty. */
    }
  }

  /* Place the prefix on the first row. */
  if(argc > 1) {
    first = malloc(strlen(argv[1]) + 1);
    strcpy(first, argv[1]);
    first_idx = bisect_left(first);
  }

  if(argc > 2) {
    last = malloc(strlen(argv[2]) + 1);
    strcpy(last, argv[2]);
    last_idx = bisect_left(last);
  }

  printf("Starting search from %s to %s.\n\n", get_word(first_idx), get_word(last_idx));
  
  /* Do the watussi, Johnny */
  fill_square(0);

  printf("Ending search from %s to %s.\n\n", get_word(first_idx), get_word(last_idx));
  
  /* So that Valgrind won't complain about memory leaks. */
  free(undo);
  free(to_check);
  free(taken);
  free(first);
  free(last);

  /* All is well in the world. */
  return 0;
}
