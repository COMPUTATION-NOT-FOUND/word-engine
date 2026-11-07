#include "wordengine/ai.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

namespace we {

namespace {

using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

// How much each tile is worth to keep for next turn, in points. Hand-tuned:
// blanks and S make many words; Q, V, U and W are hard to place.
constexpr double kKeep[27] = {
    0.5, -2.0, 0.0, 0.0, 3.0, -2.0, -2.5, 0.5, -0.5, -3.0, -2.0, -0.5, 0.0,  // a-m
    0.5, -1.5, -1.0, -7.0, 1.5, 7.0, 0.5, -3.0, -5.0, -3.5, 2.0, -0.5, 1.5,  // n-z
    22.0};                                                                    // blank
constexpr bool kVowel[26] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};

// Best placement under an equity measure, without storing every move.
struct Best {
    bool found = false;
    int score = 0;
    double equity = -1e9;
    Rack leave;
    Placement tiles[kRackSize];
    int n = 0;
    bool horizontal = true;
};

Best best_by_equity(const MoveGen &gen, const Board &b, const Rack &rack, int bag) {
    Best best;
    gen.generate(b, rack, [&](const Placement *t, int n, bool h, int score, const Rack &leave) {
        double e = score + leave_value(leave, bag);
        if (e > best.equity) {
            best.found = true;
            best.equity = e;
            best.score = score;
            best.leave = leave;
            best.n = n;
            best.horizontal = h;
            std::copy(t, t + n, best.tiles);
        }
    });
    return best;
}

Move to_move(const Placement *t, int n, bool h, int score) {
    Move m;
    m.kind = Move::Place;
    m.tiles.assign(t, t + n);
    m.horizontal = h;
    m.score = score;
    return m;
}

void take_tiles(Rack &r, const Move &m) {
    if (m.kind == Move::Place)
        for (const Placement &p : m.tiles) r.remove(is_blank_tile(p.tile) ? kBlank : p.tile - 'a');
    else if (m.kind == Move::Exchange)
        for (char ch : m.exchange) r.remove(ch == '?' ? kBlank : ch - 'a');
}

}  // namespace

double leave_value(const Rack &leave, int bag) {
    if (bag == 0) return -1.5 * leave.value();  // no more draws: tiles left only cost points
    if (leave.size == 0) return 0;
    double v = 0;
    int vowels = 0, consonants = 0;
    for (int t = 0; t < 27; ++t) {
        int n = leave.n[t];
        if (!n) continue;
        v += kKeep[t] * n;
        if (t != kBlank && n > 1) v -= (kVowel[t] ? 4.0 : 3.0) * (n - 1) * n / 2;  // duplicates
        if (t == kBlank) continue;
        (kVowel[t] ? vowels : consonants) += n;
    }
    double ideal = 0.42 * (vowels + consonants);
    v -= 1.8 * (vowels - ideal) * (vowels - ideal);
    if (leave.n['q' - 'a'] && !leave.n['u' - 'a']) v -= 5;
    return bag >= kRackSize ? v : v * (0.5 + bag / 14.0);
}

Level parse_level(const std::string &s) {
    if (s == "easy") return Level::Easy;
    if (s == "medium") return Level::Medium;
    if (s == "expert") return Level::Expert;
    return Level::Hard;
}
const char *level_name(Level l) {
    switch (l) {
    case Level::Easy: return "easy";
    case Level::Medium: return "medium";
    case Level::Hard: return "hard";
    default: return "expert";
    }
}

