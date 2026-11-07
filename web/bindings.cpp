// Browser bindings (Emscripten + embind). The page talks to these through web/worker.js.
// Everything crosses as strings: plain arguments in, JSON out.
#include <emscripten/bind.h>

#include <cstdio>
#include <memory>
#include <random>
#include <sstream>
#include <string>

#include "wordengine/ai.hpp"
#include "wordengine/wordsearch.hpp"

using namespace we;

namespace {

std::unique_ptr<Dawg> dawg;
std::unique_ptr<Game> game;
std::unique_ptr<Ai> ai;

std::string esc(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

std::string move_json(const Move &m) {
    std::ostringstream o;
    o << "{\"text\":\"" << esc(m.str()) << "\",\"score\":" << m.score << ",\"kind\":\""
      << (m.kind == Move::Place ? "place" : m.kind == Move::Exchange ? "exchange" : "pass") << "\",\"tiles\":[";
    for (size_t i = 0; i < m.tiles.size(); ++i)
        o << (i ? "," : "") << "[" << (int)m.tiles[i].r << "," << (int)m.tiles[i].c << "]";
    o << "]}";
    return o.str();
}

std::string report_json(const AiReport &r) {
    std::ostringstream o;
    o << "{\"method\":\"" << r.method << "\",\"moves\":" << r.moves_considered << ",\"iterations\":" << r.iterations
      << ",\"depth\":" << r.depth << ",\"ms\":" << (int)r.ms << ",\"top\":[";
    for (size_t i = 0; i < r.top.size(); ++i) {
        const Candidate &c = r.top[i];
        o << (i ? "," : "") << "{\"text\":\"" << esc(c.move.str()) << "\",\"equity\":" << c.equity
          << ",\"sim\":" << c.sim << ",\"n\":" << c.iterations << "}";
    }
    o << "]}";
    return o.str();
}

}  // namespace

// Builds the dictionary from the word list text (one word per line). Returns the word count.
int init(const std::string &text) {
    std::vector<std::string> words;
    std::istringstream in(text);
    std::string w;
    while (std::getline(in, w)) {
        while (!w.empty() && (w.back() == '\r' || w.back() == ' ')) w.pop_back();
        bool ok = w.size() >= 2;
        for (char c : w) ok = ok && c >= 'a' && c <= 'z';
        if (ok) words.push_back(w);
    }
    dawg = std::make_unique<Dawg>(Dawg::build(std::move(words)));
    ai = std::make_unique<Ai>(*dawg);
    return (int)dawg->word_count();
}

void new_game(double seed) { game = std::make_unique<Game>(*dawg, (uint64_t)seed); }

// The whole game for the page to draw. Player 0 is the human; the AI rack is left out.
std::string state() {
    const Game &g = *game;
    std::ostringstream o;
    std::string board;
    for (char t : g.board().cells) board += t ? t : '.';
    o << "{\"board\":\"" << board << "\",\"rack\":\"" << g.rack(0).str() << "\",\"aiTiles\":" << g.rack(1).size
      << ",\"scores\":[" << g.score(0) << "," << g.score(1) << "],\"bag\":" << g.bag_size()
      << ",\"toMove\":" << g.to_move() << ",\"over\":" << (g.over() ? "true" : "false") << ",\"endReason\":\""
      << esc(g.end_reason()) << "\",\"history\":[";
    const auto &h = g.history();
    for (size_t i = 0; i < h.size(); ++i)
        o << (i ? "," : "") << "{\"player\":" << h[i].player << ",\"total\":" << h[i].total_after
          << ",\"move\":" << move_json(h[i].move) << "}";
    o << "]";
    if (g.over()) o << ",\"aiRack\":\"" << g.rack(1).str() << "\"";
    o << "}";
    return o.str();
}

// Human move from tiles placed on the board: "r,c,t;r,c,t" (t lowercase = normal, uppercase = blank).
std::string play_tiles(const std::string &spec) {
    Move m;
    m.kind = Move::Place;
    std::istringstream in(spec);
    std::string part;
    while (std::getline(in, part, ';')) {
        int r, c;
        char t;
        if (std::sscanf(part.c_str(), "%d,%d,%c", &r, &c, &t) == 3) m.tiles.push_back({(int8_t)r, (int8_t)c, t});
    }
    return game->play(m);
}

std::string play_text(const std::string &text) {
    Move m;
    std::string err = parse_move(game->board(), text, m);
    return err.empty() ? game->play(m) : err;
}

std::string exchange(const std::string &tiles) {
    Move m;
    m.kind = Move::Exchange;
    m.exchange = tiles;
    return game->play(m);
}

std::string pass_turn() { return game->play(Move{}); }

// Lets the AI think for the side to move. With `play` set it also makes the move.
std::string ai_turn(const std::string &level, int time_ms, bool play) {
    AiOptions opt;
    opt.level = parse_level(level);
    opt.time_ms = time_ms;
    opt.seed = 1000 + game->history().size();
    AiReport rep;
    Move m = ai->choose(*game, opt, &rep);
    std::string err;
    if (play) {
        err = game->play(m);
        if (!err.empty()) game->play(Move{});
    }
    return "{\"move\":" + move_json(m) + ",\"report\":" + report_json(rep) + ",\"error\":\"" + esc(err) + "\"}";
}

// Word Hunt: a random grid and every word hidden in it (straight lines, 8 directions).
std::string hunt_grid(int rows, int cols, double seed, int min_len) {
    Grid g = random_grid(rows, cols, (uint32_t)seed);
    std::ostringstream o;
    o << "{\"rows\":" << rows << ",\"cols\":" << cols << ",\"cells\":\"" << g.cells << "\",\"words\":[";
    bool first = true;
    std::vector<std::string> seen;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc) {
                    if (!dr && !dc) continue;
                    uint32_t node = dawg->root();
                    std::string w;
                    for (int y = r, x = c; y >= 0 && y < rows && x >= 0 && x < cols; y += dr, x += dc) {
                        node = dawg->child(node, g.at(y, x) - 'a');
                        if (node == Dawg::kNone) break;
                        w += g.at(y, x);
                        if ((int)w.size() >= min_len && dawg->is_word(node)) {
                            o << (first ? "" : ",") << "{\"w\":\"" << w << "\",\"r\":" << r << ",\"c\":" << c
                              << ",\"dr\":" << dr << ",\"dc\":" << dc << "}";
                            first = false;
                        }
                    }
                }
    o << "]}";
    return o.str();
}

// Board layout (225 digits: 0 plain, 1 double letter, 2 triple letter, 3 double word, 4 triple word)
// and the letter values, so the page draws exactly what the engine scores.
std::string layout() {
    std::ostringstream o;
    o << "{\"premium\":\"";
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) o << (int)premium(r, c);
    o << "\",\"values\":[";
    for (int l = 0; l < 26; ++l) o << (l ? "," : "") << letter_value(l);
    o << "]}";
    return o.str();
}

EMSCRIPTEN_BINDINGS(wordengine) {
    emscripten::function("layout", &layout);
    emscripten::function("init", &init);
    emscripten::function("newGame", &new_game);
    emscripten::function("state", &state);
    emscripten::function("playTiles", &play_tiles);
    emscripten::function("playText", &play_text);
    emscripten::function("exchange", &exchange);
    emscripten::function("pass", &pass_turn);
    emscripten::function("aiTurn", &ai_turn);
    emscripten::function("huntGrid", &hunt_grid);
}
