# word-engine

A fast word finder in C++17. It grew out of my DSA Lab 11 word-search solution and is the base for a word-search solver and a crossword-tile game AI that anyone can play against in the browser.

## What it does now

`wsearch` finds every dictionary word written in a straight line (8 directions) in a letter grid.

Three search methods, all giving identical results:

| Method | Idea | Cost per letter |
|---|---|---|
| `lab` | Lab 11 as written: a hash set of every 3-letter piece found in any word. After each new letter, check **every** 3-letter window, then look the whole string up. | grows with the string's length |
| `ngram` | Same idea, tightened: the first 2 and first 3 letters must be a valid word *start*, and only the **newest** window is checked (older ones already passed). Bitsets instead of hash sets. | constant |
| `dawg` | Walk a DAWG (a trie where shared endings are merged). Stop the moment no word starts with the letters so far. | one array lookup |

## Results

ENABLE word list (172,823 words), random grids with English letter frequencies, seed 42, best of 3, MinGW g++ -O2 on a desktop PC:

| Grid | Words found | lab | ngram | dawg |
|---|---:|---:|---:|---:|
| 50×50 | 3,510 | 8.8 ms | 1.9 ms | 0.5 ms |
| 250×250 | 92,977 | 281 ms | 53 ms | 13 ms |
| 1000×1000 | 1,502,776 | 4.7 s | 0.89 s | 0.21 s |
| 2000×2000 | 6,028,666 | 19.0 s | 3.6 s | 0.87 s |

The DAWG for ENABLE has 54,164 nodes and takes 0.93 MB; it is built in about 0.1 s. Each node stores a 26-bit letter mask, and a child is found by counting the set bits below the letter (`popcount`), so there are no pointers and no hashing.

All three methods were also checked against the course's reference answers (3×3 up to 300×300 grids): same words, same positions, same directions.

## Build

```
cmake -S . -B build -G Ninja
cmake --build build
./build/tests
./build/wsearch data/enable1.txt --random 20x20 7
./build/wsearch --method lab data/enable1.txt my.grid.txt
./build/bench data/enable1.txt --sizes 50,250,1000
```

Grid file format (same as Lab 11): number of rows, number of columns, then the letters.

```
4
7
vztsodtpaxztaqaceetfjohtaaoe
```

## Layout

| Path | What |
|---|---|
| `include/wordengine/dawg.hpp`, `src/dawg.cpp` | DAWG built with Daciuk's incremental method, packed into flat arrays |
| `include/wordengine/wordsearch.hpp`, `src/wordsearch.cpp` | grids, the three search methods |
| `apps/wsearch.cpp` | command-line solver |
| `apps/bench.cpp` | benchmark |
| `tests/test_main.cpp` | tests |
| `archive/lab11/` | the original Lab 11 code, unchanged |
| `data/enable1.txt` | ENABLE word list (public domain) |

## Next

- Move generator for a crossword-tile board game (anchors and cross-checks over the DAWG), scoring, and three AI levels.
- WebAssembly build and a browser game: play against the AI, or paste a grid and solve it.
