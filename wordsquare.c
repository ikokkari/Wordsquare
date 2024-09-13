/*
Author: Ilkka Kokkarinen, ilkka.kokkarinen@gmail.com
Date: Sep 12, 2024
GitHub: https://github.com/ikokkari/Wordsquare
*/

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"
#include "limits.h"

/* Are we looking for double or single word squares? */
#define DOUBLE 1
#define STAT_DIV (DOUBLE ? 1000000 : 1000)

/* Big number to mean "none". */
#define M UINT_MAX

/* Output progress report about what is happening. */
#define VERBOSE 0

/* Level at which an intermediate grid is printed. */
#define PROGRESS 99

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

/* Convert a letter character to an integer code. */
#define ENC(c) (c - 'a')

/* Size of alphabet, and the corresponding remain set mask. */
#define ALPHABET 26
#define ALPHA_MASK 0x3FFFFFF

/* Starting positions of each one character prefix in the wordlist. */
uint one_start[ALPHABET];

/* Starting position of each two character prefix in the wordlist. */
uint two_start[ALPHABET][ALPHABET];

/* Starting position of each three character prefix in the wordlist. */
uint three_start[ALPHABET][ALPHABET][ALPHABET];

/* How many words start with each two character prefix. */
uint two_count[ALPHABET][ALPHABET];

/* How many words start with each three character prefix. */
uint three_count[ALPHABET][ALPHABET][ALPHABET];

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
uint undo_capacity = 100; /* Just the initial capacity, this will grow */

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

/* Bookkeeping statistics for the effectiveness of pruning. */
ulong remain_cutoffs = 0;
ulong prefix_cutoffs = 0;
uint max_level = 0;
char max_word[N + 1];

