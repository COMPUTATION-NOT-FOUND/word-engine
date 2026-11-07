// Small self-checking tests; exits non-zero if any check fails.
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "wordengine/ai.hpp"
#include "wordengine/wordsearch.hpp"

static int failures = 0;
#define CHECK(x)                                                          \
    do {                                                                  \
        if (!(x)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #x);      \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static std::set<std::string> as_set(const std::vector<we::Match> &v) {
    std::set<std::string> s;
    for (auto &m : v)
        s.insert(m.word + "@" + std::to_string(m.row) + "," + std::to_string(m.col) + "/" +
                 std::to_string(m.dr) + std::to_string(m.dc));
    return s;
}

static void test_dawg_small() {
    auto d = we::Dawg::build({"cats", "cat", "car", "bats", "bat", "cat", "card"});
    CHECK(d.word_count() == 6);
    for (auto w : {"cat", "cats", "car", "card", "bat", "bats"}) CHECK(d.contains(w));
    for (auto w : {"", "c", "ca", "ba", "cars", "bad", "dog", "catsx"}) CHECK(!d.contains(w));
    CHECK(d.has_prefix("ca") && d.has_prefix("bat") && !d.has_prefix("cb"));
    // "cat" and "bat" have the same endings ("", "s"), so they share one node.
    CHECK(d.walk("cat") != we::Dawg::kNone && d.walk("cat") == d.walk("bat"));
}

static void test_dawg_random() {
    std::mt19937 rng(7);
    std::vector<std::string> words;
    std::set<std::string> ref;
    for (int i = 0; i < 5000; ++i) {
        std::string w(1 + rng() % 8, 'a');
        for (char &c : w) c = (char)('a' + rng() % 5);
        words.push_back(w);
        ref.insert(w);
    }
    auto d = we::Dawg::build(words);
    CHECK(d.word_count() == ref.size());
    for (int i = 0; i < 20000; ++i) {
        std::string w(1 + rng() % 9, 'a');
        for (char &c : w) c = (char)('a' + rng() % 6);
        CHECK(d.contains(w) == (ref.count(w) > 0));
    }
}

static void test_search_hand() {
    // c a t
    // o x a
    // w e t
    we::WordSearcher ws({"cat", "cow", "tat", "wet", "tax", "act", "ox", "tea"});
    we::Grid g{3, 3, "catoxawet"};
    std::set<std::string> want{"cat@0,0/01", "cow@0,0/10", "tat@0,2/10",
                               "tat@2,2/-10", "wet@2,0/01"};
    for (auto m : {we::Method::Lab, we::Method::Ngram, we::Method::Dawg}) {
        auto s = as_set(ws.find(g, m));
        if (s != want) {
            std::printf("method %s found:", we::method_name(m));
            for (auto &x : s) std::printf(" %s", x.c_str());
            std::printf("\n");
        }
        CHECK(s == want);
    }
}

static void test_methods_agree() {
    std::mt19937 rng(3);
    std::vector<std::string> words;
    for (int i = 0; i < 20000; ++i) {
        std::string w(2 + rng() % 6, 'a');
        for (char &c : w) c = "etaoinsrhl"[rng() % 10];
        words.push_back(w);
    }
    we::WordSearcher ws(words);
    for (uint32_t seed = 1; seed <= 5; ++seed) {
        auto g = we::random_grid(40, 30, seed);
        auto a = as_set(ws.find(g, we::Method::Lab));
        auto b = as_set(ws.find(g, we::Method::Ngram));
        auto c = as_set(ws.find(g, we::Method::Dawg));
        CHECK(!a.empty());
        CHECK(a == b);
        CHECK(a == c);
    }
}

static void test_tessera_scoring() {
    we::Dawg d = we::Dawg::build({"cat", "cats", "at", "ta", "act"});
    we::Board b;
    we::Move m;
    // CAT across through the centre: C on G8, A on the centre (double word), T on I8.
    CHECK(we::parse_move(b, "8G CAT", m).empty());
    CHECK(we::validate(d, b, m).empty());
    CHECK(m.score == (4 + 1 + 1) * 2);  // C=4, A=1, T=1
    CHECK(m.word == "cat" && m.row == 7 && m.col == 6);
    we::Move off;
    CHECK(we::parse_move(b, "1A CAT", off).empty());
    CHECK(!we::validate(d, b, off).empty());  // misses the centre
    we::Move bad;
    CHECK(we::parse_move(b, "8G TAC", bad).empty());
    CHECK(we::validate(d, b, bad) == "TAC is not in the word list");
    // A blank (lowercase) scores nothing.
    we::Move blank;
    CHECK(we::parse_move(b, "8G cAT", blank).empty());
    CHECK(we::validate(d, b, blank).empty() && blank.score == (0 + 1 + 1) * 2);
    // Every first move the generator finds for this rack is valid with the same score.
    we::MoveGen gen(d);
    auto moves = gen.all_moves(b, we::Rack::from_string("cats"));
    CHECK(!moves.empty());
    for (auto mv : moves) {
        int s = mv.score;
        CHECK(we::validate(d, b, mv).empty() && mv.score == s);
    }
}

// Plays whole games on the real word list, checking the generator against the
// independent validator on every turn and that no tile appears or disappears.
static void test_tessera_games() {
    std::vector<std::string> words;
    try {
        words = we::load_words(WE_DICT);
    } catch (...) {
        std::printf("skipping game tests: %s not found\n", WE_DICT);
        return;
    }
    we::Dawg d = we::Dawg::build(words);
    we::MoveGen gen(d);
    we::Ai ai(d);
    for (uint64_t seed = 1; seed <= 3; ++seed) {
        we::Game g(d, seed);
        int turns = 0;
        while (!g.over() && turns < 200) {
            const we::Board &b = g.board();
            auto moves = gen.all_moves(b, g.rack(g.to_move()));
            std::set<std::string> keys;
            for (auto mv : moves) {
                int s = mv.score;
                std::string err = we::validate(d, b, mv);
                CHECK(err.empty());
                CHECK(mv.score == s);
                if (!err.empty() || mv.score != s) {
                    std::printf("  seed %llu turn %d: %s -> %s (%d vs %d)\n", (unsigned long long)seed, turns,
                                mv.str().c_str(), err.c_str(), s, mv.score);
                    return;
                }
                std::string key = mv.horizontal ? "h" : "v";
                for (auto &p : mv.tiles) key += std::to_string(p.r * 15 + p.c) + p.tile;
                CHECK(keys.insert(key).second);  // no move found twice
            }
            we::AiOptions o;
            o.level = seed == 2 ? we::Level::Medium : we::Level::Hard;
            we::Move m = ai.choose(g, o);
            // The move's text form parses back to the same tiles.
            if (m.kind == we::Move::Place) {
                we::Move back;
                CHECK(we::parse_move(b, m.str(), back).empty());
                CHECK(back.tiles.size() == m.tiles.size());
            }
            CHECK(g.play(m).empty());
            ++turns;
            int count = g.board().tile_count() + g.rack(0).size + g.rack(1).size + g.bag_size();
            CHECK(count == 100);
        }
        CHECK(g.over());
    }
}

int main() {
    test_dawg_small();
    test_dawg_random();
    test_search_hand();
    test_methods_agree();
    test_tessera_scoring();
    test_tessera_games();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all tests passed\n");
}
