#pragma once

#include "physics.h"
#include "precise_decimal.h"
#include "simulation_export_dialog.h"
#include "sweep_dialog.h"
#include "zoom_dialog.h"

#include <QElapsedTimer>
#include <QImage>
#include <QJsonArray>
#include <QMainWindow>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class ParameterView;
class PreciseSpinBox;
class SimulationView;
class QCloseEvent;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QSplitter;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    static QDoubleSpinBox* doubleSpin(
        double value, double low, double high, int decimals = 3);
    static PreciseSpinBox* preciseSpin(
        const char* value, const char* low, const char* high,
        const char* step = "0.001");
    static QSpinBox* integerSpin(int value, int low, int high);
    void buildUi();
    Config config() const;
    void scheduleSimulation();
    void updateAxes();
    void setParametersEnabled(bool enabled);
    void runSelectedSimulation();
    QJsonObject groundTruthJson() const;
    QJsonObject groundTruthJson(const Config& config) const;
    QString groundTruthKey() const;
    QString groundTruthKey(const QJsonObject& truth) const;
    QJsonObject simulationJson() const;
    QJsonObject renderSettingsJson() const;
    void copySimulationJson();
    void pasteSimulationJson();
    void renderFractal();
    void showSweepDialog();
    void startNextSweepImage();
    void showZoomDialog();
    void startNextZoomFrame();
    void pauseBulkRender();
    void resumeBulkRender();
    void savePausedBulkRender();
    void restorePausedBulkRender();
    void clearPausedBulkRender();
    void startFractalRender(
        Config base, const QString& key, const QJsonObject& truth,
        const QJsonObject& renderSettings,
        const std::array<PreciseDecimal, 4>& axes,
        int width, int height, bool persist,
        const QString& statusPrefix);
    void finishRendering(const QString& status, bool restoreFractalView = true);
    static void applySweepValue(
        Config& config, SweepParameter parameter, double value);
    void restoreSweepSettings();
    QString manifestPath() const;
    void writeManifest();
    void loadFractals();
    void persistLatestLayer(
        const QString& key, const QJsonObject& truth,
        const QJsonObject& renderSettings);
    void exportPng();
    void exportSimulationLoop();
    QString suggestedSavePath(const QString& fileName) const;
    void rememberSaveDirectory(const QString& path);
    void restoreSettings();
    void saveSettings();

    ParameterView* parameterView_ = nullptr;
    SimulationView* simulationView_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QLabel* renderStatus_ = nullptr;
    QLabel* simulationStatus_ = nullptr;
    QPushButton* renderButton_ = nullptr;
    QPushButton* sweepButton_ = nullptr;
    QPushButton* zoomButton_ = nullptr;
    QPushButton* cancelRenderButton_ = nullptr;
    QPushButton* pauseRenderButton_ = nullptr;
    QPushButton* resumeRenderButton_ = nullptr;
    QPushButton* pasteButton_ = nullptr;
    PreciseSpinBox* leftAngle_ = nullptr;
    PreciseSpinBox* rightAngle_ = nullptr;
    QDoubleSpinBox* gravity_ = nullptr;
    QDoubleSpinBox* radius_ = nullptr;
    QDoubleSpinBox* restitution_ = nullptr;
    QDoubleSpinBox* gap_ = nullptr;
    QDoubleSpinBox* segmentLength_ = nullptr;
    QDoubleSpinBox* spawnInterval_ = nullptr;
    QDoubleSpinBox* spawnY_ = nullptr;
    QDoubleSpinBox* cutoff_ = nullptr;
    QDoubleSpinBox* playbackSpeed_ = nullptr;
    PreciseSpinBox* xmin_ = nullptr;
    PreciseSpinBox* xmax_ = nullptr;
    PreciseSpinBox* ymin_ = nullptr;
    PreciseSpinBox* ymax_ = nullptr;
    QSpinBox* fractalWidth_ = nullptr;
    QSpinBox* fractalHeight_ = nullptr;
    QSpinBox* maxBalls_ = nullptr;
    QSpinBox* analysisBalls_ = nullptr;
    QSpinBox* collisionBudget_ = nullptr;
    QSpinBox* precisionBits_ = nullptr;
    QCheckBox* certificateShading_ = nullptr;
    QDoubleSpinBox* certificateShadingStrength_ = nullptr;
    QDoubleSpinBox* certificateShadingScale_ = nullptr;

    std::shared_ptr<std::atomic_bool> cancel_;
    std::shared_ptr<std::atomic_bool> previewCancel_;
    bool rendering_ = false;
    bool pauseRequested_ = false;
    bool applyingJson_ = false;
    QSettings settings_{"ChaosV", "ChaosV"};
    QTimer previewDelay_;
    QElapsedTimer renderClock_;
    QThreadPool renderPool_;
    QThreadPool previewPool_;
    std::optional<std::pair<PreciseDecimal, PreciseDecimal>> selectedAngles_;
    struct PixelProbeContext {
        int expectedPeriod = 0;
        int precisionBits = 0;
        int analysisBalls = 0;
        int maxBalls = 0;
        int collisionBudget = 0;
    };
    std::optional<PixelProbeContext> pixelProbe_;
    std::vector<QWidget*> lockedControls_;
    QString fractalDirectory_;
    QJsonArray storedLayers_;

    struct SweepState {
        SweepDefinition definition;
        Config baseConfig;
        std::array<PreciseDecimal, 4> axes;
        QString basePath;
        qint64 index = 0;
        qint64 count = 0;
        int width = 0;
        int height = 0;
    };
    std::optional<SweepState> sweep_;

    struct ZoomState {
        ZoomDefinition definition;
        QString basePath;
        qint64 index = 0;
        qint64 groupEnd = 0;
        std::vector<std::array<PreciseDecimal, 4>> groupAxes;
        QImage previousAnchor;
        std::array<PreciseDecimal, 4> previousAxes{};
    };
    std::optional<ZoomState> zoom_;
};
