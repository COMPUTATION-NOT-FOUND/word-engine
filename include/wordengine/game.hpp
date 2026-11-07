#pragma once
// Two-player Tessera game: bag, racks, turns, scoring and the end of the game.
#include <random>
#include <string>
#include <vector>

#include "wordengine/board.hpp"
#include "wordengine/dawg.hpp"

namespace we {

struct Turn {
    int player;
    Move move;              // word / score filled in
    std::string rack_before;
    int total_after;        // player's score after this turn
};

class Game {
public:
    Game(const Dawg &d, uint64_t seed);

    // Plays a move for the player to move. Place moves are checked first.
    // Returns "" on success, otherwise why the move was refused (nothing changes).
    std::string play(Move m);

    const Board &board() const { return board_; }
    const Rack &rack(int p) const { return racks_[p]; }
    int score(int p) const { return scores_[p]; }
    int to_move() const { return to_move_; }
    int bag_size() const { return (int)bag_.size(); }
    bool over() const { return over_; }
    int scoreless_turns() const { return scoreless_; }
    const std::vector<Turn> &history() const { return history_; }
    std::string end_reason() const { return end_reason_; }

    // Tiles player p cannot see: the bag plus the other rack (index 26 = blank).
    std::array<int, 27> unseen(int p) const;

private:
    void draw(int p);

    const Dawg &d_;
    std::mt19937_64 rng_;
    Board board_;
    std::vector<int> bag_;
    Rack racks_[2];
    int scores_[2] = {0, 0};
    int to_move_ = 0;
    int scoreless_ = 0;
    bool over_ = false;
    std::string end_reason_;
    std::vector<Turn> history_;
};

// Turns "8H across WORD" style text into a move: row 1-15, column A-O, then the letters;
// '.' or a letter already on the board skips that square, lowercase letters are blanks.
// Down moves put the column letter first ("H8 WORD"). Returns "" or an error.
std::string parse_move(const Board &b, const std::string &text, Move &m);

}  // namespace we
