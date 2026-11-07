#pragma once
// Tessera: a crossword-tile game with its own board layout, tile set and letter values.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace we {

constexpr int kSize = 15;          // board is kSize x kSize
constexpr int kCenter = kSize / 2;
constexpr int kRackSize = 7;
constexpr int kFullRackBonus = 50; // for playing all 7 tiles in one move
constexpr int kBlank = 26;         // index of the blank in letter-count arrays

enum class Premium : uint8_t { None, DoubleLetter, TripleLetter, DoubleWord, TripleWord };
Premium premium(int r, int c);     // layout is symmetric under all 8 board symmetries
int letter_mult(int r, int c);
int word_mult(int r, int c);

int letter_value(int letter);      // 0..25; blanks score 0
extern const std::array<int, 27> kTileCounts;  // tiles of each letter in a full bag (+ blanks)

// A tile on the board: 'a'..'z' a normal tile, 'A'..'Z' a blank standing for that letter, 0 empty.
inline bool is_blank_tile(char t) { return t >= 'A' && t <= 'Z'; }
inline int tile_letter(char t) { return is_blank_tile(t) ? t - 'A' : t - 'a'; }
inline int tile_score(char t) { return is_blank_tile(t) ? 0 : letter_value(t - 'a'); }

struct Board {
    std::array<char, kSize * kSize> cells{};
    char at(int r, int c) const { return cells[r * kSize + c]; }
    char &at(int r, int c) { return cells[r * kSize + c]; }
    bool empty(int r, int c) const { return at(r, c) == 0; }
    bool in(int r, int c) const { return r >= 0 && r < kSize && c >= 0 && c < kSize; }
    bool is_empty_board() const;
    int tile_count() const;
};

// Tile counts by letter (index 26 = blank). Text form: letters, '?' for a blank.
struct Rack {
    std::array<uint8_t, 27> n{};
    int size = 0;
    static Rack from_string(const std::string &s);
    std::string str() const;
    void add(int t) { ++n[t]; ++size; }
    void remove(int t) { --n[t]; --size; }
    bool has(int t) const { return n[t] > 0; }
    int value() const;  // sum of letter values left (for end-of-game adjustment)
};

struct Placement {
    int8_t r, c;
    char tile;  // board form: 'a'..'z' or 'A'..'Z' for a blank
};

struct Move {
    enum Kind : uint8_t { Place, Exchange, Pass } kind = Pass;
    std::vector<Placement> tiles;  // Place: tiles laid down, left-to-right / top-to-bottom
    bool horizontal = true;
    int score = 0;
    std::string exchange;  // Exchange: tiles returned to the bag
    std::string word;      // Place: the main word, uppercase letters = blanks (filled in by describe())
    int row = 0, col = 0;  // Place: start of the main word

    std::string str() const;  // "H8 across WORD 24" style, or "exchange abc", "pass"
};

void apply(Board &b, const Move &m);
// Fills word / row / col from the board the move is about to be played on.
void describe(const Board &b, Move &m);
// Every word the move forms (main word first), for display.
std::vector<std::string> words_formed(const Board &b, const Move &m);

}  // namespace we
