// bench: dictionary/DAWG stats, then time each method on random grids.
//   bench [dictionary] [--sizes 50,250,1000] [--reps 3]
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "wordengine/wordsearch.hpp"

using clk = std::chrono::steady_clock;
static double since(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }

int main(int argc, char **argv) {
    std::string dict = "data/enable1.txt";
    std::vector<int> sizes{50, 250, 1000};
    int reps = 3;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--sizes" && i + 1 < argc) {
            sizes.clear();
            std::stringstream ss(argv[++i]);
            std::string x;
            while (std::getline(ss, x, ',')) sizes.push_back(std::stoi(x));
        } else if (a == "--reps" && i + 1 < argc) {
            reps = std::max(1, std::stoi(argv[++i]));
        } else {
            dict = a;
        }
    }
    try {
        auto words = we::load_words(dict);
        size_t text = 0;
        for (auto &w : words) text += w.size() + 1;
        auto t0 = clk::now();
        we::WordSearcher ws(words);
        double build = since(t0);
        const auto &d = ws.dawg();
        std::printf("dictionary: %zu words, %zu of length >= 3, %.2f MB as text\n", words.size(),
                    d.word_count(), text / 1e6);
        std::printf("dawg: %zu nodes, %zu edges, %.2f MB (filters + dawg built in %.2f s)\n\n",
                    d.node_count(), d.edge_count(), d.bytes() / 1e6, build);

        std::printf("%-10s %-6s %8s %12s %12s %10s\n", "grid", "method", "found", "letters", "lookups",
                    "ms (best)");
        for (int n : sizes) {
            we::Grid g = we::random_grid(n, n, 42);
            size_t ref = SIZE_MAX;
            for (auto m : {we::Method::Lab, we::Method::Ngram, we::Method::Dawg}) {
                double best = 1e30;
                we::SearchStats st;
                size_t found = 0;
                for (int r = 0; r < reps; ++r) {
                    auto t = clk::now();
                    found = ws.find(g, m, &st).size();
                    best = std::min(best, since(t));
                }
                if (ref == SIZE_MAX) ref = found;
                std::printf("%4dx%-5d %-6s %8zu %12llu %12llu %10.1f%s\n", n, n, we::method_name(m), found,
                            (unsigned long long)st.letters, (unsigned long long)st.checks, best * 1e3,
                            found == ref ? "" : "  MISMATCH");
            }
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
