#pragma once
// Word search: find every dictionary word that starts at some cell and runs in a
// straight line in one of the 8 directions.
#include <bitset>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "wordengine/dawg.hpp"

namespace we {

struct Grid {
    int rows = 0, cols = 0;
    std::string cells;  // row-major, lowercase a-z
    char at(int r, int c) const { return cells[(size_t)r * cols + c]; }
};

// Lab 11 format: rows, cols, then the letters (one line or several).
Grid load_grid(const std::string &path);
// Random grid with English-like letter frequencies. Same seed = same grid on every platform.
Grid random_grid(int rows, int cols, uint32_t seed);

struct Match {
    std::string word;
    int row, col, dr, dc;
};

struct SearchStats {
    uint64_t letters = 0;  // letters appended while walking (search-space size)
    uint64_t checks = 0;   // n-gram or dictionary lookups
};

enum class Method {
    Lab,    // Lab 11 as written: any-position trigram filter, re-checks every window each step
    Ngram,  // same idea, but only the new window is checked and the first 2/3 letters
            // must be a valid word *start*
    Dawg,   // walk the DAWG: stop as soon as no word starts with the letters so far
};
const char *method_name(Method m);

class WordSearcher {
public:
    explicit WordSearcher(const std::vector<std::string> &words, size_t min_len = 3);

    std::vector<Match> find(const Grid &g, Method m, SearchStats *stats = nullptr) const;

    const Dawg &dawg() const { return dawg_; }
    size_t min_len() const { return min_len_; }

private:
    static int tri(char a, char b, char c) { return ((a - 'a') * 26 + (b - 'a')) * 26 + (c - 'a'); }
    static int bi(char a, char b) { return (a - 'a') * 26 + (b - 'a'); }

    size_t min_len_;
    std::unordered_set<std::string> words_;   // words of length >= min_len
    std::unordered_set<std::string> tri_any_; // Lab: every 3-letter substring, as strings
    std::bitset<26 * 26> start2_;             // first 2 letters of some word
    std::bitset<26 * 26 * 26> start3_;        // first 3 letters of some word
    std::bitset<26 * 26 * 26> tri_bits_;      // any 3-letter substring
    Dawg dawg_;
};

}  // namespace we
