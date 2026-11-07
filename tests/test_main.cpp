// Small self-checking tests; exits non-zero if any check fails.
#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

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

int main() {
    test_dawg_small();
    test_dawg_random();
    test_search_hand();
    test_methods_agree();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all tests passed\n");
}
