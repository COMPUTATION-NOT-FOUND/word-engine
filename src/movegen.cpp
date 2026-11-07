#include "wordengine/movegen.hpp"

#include <algorithm>
#include <set>
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace we {

namespace {

constexpr uint32_t kAllLetters = (1u << 26) - 1;

int lowest_bit(uint32_t x) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long i;
    _BitScanForward(&i, x);
    return (int)i;
#else
    return __builtin_ctz(x);
#endif
}

// One pass over the board in one direction. With `tr` set the board is read transposed,
// so "rows" are columns and every move found is a down move. The premium layout is
// symmetric, so premiums need no transposing.
struct Gen {
    const Dawg &d;
    const Board &b;
    bool tr;
    const MoveSink &sink;
    Rack rack;

    uint32_t cross[kSize][kSize];
    int cross_score[kSize][kSize];
    bool has_cross[kSize][kSize];
    bool anchor[kSize][kSize];

    int row = 0, anchor_col = 0;
    Placement placed[kRackSize];
    int nplaced = 0;
    char left[kRackSize];
    int left_len = 0;

    Gen(const Dawg &d_, const Board &b_, bool tr_, const MoveSink &s, const Rack &r)
        : d(d_), b(b_), tr(tr_), sink(s), rack(r) {}

    char cell(int r, int c) const { return tr ? b.at(c, r) : b.at(r, c); }
    bool occupied(int r, int c) const { return r >= 0 && r < kSize && c >= 0 && c < kSize && cell(r, c); }

    void prepare(bool empty_board) {
        for (int r = 0; r < kSize; ++r)
            for (int c = 0; c < kSize; ++c) {
                anchor[r][c] = false;
                cross[r][c] = kAllLetters;
                cross_score[r][c] = 0;
                has_cross[r][c] = false;
                if (cell(r, c)) continue;
                anchor[r][c] = occupied(r - 1, c) || occupied(r + 1, c) || occupied(r, c - 1) || occupied(r, c + 1);
                if (!occupied(r - 1, c) && !occupied(r + 1, c)) continue;
                // Letters that make a word with the tiles directly above and below.
                has_cross[r][c] = true;
                int top = r;
                while (occupied(top - 1, c)) --top;
                uint32_t node = d.root();
                int sc = 0;
                for (int i = top; i < r && node != Dawg::kNone; ++i) {
                    node = d.child(node, tile_letter(cell(i, c)));
                    sc += tile_score(cell(i, c));
                }
                for (int i = r + 1; occupied(i, c); ++i) sc += tile_score(cell(i, c));
                cross_score[r][c] = sc;
                uint32_t ok = 0;
                if (node != Dawg::kNone) {
                    for (uint32_t m = d.letters(node); m; m &= m - 1) {
                        int L = lowest_bit(m);
                        uint32_t n = d.child(node, L);
                        for (int i = r + 1; occupied(i, c) && n != Dawg::kNone; ++i) n = d.child(n, tile_letter(cell(i, c)));
                        if (n != Dawg::kNone && d.is_word(n)) ok |= 1u << L;
                    }
                }
                cross[r][c] = ok;
            }
        if (empty_board) anchor[kCenter][kCenter] = true;
    }

    void run(bool empty_board) {
        prepare(empty_board);
        for (row = 0; row < kSize; ++row)
            for (anchor_col = 0; anchor_col < kSize; ++anchor_col) {
                if (!anchor[row][anchor_col] || !cross[row][anchor_col]) continue;
                if (occupied(row, anchor_col - 1)) {
                    // Left part is the tiles already on the board.
                    int c = anchor_col - 1;
                    while (occupied(row, c - 1)) --c;
                    uint32_t node = d.root();
                    int sum = 0;
                    for (; c < anchor_col && node != Dawg::kNone; ++c) {
                        node = d.child(node, tile_letter(cell(row, c)));
                        sum += tile_score(cell(row, c));
                    }
                    if (node == Dawg::kNone) continue;
                    left_len = nplaced = 0;
                    extend(node, anchor_col, sum, 1, 0);
                } else {
                    // Left part from the rack, only over empty squares that are not anchors
                    // (so each move is found from exactly one anchor).
                    int k = 0;
                    for (int c = anchor_col - 1; c >= 0 && !cell(row, c) && !anchor[row][c]; --c) ++k;
                    left_len = 0;
                    left_part(d.root(), std::min(k, rack.size - 1));
                }
            }
    }

    void left_part(uint32_t node, int limit) {
        // Lay the left part down ending just before the anchor and score it.
        nplaced = 0;
        int sum = 0, mul = 1;
        for (int i = 0; i < left_len; ++i) {
            int c = anchor_col - left_len + i;
            placed[nplaced++] = {(int8_t)row, (int8_t)c, left[i]};
            sum += tile_score(left[i]) * letter_mult(row, c);
            mul *= word_mult(row, c);
        }
        extend(node, anchor_col, sum, mul, 0);
        if (limit <= 0) return;
        for (uint32_t m = d.letters(node); m; m &= m - 1) {
            int L = lowest_bit(m);
            uint32_t child = d.child(node, L);
            for (int blank = 0; blank < 2; ++blank) {
                int t = blank ? kBlank : L;
                if (!rack.has(t)) continue;
                rack.remove(t);
                left[left_len++] = blank ? char('A' + L) : char('a' + L);
                left_part(child, limit - 1);
                --left_len;
                rack.add(t);
            }
        }
    }

    void extend(uint32_t node, int col, int sum, int mul, int cross_total) {
        if (col < kSize && cell(row, col)) {
            char t = cell(row, col);
            uint32_t child = d.child(node, tile_letter(t));
            if (child != Dawg::kNone) extend(child, col + 1, sum + tile_score(t), mul, cross_total);
            return;
        }
        if (col > anchor_col && d.is_word(node)) record(sum, mul, cross_total);
        if (col >= kSize) return;
        uint32_t allowed = d.letters(node) & cross[row][col];
        if (!allowed) return;
        int lm = letter_mult(row, col), wm = word_mult(row, col);
        for (uint32_t m = allowed; m; m &= m - 1) {
            int L = lowest_bit(m);
            uint32_t child = d.child(node, L);
            for (int blank = 0; blank < 2; ++blank) {
                int t = blank ? kBlank : L;
                if (!rack.has(t)) continue;
                int v = blank ? 0 : letter_value(L);
                rack.remove(t);
                placed[nplaced++] = {(int8_t)row, (int8_t)col, blank ? char('A' + L) : char('a' + L)};
                int ct = cross_total + (has_cross[row][col] ? (cross_score[row][col] + v * lm) * wm : 0);
                extend(child, col + 1, sum + v * lm, mul * wm, ct);
                --nplaced;
                rack.add(t);
            }
        }
    }

    void record(int sum, int mul, int cross_total) {
        // A one-tile move that also makes an across word was already found by the across pass.
        if (tr && nplaced == 1 && has_cross[placed[0].r][placed[0].c]) return;
        int score = sum * mul + cross_total + (nplaced == kRackSize ? kFullRackBonus : 0);
        Placement out[kRackSize];
        for (int i = 0; i < nplaced; ++i)
            out[i] = tr ? Placement{placed[i].c, placed[i].r, placed[i].tile} : placed[i];
        sink(out, nplaced, !tr, score, rack);
    }
};

}  // namespace

