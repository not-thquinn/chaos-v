#include "zoom_dialog.h"
#include "zoom_math.h"

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

std::optional<QJsonObject> parseObject(const QString& text) {
    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(text.trimmed().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {};
    return document.object();
}

bool sameNumber(
    const QJsonObject& first, const QJsonObject& second, const char* name) {
    return first.contains(name) && second.contains(name) &&
           first.value(name).isDouble() && second.value(name).isDouble() &&
           first.value(name).toDouble() == second.value(name).toDouble();
}

bool sameSimulation(const QJsonObject& first, const QJsonObject& second) {
    if (first.value("schema").toString() != "chaos-v-simulation-1" ||
        second.value("schema").toString() != "chaos-v-simulation-1")
        return false;
    for (const char* name : {
             "modelVersion", "gravity", "ballRadius", "restitution",
             "segmentGap", "segmentLength", "spawnInterval", "cutoffY",
             "spawnY", "segmentCenterY"}) {
        if (!sameNumber(first, second, name))
            return false;
    }
    return true;
}

QString exactOrNumber(
    const QJsonObject& object, const char* exactName, const char* numberName) {
    const QString exact = object.value(exactName).toString();
    if (!exact.isEmpty())
        return exact;
    if (!object.value(numberName).isDouble())
        return {};
    return QString::number(object.value(numberName).toDouble(), 'g', 17);
}

std::optional<std::array<PreciseDecimal, 4>> fittedCamera(
    const QJsonObject& object, int outputWidth, int outputHeight) {
    try {
        const PreciseDecimal x0 = preciseDecimal(
            exactOrNumber(object, "axisXMinExact", "axisXMin"));
        const PreciseDecimal x1 = preciseDecimal(
            exactOrNumber(object, "axisXMaxExact", "axisXMax"));
        const PreciseDecimal y0 = preciseDecimal(
            exactOrNumber(object, "axisYMinExact", "axisYMin"));
        const PreciseDecimal y1 = preciseDecimal(
            exactOrNumber(object, "axisYMaxExact", "axisYMax"));
        const int sourceWidth = object.value("fractalWidth").toInt();
        const int sourceHeight = object.value("fractalHeight").toInt();
        if (x0 == x1 || y0 == y1 || sourceWidth <= 0 || sourceHeight <= 0 ||
            outputWidth <= 0 || outputHeight <= 0)
            return {};

        return fitZoomCamera(
            {x0, x1, y0, y1}, sourceWidth, sourceHeight,
            outputWidth, outputHeight);
    } catch (...) {
        return {};
    }
}

Config configFrom(const QJsonObject& object) {
    Config config;
    config.gravity = object.value("gravity").toDouble(config.gravity);
    config.radius = object.value("ballRadius").toDouble(config.radius);
    config.restitution =
        object.value("restitution").toDouble(config.restitution);
    config.gap = object.value("segmentGap").toDouble(config.gap);
    config.segmentLength =
        object.value("segmentLength").toDouble(config.segmentLength);
    config.spawnX = -config.gap / 2;
    config.spawnY = object.value("spawnY").toDouble(config.spawnY);
    config.spawnInterval =
        object.value("spawnInterval").toDouble(config.spawnInterval);
    config.cutoffY = object.value("cutoffY").toDouble(config.cutoffY);
    config.maxBalls =
        object.value("maxLiveBalls").toInt(config.maxBalls);
    config.analysisBalls =
        object.value("ballsToAnalyze").toInt(config.analysisBalls);
    config.collisionBudget =
        object.value("collisionBudget").toInt(config.collisionBudget);
    config.precisionBits =
        object.value("precisionBits").toInt(config.precisionBits);
    return config;
}

QString groupedInteger(const boost::multiprecision::cpp_int& value) {
    const std::string plain = value.convert_to<std::string>();
    QString grouped;
    grouped.reserve(int(plain.size() + plain.size() / 3));
    for (size_t index = 0; index < plain.size(); ++index) {
        if (index && (plain.size() - index) % 3 == 0)
            grouped += ',';
        grouped += QChar::fromLatin1(plain[index]);
    }
    return grouped;
}

} // namespace

