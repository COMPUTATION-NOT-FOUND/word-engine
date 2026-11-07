#include "wordengine/wordsearch.hpp"

#include <cctype>
#include <fstream>
#include <random>
#include <stdexcept>

namespace we {

namespace {
const int kDirs[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}};
}

const char *method_name(Method m) {
    switch (m) {
        case Method::Lab: return "lab";
        case Method::Ngram: return "ngram";
        case Method::Dawg: return "dawg";
    }
    return "?";
}

Grid load_grid(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open grid: " + path);
    Grid g;
    in >> g.rows >> g.cols;
    char c;
    while (in.get(c) && g.cells.size() < (size_t)g.rows * g.cols)
        if (std::isalpha((unsigned char)c)) g.cells += (char)std::tolower((unsigned char)c);
    if (g.rows <= 0 || g.cols <= 0 || g.cells.size() != (size_t)g.rows * g.cols)
        throw std::runtime_error("bad grid file: " + path);
    return g;
}

Grid random_grid(int rows, int cols, uint32_t seed) {
    // Approximate letter frequencies in English text, per 1000 letters.
    static const int freq[26] = {82, 15, 28, 43, 127, 22, 20, 61, 70, 2,  8,  40, 24,
                                 67, 75, 19, 1,  60, 63, 91, 28, 10, 24, 2,  20, 1};
    int total = 0;
    for (int f : freq) total += f;
    std::mt19937 rng(seed);  // mt19937 output is fixed by the standard; distributions are not
    Grid g{rows, cols, std::string((size_t)rows * cols, 'a')};
    for (char &ch : g.cells) {
        int x = (int)(rng() % (uint32_t)total), i = 0;
        while (x >= freq[i]) x -= freq[i++];
        ch = (char)('a' + i);
    }
    return g;
}

WordSearcher::WordSearcher(const std::vector<std::string> &words, size_t min_len)
    : min_len_(min_len) {
    std::vector<std::string> kept;
    for (const auto &w : words) {
        if (w.size() < min_len_) continue;
        kept.push_back(w);
        words_.insert(w);
        start2_.set(bi(w[0], w[1]));
        if (w.size() >= 3) start3_.set(tri(w[0], w[1], w[2]));
        for (size_t i = 0; i + 3 <= w.size(); ++i) {
            tri_bits_.set(tri(w[i], w[i + 1], w[i + 2]));
            tri_any_.insert(w.substr(i, 3));
        }
    }
    dawg_ = Dawg::build(std::move(kept));
}

std::vector<Match> WordSearcher::find(const Grid &g, Method m, SearchStats *stats) const {
    SearchStats st;
    std::vector<Match> out;
    std::string s;
    for (int r = 0; r < g.rows; ++r)
        for (int c = 0; c < g.cols; ++c)
            for (auto &d : kDirs) {
                s.clear();
                uint32_t node = dawg_.root();
                for (int k = 0;; ++k) {
                    int rr = r + k * d[0], cc = c + k * d[1];
                    if (rr < 0 || rr >= g.rows || cc < 0 || cc >= g.cols) break;
                    s += g.at(rr, cc);
                    ++st.letters;
                    size_t n = s.size();
                    bool is_word = false;

                    if (m == Method::Lab) {
                        if (n < 3) continue;
                        bool ok = true;
                        for (size_t i = 0; i + 3 <= n; ++i) {  // every window, every step
                            ++st.checks;
                            if (!tri_any_.count(s.substr(i, 3))) { ok = false; break; }
                        }
                        if (!ok) break;
                        ++st.checks;
                        is_word = words_.count(s) > 0;
                    } else if (m == Method::Ngram) {
                        ++st.checks;
                        if (n == 2 && !start2_.test(bi(s[0], s[1]))) break;
                        if (n == 3 && !start3_.test(tri(s[0], s[1], s[2]))) break;
                        if (n > 3 && !tri_bits_.test(tri(s[n - 3], s[n - 2], s[n - 1]))) break;
                        if (n >= min_len_) { ++st.checks; is_word = words_.count(s) > 0; }
                    } else {
                        ++st.checks;
                        node = dawg_.child(node, s.back() - 'a');
                        if (node == Dawg::kNone) break;
                        is_word = n >= min_len_ && dawg_.is_word(node);
                    }
                    if (is_word) out.push_back({s, r, c, d[0], d[1]});
                }
            }
    if (stats) *stats = st;
    return out;
}

}  // namespace we
