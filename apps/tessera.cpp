// tessera: play the Tessera crossword-tile game against the AI in the terminal,
// or let two AI levels play each other.
//   tessera play <dictionary> [--level easy|medium|hard|expert] [--time MS] [--seed N] [--second]
//   tessera selfplay <dictionary> --a LEVEL --b LEVEL [--games N] [--time MS] [--seed N]
#include <cstdio>
#include <iostream>
#include <string>

#include "wordengine/ai.hpp"

using namespace we;

static void print_board(const Board &b) {
    std::printf("\n    ");
    for (int c = 0; c < kSize; ++c) std::printf(" %c ", 'A' + c);
    std::printf("\n");
    for (int r = 0; r < kSize; ++r) {
        std::printf("%3d ", r + 1);
        for (int c = 0; c < kSize; ++c) {
            char t = b.at(r, c);
            if (t) {
                std::printf(" %c ", is_blank_tile(t) ? char(t - 'A' + 'a') : char(t - 'a' + 'A'));
                continue;
            }
            const char *mark = " . ";
            switch (premium(r, c)) {
            case Premium::DoubleLetter: mark = " ' "; break;
            case Premium::TripleLetter: mark = " \" "; break;
            case Premium::DoubleWord: mark = " - "; break;
            case Premium::TripleWord: mark = " = "; break;
            default: break;
            }
            if (r == kCenter && c == kCenter) mark = " * ";
            std::printf("%s", mark);
        }
        std::printf("\n");
    }
    std::printf("  ' double letter   \" triple letter   - double word   = triple word   lowercase = blank\n\n");
}

static void print_report(const AiReport &rep) {
    std::printf("  [%s: %d moves, %d %s, depth %d, %.0f ms]\n", rep.method.c_str(), rep.moves_considered,
                rep.iterations, rep.method == "endgame" ? "nodes" : "playouts", rep.depth, rep.ms);
    for (const Candidate &c : rep.top) {
        std::printf("     %-28s equity %6.1f", c.move.str().c_str(), c.equity);
        if (rep.method == "simulation" || rep.method == "endgame") std::printf("  sim %6.1f (%d)", c.sim, c.iterations);
        std::printf("\n");
    }
}

static int play(const Dawg &d, Level level, int time_ms, uint64_t seed, bool human_second) {
    Game g(d, seed);
    Ai ai(d);
    int human = human_second ? 1 : 0;
    std::printf("Tessera vs %s AI. Moves: '8H WORD' across, 'H8 WORD' down, lowercase letter = blank,\n"
                "'.' skips a tile already on the board, 'exchange abc', 'pass', 'hint', 'quit'.\n",
                level_name(level));
    while (!g.over()) {
        print_board(g.board());
        std::printf("You %d  AI %d   bag %d\n", g.score(human), g.score(1 - human), g.bag_size());
        if (g.to_move() == human) {
            std::printf("Your rack: %s\n> ", g.rack(human).str().c_str());
            std::string line;
            if (!std::getline(std::cin, line) || line == "quit") return 0;
            if (line == "hint") {
                AiReport rep;
                AiOptions o{Level::Hard, time_ms, 12, seed};
                ai.choose(g, o, &rep);
                print_report(rep);
                continue;
            }
            Move m;
            std::string err = parse_move(g.board(), line, m);
            if (err.empty()) err = g.play(m);
            if (!err.empty()) std::printf("  %s\n", err.c_str());
            else std::printf("  you played %s\n", g.history().back().move.str().c_str());
        } else {
            AiReport rep;
            AiOptions o{level, time_ms, 12, seed + g.history().size()};
            Move m = ai.choose(g, o, &rep);
            std::string err = g.play(m);
            if (!err.empty()) {
                std::printf("  AI move refused (%s), passing\n", err.c_str());
                g.play(Move{});
            } else {
                std::printf("  AI played %s\n", g.history().back().move.str().c_str());
            }
            print_report(rep);
        }
    }
    print_board(g.board());
    std::printf("Game over (%s). You %d, AI %d.\n", g.end_reason().c_str(), g.score(human), g.score(1 - human));
    return 0;
}

static int selfplay(const Dawg &d, Level a, Level b, int games, int time_ms, uint64_t seed) {
    Ai ai(d);
    int wins[2] = {0, 0}, ties = 0;
    long total[2] = {0, 0};
    for (int i = 0; i < games; ++i) {
        Game g(d, seed + i);
        int a_seat = i % 2;  // alternate who starts
        while (!g.over()) {
            Level l = g.to_move() == a_seat ? a : b;
            AiOptions o{l, time_ms, 12, seed * 7919 + g.history().size()};
            Move m = ai.choose(g, o);
            if (!g.play(m).empty()) g.play(Move{});
        }
        int sa = g.score(a_seat), sb = g.score(1 - a_seat);
        total[0] += sa, total[1] += sb;
        if (sa > sb) ++wins[0]; else if (sb > sa) ++wins[1]; else ++ties;
        std::printf("game %3d: %s %4d  %s %4d\n", i + 1, level_name(a), sa, level_name(b), sb);
        std::fflush(stdout);
    }
    std::printf("\n%s won %d, %s won %d, %d tied. Average %.1f vs %.1f.\n", level_name(a), wins[0], level_name(b),
                wins[1], ties, (double)total[0] / games, (double)total[1] / games);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "usage: tessera play <dictionary> [--level easy|medium|hard|expert] [--time MS] [--seed N] [--second]\n"
                     "       tessera selfplay <dictionary> --a LEVEL --b LEVEL [--games N] [--time MS] [--seed N]\n";
        return 2;
    }
    std::string mode = argv[1], dict = argv[2];
    Level level = Level::Expert, a = Level::Hard, b = Level::Medium;
    int time_ms = 1500, games = 10;
    uint64_t seed = 1;
    bool second = false;
    for (int i = 3; i < argc; ++i) {
        std::string s = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (s == "--level") level = parse_level(next());
        else if (s == "--a") a = parse_level(next());
        else if (s == "--b") b = parse_level(next());
        else if (s == "--time") time_ms = std::stoi(next());
        else if (s == "--games") games = std::stoi(next());
        else if (s == "--seed") seed = std::stoull(next());
        else if (s == "--second") second = true;
    }
    try {
        Dawg d = Dawg::build(load_words(dict));
        if (mode == "play") return play(d, level, time_ms, seed, second);
        if (mode == "selfplay") return selfplay(d, a, b, games, time_ms, seed);
        std::cerr << "unknown mode " << mode << "\n";
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
