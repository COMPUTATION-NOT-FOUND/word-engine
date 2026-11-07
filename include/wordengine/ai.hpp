#pragma once
// Tessera AI.
//
//   Easy    picks a modest move at random.
//   Medium  plays the highest score.
//   Hard    plays the highest "equity": score + value of the tiles kept (the leave).
//           Also considers exchanging.
//   Expert  looks ahead. While tiles remain in the bag it cannot see the opponent's rack,
//           so it simulates: for the best candidates, deal the opponent many random racks
//           from the unseen tiles, let both sides reply, and keep the move with the best
//           average outcome (Monte-Carlo simulation, as in Maven and Quackle). Weak
//           candidates are dropped early (successive halving) so time goes to close calls.
//           Once the bag is empty both racks are known, and it switches to an exact
//           alpha-beta search of the endgame with a hash table and iterative deepening.
#include <cstdint>
#include <string>
#include <vector>

#include "wordengine/game.hpp"
#include "wordengine/movegen.hpp"

namespace we {

enum class Level { Easy, Medium, Hard, Expert };
Level parse_level(const std::string &s);  // "easy" ... "expert"
const char *level_name(Level l);

struct Candidate {
    Move move;
    double equity = 0;    // static: score + leave
    double sim = 0;       // mean simulated outcome (expert only)
    int iterations = 0;
};

struct AiReport {
    std::string method;   // "random", "greedy", "equity", "simulation", "endgame"
    int moves_considered = 0;
    int iterations = 0;   // simulation playouts, or endgame nodes
    int depth = 0;        // endgame search depth reached
    double ms = 0;
    std::vector<Candidate> top;  // best few, best first
};

struct AiOptions {
    Level level = Level::Hard;
    int time_ms = 1500;   // expert thinking time
    int candidates = 12;  // expert: moves to simulate
    uint64_t seed = 1;
};

double leave_value(const Rack &leave, int bag_size);

class Ai {
public:
    explicit Ai(const Dawg &d) : d_(d), gen_(d) {}
    Move choose(const Game &g, const AiOptions &opt, AiReport *report = nullptr) const;

private:
    Move expert_sim(const Game &g, const AiOptions &opt, std::vector<Candidate> cands, AiReport &rep) const;
    Move endgame(const Game &g, const AiOptions &opt, AiReport &rep) const;

    const Dawg &d_;
    MoveGen gen_;
};

}  // namespace we