ZoomDialog::ZoomDialog(
    int initialWidth, int initialHeight, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Generate fractal zoom");

    startJson_ = new QPlainTextEdit;
    endJson_ = new QPlainTextEdit;
    startJson_->setPlaceholderText("Paste starting simulation JSON here");
    endJson_->setPlaceholderText("Paste ending simulation JSON here");

    auto* startGroup = new QGroupBox("Starting camera JSON");
    auto* startLayout = new QVBoxLayout(startGroup);
    startLayout->addWidget(startJson_);
    auto* endGroup = new QGroupBox("Ending camera JSON");
    auto* endLayout = new QVBoxLayout(endGroup);
    endLayout->addWidget(endJson_);

    frameRate_ = new QDoubleSpinBox;
    frameRate_->setRange(.01, 1000);
    frameRate_->setDecimals(3);
    frameRate_->setValue(30);
    duration_ = new QDoubleSpinBox;
    duration_->setRange(.001, 86400);
    duration_->setDecimals(3);
    duration_->setValue(10);
    duration_->setSuffix(" s");
    dampingTime_ = new QDoubleSpinBox;
    dampingTime_->setRange(0, 43200);
    dampingTime_->setDecimals(3);
    dampingTime_->setValue(1);
    dampingTime_->setSuffix(" s");
    panTime_ = new QDoubleSpinBox;
    panTime_->setRange(0, 86400);
    panTime_->setDecimals(3);
    panTime_->setValue(2);
    panTime_->setSuffix(" s");
    panDampingTime_ = new QDoubleSpinBox;
    panDampingTime_->setRange(0, 43200);
    panDampingTime_->setDecimals(3);
    panDampingTime_->setValue(.5);
    panDampingTime_->setSuffix(" s");
    adaptivePrecision_ = new QCheckBox(
        "Automatically increase precision for deep zooms");
    adaptivePrecision_->setToolTip(
        "Overrides the starting JSON precision. Begins at 64 bits and "
        "increases in 64-bit steps before coordinate resolution becomes visible.");

    width_ = new QSpinBox;
    height_ = new QSpinBox;
    width_->setRange(1, std::numeric_limits<int>::max());
    height_->setRange(1, std::numeric_limits<int>::max());
    startJson_->setPlainText(settings_.value("zoom/startJson").toString());
    endJson_->setPlainText(settings_.value("zoom/endJson").toString());
    frameRate_->setValue(settings_.value("zoom/frameRate", 30).toDouble());
    duration_->setValue(settings_.value("zoom/duration", 10).toDouble());
    dampingTime_->setValue(settings_.value("zoom/dampingTime", 1).toDouble());
    panTime_->setValue(settings_.value("zoom/panTime", 2).toDouble());
    panDampingTime_->setValue(
        settings_.value("zoom/panDampingTime", .5).toDouble());
    adaptivePrecision_->setChecked(
        settings_.value("zoom/adaptivePrecision", true).toBool());
    width_->setValue(settings_.value("zoom/width", initialWidth).toInt());
    height_->setValue(settings_.value("zoom/height", initialHeight).toInt());
    frameCount_ = new QLabel;

    auto* form = new QFormLayout;
    form->addRow("Frame rate", frameRate_);
    form->addRow("Total time", duration_);
    form->addRow("Zoom damping time (each end)", dampingTime_);
    form->addRow("Pan time", panTime_);
    form->addRow("Pan damping time (each end)", panDampingTime_);
    form->addRow(adaptivePrecision_);
    form->addRow("Output width", width_);
    form->addRow("Output height", height_);
    form->addRow("Frames", frameCount_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &ZoomDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(frameRate_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this] { updateLimitsAndCount(); });
    connect(duration_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this] { updateLimitsAndCount(); });
    connect(panTime_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this] { updateLimitsAndCount(); });
    connect(width_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this] { updateLimitsAndCount(); });
    connect(height_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this] { updateLimitsAndCount(); });

    auto* jsonLayout = new QHBoxLayout;
    jsonLayout->addWidget(startGroup);
    jsonLayout->addWidget(endGroup);
    auto* explanation = new QLabel(
        "Physics settings must match. Precision and stopping limits are taken "
        "from the starting JSON. Output resolution overrides both JSON "
        "resolutions; nonsquare output is fitted centrally inside each camera.");
    explanation->setWordWrap(true);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(jsonLayout, 1);
    layout->addWidget(explanation);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(860, 620);
    updateLimitsAndCount();
}

