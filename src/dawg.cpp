#include "wordengine/dawg.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace we {

namespace {

// Mutable node used while building.
struct BuildNode {
    bool final = false;
    std::vector<std::pair<char, uint32_t>> kids;  // added in letter order (input is sorted)
};

std::string signature(const BuildNode &n) {
    std::string s(1, n.final ? '1' : '0');
    for (auto [c, id] : n.kids) {
        s += c;
        s.append(reinterpret_cast<const char *>(&id), sizeof id);
    }
    return s;
}

}  // namespace

// Incremental construction from sorted words (Daciuk et al., 2000): after each word,
// the part of the previous word's path that can no longer change is merged with an
// equal node already seen, so the graph stays minimal as it grows.
Dawg Dawg::build(std::vector<std::string> words) {
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());

    std::vector<BuildNode> t(1);
    std::unordered_map<std::string, uint32_t> registry;
    std::vector<uint32_t> path{0};  // path[i] = node after i letters of the previous word

    auto minimize = [&](size_t keep) {
        while (path.size() > keep + 1) {
            uint32_t node = path.back();
            path.pop_back();
            auto [it, inserted] = registry.try_emplace(signature(t[node]), node);
            if (!inserted) t[path.back()].kids.back().second = it->second;
        }
    };

    std::string prev;
    size_t count = 0;
    for (const std::string &w : words) {
        if (w.empty()) continue;
        for (char c : w)
            if (c < 'a' || c > 'z') throw std::invalid_argument("word outside a-z: " + w);
        size_t common = 0;
        while (common < w.size() && common < prev.size() && w[common] == prev[common]) ++common;
        minimize(common);
        for (size_t i = common; i < w.size(); ++i) {
            uint32_t id = (uint32_t)t.size();
            t.emplace_back();
            t[path.back()].kids.emplace_back(w[i], id);
            path.push_back(id);
        }
        t[path.back()].final = true;
        prev = w;
        ++count;
    }
    minimize(0);

    // Number the nodes still reachable (breadth-first) and pack them.
    std::vector<uint32_t> remap(t.size(), kNone);
    std::vector<uint32_t> order{0};
    remap[0] = 0;
    for (size_t i = 0; i < order.size(); ++i)
        for (auto [c, kid] : t[order[i]].kids)
            if (remap[kid] == kNone) {
                remap[kid] = (uint32_t)order.size();
                order.push_back(kid);
            }

    Dawg d;
    d.words_ = count;
    d.nodes_.resize(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        const BuildNode &b = t[order[i]];
        Node &n = d.nodes_[i];
        n.mask = b.final ? kFinal : 0;
        n.first = (uint32_t)d.edges_.size();
        for (auto [c, kid] : b.kids) {
            n.mask |= 1u << (c - 'a');
            d.edges_.push_back(remap[kid]);
        }
    }
    return d;
}

uint32_t Dawg::walk(std::string_view s, uint32_t from) const {
    uint32_t n = from;
    for (char c : s) {
        if (c < 'a' || c > 'z') return kNone;
        n = child(n, c - 'a');
        if (n == kNone) return kNone;
    }
    return n;
}

bool Dawg::contains(std::string_view w) const {
    uint32_t n = walk(w);
    return n != kNone && is_word(n);
}

std::vector<std::string> load_words(const std::string &path, size_t min_len) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open word list: " + path);
    std::vector<std::string> out;
    std::string line;
    while (std::getline(in, line)) {
        std::string w;
        bool ok = true;
        for (char c : line) {
            if (c == '\r' || c == ' ' || c == '\t') continue;
            if (c < 'a' || c > 'z') { ok = false; break; }  // also drops capitalised proper nouns
            w += c;
        }
        if (ok && w.size() >= min_len) out.push_back(std::move(w));
    }
    return out;
}

}  // namespace we
