#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

/* Size of grid */
#define N 9
/* Maximum size of wordlist */
#define MAXWORDS 100000
/* Should program terminate after finding first solution? */
#define EXIT_AFTER_FIRST 1

/* Opcodes for operations in undo stack */
#define UNDO_DONE 0
#define UNDO_PLACE 1
#define UNDO_REMAIN 2

/* We don't need negative numbers for anything in this program. */
typedef unsigned int uint;

/* For each cell of the square, the characters that are still possible for that cell,
   encoded as a bit vector in the lowest 26 bits of the unsigned integer. */
uint remain[N][N];

/* The contents of the square. Each element is either the character placed in that cell,
   or the period '.' to indicate an empty cell. */
char square[N][N];

/* The undo stack used to store the operations that restore previous state in backtracking. */
uint* undo;
int undo_top = 0;
int undo_capacity = 100;

/* Rows and columns that need to be checked in consistency propagation. */
uint* to_check;

/* A vector to keep track which words have already been taken into the double word square. */
uint* taken;

/* The possible words used to fill in the double word square. */
int word_count = 0;
char wordlist[MAXWORDS][N+1];

/* Cutoff statistics bookkeeping. */
unsigned long remain_cutoffs = 0;
unsigned long remain_update = 100000000;
unsigned long remain_goal;
char first = '$';

/* Read the list of words from the file words_sorted, keeping the words of length N. */

void read_wordlist() {
  char buffer[100];
  FILE* file = fopen("words_sorted.txt", "r");
  while(fgets(buffer, sizeof buffer, file) != NULL) {
    int j = 0;
    while('a' <= buffer[j] && buffer[j] <= 'z') {
      j++;
    }
    buffer[j] = '\0';
    if(j == N) {
      strcpy(wordlist[word_count++], buffer);
    }
  }
  fclose(file);
  printf("Read a total of %d words.\n", word_count);
  taken = calloc(word_count, sizeof(uint));
}

/* Print the contents of the current square. */
void print_square() {
  for(int i = 0; i < N; i++) {
    for(int j = 0; j < N; j++) {
      printf("%c", square[i][j]);
    }
    printf("\n");
  }
  printf("\n");
}

/* Push an item into the undo stack, expanding the stack space if needed. */
void undo_push(int item) {
  if(undo_top == undo_capacity) {
    undo_capacity *= 2;
    undo = realloc(undo, undo_capacity * sizeof(uint));
  }
  undo[undo_top++] = item;
}

/* Place the word into the square starting at cell (x, y) into direction (dx, dy). */
void place_word(char* word, int x, int y, int dx, int dy) {
  for(int j = 0; j < N; j++) { 
    if(square[x][y] == '.') {
      square[x][y] = word[j];
      undo_push(x);
      undo_push(y);
      undo_push(UNDO_PLACE);
      if(remain[x][y] != 1 << (word[j] - 'a')) {
	if(dx == 0) {
	  to_check[2*y+1] = 1;
	}
	else {
	  to_check[2*x] = 1;
	}
      }
    }
    x += dx;
    y += dy;
  }
}

/* Determine if the given word would fit into the square starting at cell (x, y)
   into direction (dx, dy). */
int word_fits(char* word, int x, int y, int dx, int dy) {
  for(int i = 0; i < N; i++) {
    char ch = word[i];
    if(square[x][y] != '.' && square[x][y] != ch) {
      return 0;
    }
    if(square[x][y] == '.' && (remain[x][y] & (1 << (ch-'a'))) == 0) {
      return 0;
    }
    x += dx;
    y += dy;
  }
  return 1;
}

/* Find the longest contiguous prefix in the square starting from position (x, y)
   into direction (dx, dy). */
