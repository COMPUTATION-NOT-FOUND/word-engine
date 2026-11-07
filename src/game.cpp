#include "wordengine/game.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "wordengine/movegen.hpp"

namespace we {

namespace {
constexpr int kMaxScoreless = 6;  // game ends after six scoreless turns in a row
}

Game::Game(const Dawg &d, uint64_t seed) : d_(d), rng_(seed) {
    for (int t = 0; t < 27; ++t) bag_.insert(bag_.end(), kTileCounts[t], t);
    std::shuffle(bag_.begin(), bag_.end(), rng_);
    draw(0);
    draw(1);
}

void Game::draw(int p) {
    while (racks_[p].size < kRackSize && !bag_.empty()) {
        racks_[p].add(bag_.back());
        bag_.pop_back();
    }
}

std::array<int, 27> Game::unseen(int p) const {
    std::array<int, 27> u = kTileCounts;
    for (char t : board_.cells)
        if (t) --u[is_blank_tile(t) ? kBlank : t - 'a'];
    for (int t = 0; t < 27; ++t) u[t] -= racks_[p].n[t];
    return u;
}

std::string Game::play(Move m) {
    if (over_) return "the game is over";
    int p = to_move_;
    Rack &rack = racks_[p];
    std::string before = rack.str();

    if (m.kind == Move::Place) {
        Rack need;
        for (const Placement &pl : m.tiles) need.add(is_blank_tile(pl.tile) ? kBlank : pl.tile - 'a');
        for (int t = 0; t < 27; ++t)
            if (need.n[t] > rack.n[t]) return "those tiles are not on the rack";
        std::string err = validate(d_, board_, m);
        if (!err.empty()) return err;
        apply(board_, m);
        for (int t = 0; t < 27; ++t) rack.n[t] -= need.n[t];
        rack.size -= need.size;
        draw(p);
    } else if (m.kind == Move::Exchange) {
        if ((int)bag_.size() < kRackSize) return "exchanges need at least 7 tiles in the bag";
        Rack give = Rack::from_string(m.exchange);
        if (give.size == 0) return "choose tiles to exchange";
        for (int t = 0; t < 27; ++t)
            if (give.n[t] > rack.n[t]) return "those tiles are not on the rack";
        for (int t = 0; t < 27; ++t) rack.n[t] -= give.n[t];
        rack.size -= give.size;
        draw(p);
        for (int t = 0; t < 27; ++t) bag_.insert(bag_.end(), give.n[t], t);
        std::shuffle(bag_.begin(), bag_.end(), rng_);
        m.score = 0;
    } else {
        m.score = 0;
    }

    scores_[p] += m.score;
    scoreless_ = m.score == 0 ? scoreless_ + 1 : 0;
    history_.push_back({p, m, before, scores_[p]});

    int q = 1 - p;
    if (m.kind == Move::Place && rack.size == 0 && bag_.empty()) {
        // Going out: collect the value of the tiles the opponent is still holding.
        int left = racks_[q].value();
        scores_[p] += left;
        scores_[q] -= left;
        over_ = true;
        end_reason_ = "player " + std::to_string(p + 1) + " used all their tiles";
    } else if (scoreless_ >= kMaxScoreless) {
        scores_[0] -= racks_[0].value();
        scores_[1] -= racks_[1].value();
        over_ = true;
        end_reason_ = "six scoreless turns in a row";
    }
    to_move_ = q;
    return "";
}

std::string parse_move(const Board &b, const std::string &text, Move &m) {
    std::istringstream in(text);
    std::string pos, letters;
    in >> pos;
    std::string lower = pos;
    for (char &ch : lower) ch = (char)std::tolower((unsigned char)ch);
    m = Move{};
    if (lower == "pass") {
        m.kind = Move::Pass;
        return "";
    }
    if (lower == "exchange" || lower == "swap") {
        in >> letters;
        m.kind = Move::Exchange;
        m.exchange = letters;
        return letters.empty() ? "say which tiles to exchange, e.g. 'exchange qvu' ('?' = blank)" : "";
    }
    in >> letters;
    if (pos.size() < 2 || letters.empty()) return "write a move like '8H WORD' (across) or 'H8 WORD' (down)";
    bool across = std::isdigit((unsigned char)pos[0]);
    int row, col;
    if (across) {
        size_t i = 0;
        while (i < pos.size() && std::isdigit((unsigned char)pos[i])) ++i;
        if (i == pos.size()) return "missing column letter";
        row = std::stoi(pos.substr(0, i)) - 1;
        col = std::toupper((unsigned char)pos[i]) - 'A';
    } else {
        col = std::toupper((unsigned char)pos[0]) - 'A';
        if (!std::isdigit((unsigned char)pos[1])) return "missing row number";
        row = std::stoi(pos.substr(1)) - 1;
    }
    if (!b.in(row, col)) return "that square is off the board";
    m.kind = Move::Place;
    m.horizontal = across;
    int r = row, c = col;
    for (char ch : letters) {
        if (!b.in(r, c)) return "the word runs off the board";
        if (!b.empty(r, c)) {
            if (ch != '.' && std::tolower((unsigned char)ch) != 'a' + tile_letter(b.at(r, c)))
                return std::string("square ") + std::to_string(r + 1) + char('A' + c) + " already holds " +
                       char('A' + tile_letter(b.at(r, c)));
        } else {
            if (!std::isalpha((unsigned char)ch)) return "use letters; '.' only over tiles already placed";
            // Uppercase = normal tile, lowercase = blank standing for that letter.
            char tile = std::isupper((unsigned char)ch) ? char(ch - 'A' + 'a') : char(ch - 'a' + 'A');
            m.tiles.push_back({(int8_t)r, (int8_t)c, tile});
        }
        if (across) ++c; else ++r;
    }
    if (m.tiles.empty()) return "that places no new tiles";
    return "";
}

}  // namespace we
