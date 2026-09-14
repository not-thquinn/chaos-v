#include "sweep_dialog.h"

#include <QDialogButtonBox>
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
    const QJsonObject object = document.object();
    if (object.value("schema").toString() != "chaos-v-simulation-1")
        return {};
    return object;
}

bool finiteNumber(const QJsonObject& object, const char* name) {
    return object.value(name).isDouble() &&
           std::isfinite(object.value(name).toDouble());
}

bool inRange(const QJsonObject& object, const char* name,
             double minimum, double maximum) {
    return finiteNumber(object, name) &&
           object.value(name).toDouble() >= minimum &&
           object.value(name).toDouble() <= maximum;
}

std::optional<Config> configFrom(const QJsonObject& object) {
    if (!inRange(object, "gravity", .01, 5000) ||
        !inRange(object, "ballRadius", .1, 100) ||
        !inRange(object, "restitution", .5, 100) ||
        !inRange(object, "segmentGap", 1, 1000) ||
        !inRange(object, "segmentLength", 1, 2000) ||
        !inRange(object, "spawnInterval", .01, 10) ||
        !inRange(object, "spawnY", -5000, 5000) ||
        !inRange(object, "cutoffY", 100, 5000))
        return {};

    Config config;
    config.gravity = object.value("gravity").toDouble();
    config.radius = object.value("ballRadius").toDouble();
    config.restitution = object.value("restitution").toDouble();
    config.gap = object.value("segmentGap").toDouble();
    config.segmentLength = object.value("segmentLength").toDouble();
    config.spawnX = -config.gap / 2;
    config.spawnY = object.value("spawnY").toDouble();
    config.spawnInterval = object.value("spawnInterval").toDouble();
    config.cutoffY = object.value("cutoffY").toDouble();

    const int maxBalls = object.value("maxLiveBalls").toInt(config.maxBalls);
    const int analysis =
        object.value("ballsToAnalyze").toInt(config.analysisBalls);
    const int budget =
        object.value("collisionBudget").toInt(config.collisionBudget);
    const int precision =
        object.value("precisionBits").toInt(config.precisionBits);
    if (maxBalls < 1 || maxBalls > 10000 || analysis < 4 ||
        analysis > 100000 || budget < 1 || budget > 1000000 ||
        precision < 32 || precision > 2048)
        return {};
    config.maxBalls = maxBalls;
    config.analysisBalls = analysis;
    config.collisionBudget = budget;
    config.precisionBits = precision;
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

double interpolate(double first, double last, double progress) {
    return first + (last - first) * progress;
}

} // namespace

qint64 sweepFrameCount(double frameRate, double duration) {
    if (!std::isfinite(frameRate) || !std::isfinite(duration) ||
        !(frameRate > 0) || !(duration > 0))
        return 0;
    const long double frames = std::round(
        static_cast<long double>(frameRate) * duration);
    if (frames > static_cast<long double>(
                     std::numeric_limits<qint64>::max()))
        return 0;
    return std::max<qint64>(2, qint64(frames));
}

Config interpolateSweepConfig(
    const SweepDefinition& definition, double progress) {
    progress = std::clamp(progress, 0., 1.);
    Config result = definition.startConfig;
    const Config& end = definition.endConfig;
    result.gravity = interpolate(result.gravity, end.gravity, progress);
    result.radius = interpolate(result.radius, end.radius, progress);
    result.restitution =
        interpolate(result.restitution, end.restitution, progress);
    result.gap = interpolate(result.gap, end.gap, progress);
    result.segmentLength =
        interpolate(result.segmentLength, end.segmentLength, progress);
    result.spawnY = interpolate(result.spawnY, end.spawnY, progress);
    result.spawnInterval =
        interpolate(result.spawnInterval, end.spawnInterval, progress);
    result.cutoffY = interpolate(result.cutoffY, end.cutoffY, progress);
    result.spawnX = -result.gap / 2;
    return result;
}

