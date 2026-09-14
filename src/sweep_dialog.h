#pragma once

#include "physics.h"

#include <QDialog>
#include <QSettings>

#include <optional>

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;

qint64 sweepFrameCount(double frameRate, double duration);

struct SweepDefinition {
    Config startConfig;
    Config endConfig;
    double frameRate = 30;
    double duration = 10;
    qint64 frameCount = 300;
    int width = 1920;
    int height = 1080;
};

Config interpolateSweepConfig(
    const SweepDefinition& definition, double progress);

class SweepDialog : public QDialog {
public:
    SweepDialog(const QString& currentJson, int initialWidth,
                int initialHeight, QWidget* parent = nullptr);
    ~SweepDialog() override;

    const SweepDefinition& definition() const;

protected:
    void accept() override;

private:
    void updateFrameCount();
    void saveSettings();

    QPlainTextEdit* startJson_ = nullptr;
    QPlainTextEdit* endJson_ = nullptr;
    QDoubleSpinBox* frameRate_ = nullptr;
    QDoubleSpinBox* duration_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QLabel* frameCount_ = nullptr;
    std::optional<SweepDefinition> definition_;
    QSettings settings_{"ChaosV", "ChaosV"};
};