Move Ai::choose(const Game &g, const AiOptions &opt, AiReport *report) const {
    auto t0 = Clock::now();
    AiReport rep;
    int p = g.to_move();
    const Rack &rack = g.rack(p);
    int bag = g.bag_size();

    std::vector<Candidate> cands;
    gen_.generate(g.board(), rack, [&](const Placement *t, int n, bool h, int score, const Rack &leave) {
        cands.push_back({to_move(t, n, h, score), score + leave_value(leave, bag), 0, 0});
    });
    rep.moves_considered = (int)cands.size();

    std::mt19937_64 rng(opt.seed);
    Move chosen;  // pass by default

    if (opt.level == Level::Easy || opt.level == Level::Medium) {
        rep.method = opt.level == Level::Easy ? "random" : "greedy";
        std::sort(cands.begin(), cands.end(), [](auto &a, auto &b) { return a.move.score > b.move.score; });
        if (!cands.empty()) {
            size_t i = 0;
            if (opt.level == Level::Easy) {
                size_t lo = cands.size() / 3, hi = std::max(lo, cands.size() * 2 / 3);
                i = std::uniform_int_distribution<size_t>(lo, hi)(rng);
                i = std::min(i, cands.size() - 1);
            }
            chosen = cands[i].move;
        } else if (bag >= kRackSize) {
            chosen.kind = Move::Exchange;
            chosen.exchange = rack.str();
        }
    } else {
        // Exchanges: try keeping every subset of the rack.
        if (bag >= kRackSize) {
            std::string tiles = rack.str();
            int n = (int)tiles.size();
            std::unordered_map<std::string, bool> seen;
            for (int mask = 1; mask < (1 << n); ++mask) {
                std::string give, keep;
                for (int i = 0; i < n; ++i) (mask >> i & 1 ? give : keep) += tiles[i];
                if (!seen.emplace(give, true).second) continue;
                Move m;
                m.kind = Move::Exchange;
                m.exchange = give;
                cands.push_back({m, leave_value(Rack::from_string(keep), bag), 0, 0});
            }
        }
        std::sort(cands.begin(), cands.end(), [](auto &a, auto &b) { return a.equity > b.equity; });
        if (cands.empty()) {
            rep.method = "equity";
        } else if (opt.level == Level::Hard) {
            rep.method = "equity";
            chosen = cands[0].move;
        } else if (bag == 0) {
            chosen = endgame(g, opt, rep);
        } else {
            // Simulate the top placements plus the best exchange.
            std::vector<Candidate> top;
            bool have_exchange = false;
            for (auto &c : cands) {
                bool ex = c.move.kind == Move::Exchange;
                if (ex && have_exchange) continue;
                if ((int)top.size() < opt.candidates || (ex && !have_exchange && c.equity > cands[0].equity - 25)) {
                    top.push_back(c);
                    have_exchange |= ex;
                }
                if ((int)top.size() >= opt.candidates + 1) break;
            }
            chosen = expert_sim(g, opt, std::move(top), rep);
        }
        if (rep.top.empty())
            for (size_t i = 0; i < cands.size() && i < 5; ++i) rep.top.push_back(cands[i]);
    }

    if (chosen.kind == Move::Place) describe(g.board(), chosen);
    for (auto &c : rep.top) describe(g.board(), c.move);
    rep.ms = ms_since(t0);
    if (report) *report = std::move(rep);
    return chosen;
}

// ---- Monte-Carlo simulation -------------------------------------------------

