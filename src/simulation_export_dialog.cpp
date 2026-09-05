#include "simulation_export_dialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QSpinBox* integerBox(int value, int low, int high, QWidget* parent) {
    auto* box = new QSpinBox(parent);
    box->setRange(low, high);
    box->setValue(value);
    return box;
}
QDoubleSpinBox* realBox(double value, double low, double high, QWidget* parent) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(low, high);
    box->setDecimals(6);
    box->setValue(value);
    return box;
}
void showColor(QPushButton* button, QColor color) {
    button->setProperty("chosenColor", color);
    button->setText(color.name(QColor::HexRgb));
    button->setStyleSheet(QString("background:%1").arg(color.name()));
}
}

SimulationExportDialog::SimulationExportDialog(
    double cameraX, double cameraY, double cameraZoom, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Export looping simulation frames");
    settings_.beginGroup("simulationExport");
    framesPerSpawn_ = integerBox(settings_.value("framesPerSpawn", 30).toInt(), 1, 10000, this);
    width_ = integerBox(settings_.value("width", 1920).toInt(), 1, 100000, this);
    height_ = integerBox(settings_.value("height", 1080).toInt(), 1, 100000, this);
    overridePeriod_ = new QCheckBox("Use period override", this);
    overridePeriod_->setChecked(settings_.value("override", false).toBool());
    period_ = integerBox(settings_.value("period", 1).toInt(), 1, 100000, this);
    centerX_ = realBox(settings_.value("centerX", cameraX).toDouble(), -1e9, 1e9, this);
    centerY_ = realBox(settings_.value("centerY", cameraY).toDouble(), -1e9, 1e9, this);
    zoom_ = realBox(settings_.value("zoom", cameraZoom).toDouble(), 1e-6, 1e6, this);
    segmentWidth_ = realBox(settings_.value("segmentWidth", 3.).toDouble(), .1, 1000, this);
    background_ = colorButton("background", QColor(18, 22, 30));
    balls_ = colorButton("balls", QColor(100, 190, 255));
    segments_ = colorButton("segments", QColor(230, 180, 80));
    antialias_ = new QCheckBox("Antialias shapes", this);
    antialias_->setChecked(settings_.value("antialias", true).toBool());
    settings_.endGroup();
    period_->setEnabled(overridePeriod_->isChecked());
    connect(overridePeriod_, &QCheckBox::toggled, period_, &QWidget::setEnabled);

    auto* form = new QFormLayout;
    form->addRow("Frames per spawning period", framesPerSpawn_);
    form->addRow("Image width", width_);
    form->addRow("Image height", height_);
    form->addRow(overridePeriod_, period_);
    form->addRow("Camera center X", centerX_);
    form->addRow("Camera center Y", centerY_);
    form->addRow("Pixels per world unit", zoom_);
    form->addRow("Segment width", segmentWidth_);
    form->addRow("Background color", background_);
    form->addRow("Ball color", balls_);
    form->addRow("Segment color", segments_);
    form->addRow(antialias_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SimulationExportDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    setMinimumWidth(430);
}

QPushButton* SimulationExportDialog::colorButton(const QString& key, const QColor& fallback) {
    auto* button = new QPushButton(this);
    QColor color(settings_.value(key, fallback).toString());
    if (!color.isValid()) color = fallback;
    showColor(button, color);
    connect(button, &QPushButton::clicked, this, [button, this] {
        const QColor chosen = QColorDialog::getColor(buttonColor(button), this);
        if (chosen.isValid()) showColor(button, chosen);
    });
    return button;
}

QColor SimulationExportDialog::buttonColor(QPushButton* button) const {
    return button->property("chosenColor").value<QColor>();
}

SimulationExportDefinition SimulationExportDialog::definition() const {
    return {framesPerSpawn_->value(), width_->value(), height_->value(),
            overridePeriod_->isChecked(), period_->value(), centerX_->value(),
            centerY_->value(), zoom_->value(), segmentWidth_->value(),
            buttonColor(background_), buttonColor(balls_), buttonColor(segments_),
            antialias_->isChecked()};
}

void SimulationExportDialog::accept() {
    const auto d = definition();
    settings_.beginGroup("simulationExport");
    settings_.setValue("framesPerSpawn", d.framesPerSpawn);
    settings_.setValue("width", d.width);
    settings_.setValue("height", d.height);
    settings_.setValue("override", d.overridePeriod);
    settings_.setValue("period", d.period);
    settings_.setValue("centerX", d.centerX);
    settings_.setValue("centerY", d.centerY);
    settings_.setValue("zoom", d.pixelsPerUnit);
    settings_.setValue("segmentWidth", d.segmentWidth);
    settings_.setValue("background", d.background.name());
    settings_.setValue("balls", d.balls.name());
    settings_.setValue("segments", d.segments.name());
    settings_.setValue("antialias", d.antialias);
    settings_.endGroup();
    settings_.sync();
    QDialog::accept();
}
