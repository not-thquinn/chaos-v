#pragma once

#include "physics.h"

#include <QDialog>
#include <QString>

class QComboBox;
class QDoubleSpinBox;

enum class SweepParameter {
    Gravity,
    BallRadius,
    Restitution,
    SegmentGap,
    SegmentLength,
    SpawnInterval,
    SpawnY,
    CutoffY
};

struct SweepDefinition {
    SweepParameter parameter = SweepParameter::BallRadius;
    QString label;
    double minimum = 0;
    double maximum = 0;
    double increment = 1;
};

class SweepDialog : public QDialog {
public:
    explicit SweepDialog(const Config& current, QWidget* parent = nullptr);

    SweepDefinition definition() const;

private:
    void selectParameter(int index);

    Config current_;
    QComboBox* parameter_ = nullptr;
    QDoubleSpinBox* minimum_ = nullptr;
    QDoubleSpinBox* maximum_ = nullptr;
    QDoubleSpinBox* increment_ = nullptr;
};
