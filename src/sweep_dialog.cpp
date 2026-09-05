#include "sweep_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <array>

namespace {

struct ParameterDescriptor {
    SweepParameter parameter;
    const char* label;
    double low;
    double high;
    double step;
};

constexpr std::array descriptors{
    ParameterDescriptor{SweepParameter::BallRadius, "Ball radius", .1, 100, .1},
    ParameterDescriptor{SweepParameter::Gravity, "Gravity", .01, 5000, 10},
    ParameterDescriptor{SweepParameter::Restitution, "Restitution", .5, 100, .01},
    ParameterDescriptor{SweepParameter::SegmentGap, "Segment gap", 1, 1000, 1},
    ParameterDescriptor{SweepParameter::SegmentLength, "Segment length", 1, 2000, 1},
    ParameterDescriptor{SweepParameter::SpawnInterval, "Spawn interval", .01, 10, .01},
    ParameterDescriptor{SweepParameter::SpawnY, "Spawn y", -5000, 5000, 10},
    ParameterDescriptor{SweepParameter::CutoffY, "Cutoff y", 100, 5000, 10}
};

double currentValue(const Config& config, SweepParameter parameter) {
    switch (parameter) {
    case SweepParameter::Gravity: return config.gravity;
    case SweepParameter::BallRadius: return config.radius;
    case SweepParameter::Restitution: return config.restitution;
    case SweepParameter::SegmentGap: return config.gap;
    case SweepParameter::SegmentLength: return config.segmentLength;
    case SweepParameter::SpawnInterval: return config.spawnInterval;
    case SweepParameter::SpawnY: return config.spawnY;
    case SweepParameter::CutoffY: return config.cutoffY;
    }
    return 0;
}

} // namespace

SweepDialog::SweepDialog(const Config& current, QWidget* parent)
    : QDialog(parent), current_(current) {
    setWindowTitle("Generate parameter sweep");

    parameter_ = new QComboBox;
    for (const auto& descriptor : descriptors)
        parameter_->addItem(
            QString::fromLatin1(descriptor.label), int(descriptor.parameter));

    const auto makeValue = [this] {
        auto* spin = new QDoubleSpinBox(this);
        spin->setDecimals(12);
        spin->setKeyboardTracking(false);
        return spin;
    };
    minimum_ = makeValue();
    maximum_ = makeValue();
    increment_ = makeValue();

    auto* form = new QFormLayout;
    form->addRow("Parameter", parameter_);
    form->addRow("Minimum", minimum_);
    form->addRow("Maximum", maximum_);
    form->addRow("Increment", increment_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(parameter_, &QComboBox::currentIndexChanged,
            this, &SweepDialog::selectParameter);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    selectParameter(0);
    setMinimumWidth(340);
}

void SweepDialog::selectParameter(int index) {
    if (index < 0 || index >= int(descriptors.size()))
        return;
    const auto& descriptor = descriptors[size_t(index)];
    const double value = currentValue(current_, descriptor.parameter);
    minimum_->setRange(descriptor.low, descriptor.high);
    maximum_->setRange(descriptor.low, descriptor.high);
    increment_->setRange(1e-12, descriptor.high - descriptor.low);
    minimum_->setSingleStep(descriptor.step);
    maximum_->setSingleStep(descriptor.step);
    increment_->setSingleStep(descriptor.step);
    minimum_->setValue(value);
    maximum_->setValue(value);
    increment_->setValue(descriptor.step);
}

SweepDefinition SweepDialog::definition() const {
    return {
        SweepParameter(parameter_->currentData().toInt()),
        parameter_->currentText(),
        minimum_->value(), maximum_->value(), increment_->value()
    };
}