Move Ai::expert_sim(const Game &g, const AiOptions &opt, std::vector<Candidate> cands, AiReport &rep) const {
    auto t0 = Clock::now();
    rep.method = "simulation";
    int p = g.to_move();
    std::vector<int> pool;
    auto unseen = g.unseen(p);
    for (int t = 0; t < 27; ++t) pool.insert(pool.end(), std::max(0, unseen[t]), t);
    int opp_size = g.rack(1 - p).size;

    struct Stat { double sum = 0, sq = 0; int n = 0; bool alive = true; };
    std::vector<Stat> st(cands.size());

    // One playout: our candidate, the opponent's best reply, our best reply.
    // Playout i deals the same random tiles for every candidate (common random numbers),
    // so differences between candidates come from the moves, not the luck of the draw.
    auto playout = [&](const Candidate &c, int i) {
        std::mt19937_64 r(opt.seed * 0x9E3779B97F4A7C15ull + (uint64_t)i);
        std::vector<int> bag = pool;
        std::shuffle(bag.begin(), bag.end(), r);
        Rack opp, mine = g.rack(p);
        size_t next = 0;
        for (int k = 0; k < opp_size && next < bag.size(); ++k) opp.add(bag[next++]);
        Board b = g.board();
        apply(b, c.move);
        take_tiles(mine, c.move);
        while (mine.size < kRackSize && next < bag.size()) mine.add(bag[next++]);
        if (c.move.kind == Move::Exchange)
            for (char ch : c.move.exchange) bag.push_back(ch == '?' ? kBlank : ch - 'a');
        int left = (int)(bag.size() - next);

        double v = c.move.score;
        Best o = best_by_equity(gen_, b, opp, left);
        if (o.found) {
            Move om = to_move(o.tiles, o.n, o.horizontal, o.score);
            apply(b, om);
            opp = o.leave;
            v -= o.score;
            while (opp.size < kRackSize && next < bag.size()) opp.add(bag[next++]);
            left = (int)(bag.size() - next);
        }
        Best me = best_by_equity(gen_, b, mine, left);
        if (me.found) v += me.score + leave_value(me.leave, left);
        else v += leave_value(mine, left);
        v -= leave_value(opp, left) * 0.5;
        return v;
    };

    const int batch = 4;
    int round = 0;
    int alive = (int)cands.size();
    while (true) {
        for (size_t k = 0; k < cands.size(); ++k) {
            if (!st[k].alive) continue;
            for (int j = 0; j < batch; ++j) {
                double v = playout(cands[k], st[k].n);
                st[k].sum += v;
                st[k].sq += v * v;
                ++st[k].n;
                ++rep.iterations;
            }
        }
        ++round;
        if (ms_since(t0) > opt.time_ms || alive <= 1 || st[0].n >= 400) break;
        // Successive halving: every 4 rounds, drop the weakest quarter.
        if (round % 4 == 0 && alive > 2) {
            std::vector<int> idx;
            for (size_t k = 0; k < cands.size(); ++k)
                if (st[k].alive) idx.push_back((int)k);
            std::sort(idx.begin(), idx.end(), [&](int a, int b) { return st[a].sum / st[a].n > st[b].sum / st[b].n; });
            int drop = std::max(1, alive / 4);
            for (int i = alive - drop; i < alive; ++i) st[idx[i]].alive = false;
            alive -= drop;
        }
    }

    for (size_t k = 0; k < cands.size(); ++k) {
        cands[k].sim = st[k].n ? st[k].sum / st[k].n : -1e9;
        cands[k].iterations = st[k].n;
        if (!st[k].alive) cands[k].sim -= 1000;  // pruned: rank below survivors
    }
    std::sort(cands.begin(), cands.end(), [](auto &a, auto &b) { return a.sim > b.sim; });
    for (auto &c : cands)
        if (c.sim < -500) c.sim += 1000;
    for (size_t i = 0; i < cands.size() && i < 5; ++i) rep.top.push_back(cands[i]);
    return cands.empty() ? Move{} : cands[0].move;
}

// ---- Endgame: bag empty, both racks known ------------------------------------

namespace {

struct EndgameSearch {
    const MoveGen &gen;
    Clock::time_point start;
    double limit_ms;
    bool aborted = false;
    long nodes = 0;

    struct Entry { int depth; int value; int flag; };  // flag: 0 exact, 1 lower, 2 upper
    std::unordered_map<uint64_t, Entry> tt;

    static uint64_t hash(const Board &b, const Rack r[2], int side, int passes) {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](uint64_t x) { h = (h ^ x) * 1099511628211ull; };
        for (char t : b.cells) mix((uint8_t)t);
        for (int s = 0; s < 2; ++s)
            for (int t = 0; t < 27; ++t) mix(r[s].n[t]);
        mix(side * 2 + passes);
        return h;
    }

    // Moves worth searching: the highest scoring ones, best first, plus passing.
    std::vector<Move> moves(const Board &b, const Rack &r, int width) {
        std::vector<Move> ms;
        gen.generate(b, r, [&](const Placement *t, int n, bool h, int score, const Rack &) {
            ms.push_back(to_move(t, n, h, score));
        });
        // Going out first, then by score.
        auto key = [&](const Move &m) { return m.score + ((int)m.tiles.size() == r.size ? 1000 : 0); };
        std::sort(ms.begin(), ms.end(), [&](auto &a, auto &b) { return key(a) > key(b); });
        if ((int)ms.size() > width) ms.resize(width);
        return ms;
    }

