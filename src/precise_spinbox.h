#pragma once

#include "precise_decimal.h"

#include <QAbstractSpinBox>

class PreciseSpinBox : public QAbstractSpinBox {
    Q_OBJECT

public:
    explicit PreciseSpinBox(QWidget* parent = nullptr);

    void setRange(const PreciseDecimal& minimum, const PreciseDecimal& maximum);
    void setSingleStep(const PreciseDecimal& step);
    void setValue(const PreciseDecimal& value);
    void setValue(const QString& value);
    void setValue(double value);
    const PreciseDecimal& value() const;
    QString valueString() const;
    double valueDouble() const;
    const PreciseDecimal& minimum() const;
    const PreciseDecimal& maximum() const;

signals:
    void valueChanged(const QString& exactValue);

protected:
    void stepBy(int steps) override;
    StepEnabled stepEnabled() const override;
    QValidator::State validate(QString& text, int& position) const override;
    void fixup(QString& input) const override;

private:
    void commitText();
    void updateText();

    PreciseDecimal value_ = 0;
    PreciseDecimal minimum_ = -1000000;
    PreciseDecimal maximum_ = 1000000;
    PreciseDecimal step_ = 1;
};

