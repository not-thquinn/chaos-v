#include "physics.h"

#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    const int side = argc > 1 ? std::max(1, std::atoi(argv[1])) : 24;
    const int precisionBits = argc > 2 ? std::max(32, std::atoi(argv[2])) : 64;

    Config base;
    base.gravity = 400;
    base.radius = 35;
    base.restitution = .9;
    base.gap = 320;
    base.segmentLength = 150;
    base.spawnX = -160;
    base.spawnY = -230;
    base.spawnInterval = 2;
    base.cutoffY = 560;
    base.maxBalls = 12;
    base.analysisBalls = 50;
    base.collisionBudget = 1000;
    base.precisionBits = precisionBits;

    int periodic = 0;
    int unresolved = 0;
    int errors = 0;
    long long collisions = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            Config point = base;
            point.leftDeg = 68. + (x + .5) / side * 6.;
            point.rightDeg = -74. + (y + .5) / side * 6.;
            const Result result = Simulator(point).run(false);
            collisions += result.collisionEvents;
            if (result.outcome == Outcome::Periodic)
                ++periodic;
            else if (result.outcome == Outcome::Unresolved)
                ++unresolved;
            else
                ++errors;
        }
    }
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const int simulations = side * side;
    std::cout << "simulations=" << simulations
              << " seconds=" << seconds
              << " simulations_per_second=" << simulations / seconds
              << " precision_bits=" << precisionBits
              << " periodic=" << periodic
              << " unresolved=" << unresolved
              << " errors=" << errors
              << " collisions=" << collisions << '\n';
    return 0;
}