/* Read the list of words from the file words_sorted.txt, keeping the words of length N. */
void read_wordlist() {
  /* Initialize the table entries */
  for(uint i = 0; i < ALPHABET; i++) {
    one_start[i] = M;
    for(uint j = 0; j < ALPHABET; j++) {
      two_start[i][j] = M;
      two_count[i][j] = 0;
      for(uint k = 0; k < ALPHABET; k++) {
	three_start[i][j][k] = M;
	three_count[i][j][k]= 0;
      }
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
      /* One letter prefixes */
      if(one_start[ENC(buffer[0])] == M) {
	one_start[ENC(buffer[0])] = word_count;
      }
      /* Two letter prefixes */
      if(two_start[ENC(buffer[0])][ENC(buffer[1])] == M) {
	two_start[ENC(buffer[0])][ENC(buffer[1])] = word_count;
      }
      two_count[ENC(buffer[0])][ENC(buffer[1])]++;
      /* Three letter prefixes */
      if(three_start[ENC(buffer[0])][ENC(buffer[1])][ENC(buffer[2])] == M) {
	three_start[ENC(buffer[0])][ENC(buffer[1])][ENC(buffer[2])] = word_count;
      }
      three_count[ENC(buffer[0])][ENC(buffer[1])][ENC(buffer[2])]++;
      word_count++;
    }
  }
  fclose(file);
  taken = calloc(word_count, sizeof(uint));
  last_idx = word_count;
  if(VERBOSE) {
    printf("Finished reading wordlist of %d words.\n", word_count);
  }
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

/* Push an item into the undo stack. */
void undo_push(uint item) {
  /* Expand the stack size if needed. */
  if(undo_top == undo_capacity) {
    undo_capacity *= 2;
    undo = realloc(undo, undo_capacity * sizeof(uint));
  }
  undo[undo_top++] = item;
}

/* Place the word into the square starting at cell (x, y) into direction (dx, dy). */
void place_word(char* word, uint x, uint y, uint dx, uint dy, int update_checks) {
  if(update_checks) {
    memset(to_check, 0, 2 * N);
  }
  for(uint j = 0; j < N; j++) { 
    if(square[x][y] == '.') {
      square[x][y] = word[j];
      undo_push(x);
      undo_push(y);
      undo_push(UNDO_PLACE);
      if(update_checks && remain[x][y] != 1 << (ENC(word[j]))) {
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

/* Determine if the given word would fit starting at cell (x, y) to direction (dx, dy). */
uint word_fits(char* word, uint x, uint y, uint dx, uint dy) {
  for(uint i = 0; i < N; i++) {
    if(square[x][y] == '.' && (remain[x][y] & (1 << (ENC(word[i])))) == 0) {
      return 0;
    }
    x += dx;
    y += dy;
  }
  return 1;
}

/* Find the character prefix in the square starting from position (x, y) to direction (dx, dy). */
void find_prefix(char* buffer, uint x, uint y, uint dx, uint dy) {
  uint i = 0;
  while(i < N && square[x][y] != '.') {
    buffer[i++] = square[x][y];
    x += dx;
    y += dy;
  }
  buffer[i] = '\0'; /* Don't forget to put the NUL character in the end... */
}

/* Binary search to find the position of the first word that starts with the given prefix. */ 
uint bisect_left(char* prefix) {
  /* Look up the precomputed results for one-, two- and three-character prefixes. */
  if(prefix[1] == '\0') {
    return one_start[ENC(prefix[0])];
  }
  if(prefix[2] == '\0') {
    return two_start[ENC(prefix[0])][ENC(prefix[1])];
  }
  if(prefix[3] == '\0') {
    return three_start[ENC(prefix[0])][ENC(prefix[1])][ENC(prefix[2])];
  }
  
  /* Prefix is after the last word in the wordlist. */
  if(strcmp(wordlist[word_count - 1], prefix) < 0) {
    return word_count;
  }

  /* Find the first word alphabetically not before the given prefix. */
  uint lo = 0, hi = word_count - 1; /* Classic binary search. */
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
  uint possible[N]; /* Letters that are possible for each position. */
  char buffer[N + 1];
  for(uint j = 0; j < N; j++) { possible[j] = 0; }
  
  find_prefix(buffer, x, y, dx, dy);
  uint i = bisect_left(buffer);

  /* Go through the words that start with the prefix of the current row or column. */
  while(i < word_count && starts_with(buffer, wordlist[i])) {
    if(word_fits(wordlist[i], x, y, dx, dy)) {
      for(uint j = 0; j < N; j++) {
	if(square[x + dx * j][y + dy * j] == '.') {
	  /* This character is now possible in this position. */
	  possible[j] |= 1 << ENC(wordlist[i][j]);
	}
      }
    }
    i++;
  }

  /* Update the remain character set for the positions in the current row or column. */
  for(uint j = 0; j < N; j++) {
    if(square[x][y] == '.') {
      uint new_remain = possible[j] & remain[x][y];
      /* If no character is possible in this position, this partial solution is a dead end. */
      if(new_remain == 0) { 
        remain_cutoffs++;
	return 1;
      }
      /* If the set of remaining characters has decreased, record the change. */
      if(new_remain != remain[x][y]) {
	undo_push(remain[x][y]);
	remain[x][y] = new_remain;
	undo_push(x);
	undo_push(y);
	undo_push(UNDO_REMAIN);
	/* Force the perpendicular row or column to be checked again. */
	to_check[dx == 0 ? 2 * y + 1 : 2 * x] = 1;
      }
    }
    x += dx;
    y += dy;
  }
  return 0;
}

/* Perform consistency propagation throughout the entire square until convergence. */
uint update_all_remains(uint level) {
  while(1) {
    uint best_v = M; /* Tightest level found so far. */
    uint bv = M; /* Number of possible words starting with tightest prefix. */
    /* Find the tightest level that needs checking. */
    for(uint v = level; v < 2*N; v++) {
      if(to_check[v]) {
	  uint cv;
	  if(v & 1) {
	    if(level == 6) {
	      cv = three_count[ENC(square[0][v / 2])][ENC(square[1][v / 2])][ENC(square[2][v / 2])];
	    }
	    else {
	      cv = two_count[ENC(square[0][v / 2])][ENC(square[1][v / 2])];
	    }
	  }
	  else {
	    if(level == 6) {
	      cv = three_count[ENC(square[v / 2][0])][ENC(square[v / 2][1])][ENC(square[v / 2][2])];
	    }
	    else {
	      cv = two_count[ENC(square[v / 2][0])][ENC(square[v / 2][1])];
	    }
	  }
	  if(cv < bv) { /* We found a tighter level than the previous tightest one. */
	    best_v = v;
	    bv = cv;
	  }
      }
    }
    if(best_v == M) { /* No levels to check, we are done with constraint propagation. */
      return 1;
    }
    /* Update the remain sets for the chosen tightest level. */
    to_check[best_v] = 0;
    if(best_v & 1 ? update_one_remain(0, best_v / 2, 1, 0) : update_one_remain(best_v / 2, 0, 0, 1)) {
      return 0; /* Some remain set became empty, yay, we can backtrack. */
    }
  }
  return 1;
}

/* Verify that the two words on the first two rows are possible to complete into column words. */
uint verify_col_prefixes(char* word) {
  for(uint j = 1; j < N; j++) {
    if(two_start[ENC(square[0][j])][ENC(word[j])] == M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Verify that the three words on the first two rows are possible to complete into column words. */
uint verify_col_prefixes_three(char* word) {
  for(uint j = 2; j < N; j++) {
    if(three_start[ENC(square[0][j])][ENC(square[1][j])][ENC(word[j])]== M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Verify that the two words on the first two columns are possible to complete into row words. */
uint verify_row_prefixes(char* word) {
  for(uint i = 2; i < N; i++) {
    if(two_start[ENC(square[i][0])][ENC(word[i])] == M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Verify that the three words on the first two columns are possible to complete into row words. */
uint verify_row_prefixes_three(char* word) {
  for(uint i = 2; i < N; i++) {
    if(three_start[ENC(square[i][0])][ENC(square[i][1])][ENC(word[i])] == M) {
      prefix_cutoffs++;
      return 0;
    }
  }
  return 1;
}

/* Pop and execute instructions from the undo stack to downdate to previous state in backtracking. */
void unroll_choices() {
  uint item;
  while((item = undo[--undo_top]) != UNDO_DONE) {
    if(item == UNDO_PLACE) { /* Undo placing a letter into the grid. */
      uint y = undo[--undo_top];
      uint x = undo[--undo_top];
      square[x][y] = '.';
    }
    else if(item == UNDO_REMAIN) { /* Undo decreasing the size of remain set. */
      uint y = undo[--undo_top];
      uint x = undo[--undo_top];
      remain[x][y] = undo[--undo_top];
    }
  }
}

/* Recursive backtracking algorithm to fill in the double word square. */
void fill_square(uint level) {
  if(level > max_level) {
    max_level = level;
    find_prefix(max_word, 0, 0, 0, 1);
  }
  if(level == PROGRESS || level == 2 * N) { /* Print the complete square. */
    print_square();
  }
  if(level == 2 * N) { /* The grid is complete. */
    return;
  }
  uint x, y, dx, dy;
  if(level & 1) { /* Fill in a vertical column at odd levels of recursion. */
    x = 0; y = level / 2; dx = 1; dy = 0;
  }
  else { /* Fill in a horizontal row at even levels of recursion. */
    x = level / 2; y = 0; dx = 0; dy = 1;
  }
  find_prefix(word_buffer[level], x, y, dx, dy);
  uint i = level == 0 ? first_idx : bisect_left(word_buffer[level]);
  while(i < (level == 0 ? last_idx: word_count) &&
	(level != 1 || i < first_row_idx) &&
	(level == 0 || starts_with(word_buffer[level], wordlist[i]))) {
    if(!taken[i] && word_fits(wordlist[i], x, y, dx, dy)) {
      if(level == 0) { /* Place the current word in the first row as a special case. */
        first_row_idx = i;
        if(VERBOSE) {
          printf("\x1b[AMoving to #%d '%s' (%ldR and %ldP), max level %d (%s).\n",
		 i, wordlist[i], remain_cutoffs / STAT_DIV, prefix_cutoffs / STAT_DIV, max_level, max_word);
        }
      }
      uint prefixes_fit = 1;
      if(level == 2) { prefixes_fit = verify_col_prefixes(wordlist[i]); }
      else if(level == 3) { prefixes_fit = verify_row_prefixes(wordlist[i]); }
      else if(level == 4) { prefixes_fit = verify_col_prefixes_three(wordlist[i]); }
      else if(level == 5) { prefixes_fit = verify_row_prefixes_three(wordlist[i]); }
      if(prefixes_fit) {
	undo_push(UNDO_DONE);
	place_word(wordlist[i], x, y, dx, dy, level == 3);
	if(!DOUBLE) {
	  place_word(wordlist[i], y, x, dy, dx, level == 3);
	}
	if(!DOUBLE || level != 3 || update_all_remains(level + 1)) {
	  taken[i] = 1;
	  fill_square(DOUBLE? level + 1 : level + 2);
	  taken[i] = 0;
	}
	unroll_choices();
      }
    }
    i++;
  }
}

/* Returns the word from the given position of the wordlist, or empty string for positions outside the list. */
char* get_word(uint idx) {
  return idx < word_count ? wordlist[idx] : "";
}

/* Main driver of the backtracking algorithm. */
int main(int argc, char** argv) {
  read_wordlist();
  undo = malloc(sizeof(uint) * undo_capacity);
  to_check = calloc(2 * N, sizeof(uint));
  strcpy(max_word, "$");
  for(uint x = 0; x < N; x++) {
    for(uint y = 0; y < N; y++) {
      remain[x][y] = ALPHA_MASK; /* Integer with the lowest ALPHABET bits on. */
      square[x][y] = '.'; /* Grid is initially all empty. */
    }
  }

  /* Starting word of the search. */
  if(argc > 1) {
    first = malloc(strlen(argv[1]) + 1);
    strcpy(first, argv[1]);
    first_idx = bisect_left(first);
  }

  /* Ending word of the search. */
  if(argc > 2) {
    last = malloc(strlen(argv[2]) + 1);
    strcpy(last, argv[2]);
    last_idx = bisect_left(last);
  }

  if(VERBOSE) {
    printf("%s word squares of N = %d, ", DOUBLE ? "Double" : "Single", N);
    printf("from %s (#%d) to %s (#%d).\n\n", get_word(first_idx), first_idx, get_word(last_idx), last_idx);
  }
    
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
