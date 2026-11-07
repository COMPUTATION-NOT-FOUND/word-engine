// wsearch: solve a word-search grid.
//   wsearch [--method lab|ngram|dawg] [--quiet] <dictionary> <grid file>
//   wsearch [--method ...] [--quiet] <dictionary> --random ROWSxCOLS [SEED]
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

#include "wordengine/wordsearch.hpp"

static const char *dir_name(int dr, int dc) {
    static const char *n[3][3] = {{"NW", "N ", "NE"}, {"W ", "? ", "E "}, {"SW", "S ", "SE"}};
    return n[dr + 1][dc + 1];
}

int main(int argc, char **argv) {
    we::Method method = we::Method::Dawg;
    bool quiet = false;
    std::string dict, grid_path;
    int rows = 0, cols = 0;
    uint32_t seed = 1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--method" && i + 1 < argc) {
            std::string m = argv[++i];
            if (m == "lab") method = we::Method::Lab;
            else if (m == "ngram") method = we::Method::Ngram;
            else if (m == "dawg") method = we::Method::Dawg;
            else { std::cerr << "unknown method " << m << "\n"; return 2; }
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--random" && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%dx%d", &rows, &cols) != 2 || rows <= 0 || cols <= 0) {
                std::cerr << "--random wants ROWSxCOLS\n";
                return 2;
            }
            if (i + 1 < argc && argv[i + 1][0] != '-') seed = (uint32_t)std::stoul(argv[++i]);
        } else if (dict.empty()) {
            dict = a;
        } else {
            grid_path = a;
        }
    }
    if (dict.empty() || (grid_path.empty() && rows == 0)) {
        std::cerr << "usage: wsearch [--method lab|ngram|dawg] [--quiet] <dictionary> <grid file>\n"
                     "       wsearch [--method ...] [--quiet] <dictionary> --random ROWSxCOLS [SEED]\n";
        return 2;
    }
    try {
        we::WordSearcher ws(we::load_words(dict));
        we::Grid g = rows ? we::random_grid(rows, cols, seed) : we::load_grid(grid_path);
        we::SearchStats st;
        auto t0 = std::chrono::steady_clock::now();
        auto found = ws.find(g, method, &st);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (!quiet)
            for (auto &m : found)
                std::cout << dir_name(m.dr, m.dc) << "(" << m.row << ", " << m.col << "):\t" << m.word << "\n";
        std::cout << found.size() << " words found\n";
        std::cout << "Found all words in " << secs << " seconds (method " << we::method_name(method) << ", "
                  << st.letters << " letters walked, " << st.checks << " lookups)\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
