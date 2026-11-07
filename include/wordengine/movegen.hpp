#pragma once
// Finds every legal placement for a rack (Appel & Jacobson, 1988): for each row, work out
// which letters may go in each empty square without breaking the words above and below
// (cross-checks), then grow words through "anchor" squares next to existing tiles by
// walking the DAWG. Scores are added up during the walk, so no move is scored twice.
#include <functional>
#include <vector>

#include "wordengine/board.hpp"
#include "wordengine/dawg.hpp"

namespace we {

// Called once per legal placement. `tiles` is valid only during the call.
// `leave` is what stays on the rack after the move (before drawing).
using MoveSink = std::function<void(const Placement *tiles, int n, bool horizontal, int score, const Rack &leave)>;

class MoveGen {
public:
    explicit MoveGen(const Dawg &d) : d_(d) {}

    void generate(const Board &b, const Rack &rack, const MoveSink &sink) const;
    std::vector<Move> all_moves(const Board &b, const Rack &rack) const;  // placements only, word filled in

private:
    const Dawg &d_;
};

// Checks a move typed in by a player. Returns "" if legal (and fills m.score, m.horizontal,
// m.word, m.row, m.col), otherwise a reason it is not.
std::string validate(const Dawg &d, const Board &b, Move &m);

}  // namespace we