void MoveGen::generate(const Board &b, const Rack &rack, const MoveSink &sink) const {
    if (rack.size == 0) return;
    bool empty = b.is_empty_board();
    Gen across(d_, b, false, sink, rack);
    across.run(empty);
    if (empty) return;  // the layout is symmetric: first-move down plays mirror the across ones
    Gen down(d_, b, true, sink, rack);
    down.run(false);
}

std::vector<Move> MoveGen::all_moves(const Board &b, const Rack &rack) const {
    std::vector<Move> out;
    generate(b, rack, [&](const Placement *t, int n, bool h, int score, const Rack &) {
        Move m;
        m.kind = Move::Place;
        m.tiles.assign(t, t + n);
        m.horizontal = h;
        m.score = score;
        out.push_back(std::move(m));
    });
    for (Move &m : out) describe(b, m);
    return out;
}

std::string validate(const Dawg &d, const Board &b, Move &m) {
    if (m.kind != Move::Place) return "";
    if (m.tiles.empty()) return "place at least one tile";
    if ((int)m.tiles.size() > kRackSize) return "too many tiles";
    std::set<std::pair<int, int>> seen;
    for (const Placement &p : m.tiles) {
        if (!b.in(p.r, p.c)) return "tile off the board";
        if (!b.empty(p.r, p.c)) return "square already taken";
        if (!seen.insert({p.r, p.c}).second) return "two tiles on one square";
        if (!((p.tile >= 'a' && p.tile <= 'z') || is_blank_tile(p.tile))) return "bad tile";
    }
    bool same_row = std::all_of(m.tiles.begin(), m.tiles.end(), [&](auto &p) { return p.r == m.tiles[0].r; });
    bool same_col = std::all_of(m.tiles.begin(), m.tiles.end(), [&](auto &p) { return p.c == m.tiles[0].c; });
    if (!same_row && !same_col) return "tiles must be in one row or one column";

    Board after = b;
    apply(after, m);
    auto filled = [&](int r, int c) { return after.in(r, c) && !after.empty(r, c); };
    if (m.tiles.size() > 1) m.horizontal = same_row;
    else {
        int r = m.tiles[0].r, c = m.tiles[0].c;
        m.horizontal = filled(r, c - 1) || filled(r, c + 1) || !(filled(r - 1, c) || filled(r + 1, c));
    }
    std::sort(m.tiles.begin(), m.tiles.end(),
              [&](auto &x, auto &y) { return m.horizontal ? x.c < y.c : x.r < y.r; });
    int dr = m.horizontal ? 0 : 1, dc = m.horizontal ? 1 : 0;

    const Placement &first = m.tiles.front(), &last = m.tiles.back();
    for (int r = first.r, c = first.c; r != last.r || c != last.c; r += dr, c += dc)
        if (!filled(r, c)) return "tiles must form one unbroken line";

    if (b.is_empty_board()) {
        if (!seen.count({kCenter, kCenter})) return "the first word must cover the centre square";
    } else {
        bool touches = false;
        for (const Placement &p : m.tiles)
            for (auto [ar, ac] : {std::pair{p.r - 1, p.c}, {p.r + 1, p.c}, {p.r, p.c - 1}, {p.r, p.c + 1}})
                if (b.in(ar, ac) && !b.empty(ar, ac)) touches = true;
        if (!touches) return "the word must join tiles already on the board";
    }

    // Scores the word through (r, c) in direction (wr, wc); returns -1 if it is not a word.
    std::string bad;
    auto score_word = [&](int r, int c, int wr, int wc, bool &is_word) {
        while (filled(r - wr, c - wc)) r -= wr, c -= wc;
        int sum = 0, mul = 1, len = 0;
        std::string w;
        for (; filled(r, c); r += wr, c += wc, ++len) {
            char t = after.at(r, c);
            w += char('a' + tile_letter(t));
            if (b.empty(r, c)) {
                sum += tile_score(t) * letter_mult(r, c);
                mul *= word_mult(r, c);
            } else sum += tile_score(t);
        }
        is_word = len >= 2;
        if (!is_word) return 0;  // a lone letter is not a cross-word
        if (!d.contains(w)) {
            if (bad.empty()) bad = w;
            return 0;
        }
        return sum * mul;
    };

    bool long_enough;
    int total = score_word(first.r, first.c, dr, dc, long_enough);
    if (!long_enough) return "a word needs at least 2 letters";
    for (const Placement &p : m.tiles) {
        bool cross;
        total += score_word(p.r, p.c, dc, dr, cross);
    }
    if (!bad.empty()) {
        for (char &ch : bad) ch = char(ch - 'a' + 'A');
        return bad + " is not in the word list";
    }
    if ((int)m.tiles.size() == kRackSize) total += kFullRackBonus;
    m.score = total;
    describe(b, m);
    return "";
}

}  // namespace we
