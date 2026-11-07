#pragma once
// Minimal stand-in for the course's stopwatch header (not included here),
// so LAB11.cpp builds unchanged. Same interface: construct to start,
// elapsedTime() returns seconds since then.
#include <chrono>

class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}
    double elapsedTime() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};
