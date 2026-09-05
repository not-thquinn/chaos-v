#include "fractal_classifier.h"

#include <iostream>

namespace {

Config common(double left, double right) {
    Config config;
    config.leftDeg = left;
    config.rightDeg = right;
    config.gravity = 400;
    config.radius = 35;
    config.restitution = .9;
    config.gap = 320;
    config.segmentLength = 150;
    config.spawnX = -160;
    config.spawnY = -230;
    config.spawnInterval = 2;
    config.cutoffY = 560;
    config.maxBalls = 12;
    config.analysisBalls = 50;
    config.collisionBudget = 1000;
    config.precisionBits = 64;
    return config;
}

bool expectPeriod(const char* name, Config config, int period) {
    const Result result = classifyFractalPoint(config);
    if (result.outcome == Outcome::Periodic && result.period == period)
        return true;
    std::cerr << name << " failed: outcome=" << int(result.outcome)
              << " period=" << result.period << '\n';
    return false;
}

} // namespace

int main() {
    bool passed = true;
    passed &= expectPeriod(
        "period 3", common(72.0703125, -70.6640625), 3);
    passed &= expectPeriod(
        "period 6 broad", common(41.120895359374998, -71.947166175781248), 6);
    passed &= expectPeriod(
        "period 6 confirmation",
        common(71.3525576171875, -71.230935058593744), 6);
    passed &= expectPeriod(
        "period 5 confirmation",
        common(71.331697588867186, -71.248290801757818), 5);
    passed &= expectPeriod(
        "period 2 travelling stream",
        common(74.71976834763268, -41.282606356111067), 2);

    Config reportedTwentyTwo =
        common(36.5950033242447, -48.08591010396825);
    reportedTwentyTwo.spawnInterval = 1;
    reportedTwentyTwo.leftDegExact =
        "36.5950033242447035245240265217831821235";
    reportedTwentyTwo.rightDegExact =
        "-48.0859101039682542435295869007315153084";
    reportedTwentyTwo.precisionBits = 128;
    passed &= expectPeriod(
        "reported false period one", reportedTwentyTwo, 22);

    Config reportedFour =
        common(33.77798408629245, -37.80149595282757);
    reportedFour.leftDegExact =
        "33.7779840862924485319729480493937082820";
    reportedFour.rightDegExact =
        "-37.8014959528275746712983768189985373964";
    reportedFour.precisionBits = 128;
    passed &= expectPeriod(
        "reported unresolved period four", reportedFour, 4);

    Config fixed257 = common(72.0703125, -70.6640625);
    fixed257.precisionBits = 200;
    passed &= expectPeriod("fixed 257-bit band", fixed257, 3);

    Config fixed513 = common(72.0703125, -70.6640625);
    fixed513.precisionBits = 400;
    passed &= expectPeriod("fixed 513-bit band", fixed513, 3);
    return passed ? 0 : 1;
}