void find_prefix(char* buffer, int x, int y, int dx, int dy) {
  int i = 0;
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
int bisect_left(char* prefix) {
  if(strcmp(wordlist[word_count-1], prefix) < 0) {
    return word_count;
  }
  int lo = 0, hi = word_count;
  while(lo < hi) {
    int mid = (lo + hi) / 2;
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
int starts_with(char* first, char* second) {
  int i = 0;
  while(1) {
    if(first[i] == '\0') { return 1; }
    if(first[i] != second[i]) { return 0; }
    i++;
  }
}

/* Update the remaining characters table for the row or column that starts at (x, y)
   and goes in direction (dx, dy). Returns 1 if dead end reached, 0 otherwise. */
int update_one_remain(int x, int y, int dx, int dy) {
  uint possible[N];
  char buffer[N+1];
  for(int j = 0; j < N; j++) { possible[j] = 0; }
  find_prefix(buffer, x, y, dx, dy);
  int i = bisect_left(buffer);
  if(i >= word_count || !starts_with(buffer, wordlist[i])) {
    return 1;
  }
  while(i < word_count && starts_with(buffer, wordlist[i])) {
    if(word_fits(wordlist[i], x, y, dx, dy)) {
      for(int j = 0; j < N; j++) {
	if(square[x+dx*j][y+dy*j] == '.') {
	  possible[j] |= 1 << (wordlist[i][j] - 'a');
	}
      }
    }
    i++;
  }
  for(int j = 0; j < N; j++) {
    int xx = x+dx*j, yy = y+dy*j;
    if(square[xx][yy] == '.') {
      uint new_remain = possible[j] & remain[xx][yy];
      if(new_remain == 0) {
	if(++remain_cutoffs == remain_goal) {
	  printf("Remain cutoffs for %c: %lu\n", first, remain_cutoffs);
	  remain_goal += remain_update;
	}
	return 1;
      }
      if(new_remain != remain[xx][yy]) {
	undo_push(remain[xx][yy]);
	remain[xx][yy] = new_remain;
	undo_push(xx);
	undo_push(yy);
	undo_push(UNDO_REMAIN);
	if(dx == 0) {
	  to_check[2*yy+1] = 1;
	}
	else {
	  to_check[2*xx] = 1;
	}
      }
    }
  }
  return 0;
}

/* Perform consistency propagation throughout the entire square until convergence. */
int update_all_remains() {
  int result;
  while(1) {
    int v = 0;
    while(v < 2*N) {
      if(to_check[v]) { break; }
      v++;
    }
    if(v == 2*N) { return 1; }
    to_check[v] = 0;
    if(v & 1) {
      result = update_one_remain(0, v/2, 1, 0);
    }
    else {
      result = update_one_remain(v/2, 0, 0, 1);
    }
    if(result) { return 0; }
  }
  return 1;
}

/* Verify that the the first four words placed on first two rows and columns haven't
   created prefixes that are impossible to extend into words. */
int verify_prefixes(int level) {
  char buffer[N+1];
  int x, y, dx, dy;
  for(int v = level/2; v < 2*N; v++) {
    if(v & 1) {
      x = 0; y = v/2; dx = 1; dy = 0;
    }
    else {
      x = v/2; y = 0; dx = 0; dy = 1;
    }
    find_prefix(buffer, x, y, dx, dy);
    int i = bisect_left(buffer);
    if(i >= word_count || !starts_with(buffer, wordlist[i])) {
      return 0;
    }
  }
  return 1;
}

/* Pop and execute instructions from the undo stack to restore previous state in backtracking. */
void unroll_choices() {
  int item = undo[--undo_top];
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
void fill_square(int level) {
  char buffer[N+1];
  char first_row[N+1];
  if(level == 2*N) {
    print_square();
    if(EXIT_AFTER_FIRST) {
      exit(0);
    }
    return;
  }
  int x, y, dx, dy;
  if(level & 1) { // Fill in a vertical column at this level of recursion.
    x = 0; y = level/2; dx = 1; dy = 0;
  }
  else { // Fill in a horizontal row at this level of recursion.
    x = level/2; y = 0; dx = 0; dy = 1;
  }
  if(level == 1) {
    find_prefix(first_row, 0, 0, 0, 1);
  }
  find_prefix(buffer, x, y, dx, dy);
  int i = bisect_left(buffer);
  while(i < word_count && starts_with(buffer, wordlist[i])) {
    if(!taken[i] && word_fits(wordlist[i], x, y, dx, dy)) {
      if(level == 1 && strcmp(wordlist[i], first_row) > 0) { break; }
      for(int j = 0; j < 2*N; j++) { to_check[j] = 0; }
      undo_push(UNDO_DONE);
      place_word(wordlist[i], x, y, dx, dy);
      if(
	 (level != 4 || verify_prefixes(4)) &&
	 (level != 3 || verify_prefixes(3)) &&
	 (level < 4 || update_all_remains())
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

/* Main driver of the backtracking algorithm. */
int main(int argc, char** argv) {
  remain_goal = remain_update;
  read_wordlist();
  undo = malloc(sizeof(uint) * undo_capacity);
  to_check = calloc(3*N, sizeof(uint));
  for(int x = 0; x < N; x++) {
    for(int y = 0; y < N; y++) {
      remain[x][y] = 0x3FFFFFF; // Integer with the lowest 26 bits on
      square[x][y] = '.'; // Grid is initially all empty
    }
  }
  if(argc > 1) {
    first = square[0][0] = argv[1][0];
  }
  fill_square(0);
  printf("All done!\n");

  /* So that Valgrind won't complain about memory leaks. */
  free(undo);
  free(to_check);
  free(taken);

  /* All is well in the world. */
  return 0;
}
