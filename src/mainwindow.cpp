#include "mainwindow.h"

#include "fractal_classifier.h"
#include "fractal_shading.h"
#include "precise_spinbox.h"
#include "render_schedule.h"
#include "simulation_export_dialog.h"
#include "views.h"
#include "zoom_math.h"

#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRunnable>
#include <QSaveFile>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
QJsonObject bulkConfigJson(const Config& c) {
    QJsonObject o;
    o["left"] = c.leftDeg; o["right"] = c.rightDeg;
    o["leftExact"] = QString::fromStdString(c.leftDegExact);
    o["rightExact"] = QString::fromStdString(c.rightDegExact);
    o["gravity"] = c.gravity; o["radius"] = c.radius;
    o["restitution"] = c.restitution; o["gap"] = c.gap;
    o["length"] = c.segmentLength; o["spawnX"] = c.spawnX;
    o["spawnY"] = c.spawnY; o["interval"] = c.spawnInterval;
    o["cutoff"] = c.cutoffY; o["maxBalls"] = c.maxBalls;
    o["analysis"] = c.analysisBalls; o["budget"] = c.collisionBudget;
    o["bits"] = c.precisionBits;
    return o;
}
Config bulkConfig(const QJsonObject& o) {
    Config c;
    c.leftDeg = o["left"].toDouble(); c.rightDeg = o["right"].toDouble();
    c.leftDegExact = o["leftExact"].toString().toStdString();
    c.rightDegExact = o["rightExact"].toString().toStdString();
    c.gravity = o["gravity"].toDouble(); c.radius = o["radius"].toDouble();
    c.restitution = o["restitution"].toDouble(); c.gap = o["gap"].toDouble();
    c.segmentLength = o["length"].toDouble(); c.spawnX = o["spawnX"].toDouble();
    c.spawnY = o["spawnY"].toDouble(); c.spawnInterval = o["interval"].toDouble();
    c.cutoffY = o["cutoff"].toDouble(); c.maxBalls = o["maxBalls"].toInt();
    c.analysisBalls = o["analysis"].toInt(); c.collisionBudget = o["budget"].toInt();
    c.precisionBits = o["bits"].toInt();
    return c;
}
QJsonArray axesJson(const std::array<PreciseDecimal, 4>& axes) {
    QJsonArray a;
    for (const auto& value : axes) a.append(preciseString(value));
    return a;
}
std::array<PreciseDecimal, 4> axesFromJson(const QJsonArray& a) {
    return {preciseDecimal(a[0].toString()), preciseDecimal(a[1].toString()),
            preciseDecimal(a[2].toString()), preciseDecimal(a[3].toString())};
}
QImage cropFractal(const QImage& source,
                   const std::array<PreciseDecimal, 4>& sourceAxes,
                   const std::array<PreciseDecimal, 4>& targetAxes,
                   int width, int height) {
    const double x0 = preciseDouble((targetAxes[0] - sourceAxes[0]) /
                                    (sourceAxes[1] - sourceAxes[0])) * source.width();
    const double x1 = preciseDouble((targetAxes[1] - sourceAxes[0]) /
                                    (sourceAxes[1] - sourceAxes[0])) * source.width();
    const double y0 = preciseDouble((sourceAxes[3] - targetAxes[3]) /
                                    (sourceAxes[3] - sourceAxes[2])) * source.height();
    const double y1 = preciseDouble((sourceAxes[3] - targetAxes[2]) /
                                    (sourceAxes[3] - sourceAxes[2])) * source.height();
    QImage output(width, height, QImage::Format_RGB32);
    output.fill(Qt::black);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(0, 0, width, height), source,
                      QRectF(x0, y0, x1 - x0, y1 - y0));
    return output;
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    renderPool_.setMaxThreadCount(std::max(1, QThread::idealThreadCount() - 1));
    previewPool_.setMaxThreadCount(1);
    buildUi();
    restoreSettings();

    // Launch consistently at the center of the default angle space while
    // retaining all other saved simulation settings.
    selectedAngles_ = {{PreciseDecimal(45), PreciseDecimal(-45)}};
    parameterView_->setSelection(45, -45);
    {
        QSignalBlocker leftBlock(leftAngle_);
        QSignalBlocker rightBlock(rightAngle_);
        leftAngle_->setValue(45);
        rightAngle_->setValue(-45);
    }

    parameterView_->setLayerKey(groundTruthKey());
    loadFractals();
    restorePausedBulkRender();
    updateAxes();
    simulationView_->setPlaybackSpeed(playbackSpeed_->value());
    simulationView_->setCameraState(
        settings_.value("cameraX", 0.).toDouble(),
        settings_.value("cameraY", 220.).toDouble(),
        settings_.value("cameraZoom", 1.).toDouble());

    QTimer::singleShot(0, this, &MainWindow::scheduleSimulation);
    if (settings_.contains("windowGeometry"))
        restoreGeometry(settings_.value("windowGeometry").toByteArray());
    if (settings_.contains("splitterState"))
        splitter_->restoreState(settings_.value("splitterState").toByteArray());
}

QDoubleSpinBox* MainWindow::doubleSpin(
    double value, double low, double high, int decimals) {
    auto* spin = new QDoubleSpinBox;
    spin->setRange(low, high);
    spin->setDecimals(decimals);
    spin->setValue(value);
    return spin;
}

PreciseSpinBox* MainWindow::preciseSpin(
    const char* value, const char* low, const char* high,
    const char* step) {
    auto* spin = new PreciseSpinBox;
    spin->setRange(PreciseDecimal(low), PreciseDecimal(high));
    spin->setSingleStep(PreciseDecimal(step));
    spin->setValue(QString::fromLatin1(value));
    return spin;
}

QSpinBox* MainWindow::integerSpin(int value, int low, int high) {
    auto* spin = new QSpinBox;
    spin->setRange(low, high);
    spin->setValue(value);
    return spin;
}

