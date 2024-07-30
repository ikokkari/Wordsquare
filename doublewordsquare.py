from bisect import bisect_left
from sys import exit


def compute_order(n):
    order, curr = [], 0
    for s in range(0, 2*n):
        for x in range(s+1):
            y = s-x
            if 0 <= x < n and 0 <= y < n:
                order.append((x, y))
    return order


letters = 'abcdefghijklmnopqrstuvwxyz'
cutoffs = 0


def read_words(n):
    global letters
    with open('words_sorted.txt', encoding="utf-8") as f:
        wordlist = [word.strip() for word in f]
    print(f"Read in a word list of {len(wordlist)} words.")
    wordlist = [word for word in wordlist if len(word) == n]
    print(f"There remain {len(wordlist)} words of length {n}.")
    counts = [0]*26
    for word in wordlist:
        for c in word:
            counts[ord(c) - ord('a')] -= 1
    letters = [c for c in letters]
    letters.sort(key=lambda c:counts[ord(c) - ord('a')])
    letters = "".join(letters)
    print(f"Letter order is {letters}")
    return wordlist


def print_square(square):
    for x in range(len(square)):
        for y in range(len(square[x])):
            print(square[x][y], end="")
        print("   ", end="")
        for y in range(len(square[x])):
            print(square[y][x], end="")
        print()
    print("")


def fill_square(d, n, order, square, wordlist, taken, naket, watch=-1):
    global letters, cutoffs
    if d >= len(order):
        print(f"Taken is: {len(taken)} {taken}")
        print_square(square)
        exit(0)
    if d == watch:
        print(f"Progress so far: {cutoffs=}")
        print(f"Taken is: {len(taken)} {taken}")
        print_square(square)
    (x, y) = order[d]
    row = "".join(square[x][yy] for yy in range(y))
    col = "".join(square[xx][y] for xx in range(x))
    added = []

    if (x+1) in naket:
        chars = naket[x+1][y]
    elif -(y+1) in naket:
        chars = naket[-(y+1)][x]
    else:
        chars = letters

    for c in chars:
        for (word, idx) in added:
            del taken[word]
            del naket[idx]
        added = []

        rowc = row + c
        colc = col + c

        if y == 0 and colc < "".join(square[0][yy] for yy in range(x)):
            continue

        square[x][y] = c
        i = bisect_left(wordlist, rowc)
        if i < len(wordlist):
            word = wordlist[i]
            if not word.startswith(rowc):
                continue
            if i + 1 < len(wordlist) and not wordlist[i + 1].startswith(rowc):
                if x > 0 and y < n - 1:
                    coln = "".join(square[xx][y + 1] for xx in range(x))
                    coln += word[y + 1]
                    j = bisect_left(wordlist, coln)
                    if j >= len(wordlist) or not wordlist[j].startswith(coln):
                        cutoffs += 1
                        continue
                if word in taken:
                    if taken[word] != x + 1:
                        continue
                else:
                    taken[word] = x + 1
                    naket[x + 1] = word
                    added.append((word, x + 1))

        i = bisect_left(wordlist, colc)
        if i < len(wordlist):
            word = wordlist[i]
            if not word.startswith(colc):
                continue
            if i + 1 < len(wordlist) and not wordlist[i + 1].startswith(colc):
                if word in taken:
                    if taken[word] != -(y+1):
                        continue
                else:
                    taken[word] = -(y+1)
                    naket[-(y+1)] = word
                    added.append((word, -(y+1)))

        fill_square(d + 1, n, order, square, wordlist, taken, naket, watch)

    for (word, idx) in added:
        del taken[word]
        del naket[idx]
    square[x][y] = '.'


def word_square(n, start=None, watch=-1):
    wordlist = read_words(n)
    order = compute_order(n)
    square = [['.' for _ in range(n)] for _ in range(n)]
    if start:
        square[0][0] = start
        fill_square(1, n, order, square, wordlist, dict(), dict(), watch)
    else:
        fill_square(0, n, order, square, wordlist, dict(), dict(), watch)


word_square(9, watch=45, start='e')