SweepDialog::SweepDialog(const QString& currentJson, int initialWidth,
                         int initialHeight, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Generate parameter sweep");

    startJson_ = new QPlainTextEdit;
    endJson_ = new QPlainTextEdit;
    startJson_->setPlaceholderText("Paste starting simulation JSON here");
    endJson_->setPlaceholderText("Paste ending simulation JSON here");
    const QString savedStart = settings_.value("sweep/startJson").toString();
    const QString savedEnd = settings_.value("sweep/endJson").toString();
    startJson_->setPlainText(savedStart.isEmpty() ? currentJson : savedStart);
    endJson_->setPlainText(savedEnd.isEmpty() ? currentJson : savedEnd);

    auto* startGroup = new QGroupBox("Starting configuration JSON");
    auto* startLayout = new QVBoxLayout(startGroup);
    startLayout->addWidget(startJson_);
    auto* endGroup = new QGroupBox("Ending configuration JSON");
    auto* endLayout = new QVBoxLayout(endGroup);
    endLayout->addWidget(endJson_);

    frameRate_ = new QDoubleSpinBox;
    frameRate_->setRange(.01, 1000);
    frameRate_->setDecimals(3);
    frameRate_->setValue(settings_.value("sweep/frameRate", 30).toDouble());
    duration_ = new QDoubleSpinBox;
    duration_->setRange(.001, 86400);
    duration_->setDecimals(3);
    duration_->setSuffix(" s");
    duration_->setValue(settings_.value("sweep/duration", 10).toDouble());
    width_ = new QSpinBox;
    height_ = new QSpinBox;
    width_->setRange(1, std::numeric_limits<int>::max());
    height_->setRange(1, std::numeric_limits<int>::max());
    width_->setValue(settings_.value("sweep/width", initialWidth).toInt());
    height_->setValue(settings_.value("sweep/height", initialHeight).toInt());
    frameCount_ = new QLabel;

    auto* form = new QFormLayout;
    form->addRow("Frame rate", frameRate_);
    form->addRow("Total time", duration_);
    form->addRow("Output width", width_);
    form->addRow("Output height", height_);
    form->addRow("Frames", frameCount_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &SweepDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    for (auto* spin : {frameRate_, duration_})
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this] { updateFrameCount(); });
    connect(width_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this] { updateFrameCount(); });
    connect(height_, qOverload<int>(&QSpinBox::valueChanged),
            this, [this] { updateFrameCount(); });

    auto* jsonLayout = new QHBoxLayout;
    jsonLayout->addWidget(startGroup);
    jsonLayout->addWidget(endGroup);
    auto* explanation = new QLabel(
        "Gravity, ball radius, restitution, segment gap and length, spawn "
        "interval and height, and cutoff height are interpolated linearly. "
        "Precision and stopping limits come from the starting JSON. The "
        "selected angles, playback settings, JSON axis ranges, and JSON "
        "image sizes are ignored; every frame uses the current fractal view.");
    explanation->setWordWrap(true);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(jsonLayout, 1);
    layout->addWidget(explanation);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(860, 570);
    updateFrameCount();
}

SweepDialog::~SweepDialog() {
    saveSettings();
}

const SweepDefinition& SweepDialog::definition() const {
    return *definition_;
}

void SweepDialog::updateFrameCount() {
    const qint64 count = sweepFrameCount(
        frameRate_->value(), duration_->value());
    boost::multiprecision::cpp_int pixels = count;
    pixels *= width_->value();
    pixels *= height_->value();
    frameCount_->setText(
        count > 0
            ? QString("%1   (%2 pixels)")
                  .arg(QString::number(count), groupedInteger(pixels))
            : QString("Invalid"));
}

void SweepDialog::saveSettings() {
    settings_.setValue("sweep/startJson", startJson_->toPlainText());
    settings_.setValue("sweep/endJson", endJson_->toPlainText());
    settings_.setValue("sweep/frameRate", frameRate_->value());
    settings_.setValue("sweep/duration", duration_->value());
    settings_.setValue("sweep/width", width_->value());
    settings_.setValue("sweep/height", height_->value());
    settings_.sync();
}

void SweepDialog::accept() {
    const auto startObject = parseObject(startJson_->toPlainText());
    const auto endObject = parseObject(endJson_->toPlainText());
    if (!startObject || !endObject) {
        QMessageBox::warning(
            this, "Invalid JSON",
            "Both fields must contain complete Chaos V simulation JSON objects.");
        return;
    }
    const auto start = configFrom(*startObject);
    const auto end = configFrom(*endObject);
    if (!start || !end) {
        QMessageBox::warning(
            this, "Invalid configuration",
            "A configuration is missing a physics value or contains a value "
            "outside the supported range.");
        return;
    }
    const qint64 count = sweepFrameCount(
        frameRate_->value(), duration_->value());
    if (count <= 0) {
        QMessageBox::warning(
            this, "Invalid timing", "Frame rate and total time must be positive.");
        return;
    }
    definition_ = SweepDefinition{
        *start, *end, frameRate_->value(), duration_->value(), count,
        width_->value(), height_->value()};
    saveSettings();
    QDialog::accept();
}
