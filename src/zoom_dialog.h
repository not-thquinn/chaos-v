#pragma once

#include "physics.h"
#include "precise_decimal.h"

#include <QDialog>
#include <QSettings>

#include <array>
#include <optional>

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;
class QCheckBox;

struct ZoomDefinition {
    Config config;
    std::array<PreciseDecimal, 4> startAxes;
    std::array<PreciseDecimal, 4> endAxes;
    double frameRate = 30;
    double duration = 10;
    double dampingTime = 1;
    double panTime = 2;
    double panDampingTime = .5;
    bool adaptivePrecision = true;
    qint64 frameCount = 300;
    int width = 1920;
    int height = 1080;
};

class ZoomDialog : public QDialog {
public:
    ZoomDialog(int initialWidth, int initialHeight, QWidget* parent = nullptr);
    ~ZoomDialog() override;

    const ZoomDefinition& definition() const;

protected:
    void accept() override;

private:
    void updateLimitsAndCount();
    void saveSettings();

    QPlainTextEdit* startJson_ = nullptr;
    QPlainTextEdit* endJson_ = nullptr;
    QDoubleSpinBox* frameRate_ = nullptr;
    QDoubleSpinBox* duration_ = nullptr;
    QDoubleSpinBox* dampingTime_ = nullptr;
    QDoubleSpinBox* panTime_ = nullptr;
    QDoubleSpinBox* panDampingTime_ = nullptr;
    QCheckBox* adaptivePrecision_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QLabel* frameCount_ = nullptr;
    std::optional<ZoomDefinition> definition_;
    QSettings settings_{"ChaosV", "ChaosV"};
};