ZoomDialog::~ZoomDialog() {
    saveSettings();
}

const ZoomDefinition& ZoomDialog::definition() const {
    return *definition_;
}

void ZoomDialog::updateLimitsAndCount() {
    dampingTime_->setRange(0, duration_->value() / 2);
    panTime_->setRange(0, duration_->value());
    panDampingTime_->setRange(0, panTime_->value() / 2);
    const long double frames = std::round(
        static_cast<long double>(frameRate_->value()) * duration_->value());
    const qint64 count = qint64(std::clamp<long double>(
        frames, 2, std::numeric_limits<qint64>::max()));
    boost::multiprecision::cpp_int pixels = count;
    pixels *= width_->value();
    pixels *= height_->value();
    frameCount_->setText(
        QString("%1   (%2 pixels)")
            .arg(QString::number(count), groupedInteger(pixels)));
}

void ZoomDialog::saveSettings() {
    settings_.setValue("zoom/startJson", startJson_->toPlainText());
    settings_.setValue("zoom/endJson", endJson_->toPlainText());
    settings_.setValue("zoom/frameRate", frameRate_->value());
    settings_.setValue("zoom/duration", duration_->value());
    settings_.setValue("zoom/dampingTime", dampingTime_->value());
    settings_.setValue("zoom/panTime", panTime_->value());
    settings_.setValue("zoom/panDampingTime", panDampingTime_->value());
    settings_.setValue(
        "zoom/adaptivePrecision", adaptivePrecision_->isChecked());
    settings_.setValue("zoom/width", width_->value());
    settings_.setValue("zoom/height", height_->value());
    settings_.sync();
}

void ZoomDialog::accept() {
    const auto start = parseObject(startJson_->toPlainText());
    const auto end = parseObject(endJson_->toPlainText());
    if (!start || !end) {
        QMessageBox::warning(
            this, "Invalid JSON",
            "Both camera fields must contain complete simulation JSON objects.");
        return;
    }
    if (!sameSimulation(*start, *end)) {
        QMessageBox::warning(
            this, "Configurations do not match",
            "The two JSON objects must have identical simulation-determining settings.");
        return;
    }

    const auto startCamera = fittedCamera(*start, width_->value(), height_->value());
    const auto endCamera = fittedCamera(*end, width_->value(), height_->value());
    if (!startCamera || !endCamera) {
        QMessageBox::warning(
            this, "Invalid camera",
            "Each JSON object must contain nonzero axis ranges and a valid fractal resolution.");
        return;
    }
    if (((*startCamera)[1] > (*startCamera)[0]) !=
            ((*endCamera)[1] > (*endCamera)[0]) ||
        ((*startCamera)[3] > (*startCamera)[2]) !=
            ((*endCamera)[3] > (*endCamera)[2])) {
        QMessageBox::warning(
            this, "Incompatible axes",
            "Starting and ending cameras must use the same axis directions.");
        return;
    }

    const qint64 frames = std::max<qint64>(
        2, qint64(std::llround(frameRate_->value() * duration_->value())));
    definition_ = ZoomDefinition{
        configFrom(*start), *startCamera, *endCamera,
        frameRate_->value(), duration_->value(), dampingTime_->value(),
        panTime_->value(), panDampingTime_->value(),
        adaptivePrecision_->isChecked(), frames,
        width_->value(), height_->value()
    };
    QDialog::accept();
}
