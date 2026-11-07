#pragma once
// DAWG: a trie of the word list where identical suffix subtrees are shared.
// Answers "is this a word?" and "can a word start like this?" one letter at a time.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace we {

class Dawg {
public:
    static constexpr uint32_t kNone = UINT32_MAX;

    // Words must be lowercase a-z. Order and duplicates don't matter.
    static Dawg build(std::vector<std::string> words);

    uint32_t root() const { return 0; }
    // Node reached from `node` by letter (0 = 'a' ... 25 = 'z'), or kNone.
    uint32_t child(uint32_t node, int letter) const {
        const Node &n = nodes_[node];
        uint32_t bit = 1u << letter;
        if (!(n.mask & bit)) return kNone;
        return edges_[n.first + popcount(n.mask & (bit - 1))];
    }
    bool is_word(uint32_t node) const { return nodes_[node].mask & kFinal; }
    // Bit i set = letter i continues from this node.
    uint32_t letters(uint32_t node) const { return nodes_[node].mask & kLetters; }

    uint32_t walk(std::string_view s, uint32_t from = 0) const;
    bool contains(std::string_view w) const;
    bool has_prefix(std::string_view p) const { return walk(p) != kNone; }

    size_t node_count() const { return nodes_.size(); }
    size_t edge_count() const { return edges_.size(); }
    size_t word_count() const { return words_; }
    size_t bytes() const { return nodes_.size() * sizeof(Node) + edges_.size() * sizeof(uint32_t); }

private:
    static constexpr uint32_t kFinal = 1u << 26;
    static constexpr uint32_t kLetters = kFinal - 1;
    struct Node {
        uint32_t mask;   // letter bits 0-25, word-end bit 26
        uint32_t first;  // index of the first child in edges_
    };
    static int popcount(uint32_t x) {
#if defined(_MSC_VER) && !defined(__clang__)
        return (int)__popcnt(x);
#else
        return __builtin_popcount(x);
#endif
    }
    std::vector<Node> nodes_;
    std::vector<uint32_t> edges_;  // children, in letter order
    size_t words_ = 0;
};

// Reads one word per line. Keeps only all-lowercase a-z words (so proper nouns like
// "Ottawa" are dropped, as in word games) of at least min_len letters.
std::vector<std::string> load_words(const std::string &path, size_t min_len = 2);

}  // namespace we
