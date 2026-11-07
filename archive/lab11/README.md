# Lab 11 (original)

My DSA Lab 11 word-search solution, kept unchanged as the starting point for this repo.

- Two chained hash tables: `h1` holds the dictionary words, `h2` holds every 3-letter piece found inside any word.
- From each cell it walks in 8 directions. Once the string has 3+ letters it checks every 3-letter window against `h2`, stops at the first miss, then looks the whole string up in `h1`.

`stopWatch.hpp` here is a small replacement for the course-provided header, which isn't mine to publish. The course grids and word lists aren't included either; use `data/enable1.txt` and `wsearch --random` instead.

```
g++ -O2 -std=c++17 LAB11.cpp -o lab11
./lab11 ../../data/enable1.txt some.grid.txt
```

The improved versions are in `src/wordsearch.cpp` as `--method ngram` and `--method dawg`. `--method lab` re-implements this file's search for side-by-side benchmarks.
