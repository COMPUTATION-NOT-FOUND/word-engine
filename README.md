# word-engine

A fast word finder in C++17. It grew out of my DSA Lab 11 word-search solution and now also runs two games with an AI opponent: **Tessera**, a crossword-tile board game, and **Word Hunt**, a race to find hidden words. Both play in the browser (the C++ is compiled to WebAssembly).

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

## Tessera (crossword-tile game)

An original game in the crossword-tile family, with its own board layout and letter values. 15Ã—15 board, 100 tiles (2 blanks), 7-tile racks, double/triple letter and word squares, +50 for using all 7 tiles. The engine tracks the bag, both racks, scores and the full move history, and checks every human move (one line, joined to the board, all words in the list).

**Move generation:** Appel–Jacobson over the DAWG. Only squares next to existing tiles ("anchors") start a search, and each empty square keeps a precomputed set of letters that make a valid word with the tiles above and below it. A typical position has 300–600 legal moves, found in well under a millisecond.

**AI levels**

| Level | How it chooses |
|---|---|
| Easy | a random move from the middle third by score |
| Medium | the highest score this turn |
| Hard | score + value of the tiles it keeps ("leave": penalises duplicates, bad vowel balance, Q without U); also considers swapping tiles |
| Expert | looks ahead: Monte-Carlo simulation, then an exact search once the bag is empty |

**How Expert looks ahead**

- *Simulation (bag not empty).* It takes the best ~12 candidates by Hard's measure and, for each one, plays out sample futures: draw random tiles for itself and a random rack for you from the tiles it hasn't seen, let you reply with your best move, then score its own next turn. The candidate with the best average outcome wins. Every candidate is tested against the same random futures (fairer comparisons with fewer samples), and every few rounds the weakest quarter is dropped so the time goes to the close calls ("successive halving"). It stops at the time limit.
- *Endgame (bag empty).* Now nothing is hidden: the AI knows exactly which tiles you hold. It runs a negamax alpha-beta search with iterative deepening (up to 12 moves deep), a transposition table, and the widest look at its own first move. Going out first scores the opponent's leftover tiles twice.

**Why not a chess engine's search?** Chess search (alpha-beta, PVS, null move, late-move reductions, futility pruning, SEE) assumes both players see the whole position. In Tessera you can't see the other rack or the next tiles from the bag, so a deep tree mostly explores positions that won't happen. Sampling those hidden tiles (simulation) is the technique strong programs for this kind of game use, and alpha-beta is used only at the end, where the game becomes perfect-information. Some chess ideas still carry over: move ordering, iterative deepening under a time budget, and a transposition table in the endgame.

**Self-play** (`tessera selfplay`, 20 games each, seats alternated):

| Match | Wins | Average score |
|---|---|---|
| Hard vs Medium | 16–4 | 467 vs 410 |
| Expert (300 ms/move) vs Hard | 13–7 | 461 vs 422 |

Expert typically samples 1,300–4,100 futures per move; a full endgame search to depth 12 took about 0.1 s.

## Word Hunt

A 12Ã—12 letter grid with every hidden word (4+ letters, any of 8 directions) found by the DAWG search. You and the AI race for 3 minutes; a word counts for whoever finds it first, worth its length minus 3. The AI's level sets how often it finds a word and how long its words are.

## Play in the browser

`web/` holds the page: `bindings.cpp` exposes the engine through embind, `worker.js` runs it in a Web Worker so the page stays smooth while the AI thinks, and `index.html`/`app.js` draw the games. `.github/workflows/pages.yml` builds and tests on every push, compiles the WebAssembly with Emscripten, and publishes the `release` branch to GitHub Pages (enable it once under Settings → Pages → Source: GitHub Actions).

Local web build (needs the Emscripten SDK):

```
emcmake cmake -S . -B build-web
cmake --build build-web
```

Then copy `web/*`, `build-web/engine.js`, `build-web/engine.wasm` and `data/enable1.txt` into one folder and serve it (for example `python -m http.server`); opening the file directly won't load the worker.

## Build

```
cmake -S . -B build -G Ninja
cmake --build build
./build/tests
./build/wsearch data/enable1.txt --random 20x20 7
./build/wsearch --method lab data/enable1.txt my.grid.txt
./build/bench data/enable1.txt --sizes 50,250,1000
./build/tessera play data/enable1.txt --level expert
./build/tessera selfplay data/enable1.txt --a expert --b hard --games 20 --time 300
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
| `board`, `movegen`, `game`, `ai` (.hpp/.cpp) | Tessera: board and tiles, move generator and validator, game rules, AI |
| `apps/bench.cpp` | benchmark |
| `apps/tessera.cpp` | play Tessera in the terminal, or AI self-play matches |
| `web/` | browser page, WebAssembly bindings and worker |
| `tests/test_main.cpp` | tests |
| `archive/lab11/` | the original Lab 11 code, unchanged |
| `data/enable1.txt` | ENABLE word list (public domain) |

## Next

- Learn the leave values from self-play instead of the hand-set table.
- Let Expert look further ahead when the bag is nearly empty ("pre-endgame").
- Solve a pasted grid in the browser.

## Licence

MIT, see [LICENSE](LICENSE). The word list in `data/enable1.txt` is the public-domain ENABLE list.
