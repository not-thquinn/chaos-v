#include "physics.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <iomanip>
#include <cstdlib>
#include <iostream>

namespace {

Config configFromJson(const QJsonObject& object) {
    Config config;
    const auto number = [&](const char* name, double fallback) {
        return object.value(name).toDouble(fallback);
    };

    bool valid = false;
    const QString leftExact = object.value("leftAngleExact").toString();
    config.leftDeg = leftExact.toDouble(&valid);
    if (!valid)
        config.leftDeg = number("leftAngle", config.leftDeg);
    else
        config.leftDegExact = leftExact.toStdString();
    const QString rightExact = object.value("rightAngleExact").toString();
    config.rightDeg = rightExact.toDouble(&valid);
    if (!valid)
        config.rightDeg = number("rightAngle", config.rightDeg);
    else
        config.rightDegExact = rightExact.toStdString();

    config.gravity = number("gravity", config.gravity);
    config.radius = number("ballRadius", config.radius);
    config.restitution = number("restitution", config.restitution);
    config.gap = number("segmentGap", config.gap);
    config.segmentLength = number("segmentLength", config.segmentLength);
    config.spawnX = -config.gap / 2;
    config.spawnY = number("spawnY", config.spawnY);
    config.spawnInterval = number("spawnInterval", config.spawnInterval);
    config.cutoffY = number("cutoffY", config.cutoffY);
    config.maxBalls = object.value("maxLiveBalls").toInt(config.maxBalls);
    config.analysisBalls =
        object.value("ballsToAnalyze").toInt(config.analysisBalls);
    config.collisionBudget =
        object.value("collisionBudget").toInt(config.collisionBudget);
    config.precisionBits =
        object.value("precisionBits").toInt(config.precisionBits);
    return config;
}

int diagnose(const QString& path, int precisionOverride) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Could not open JSON file\n";
        return 2;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        std::cerr << "Invalid JSON\n";
        return 2;
    }

    Config config = configFromJson(document.object());
    config.trackPeriodStability = true;
    if (precisionOverride > 0)
        config.precisionBits = precisionOverride;
    const Result result = Simulator(config).run(false);
    std::cout << "outcome=" << int(result.outcome)
              << " period=" << result.period
              << " spawnedAtDetection=" << result.ballsSpawnedAtDetection
              << " collisionsAtDetection=" << result.collisionsAtDetection
              << " collisions=" << result.collisionEvents
              << " stability=" << result.periodStability
              << " expansion=" << result.expansionMargin
              << " contraction=" << result.contractionMargin
              << " exits=" << result.exits.size() << '\n';
    std::cout << std::setprecision(40);
    for (size_t i = 0; i < result.exits.size(); ++i) {
        const auto& sample = result.exits[i];
        std::cout << result.exitIds[i] << ',' << sample.hits << ','
                  << sample.x << ',' << sample.vx << ',' << sample.lifetime
                  << ",partners=";
        for (const int offset : sample.partnerOffsets)
            std::cout << offset << ';';
        std::cout << '\n';
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: chaos_v_diagnose <simulation.json> [precision-bits]\n";
        return 2;
    }
    return diagnose(
        QString::fromLocal8Bit(argv[1]), argc == 3 ? std::atoi(argv[2]) : 0);
}