void MainWindow::buildUi() {
    splitter_ = new QSplitter;
    parameterView_ = new ParameterView;
    simulationView_ = new SimulationView;

    auto* controlsBody = new QWidget;
    auto* form = new QFormLayout(controlsBody);
    auto* controls = new QScrollArea;
    controls->setWidgetResizable(true);
    controls->setWidget(controlsBody);

    leftAngle_ = preciseSpin("45", "-180", "180", "0.001");
    rightAngle_ = preciseSpin("-45", "-180", "180", "0.001");
    gravity_ = doubleSpin(400, .01, 5000);
    radius_ = doubleSpin(8, .1, 100);
    restitution_ = doubleSpin(.92, .5, 100);
    gap_ = doubleSpin(115, 1, 1000);
    segmentLength_ = doubleSpin(240, 1, 2000);
    spawnInterval_ = doubleSpin(.36, .01, 10);
    spawnY_ = doubleSpin(-230, -5000, 5000);
    cutoff_ = doubleSpin(560, 100, 5000);
    playbackSpeed_ = doubleSpin(1, .05, 20, 2);
    playbackSpeed_->setSuffix("×");
    simulationView_->setPlaybackControl(playbackSpeed_);
    xmin_ = preciseSpin("0", "-1440", "1440", "0.001");
    xmax_ = preciseSpin("90", "-1440", "1440", "0.001");
    ymin_ = preciseSpin("-90", "-1440", "1440", "0.001");
    ymax_ = preciseSpin("0", "-1440", "1440", "0.001");
    fractalWidth_ = integerSpin(
        360, 16, std::numeric_limits<int>::max());
    fractalHeight_ = integerSpin(
        360, 16, std::numeric_limits<int>::max());
    maxBalls_ = integerSpin(48, 1, 10000);
    analysisBalls_ = integerSpin(96, 4, 100000);
    collisionBudget_ = integerSpin(1000, 1, 1000000);
    precisionBits_ = integerSpin(160, 32, 2048);
    certificateShading_ = new QCheckBox("Causal period-stability shading");
    certificateShading_->setChecked(true);
    certificateShadingStrength_ = doubleSpin(.7, 0, 1, 3);
    certificateShadingScale_ = doubleSpin(5, .000001, 1000, 6);
    certificateShadingStrength_->setToolTip(
        "How strongly period fragility changes the period-color lightness.");
    certificateShadingScale_->setToolTip(
        "Normalized hit/miss margin mapped to the middle of the tonal range.");
    connect(certificateShading_, &QCheckBox::toggled,
            certificateShadingStrength_, &QWidget::setEnabled);
    connect(certificateShading_, &QCheckBox::toggled,
            certificateShadingScale_, &QWidget::setEnabled);

    form->addRow("Left angle", leftAngle_);
    form->addRow("Right angle", rightAngle_);
    form->addRow("Gravity", gravity_);
    form->addRow("Ball radius", radius_);
    form->addRow("Restitution (min. 0.50)", restitution_);
    form->addRow("Segment gap", gap_);
    form->addRow("Segment length", segmentLength_);
    form->addRow("Spawn interval", spawnInterval_);
    form->addRow("Spawn y", spawnY_);
    form->addRow("Cutoff y", cutoff_);
    form->addRow("X axis min", xmin_);
    form->addRow("X axis max", xmax_);
    form->addRow("Y axis min", ymin_);
    form->addRow("Y axis max", ymax_);
    form->addRow("Fractal width", fractalWidth_);
    form->addRow("Fractal height", fractalHeight_);
    form->addRow("Max live balls", maxBalls_);
    form->addRow("Balls to analyze", analysisBalls_);
    form->addRow("Collision budget", collisionBudget_);
    precisionBits_->setToolTip(
        "Fractal renders at 32–64 bits use the native collision-topology "
        "engine; higher settings use fixed or variable MPFR.");
    form->addRow("Precision (bits)", precisionBits_);
    form->addRow(certificateShading_);
    form->addRow("Stability shading strength", certificateShadingStrength_);
    form->addRow("Period-margin scale", certificateShadingScale_);

    renderButton_ = new QPushButton("Generate fractal");
    sweepButton_ = new QPushButton("Generate parameter sweep…");
    zoomButton_ = new QPushButton("Generate fractal zoom…");
    cancelRenderButton_ = new QPushButton("Cancel rendering");
    cancelRenderButton_->setEnabled(false);
    pauseRenderButton_ = new QPushButton("Pause bulk render");
    pauseRenderButton_->setEnabled(false);
    resumeRenderButton_ = new QPushButton("Resume paused render");
    resumeRenderButton_->setEnabled(false);
    auto* exportButton = new QPushButton("Export PNG");
    auto* exportSimulationButton = new QPushButton("Export simulation loop…");
    auto* copyButton = new QPushButton("Copy simulation JSON");
    pasteButton_ = new QPushButton("Paste simulation JSON");
    renderStatus_ = new QLabel("No fractal is rendering.");
    simulationStatus_ = new QLabel("Preparing selected simulation…");

    form->addRow(renderButton_);
    form->addRow(sweepButton_);
    form->addRow(zoomButton_);
    form->addRow(cancelRenderButton_);
    form->addRow(pauseRenderButton_);
    form->addRow(resumeRenderButton_);
    form->addRow(exportButton);
    form->addRow(exportSimulationButton);
    form->addRow(copyButton);
    form->addRow(pasteButton_);
    form->addRow("Render status", renderStatus_);
    form->addRow("Simulation status", simulationStatus_);

    splitter_->addWidget(parameterView_);
    splitter_->addWidget(simulationView_);
    splitter_->addWidget(controls);
    splitter_->setSizes({500, 500, 260});
    setCentralWidget(splitter_);
    resize(1280, 700);
    setWindowTitle("Chaos V — event-driven billiards");

    previewDelay_.setSingleShot(true);
    connect(&previewDelay_, &QTimer::timeout,
            this, &MainWindow::runSelectedSimulation);

    lockedControls_ = {
        leftAngle_, rightAngle_, gravity_, radius_, restitution_, gap_,
        segmentLength_, spawnInterval_, spawnY_, cutoff_, xmin_, xmax_, ymin_, ymax_,
        fractalWidth_, fractalHeight_, maxBalls_, analysisBalls_,
        collisionBudget_, precisionBits_, renderButton_, sweepButton_,
        zoomButton_, pasteButton_, certificateShading_,
        certificateShadingStrength_, certificateShadingScale_
    };

    connect(parameterView_, &ParameterView::chosen,
            this, [this](const QString& leftText, const QString& rightText) {
        pixelProbe_.reset();
        const PreciseDecimal left = preciseDecimal(leftText);
        const PreciseDecimal right = preciseDecimal(rightText);
        selectedAngles_ = {{left, right}};
        if (!rendering_) {
            QSignalBlocker leftBlock(leftAngle_);
            QSignalBlocker rightBlock(rightAngle_);
            leftAngle_->setValue(left);
            rightAngle_->setValue(right);
        }
        scheduleSimulation();
    });
    connect(parameterView_, &ParameterView::pixelChosen, this,
            [this](const QString& leftText, const QString& rightText,
                   int expectedPeriod, int precisionBits, int analysisBalls,
                   int maxBalls, int collisionBudget) {
        const PreciseDecimal left = preciseDecimal(leftText);
        const PreciseDecimal right = preciseDecimal(rightText);
        selectedAngles_ = {{left, right}};
        pixelProbe_ = PixelProbeContext{expectedPeriod, precisionBits,
                                        analysisBalls, maxBalls,
                                        collisionBudget};
        scheduleSimulation();
    });
    connect(parameterView_, &ParameterView::zoomRequested,
            this, [this](const QString& leftText, const QString& rightText,
                         const QString& bottomText, const QString& topText) {
        const PreciseDecimal left = preciseDecimal(leftText);
        const PreciseDecimal right = preciseDecimal(rightText);
        const PreciseDecimal bottom = preciseDecimal(bottomText);
        const PreciseDecimal top = preciseDecimal(topText);
        parameterView_->setAxes(left, right, bottom, top);
        if (!rendering_) {
            xmin_->setValue(left);
            xmax_->setValue(right);
            ymin_->setValue(bottom);
            ymax_->setValue(top);
        }
    });
    connect(renderButton_, &QPushButton::clicked,
            this, &MainWindow::renderFractal);
    connect(sweepButton_, &QPushButton::clicked,
            this, &MainWindow::showSweepDialog);
    connect(zoomButton_, &QPushButton::clicked,
            this, &MainWindow::showZoomDialog);
    connect(cancelRenderButton_, &QPushButton::clicked, this, [this] {
        if (!cancel_)
            return;
        cancel_->store(true);
        renderStatus_->setText("Cancelling render…");
        cancelRenderButton_->setEnabled(false);
    });
    connect(pauseRenderButton_, &QPushButton::clicked,
            this, &MainWindow::pauseBulkRender);
    connect(resumeRenderButton_, &QPushButton::clicked,
            this, &MainWindow::resumeBulkRender);
    connect(exportButton, &QPushButton::clicked,
            this, &MainWindow::exportPng);
    connect(exportSimulationButton, &QPushButton::clicked,
            this, &MainWindow::exportSimulationLoop);
    connect(copyButton, &QPushButton::clicked,
            this, &MainWindow::copySimulationJson);
    connect(pasteButton_, &QPushButton::clicked,
            this, &MainWindow::pasteSimulationJson);
    connect(playbackSpeed_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            simulationView_, &SimulationView::setPlaybackSpeed);

    for (auto* box : {gravity_, radius_, restitution_, gap_, segmentLength_,
                      spawnInterval_, spawnY_, cutoff_}) {
        connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double) {
            if (applyingJson_)
                return;
            parameterView_->setLayerKey(groundTruthKey());
            scheduleSimulation();
        });
    }

    for (auto* box : {leftAngle_, rightAngle_}) {
        connect(box, &PreciseSpinBox::valueChanged,
                this, [this](const QString&) {
            if (applyingJson_)
                return;
            selectedAngles_ = {{leftAngle_->value(), rightAngle_->value()}};
            parameterView_->setSelection(
                leftAngle_->value(), rightAngle_->value());
            scheduleSimulation();
        });
    }

    for (auto* box : {maxBalls_, analysisBalls_, collisionBudget_,
                      precisionBits_}) {
        connect(box, qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int) {
            if (!applyingJson_)
                scheduleSimulation();
        });
    }

    for (auto* box : {xmin_, xmax_, ymin_, ymax_}) {
        connect(box, &PreciseSpinBox::valueChanged,
                this, [this](const QString&) {
            if (!applyingJson_)
                updateAxes();
        });
    }
}