    // Points the side to move will gain over the other from here (negamax).
    int search(Board &b, Rack r[2], int side, int passes, int depth, int alpha, int beta) {
        ++nodes;
        if ((nodes & 63) == 0 && ms_since(start) > limit_ms) aborted = true;
        if (aborted) return 0;
        Rack &me = r[side], &op = r[1 - side];
        if (depth == 0) return op.value() - me.value();  // rough guess: who is stuck with more points

        uint64_t key = hash(b, r, side, passes);
        auto it = tt.find(key);
        if (it != tt.end() && it->second.depth >= depth) {
            const Entry &e = it->second;
            if (e.flag == 0) return e.value;
            if (e.flag == 1) alpha = std::max(alpha, e.value);
            if (e.flag == 2) beta = std::min(beta, e.value);
            if (alpha >= beta) return e.value;
        }
        int a0 = alpha;
        int best = -1000000;
        int width = depth >= 4 ? 8 : 12;
        for (const Move &m : moves(b, me, width)) {
            int v;
            if ((int)m.tiles.size() == me.size) {
                v = m.score + 2 * op.value();  // goes out: the game ends
            } else {
                Board nb = b;
                apply(nb, m);
                Rack saved = me;
                take_tiles(me, m);
                v = m.score - search(nb, r, 1 - side, 0, depth - 1, -beta, -alpha);
                me = saved;
            }
            if (v > best) best = v;
            if (v > alpha) alpha = v;
            if (alpha >= beta) break;
        }
        // Pass. Two passes in a row end the game, each side losing what it holds.
        int pv = passes ? op.value() - me.value() : -search(b, r, 1 - side, 1, depth - 1, -beta, -alpha);
        if (pv > best) best = pv;
        if (aborted) return 0;
        int flag = best <= a0 ? 2 : best >= beta ? 1 : 0;
        tt[key] = {depth, best, flag};
        return best;
    }
};

}  // namespace

Move Ai::endgame(const Game &g, const AiOptions &opt, AiReport &rep) const {
    rep.method = "endgame";
    int p = g.to_move();
    Rack r[2];
    r[p] = g.rack(p);
    auto unseen = g.unseen(p);  // bag is empty, so this is exactly the opponent's rack
    for (int t = 0; t < 27; ++t)
        for (int k = 0; k < unseen[t]; ++k) r[1 - p].add(t);

    EndgameSearch s{gen_, Clock::now(), (double)opt.time_ms, false, 0, {}};
    Board b = g.board();
    std::vector<Move> root = s.moves(b, r[p], 30);
    Move pass;
    root.push_back(pass);
    Move best = root.empty() ? pass : root[0];
    std::vector<int> scores(root.size(), 0);

    for (int depth = 1; depth <= 12; ++depth) {
        int alpha = -1000000;
        std::vector<int> vals(root.size(), -1000000);
        for (size_t i = 0; i < root.size(); ++i) {
            const Move &m = root[i];
            int v;
            if (m.kind == Move::Pass) {
                v = -s.search(b, r, 1 - p, 1, depth - 1, -1000000, -alpha);
            } else if ((int)m.tiles.size() == r[p].size) {
                v = m.score + 2 * r[1 - p].value();
            } else {
                Board nb = b;
                apply(nb, m);
                Rack saved = r[p];
                take_tiles(r[p], m);
                v = m.score - s.search(nb, r, 1 - p, 0, depth - 1, -1000000, -alpha);
                r[p] = saved;
            }
            if (s.aborted) break;
            vals[i] = v;
            alpha = std::max(alpha, v);
        }
        if (s.aborted) break;
        rep.depth = depth;
        // Re-order the root by this depth's result so the next depth searches the best line first.
        std::vector<size_t> order(root.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t c) { return vals[a] > vals[c]; });
        std::vector<Move> sorted;
        for (size_t i : order) sorted.push_back(root[i]), scores[sorted.size() - 1] = vals[i];
        root = std::move(sorted);
        best = root[0];
        if (std::abs(scores[0]) >= 500000) break;
    }
    rep.iterations = (int)s.nodes;
    for (size_t i = 0; i < root.size() && i < 5; ++i) {
        Candidate c{root[i], (double)root[i].score, (double)scores[i], rep.depth};
        rep.top.push_back(c);
    }
    return best;
}

}  // namespace we
