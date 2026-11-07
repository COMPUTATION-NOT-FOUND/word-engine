#include "wordengine/board.hpp"

#include <algorithm>

namespace we {

namespace {

// One eighth of the board (0 <= r <= c <= 7); the rest is mirrored.
Premium octant(int r, int c) {
    struct P { int r, c; Premium p; };
    static const P table[] = {
        {0, 3, Premium::TripleWord},
        {1, 1, Premium::DoubleWord}, {4, 4, Premium::DoubleWord}, {3, 7, Premium::DoubleWord},
        {7, 7, Premium::DoubleWord},  // centre: the first word must cover it
        {0, 7, Premium::TripleLetter}, {2, 5, Premium::TripleLetter},
        {0, 0, Premium::DoubleLetter}, {1, 4, Premium::DoubleLetter}, {3, 3, Premium::DoubleLetter},
        {5, 7, Premium::DoubleLetter}, {6, 6, Premium::DoubleLetter},
    };
    for (const P &p : table)
        if (p.r == r && p.c == c) return p.p;
    return Premium::None;
}

struct Layout {
    std::array<Premium, kSize * kSize> p{};
    Layout() {
        for (int r = 0; r < kSize; ++r)
            for (int c = 0; c < kSize; ++c) {
                int a = std::min(r, kSize - 1 - r), b = std::min(c, kSize - 1 - c);
                p[r * kSize + c] = octant(std::min(a, b), std::max(a, b));
            }
    }
};
const Layout kLayout;

// Own letter values (not any commercial game's).
constexpr std::array<int, 26> kValues = {
    1, 3, 4, 2, 1, 4, 3, 3, 1, 8, 6, 2, 3,   // a-m
    1, 1, 3, 10, 1, 1, 1, 2, 5, 4, 8, 3, 10  // n-z
};

}  // namespace

// 100 tiles: a b c d e f g h i j k l m n o p q r s t u v w x y z ?
const std::array<int, 27> kTileCounts = {
    8, 2, 3, 4, 12, 2, 3, 4, 8, 1, 1, 4, 2, 6, 7, 2, 1, 6, 5, 7, 3, 1, 2, 1, 2, 1, 2};

Premium premium(int r, int c) { return kLayout.p[r * kSize + c]; }
int letter_mult(int r, int c) {
    Premium p = premium(r, c);
    return p == Premium::DoubleLetter ? 2 : p == Premium::TripleLetter ? 3 : 1;
}
int word_mult(int r, int c) {
    Premium p = premium(r, c);
    return p == Premium::DoubleWord ? 2 : p == Premium::TripleWord ? 3 : 1;
}
int letter_value(int letter) { return kValues[letter]; }

bool Board::is_empty_board() const {
    return std::all_of(cells.begin(), cells.end(), [](char t) { return t == 0; });
}
int Board::tile_count() const {
    return (int)std::count_if(cells.begin(), cells.end(), [](char t) { return t != 0; });
}

Rack Rack::from_string(const std::string &s) {
    Rack r;
    for (char ch : s) {
        if (ch == '?') r.add(kBlank);
        else if (ch >= 'a' && ch <= 'z') r.add(ch - 'a');
        else if (ch >= 'A' && ch <= 'Z') r.add(ch - 'A');
    }
    return r;
}
std::string Rack::str() const {
    std::string s;
    for (int t = 0; t < 26; ++t) s.append(n[t], char('a' + t));
    s.append(n[kBlank], '?');
    return s;
}
int Rack::value() const {
    int v = 0;
    for (int t = 0; t < 26; ++t) v += n[t] * letter_value(t);
    return v;
}

void apply(Board &b, const Move &m) {
    if (m.kind != Move::Place) return;
    for (const Placement &p : m.tiles) b.at(p.r, p.c) = p.tile;
}

void describe(const Board &b, Move &m) {
    if (m.kind != Move::Place || m.tiles.empty()) return;
    Board after = b;
    apply(after, m);
    int dr = m.horizontal ? 0 : 1, dc = m.horizontal ? 1 : 0;
    int r = m.tiles[0].r, c = m.tiles[0].c;
    while (after.in(r - dr, c - dc) && !after.empty(r - dr, c - dc)) r -= dr, c -= dc;
    m.row = r, m.col = c;
    m.word.clear();
    for (; after.in(r, c) && !after.empty(r, c); r += dr, c += dc) {
        char t = after.at(r, c);
        m.word += t;  // lowercase normal, uppercase blank
    }
}

std::vector<std::string> words_formed(const Board &b, const Move &m) {
    std::vector<std::string> out;
    if (m.kind != Move::Place) return out;
    Board after = b;
    apply(after, m);
    auto word_through = [&](int r, int c, int dr, int dc) {
        while (after.in(r - dr, c - dc) && !after.empty(r - dr, c - dc)) r -= dr, c -= dc;
        std::string w;
        for (; after.in(r, c) && !after.empty(r, c); r += dr, c += dc) w += char('a' + tile_letter(after.at(r, c)));
        return w;
    };
    int dr = m.horizontal ? 0 : 1, dc = m.horizontal ? 1 : 0;
    std::string main = word_through(m.tiles[0].r, m.tiles[0].c, dr, dc);
    if (main.size() >= 2) out.push_back(main);
    for (const Placement &p : m.tiles) {
        std::string w = word_through(p.r, p.c, dc, dr);
        if (w.size() >= 2) out.push_back(w);
    }
    return out;
}

std::string Move::str() const {
    if (kind == Pass) return "pass";
    if (kind == Exchange) return "exchange " + exchange;
    // Across moves start with the row number ("8H"), down moves with the column letter ("H8").
    std::string pos = horizontal ? std::to_string(row + 1) + char('A' + col)
                                 : std::string(1, char('A' + col)) + std::to_string(row + 1);
    std::string w;
    for (char t : word) w += is_blank_tile(t) ? char(t - 'A' + 'a') : char(t - 'a' + 'A');  // blanks shown lowercase
    return pos + " " + w + " " + std::to_string(score);
}

}  // namespace we