Config MainWindow::config() const {
    Config result;
    result.leftDeg = leftAngle_->valueDouble();
    result.rightDeg = rightAngle_->valueDouble();
    result.leftDegExact = leftAngle_->valueString().toStdString();
    result.rightDegExact = rightAngle_->valueString().toStdString();
    result.gravity = gravity_->value();
    result.radius = radius_->value();
    result.restitution = std::max(.5, restitution_->value());
    result.gap = gap_->value();
    result.segmentLength = segmentLength_->value();
    result.spawnX = -result.gap / 2;
    result.spawnY = spawnY_->value();
    result.spawnInterval = spawnInterval_->value();
    result.cutoffY = cutoff_->value();
    result.maxBalls = maxBalls_->value();
    result.analysisBalls = analysisBalls_->value();
    result.collisionBudget = collisionBudget_->value();
    result.precisionBits = precisionBits_->value();
    return result;
}

void MainWindow::scheduleSimulation() {
    if (previewCancel_)
        previewCancel_->store(true);
    if (parameterView_->hasSelection())
        previewDelay_.start(150);
}

void MainWindow::updateAxes() {
    parameterView_->setAxes(
        xmin_->value(), xmax_->value(), ymin_->value(), ymax_->value());
}

void MainWindow::setParametersEnabled(bool enabled) {
    for (auto* control : lockedControls_)
        control->setEnabled(enabled);
}

void MainWindow::runSelectedSimulation() {
    Config simulationConfig = config();
    if (selectedAngles_) {
        simulationConfig.leftDeg = preciseDouble(selectedAngles_->first);
        simulationConfig.rightDeg = preciseDouble(selectedAngles_->second);
        simulationConfig.leftDegExact =
            preciseString(selectedAngles_->first).toStdString();
        simulationConfig.rightDegExact =
            preciseString(selectedAngles_->second).toStdString();
    }
    const auto pixelProbe = pixelProbe_;
    pixelProbe_.reset();
    if (pixelProbe) {
        if (pixelProbe->precisionBits > 0)
            simulationConfig.precisionBits = pixelProbe->precisionBits;
        if (pixelProbe->analysisBalls > 0)
            simulationConfig.analysisBalls = std::max(
                pixelProbe->analysisBalls,
                pixelProbe->expectedPeriod > 0
                    ? pixelProbe->expectedPeriod * 2 + 2 : 0);
        if (pixelProbe->maxBalls > 0)
            simulationConfig.maxBalls = pixelProbe->maxBalls;
        if (pixelProbe->collisionBudget > 0)
            simulationConfig.collisionBudget = pixelProbe->collisionBudget;
    }
    simulationStatus_->setText("Simulating selected point…");
    previewCancel_ = std::make_shared<std::atomic_bool>(false);
    const auto token = previewCancel_;
    previewPool_.clear();
    previewPool_.start(QRunnable::create([this, simulationConfig, token, pixelProbe] {
        Result result = Simulator(simulationConfig).run(true, token.get());
        if (pixelProbe && pixelProbe->expectedPeriod > 0 &&
            result.outcome == Outcome::Unresolved) {
            result.outcome = Outcome::Periodic;
            result.period = pixelProbe->expectedPeriod;
            result.periodFromRenderedPixel = true;
        }
        QMetaObject::invokeMethod(this, [this, token, result = std::move(result)]() mutable {
            if (token != previewCancel_ || token->load())
                return;
            simulationView_->setResult(std::move(result));
            simulationStatus_->setText("Selected simulation complete");
        }, Qt::QueuedConnection);
    }));
}

QJsonObject MainWindow::groundTruthJson() const {
    return groundTruthJson(config());
}

QJsonObject MainWindow::groundTruthJson(const Config& config) const {
    QJsonObject object;
    object["modelVersion"] = 12;
    object["gravity"] = config.gravity;
    object["ballRadius"] = config.radius;
    object["restitution"] = std::max(.5, config.restitution);
    object["segmentGap"] = config.gap;
    object["segmentLength"] = config.segmentLength;
    object["spawnInterval"] = config.spawnInterval;
    object["cutoffY"] = config.cutoffY;
    object["spawnY"] = config.spawnY;
    object["segmentCenterY"] = 80.;
    return object;
}

QString MainWindow::groundTruthKey() const {
    return groundTruthKey(groundTruthJson());
}

