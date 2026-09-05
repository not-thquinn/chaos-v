#include "precise_spinbox.h"

#include <QLineEdit>

#include <algorithm>

PreciseSpinBox::PreciseSpinBox(QWidget* parent) : QAbstractSpinBox(parent) {
    setKeyboardTracking(false);
    connect(this, &QAbstractSpinBox::editingFinished,
            this, &PreciseSpinBox::commitText);
    updateText();
}

void PreciseSpinBox::setRange(
    const PreciseDecimal& minimum, const PreciseDecimal& maximum) {
    minimum_ = std::min(minimum, maximum);
    maximum_ = std::max(minimum, maximum);
    setValue(value_);
}

void PreciseSpinBox::setSingleStep(const PreciseDecimal& step) {
    if (step > 0)
        step_ = step;
}

void PreciseSpinBox::setValue(const PreciseDecimal& value) {
    const PreciseDecimal bounded = std::max(minimum_, std::min(maximum_, value));
    if (bounded == value_) {
        updateText();
        return;
    }
    value_ = bounded;
    updateText();
    emit valueChanged(valueString());
}

void PreciseSpinBox::setValue(const QString& value) {
    try {
        setValue(preciseDecimal(value));
    } catch (...) {
        updateText();
    }
}

void PreciseSpinBox::setValue(double value) {
    setValue(preciseDecimal(value));
}

const PreciseDecimal& PreciseSpinBox::value() const {
    return value_;
}

QString PreciseSpinBox::valueString() const {
    return preciseString(value_);
}

double PreciseSpinBox::valueDouble() const {
    return preciseDouble(value_);
}

const PreciseDecimal& PreciseSpinBox::minimum() const {
    return minimum_;
}

const PreciseDecimal& PreciseSpinBox::maximum() const {
    return maximum_;
}

void PreciseSpinBox::stepBy(int steps) {
    setValue(value_ + step_ * steps);
}

QAbstractSpinBox::StepEnabled PreciseSpinBox::stepEnabled() const {
    StepEnabled enabled = StepNone;
    if (value_ > minimum_)
        enabled |= StepDownEnabled;
    if (value_ < maximum_)
        enabled |= StepUpEnabled;
    return enabled;
}

QValidator::State PreciseSpinBox::validate(QString& text, int&) const {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed == "-" || trimmed == "+" ||
        trimmed.endsWith('e', Qt::CaseInsensitive) ||
        trimmed.endsWith("e-") || trimmed.endsWith("e+"))
        return QValidator::Intermediate;
    try {
        const PreciseDecimal parsed = preciseDecimal(trimmed);
        return parsed >= minimum_ && parsed <= maximum_
                   ? QValidator::Acceptable
                   : QValidator::Invalid;
    } catch (...) {
        return QValidator::Invalid;
    }
}

void PreciseSpinBox::fixup(QString& input) const {
    input = preciseDisplayString(value_);
}

void PreciseSpinBox::commitText() {
    setValue(lineEdit()->text());
}

void PreciseSpinBox::updateText() {
    lineEdit()->setText(preciseDisplayString(value_));
}