QString MainWindow::groundTruthKey(const QJsonObject& truth) const {
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(truth).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

QJsonObject MainWindow::simulationJson() const {
    const auto angles = selectedAngles_.value_or(
        std::pair<PreciseDecimal, PreciseDecimal>{
            leftAngle_->value(), rightAngle_->value()});
    QJsonObject object = groundTruthJson();
    object["schema"] = "chaos-v-simulation-1";
    object["leftAngle"] = preciseDouble(angles.first);
    object["rightAngle"] = preciseDouble(angles.second);
    object["leftAngleExact"] = preciseString(angles.first);
    object["rightAngleExact"] = preciseString(angles.second);
    object["playbackSpeed"] = playbackSpeed_->value();
    object["axisXMin"] = xmin_->valueDouble();
    object["axisXMax"] = xmax_->valueDouble();
    object["axisYMin"] = ymin_->valueDouble();
    object["axisYMax"] = ymax_->valueDouble();
    object["axisXMinExact"] = xmin_->valueString();
    object["axisXMaxExact"] = xmax_->valueString();
    object["axisYMinExact"] = ymin_->valueString();
    object["axisYMaxExact"] = ymax_->valueString();
    object["fractalWidth"] = fractalWidth_->value();
    object["fractalHeight"] = fractalHeight_->value();
    object["maxLiveBalls"] = maxBalls_->value();
    object["ballsToAnalyze"] = analysisBalls_->value();
    object["collisionBudget"] = collisionBudget_->value();
    object["precisionBits"] = precisionBits_->value();
    return object;
}

void MainWindow::copySimulationJson() {
    QApplication::clipboard()->setText(QString::fromUtf8(
        QJsonDocument(simulationJson()).toJson(QJsonDocument::Compact)));
    simulationStatus_->setText("Simulation JSON copied to clipboard");
}

void MainWindow::pasteSimulationJson() {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(
        QApplication::clipboard()->text().toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        simulationStatus_->setText(
            "Clipboard does not contain valid simulation JSON");
        return;
    }

    const QJsonObject object = document.object();
    if (object.value("schema").toString() != "chaos-v-simulation-1") {
        simulationStatus_->setText("Unsupported simulation JSON format");
        return;
    }

    const auto exactOrNumber = [&](const char* exactName, const char* numberName,
                                   const QString& fallback) {
        const QString exact = object.value(exactName).toString();
        return exact.isEmpty()
                   ? QString::number(
                         object.value(numberName).toDouble(
                             fallback.toDouble()), 'g', 17)
                   : exact;
    };
    const QString leftText = exactOrNumber(
        "leftAngleExact", "leftAngle", leftAngle_->valueString());
    const QString rightText = exactOrNumber(
        "rightAngleExact", "rightAngle", rightAngle_->valueString());
    PreciseDecimal left;
    PreciseDecimal right;
    try {
        left = preciseDecimal(leftText);
        right = preciseDecimal(rightText);
    } catch (...) {
        simulationStatus_->setText("Simulation JSON contains invalid angles");
        return;
    }
    if (left < leftAngle_->minimum() || left > leftAngle_->maximum() ||
        right < rightAngle_->minimum() || right > rightAngle_->maximum()) {
        simulationStatus_->setText("Simulation JSON contains invalid angles");
        return;
    }

    applyingJson_ = true;
    const auto restoreDouble = [&](const char* name, QDoubleSpinBox* spin) {
        if (object.contains(name) && object.value(name).isDouble())
            spin->setValue(object.value(name).toDouble());
    };
    const auto restoreInteger = [&](const char* name, QSpinBox* spin) {
        if (object.contains(name) && object.value(name).isDouble())
            spin->setValue(object.value(name).toInt());
    };

    restoreDouble("gravity", gravity_);
    restoreDouble("ballRadius", radius_);
    restoreDouble("restitution", restitution_);
    restoreDouble("segmentGap", gap_);
    restoreDouble("segmentLength", segmentLength_);
    restoreDouble("spawnInterval", spawnInterval_);
    restoreDouble("spawnY", spawnY_);
    restoreDouble("cutoffY", cutoff_);
    restoreDouble("playbackSpeed", playbackSpeed_);
    xmin_->setValue(exactOrNumber(
        "axisXMinExact", "axisXMin", xmin_->valueString()));
    xmax_->setValue(exactOrNumber(
        "axisXMaxExact", "axisXMax", xmax_->valueString()));
    ymin_->setValue(exactOrNumber(
        "axisYMinExact", "axisYMin", ymin_->valueString()));
    ymax_->setValue(exactOrNumber(
        "axisYMaxExact", "axisYMax", ymax_->valueString()));
    restoreInteger("fractalWidth", fractalWidth_);
    restoreInteger("fractalHeight", fractalHeight_);
    restoreInteger("maxLiveBalls", maxBalls_);
    restoreInteger("ballsToAnalyze", analysisBalls_);
    restoreInteger("collisionBudget", collisionBudget_);
    restoreInteger("precisionBits", precisionBits_);
    leftAngle_->setValue(leftText);
    rightAngle_->setValue(rightText);
    applyingJson_ = false;

    selectedAngles_ = {{left, right}};
    parameterView_->setSelection(left, right);
    parameterView_->setLayerKey(groundTruthKey());
    updateAxes();
    simulationView_->setPlaybackSpeed(playbackSpeed_->value());
    scheduleSimulation();
    simulationStatus_->setText("Simulation JSON restored");
}

QJsonObject MainWindow::renderSettingsJson() const {
    QJsonObject object;
    object["maxLiveBalls"] = maxBalls_->value();
    object["ballsToAnalyze"] = analysisBalls_->value();
    object["collisionBudget"] = collisionBudget_->value();
    object["precisionBits"] = precisionBits_->value();
    object["periodStabilityShading"] = certificateShading_->isChecked();
    object["periodShadingStrength"] = certificateShadingStrength_->value();
    object["periodMarginScale"] = certificateShadingScale_->value();
    return object;
}

void MainWindow::renderFractal() {
    if (cancel_)
        cancel_->store(true);

    const Config base = config();
    rendering_ = true;
    setParametersEnabled(false);
    cancelRenderButton_->setEnabled(true);
    const QJsonObject truth = groundTruthJson(base);
    startFractalRender(
        base, groundTruthKey(truth), truth, renderSettingsJson(),
        {xmin_->value(), xmax_->value(), ymin_->value(), ymax_->value()},
        fractalWidth_->value(), fractalHeight_->value(), true, "Rendering");
}

void MainWindow::showSweepDialog() {
    SweepDialog dialog(config(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const SweepDefinition definition = dialog.definition();
    if (!(definition.maximum >= definition.minimum) ||
        !(definition.increment > 0)) {
        QMessageBox::warning(
            this, "Invalid sweep",
            "Maximum must be at least the minimum, and increment must be positive.");
        return;
    }

    QString basePath = QFileDialog::getSaveFileName(
        this, "Choose image base name",
        suggestedSavePath("chaos-v-sweep.png"),
        "PNG image (*.png)");
    if (basePath.isEmpty())
        return;
    rememberSaveDirectory(basePath);
    if (basePath.endsWith(".png", Qt::CaseInsensitive))
        basePath.chop(4);

    const long double span =
        static_cast<long double>(definition.maximum) - definition.minimum;
    const qint64 count = qint64(std::floor(
        span / static_cast<long double>(definition.increment) + 1e-12L)) + 1;
    if (count <= 0) {
        QMessageBox::warning(this, "Invalid sweep", "The sweep is empty.");
        return;
    }

    const int numberWidth = std::max(3, int(QString::number(count).size()));
    const QString firstPath = QString("%1_%2.png").arg(
        basePath, QString::number(1).rightJustified(numberWidth, '0'));
    if (QFileInfo::exists(firstPath) &&
        QMessageBox::question(
            this, "Replace sweep images?",
            "One or more numbered images may already exist. Replace matching files?")
            != QMessageBox::Yes)
        return;

    if (cancel_)
        cancel_->store(true);
    sweep_ = SweepState{
        definition,
        config(),
        {xmin_->value(), xmax_->value(), ymin_->value(), ymax_->value()},
        basePath,
        0,
        count,
        fractalWidth_->value(),
        fractalHeight_->value()
    };
    clearPausedBulkRender();
    rendering_ = true;
    setParametersEnabled(false);
    cancelRenderButton_->setEnabled(true);
    pauseRenderButton_->setEnabled(true);
    startNextSweepImage();
}

void MainWindow::applySweepValue(
    Config& config, SweepParameter parameter, double value) {
    switch (parameter) {
    case SweepParameter::Gravity: config.gravity = value; break;
    case SweepParameter::BallRadius: config.radius = value; break;
    case SweepParameter::Restitution: config.restitution = value; break;
    case SweepParameter::SegmentGap:
        config.gap = value;
        config.spawnX = -value / 2;
        break;
    case SweepParameter::SegmentLength: config.segmentLength = value; break;
    case SweepParameter::SpawnInterval: config.spawnInterval = value; break;
    case SweepParameter::SpawnY: config.spawnY = value; break;
    case SweepParameter::CutoffY: config.cutoffY = value; break;
    }
}

void MainWindow::restoreSweepSettings() {
    if (!sweep_)
        return;
    const Config& original = sweep_->baseConfig;
    applyingJson_ = true;
    gravity_->setValue(original.gravity);
    radius_->setValue(original.radius);
    restitution_->setValue(original.restitution);
    gap_->setValue(original.gap);
    segmentLength_->setValue(original.segmentLength);
    spawnInterval_->setValue(original.spawnInterval);
    spawnY_->setValue(original.spawnY);
    cutoff_->setValue(original.cutoffY);
    applyingJson_ = false;
}

void MainWindow::startNextSweepImage() {
    if (!sweep_)
        return;
    Config point = sweep_->baseConfig;
    const double value = sweep_->definition.minimum +
                         double(sweep_->index) * sweep_->definition.increment;
    applySweepValue(point, sweep_->definition.parameter, value);
    const QJsonObject truth = groundTruthJson(point);
    const QString prefix = QString("Sweep %1/%2 — %3=%4")
        .arg(sweep_->index + 1)
        .arg(sweep_->count)
        .arg(sweep_->definition.label)
        .arg(value, 0, 'g', 12);
    startFractalRender(
        point, groundTruthKey(truth), truth, renderSettingsJson(),
        sweep_->axes, sweep_->width, sweep_->height, false, prefix);
}

void MainWindow::showZoomDialog() {
    ZoomDialog dialog(fractalWidth_->value(), fractalHeight_->value(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString basePath = QFileDialog::getSaveFileName(
        this, "Choose zoom frame base name",
        suggestedSavePath("chaos-v-zoom.png"),
        "PNG image (*.png)");
    if (basePath.isEmpty())
        return;
    rememberSaveDirectory(basePath);
    if (basePath.endsWith(".png", Qt::CaseInsensitive))
        basePath.chop(4);

    const ZoomDefinition definition = dialog.definition();
    const int numberWidth = std::max(
        3, int(QString::number(definition.frameCount).size()));
    const QString firstPath = QString("%1_%2.png").arg(
        basePath, QString::number(1).rightJustified(numberWidth, '0'));
    if (QFileInfo::exists(firstPath) &&
        QMessageBox::question(
            this, "Replace zoom frames?",
            "One or more numbered frames may already exist. Replace matching files?")
            != QMessageBox::Yes)
        return;

    if (cancel_)
        cancel_->store(true);
    zoom_ = ZoomState{definition, basePath, 0};
    clearPausedBulkRender();
    rendering_ = true;
    setParametersEnabled(false);
    cancelRenderButton_->setEnabled(true);
    pauseRenderButton_->setEnabled(true);
    parameterView_->setLayerKey("__zoom-preview__");
    parameterView_->clearActiveLayers();
    startNextZoomFrame();
}

void MainWindow::startNextZoomFrame() {
    if (!zoom_)
        return;
    const auto& definition = zoom_->definition;
    const double time = definition.duration * double(zoom_->index) /
                        double(definition.frameCount - 1);
    const double progress = easedZoomProgress(
        time, definition.duration, definition.dampingTime);
    const double centerProgress = zoomCenterProgress(
        time, definition.panTime, definition.panDampingTime);
    const auto axes = interpolateZoomAxes(
        definition.startAxes, definition.endAxes, progress, centerProgress);
    zoom_->groupAxes.clear();
    zoom_->groupAxes.push_back(axes);
    zoom_->groupEnd = zoom_->index + 1;
    const bool panComplete = centerProgress >= 1. - 1e-12;
    if (panComplete && definition.width <= std::numeric_limits<int>::max() / 2 &&
        definition.height <= std::numeric_limits<int>::max() / 2) {
        const PreciseDecimal initialX = abs(axes[1] - axes[0]);
        const PreciseDecimal initialY = abs(axes[3] - axes[2]);
        for (qint64 candidate = zoom_->index + 1;
             candidate < definition.frameCount; ++candidate) {
            const double candidateTime = definition.duration * double(candidate) /
                                         double(definition.frameCount - 1);
            const double scale = easedZoomProgress(
                candidateTime, definition.duration, definition.dampingTime);
            const double center = zoomCenterProgress(
                candidateTime, definition.panTime, definition.panDampingTime);
            if (center < 1. - 1e-12)
                break;
            const auto candidateAxes = interpolateZoomAxes(
                definition.startAxes, definition.endAxes, scale, center);
            if (abs(candidateAxes[1] - candidateAxes[0]) < initialX * PreciseDecimal("0.55") ||
                abs(candidateAxes[3] - candidateAxes[2]) < initialY * PreciseDecimal("0.55"))
                break;
            zoom_->groupAxes.push_back(candidateAxes);
            zoom_->groupEnd = candidate + 1;
        }
        // Four or fewer frames gain little after the 4x pixel cost.
        if (zoom_->groupAxes.size() <= 4) {
            zoom_->groupAxes.resize(1);
            zoom_->groupEnd = zoom_->index + 1;
        }
    }
    const auto previewAxes = squareZoomPreview(axes);
    parameterView_->setAxes(
        previewAxes[0], previewAxes[1], previewAxes[2], previewAxes[3]);
    Config frameConfig = definition.config;
    if (definition.adaptivePrecision)
        frameConfig.precisionBits = adaptiveZoomPrecision(
            axes, definition.width, definition.height);
    const QJsonObject truth = groundTruthJson(frameConfig);
    const bool grouped = zoom_->groupAxes.size() > 1;
    const QString prefix = QString("Zoom frames %1–%2/%3 — %4 bits")
        .arg(zoom_->index + 1).arg(zoom_->groupEnd).arg(definition.frameCount)
        .arg(frameConfig.precisionBits);
    startFractalRender(
        frameConfig, "__zoom-preview__", truth,
        renderSettingsJson(), axes,
        grouped ? definition.width * 2 : definition.width,
        grouped ? definition.height * 2 : definition.height,
        false, prefix);
}

void MainWindow::pauseBulkRender() {
    if (!rendering_ || (!sweep_ && !zoom_) || !cancel_)
        return;
    pauseRequested_ = true;
    cancel_->store(true);
    pauseRenderButton_->setEnabled(false);
    cancelRenderButton_->setEnabled(false);
    renderStatus_->setText("Pausing after active workers stop…");
}

void MainWindow::clearPausedBulkRender() {
    settings_.remove("pausedBulkRender");
    settings_.sync();
    if (resumeRenderButton_)
        resumeRenderButton_->setEnabled(false);
}

void MainWindow::savePausedBulkRender() {
    QJsonObject root;
    if (sweep_) {
        root["kind"] = "sweep";
        root["basePath"] = sweep_->basePath;
        root["index"] = sweep_->index;
        root["count"] = sweep_->count;
        root["width"] = sweep_->width; root["height"] = sweep_->height;
        root["axes"] = axesJson(sweep_->axes);
        root["config"] = bulkConfigJson(sweep_->baseConfig);
        QJsonObject d;
        d["parameter"] = int(sweep_->definition.parameter);
        d["label"] = sweep_->definition.label;
        d["minimum"] = sweep_->definition.minimum;
        d["maximum"] = sweep_->definition.maximum;
        d["increment"] = sweep_->definition.increment;
        root["definition"] = d;
    } else if (zoom_) {
        root["kind"] = "zoom";
        root["basePath"] = zoom_->basePath;
        root["index"] = zoom_->index;
        const auto& z = zoom_->definition;
        root["config"] = bulkConfigJson(z.config);
        root["startAxes"] = axesJson(z.startAxes);
        root["endAxes"] = axesJson(z.endAxes);
        root["frameRate"] = z.frameRate; root["duration"] = z.duration;
        root["damping"] = z.dampingTime; root["panTime"] = z.panTime;
        root["panDamping"] = z.panDampingTime;
        root["adaptive"] = z.adaptivePrecision;
        root["frameCount"] = z.frameCount;
        root["width"] = z.width; root["height"] = z.height;
    }
    if (!root.isEmpty()) {
        settings_.setValue("pausedBulkRender",
            QJsonDocument(root).toJson(QJsonDocument::Compact));
        settings_.sync();
    }
}

void MainWindow::restorePausedBulkRender() {
    const QByteArray bytes = settings_.value("pausedBulkRender").toByteArray();
    if (bytes.isEmpty())
        return;
    const QJsonObject root = QJsonDocument::fromJson(bytes).object();
    try {
        if (root["kind"].toString() == "sweep") {
            const QJsonObject d = root["definition"].toObject();
            SweepDefinition definition{
                SweepParameter(d["parameter"].toInt()), d["label"].toString(),
                d["minimum"].toDouble(), d["maximum"].toDouble(),
                d["increment"].toDouble()};
            sweep_ = SweepState{definition, bulkConfig(root["config"].toObject()),
                axesFromJson(root["axes"].toArray()), root["basePath"].toString(),
                root["index"].toInteger(), root["count"].toInteger(),
                root["width"].toInt(), root["height"].toInt()};
        } else if (root["kind"].toString() == "zoom") {
            ZoomDefinition z;
            z.config = bulkConfig(root["config"].toObject());
            z.startAxes = axesFromJson(root["startAxes"].toArray());
            z.endAxes = axesFromJson(root["endAxes"].toArray());
            z.frameRate = root["frameRate"].toDouble();
            z.duration = root["duration"].toDouble();
            z.dampingTime = root["damping"].toDouble();
            z.panTime = root["panTime"].toDouble();
            z.panDampingTime = root["panDamping"].toDouble();
            z.adaptivePrecision = root["adaptive"].toBool();
            z.frameCount = root["frameCount"].toInteger();
            z.width = root["width"].toInt(); z.height = root["height"].toInt();
            zoom_ = ZoomState{z, root["basePath"].toString(),
                              root["index"].toInteger()};
        }
    } catch (...) {
        sweep_.reset(); zoom_.reset(); clearPausedBulkRender();
        return;
    }
    resumeRenderButton_->setEnabled(sweep_.has_value() || zoom_.has_value());
    if (resumeRenderButton_->isEnabled())
        renderStatus_->setText("A paused bulk render can be resumed");
}

void MainWindow::resumeBulkRender() {
    if (rendering_)
        return;
    if (!sweep_ && !zoom_)
        restorePausedBulkRender();
    if (!sweep_ && !zoom_)
        return;
    const QString basePath = sweep_ ? sweep_->basePath : zoom_->basePath;
    if (!QDir(QFileInfo(basePath).absolutePath()).exists()) {
        QMessageBox::critical(this, "Cannot resume render",
                              "The output image directory no longer exists.");
        sweep_.reset(); zoom_.reset(); clearPausedBulkRender();
        return;
    }
    clearPausedBulkRender();
    rendering_ = true;
    setParametersEnabled(false);
    cancelRenderButton_->setEnabled(true);
    pauseRenderButton_->setEnabled(true);
    if (zoom_) {
        parameterView_->setLayerKey("__zoom-preview__");
        parameterView_->clearActiveLayers();
        startNextZoomFrame();
    } else {
        startNextSweepImage();
    }
}

void MainWindow::startFractalRender(
    Config base, const QString& key, const QJsonObject& truth,
    const QJsonObject& renderSettings,
    const std::array<PreciseDecimal, 4>& axes,
    int width, int height, bool persist, const QString& statusPrefix) {
    setThreadRealPrecision(base.precisionBits);
    parameterView_->setLayerKey(key);
    if (!parameterView_->beginLayer(
            width, height, axes[0], axes[1], axes[2], axes[3], base)) {
        restoreSweepSettings();
        sweep_.reset();
        zoom_.reset();
        finishRendering("Could not allocate an image at the requested resolution");
        return;
    }

    cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto token = cancel_;
    renderStatus_->setText(statusPrefix + "…");
    renderClock_.start();

    constexpr double targetTileCount = 4096.;
    const int tileSize = std::max(
        8, int(std::ceil(
               std::sqrt(double(width) * height / targetTileCount))));
    const int columns = 1 + (width - 1) / tileSize;
    const int rows = 1 + (height - 1) / tileSize;
    const int total = columns * rows;
    const auto schedule = makeTileSchedule(columns, rows);

    const PreciseDecimal leftAngle = axes[0];
    const PreciseDecimal rightAngle = axes[1];
    const PreciseDecimal lowAngle = axes[2];
    const PreciseDecimal highAngle = axes[3];
    const auto remaining = std::make_shared<std::atomic_int>(total);
    const auto sampledCount = std::make_shared<std::atomic_int>(0);
    const auto sampledNanoseconds =
        std::make_shared<std::atomic<long long>>(0);
    const bool shadePeriodStability = certificateShading_->isChecked();
    const double shadingStrength = certificateShadingStrength_->value();
    const double shadingScale = certificateShadingScale_->value();
    base.trackPeriodStability = shadePeriodStability;

    for (const RenderTileJob job : schedule) {
        const int tileX = job.index % columns;
        const int tileY = job.index / columns;
        const int originX = tileX * tileSize;
        const int originY = tileY * tileSize;
        const int tileWidth = std::min(tileSize, width - originX);
        const int tileHeight = std::min(tileSize, height - originY);

        renderPool_.start(QRunnable::create(
            [=, this] {
                QElapsedTimer tileClock;
                tileClock.start();
                QImage image(tileWidth, tileHeight, QImage::Format_RGB32);
                std::vector<int> tilePeriods(size_t(tileWidth) * tileHeight, 0);
                for (int y = 0; y < tileHeight && !token->load(); ++y) {
                    for (int x = 0; x < tileWidth && !token->load(); ++x) {
                        Config point = base;
                        const PreciseDecimal pixelX = precisePixelCenter(
                            leftAngle, rightAngle, originX + x, width);
                        const PreciseDecimal pixelY = precisePixelCenter(
                            highAngle, lowAngle, originY + y, height);
                        point.leftDeg = preciseDouble(pixelX);
                        point.rightDeg = preciseDouble(pixelY);
                        point.leftDegExact = precisePhysicsString(
                            pixelX, base.precisionBits).toStdString();
                        point.rightDegExact = precisePhysicsString(
                            pixelY, base.precisionBits).toStdString();
                        const Result classification =
                            classifyFractalPoint(point, token.get());
                        if (classification.outcome == Outcome::Periodic)
                            tilePeriods[size_t(y) * tileWidth + x] =
                                classification.period;
                        image.setPixel(
                            x, y, shadeFractalResult(
                                classification, shadePeriodStability, shadingStrength,
                                shadingScale).rgb());
                    }
                }

                if (job.etaSample) {
                    sampledNanoseconds->fetch_add(tileClock.nsecsElapsed());
                    sampledCount->fetch_add(1);
                }

                QMetaObject::invokeMethod(
                    this,
                    [=, this] {
                        const int left = --*remaining;
                        if (token != cancel_)
                            return;
                        if (!token->load())
                            parameterView_->tile(
                                originX, originY, image, tilePeriods);

                        const int done = total - left;
                        if (left == 0) {
                            if (token->load()) {
                                parameterView_->removeLatestLayer();
                                if (pauseRequested_ && (sweep_ || zoom_)) {
                                    pauseRequested_ = false;
                                    savePausedBulkRender();
                                    restoreSweepSettings();
                                    sweep_.reset();
                                    zoom_.reset();
                                    finishRendering("Bulk render paused");
                                    resumeRenderButton_->setEnabled(true);
                                } else {
                                    restoreSweepSettings();
                                    sweep_.reset();
                                    zoom_.reset();
                                    clearPausedBulkRender();
                                    finishRendering("Rendering canceled");
                                }
                            } else if (persist) {
                                persistLatestLayer(key, truth, renderSettings);
                                finishRendering("Fractal complete and saved");
                            } else if (sweep_) {
                                const auto layer = parameterView_->latestLayer();
                                const int numberWidth = std::max(
                                    3, int(QString::number(sweep_->count).size()));
                                const QString path = QString("%1_%2.png").arg(
                                    sweep_->basePath,
                                    QString::number(sweep_->index + 1)
                                        .rightJustified(numberWidth, '0'));
                                const bool saved = layer && layer->image.save(path, "PNG");
                                parameterView_->removeLatestLayer();
                                if (!saved) {
                                    restoreSweepSettings();
                                    sweep_.reset();
                                    finishRendering("Could not save " + path);
                                } else if (++sweep_->index >= sweep_->count) {
                                    const qint64 savedCount = sweep_->count;
                                    restoreSweepSettings();
                                    sweep_.reset();
                                    clearPausedBulkRender();
                                    finishRendering(QString(
                                        "Sweep complete — %1 images saved")
                                        .arg(savedCount));
                                } else {
                                    QTimer::singleShot(
                                        0, this, &MainWindow::startNextSweepImage);
                                }
                            } else if (zoom_) {
                                const auto layer = parameterView_->latestLayer();
                                const int numberWidth = std::max(
                                    3, int(QString::number(
                                        zoom_->definition.frameCount).size()));
                                bool saved = layer.has_value();
                                QImage lastOutput;
                                if (layer) {
                                    for (size_t j = 0; j < zoom_->groupAxes.size() && saved; ++j) {
                                        QImage output = cropFractal(
                                            layer->image,
                                            {layer->xmin, layer->xmax, layer->ymin, layer->ymax},
                                            zoom_->groupAxes[j], zoom_->definition.width,
                                            zoom_->definition.height);
                                        if (!zoom_->previousAnchor.isNull() && j < 4) {
                                            QImage previous = cropFractal(
                                                zoom_->previousAnchor, zoom_->previousAxes,
                                                zoom_->groupAxes[j], zoom_->definition.width,
                                                zoom_->definition.height);
                                            QPainter blend(&previous);
                                            blend.setOpacity(double(j + 1) / 5.);
                                            blend.drawImage(0, 0, output);
                                            blend.end();
                                            output = std::move(previous);
                                        }
                                        const qint64 frameIndex = zoom_->index + qint64(j);
                                        const QString path = QString("%1_%2.png").arg(
                                            zoom_->basePath,
                                            QString::number(frameIndex + 1)
                                                .rightJustified(numberWidth, '0'));
                                        saved = output.save(path, "PNG");
                                        lastOutput = std::move(output);
                                    }
                                }
                                if (!saved) {
                                    parameterView_->removeLatestLayer();
                                    zoom_.reset();
                                    finishRendering("Could not save a zoom frame");
                                } else {
                                    zoom_->previousAnchor = layer->image;
                                    zoom_->previousAxes = {
                                        layer->xmin, layer->xmax, layer->ymin, layer->ymax};
                                    zoom_->index = zoom_->groupEnd;
                                }
                                if (saved && zoom_ &&
                                    zoom_->index >= zoom_->definition.frameCount) {
                                    const qint64 savedCount =
                                        zoom_->definition.frameCount;
                                    const auto finalAxes = zoom_->groupAxes.back();
                                    parameterView_->replaceLatestLayer(
                                        std::move(lastOutput), finalAxes);
                                    zoom_.reset();
                                    clearPausedBulkRender();
                                    finishRendering(QString(
                                        "Zoom complete — %1 frames saved")
                                        .arg(savedCount), false);
                                } else if (saved && zoom_) {
                                    parameterView_->removeLatestLayer();
                                    QTimer::singleShot(
                                        0, this, &MainWindow::startNextZoomFrame);
                                }
                            } else {
                                finishRendering(
                                    "Render sequence ended unexpectedly");
                            }
                            return;
                        }

                        if (token->load())
                            return;
                        const int samples = sampledCount->load();
                        if (samples == 0) {
                            renderStatus_->setText(
                                QString("%1 — %2/%3 — estimating time left…")
                                    .arg(statusPrefix).arg(done).arg(total));
                            return;
                        }

                        const double averageSeconds =
                            sampledNanoseconds->load() / 1e9 / samples;
                        const int workers = std::max(1, renderPool_.maxThreadCount());
                        const int eta = std::max(
                            0, int(std::ceil(averageSeconds * left / workers)));
                        renderStatus_->setText(
                            QString("%1 — %2/%3 — about %4 s left")
                                .arg(statusPrefix).arg(done).arg(total).arg(eta));
                    },
                    Qt::QueuedConnection);
            }));
    }
}

void MainWindow::finishRendering(
    const QString& status, bool restoreFractalView) {
    rendering_ = false;
    setParametersEnabled(true);
    cancelRenderButton_->setEnabled(false);
    pauseRenderButton_->setEnabled(false);
    if (restoreFractalView) {
        parameterView_->setLayerKey(groundTruthKey());
        updateAxes();
    }
    renderStatus_->setText(status);
    scheduleSimulation();
}

QString MainWindow::manifestPath() const {
    return QDir(fractalDirectory_).filePath("manifest.json");
}

void MainWindow::writeManifest() {
    QSaveFile file(manifestPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    QJsonObject root;
    root["formatVersion"] = 2;
    root["layers"] = storedLayers_;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

void MainWindow::loadFractals() {
    fractalDirectory_ =
        QDir(QCoreApplication::applicationDirPath()).filePath("fractals");
    QDir().mkpath(fractalDirectory_);

    QFile file(manifestPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return;

    QJsonArray valid;
    for (const auto& value : document.object().value("layers").toArray()) {
        const QJsonObject object = value.toObject();
        const QString name = object.value("file").toString();
        if (name.isEmpty() || QFileInfo(name).fileName() != name)
            continue;
        QImage image(QDir(fractalDirectory_).filePath(name));
        if (image.isNull())
            continue;
        std::vector<int> periods;
        const QString periodsName = object.value("periodsFile").toString();
        if (!periodsName.isEmpty() && QFileInfo(periodsName).fileName() == periodsName) {
            QFile periodFile(QDir(fractalDirectory_).filePath(periodsName));
            if (periodFile.open(QIODevice::ReadOnly)) {
                const QByteArray raw = qUncompress(periodFile.readAll());
                const qsizetype expectedBytes =
                    qsizetype(image.width()) * image.height() * qsizetype(sizeof(int));
                if (raw.size() == expectedBytes) {
                    periods.resize(size_t(image.width()) * image.height());
                    std::memcpy(periods.data(), raw.constData(), size_t(raw.size()));
                }
            }
        }
        parameterView_->addStoredLayer(
            object.value("groundTruthKey").toString(), std::move(image),
            object.value("xminExact").toString(
                QString::number(object.value("xmin").toDouble(), 'g', 17)),
            object.value("xmaxExact").toString(
                QString::number(object.value("xmax").toDouble(), 'g', 17)),
            object.value("yminExact").toString(
                QString::number(object.value("ymin").toDouble(), 'g', 17)),
            object.value("ymaxExact").toString(
                QString::number(object.value("ymax").toDouble(), 'g', 17)),
            object.value("renderSettings").toObject().value("precisionBits").toInt(),
            object.value("renderSettings").toObject().value("ballsToAnalyze").toInt(),
            object.value("renderSettings").toObject().value("maxLiveBalls").toInt(),
            object.value("renderSettings").toObject().value("collisionBudget").toInt(),
            std::move(periods));
        valid.append(object);
    }
    storedLayers_ = valid;
}

void MainWindow::persistLatestLayer(
    const QString& key, const QJsonObject& truth,
    const QJsonObject& renderSettings) {
    const auto layer = parameterView_->latestLayer();
    if (!layer)
        return;
    if (fractalDirectory_.isEmpty()) {
        fractalDirectory_ =
            QDir(QCoreApplication::applicationDirPath()).filePath("fractals");
        QDir().mkpath(fractalDirectory_);
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString name = id + ".png";
    if (!layer->image.save(QDir(fractalDirectory_).filePath(name), "PNG"))
        return;

    QJsonObject object;
    object["file"] = name;
    if (layer->periods && layer->periods->size() ==
        size_t(layer->image.width()) * layer->image.height() &&
        layer->periods->size() <= size_t(std::numeric_limits<qsizetype>::max()) /
                                      sizeof(int)) {
        QByteArray raw(qsizetype(layer->periods->size() * sizeof(int)),
                       Qt::Uninitialized);
        std::memcpy(raw.data(), layer->periods->data(), size_t(raw.size()));
        const QString periodsName = id + ".periods";
        QSaveFile periodFile(QDir(fractalDirectory_).filePath(periodsName));
        if (periodFile.open(QIODevice::WriteOnly)) {
            periodFile.write(qCompress(raw, 9));
            if (periodFile.commit())
                object["periodsFile"] = periodsName;
        }
    }
    object["groundTruthKey"] = key;
    object["groundTruth"] = truth;
    object["renderSettings"] = renderSettings;
    object["xmin"] = preciseDouble(layer->xmin);
    object["xmax"] = preciseDouble(layer->xmax);
    object["ymin"] = preciseDouble(layer->ymin);
    object["ymax"] = preciseDouble(layer->ymax);
    object["xminExact"] = preciseString(layer->xmin);
    object["xmaxExact"] = preciseString(layer->xmax);
    object["yminExact"] = preciseString(layer->ymin);
    object["ymaxExact"] = preciseString(layer->ymax);
    object["width"] = layer->image.width();
    object["height"] = layer->image.height();
    storedLayers_.append(object);
    writeManifest();
}

void MainWindow::exportPng() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export fractal", suggestedSavePath("chaos-v.png"),
        "PNG image (*.png)");
    if (path.isEmpty())
        return;
    rememberSaveDirectory(path);
    if (!parameterView_->fractal().save(path, "PNG"))
        renderStatus_->setText("Could not save PNG");
    else
        renderStatus_->setText("PNG exported");
}

void MainWindow::exportSimulationLoop() {
    const QPointF camera = simulationView_->cameraPosition();
    SimulationExportDialog dialog(
        camera.x(), camera.y(), simulationView_->cameraZoom(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const SimulationExportDefinition definition = dialog.definition();
    QString basePath = QFileDialog::getSaveFileName(
        this, "Choose simulation frame base name",
        suggestedSavePath("chaos-v-simulation.png"), "PNG image (*.png)");
    if (basePath.isEmpty())
        return;
    rememberSaveDirectory(basePath);
    if (basePath.endsWith(".png", Qt::CaseInsensitive))
        basePath.chop(4);

    Config simulationConfig = config();
    if (selectedAngles_) {
        simulationConfig.leftDeg = preciseDouble(selectedAngles_->first);
        simulationConfig.rightDeg = preciseDouble(selectedAngles_->second);
        simulationConfig.leftDegExact = preciseString(selectedAngles_->first).toStdString();
        simulationConfig.rightDegExact = preciseString(selectedAngles_->second).toStdString();
    }
    if (definition.overridePeriod)
        simulationConfig.analysisBalls = std::max(
            simulationConfig.analysisBalls, definition.period * 2 + 2);

    simulationStatus_->setText("Preparing simulation loop export…");
    previewPool_.start(QRunnable::create(
        [this, simulationConfig, definition, basePath] {
            Result result = Simulator(simulationConfig).run(true);
            const int period = definition.overridePeriod
                                   ? definition.period
                                   : (result.outcome == Outcome::Periodic ? result.period : 0);
            if (period <= 0 || result.frames.empty()) {
                QMetaObject::invokeMethod(this, [this] {
                    simulationStatus_->setText(
                        "Loop export needs a detected period or a period override");
                }, Qt::QueuedConnection);
                return;
            }
            const qint64 count = qint64(period) * definition.framesPerSpawn;
            const int digits = std::max(3, int(QString::number(count).size()));
            const double cycleDuration = period * result.spawnInterval;
            const double recordedEnd = result.frames.back().start + result.frames.back().duration;
            const double cycleStart = std::max(0., recordedEnd - cycleDuration);
            bool okay = true;
            for (qint64 i = 0; i < count && okay; ++i) {
                const double time = cycleStart + cycleDuration * double(i) / count;
                const AnimFrame* source = nullptr;
                for (const auto& frame : result.frames) {
                    if (time >= frame.start && time < frame.start + frame.duration + 1e-12) {
                        source = &frame;
                        break;
                    }
                }
                if (!source) { okay = false; break; }
                QImage image(definition.width, definition.height, QImage::Format_RGB32);
                if (image.isNull()) { okay = false; break; }
                image.fill(definition.background);
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing, definition.antialias);
                const auto map = [&](double x, double y) {
                    return QPointF(image.width() / 2. + (x - definition.centerX) * definition.pixelsPerUnit,
                                   image.height() / 2. + (y - definition.centerY) * definition.pixelsPerUnit);
                };
                painter.setPen(QPen(definition.segments, definition.segmentWidth));
                for (const auto& segment : result.segments)
                    painter.drawLine(map(toDouble(segment.a.x), toDouble(segment.a.y)),
                                     map(toDouble(segment.b.x), toDouble(segment.b.y)));
                painter.setPen(Qt::NoPen);
                painter.setBrush(definition.balls);
                const Real offset = makeReal(time - source->start);
                for (const auto& ball : source->balls) {
                    const V2 p = position(ball, offset, makeReal(result.gravity));
                    const double radius = result.radius * definition.pixelsPerUnit;
                    painter.drawEllipse(map(toDouble(p.x), toDouble(p.y)), radius, radius);
                }
                painter.end();
                const QString path = QString("%1_%2.png").arg(
                    basePath, QString::number(i + 1).rightJustified(digits, '0'));
                okay = image.save(path, "PNG");
                if ((i % 10) == 0 || i + 1 == count)
                    QMetaObject::invokeMethod(this, [this, i, count] {
                        simulationStatus_->setText(QString("Exporting simulation loop — %1/%2")
                                                       .arg(i + 1).arg(count));
                    }, Qt::QueuedConnection);
            }
            QMetaObject::invokeMethod(this, [this, okay, count] {
                simulationStatus_->setText(
                    okay ? QString("Simulation loop complete — %1 frames saved").arg(count)
                         : QString("Simulation loop export failed"));
            }, Qt::QueuedConnection);
        }));
}

QString MainWindow::suggestedSavePath(const QString& fileName) const {
    const QString directory = settings_.value("lastImageSaveDirectory").toString();
    return directory.isEmpty() || !QDir(directory).exists()
               ? fileName
               : QDir(directory).filePath(fileName);
}

void MainWindow::rememberSaveDirectory(const QString& path) {
    settings_.setValue(
        "lastImageSaveDirectory", QFileInfo(path).absolutePath());
    settings_.sync();
}

void MainWindow::restoreSettings() {
    const auto restoreDouble = [&](const char* name, QDoubleSpinBox* spin) {
        spin->setValue(settings_.value(name, spin->value()).toDouble());
    };
    const auto restoreInteger = [&](const char* name, QSpinBox* spin) {
        spin->setValue(settings_.value(name, spin->value()).toInt());
    };

    leftAngle_->setValue(
        settings_.value("leftExact", settings_.value("left", "45")).toString());
    rightAngle_->setValue(
        settings_.value("rightExact", settings_.value("right", "-45")).toString());
    restoreDouble("gravity", gravity_);
    restoreDouble("radius", radius_);
    restoreDouble("restitution", restitution_);
    restoreDouble("gap", gap_);
    restoreDouble("length", segmentLength_);
    restoreDouble("interval", spawnInterval_);
    restoreDouble("spawnY", spawnY_);
    restoreDouble("cutoff", cutoff_);
    restoreDouble("speed", playbackSpeed_);
    restoreInteger("width", fractalWidth_);
    restoreInteger("height", fractalHeight_);
    restoreInteger("balls", maxBalls_);
    restoreInteger("analysis", analysisBalls_);
    restoreInteger("budget", collisionBudget_);
    restoreInteger("bits", precisionBits_);
    certificateShading_->setChecked(
        settings_.value("certificateShading", true).toBool());
    certificateShadingStrength_->setValue(
        settings_.value("certificateShadingStrength", .7).toDouble());
    certificateShadingScale_->setValue(
        settings_.value("certificateShadingScale", 5.).toDouble());
}

void MainWindow::saveSettings() {
    const auto saveDouble = [&](const char* name, QDoubleSpinBox* spin) {
        settings_.setValue(name, spin->value());
    };
    const auto saveInteger = [&](const char* name, QSpinBox* spin) {
        settings_.setValue(name, spin->value());
    };

    settings_.setValue("left", leftAngle_->valueDouble());
    settings_.setValue("right", rightAngle_->valueDouble());
    settings_.setValue("leftExact", leftAngle_->valueString());
    settings_.setValue("rightExact", rightAngle_->valueString());
    saveDouble("gravity", gravity_);
    saveDouble("radius", radius_);
    saveDouble("restitution", restitution_);
    saveDouble("gap", gap_);
    saveDouble("length", segmentLength_);
    saveDouble("interval", spawnInterval_);
    saveDouble("spawnY", spawnY_);
    saveDouble("cutoff", cutoff_);
    saveDouble("speed", playbackSpeed_);
    settings_.setValue("xminExact", xmin_->valueString());
    settings_.setValue("xmaxExact", xmax_->valueString());
    settings_.setValue("yminExact", ymin_->valueString());
    settings_.setValue("ymaxExact", ymax_->valueString());
    saveInteger("width", fractalWidth_);
    saveInteger("height", fractalHeight_);
    saveInteger("balls", maxBalls_);
    saveInteger("analysis", analysisBalls_);
    saveInteger("budget", collisionBudget_);
    saveInteger("bits", precisionBits_);
    settings_.setValue("certificateShading", certificateShading_->isChecked());
    settings_.setValue("certificateShadingStrength",
                       certificateShadingStrength_->value());
    settings_.setValue("certificateShadingScale",
                       certificateShadingScale_->value());

    const QPointF camera = simulationView_->cameraPosition();
    settings_.setValue("cameraX", camera.x());
    settings_.setValue("cameraY", camera.y());
    settings_.setValue("cameraZoom", simulationView_->cameraZoom());
    settings_.setValue("windowGeometry", saveGeometry());
    settings_.setValue("splitterState", splitter_->saveState());
    settings_.sync();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    previewDelay_.stop();
    if (cancel_)
        cancel_->store(true);
    if (previewCancel_)
        previewCancel_->store(true);
    renderPool_.clear();
    previewPool_.clear();
    renderPool_.waitForDone();
    previewPool_.waitForDone();
    QMainWindow::closeEvent(event);
}
