#pragma once

#include "physics.h"
#include "precise_decimal.h"

#include <QColor>
#include <QImage>
#include <QLineF>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QTimer>
#include <QWidget>

#include <map>
#include <memory>
#include <array>
#include <optional>
#include <vector>

class QMouseEvent;
class QPainter;
class QResizeEvent;
class QDoubleSpinBox;
class QLabel;
class QToolButton;
class QWheelEvent;
class QCheckBox;
class QSlider;
class QSpinBox;

struct FractalPaletteSettings {
    bool ultraFractal = false;
    double curve = .5; // -1: sublinear, 0: linear, +1: superlinear
    int maximumPeriod = 50;
};

QColor periodColor(int period);
QColor periodColor(double period);
QColor periodColor(double period, const FractalPaletteSettings& settings);
QColor colorFor(const Result& result);
QColor colorFor(const Result& result, const FractalPaletteSettings& settings);
QColor colorForFractalValue(
    float value, const FractalPaletteSettings& settings);

class ParameterView : public QWidget {
    Q_OBJECT

public:
    struct Layer {
        QImage image;
        PreciseDecimal xmin = 0;
        PreciseDecimal xmax = 0;
        PreciseDecimal ymin = 0;
        PreciseDecimal ymax = 0;
        double detail = 0;
        int precisionBits = 0;
        int analysisBalls = 0;
        int maxBalls = 0;
        int collisionBudget = 0;
        std::shared_ptr<std::vector<int>> periods;
        std::shared_ptr<std::vector<float>> colorValues;
        QString periodsPath;
        QString colorValuesPath;
        FractalPaletteSettings appliedPalette;
        bool paletteKnown = false;
    };

    struct PixelSelection {
        PreciseDecimal x;
        PreciseDecimal y;
        int expectedPeriod = 0;
        int precisionBits = 0;
        int analysisBalls = 0;
        int maxBalls = 0;
        int collisionBudget = 0;
    };

    explicit ParameterView(QWidget* parent = nullptr);

    void setLayerKey(const QString& key);
    bool beginLayer(int width, int height,
                    const PreciseDecimal& left, const PreciseDecimal& right,
                    const PreciseDecimal& bottom, const PreciseDecimal& top,
                    const Config& renderConfig,
                    const FractalPaletteSettings& palette);
    void addStoredLayer(const QString& key, QImage image,
                        const QString& left, const QString& right,
                        const QString& bottom, const QString& top,
                        int precisionBits = 0, int analysisBalls = 0,
                        int maxBalls = 0, int collisionBudget = 0,
                        std::vector<int> periods = {},
                        std::vector<float> colorValues = {},
                        bool recolor = true,
                        const QString& periodsPath = {},
                        const QString& colorValuesPath = {},
                        const FractalPaletteSettings& savedPalette = {},
                        bool paletteKnown = false);
    std::optional<Layer> latestLayer() const;
    void removeLatestLayer();
    void clearActiveLayers();
    void setAxes(const PreciseDecimal& left, const PreciseDecimal& right,
                 const PreciseDecimal& bottom, const PreciseDecimal& top);
    void setSelection(const PreciseDecimal& x, const PreciseDecimal& y);
    std::array<PreciseDecimal, 4> axes() const;
    void tile(int x, int y, const QImage& image,
              const std::vector<int>& periods = {},
              const std::vector<float>& colorValues = {});
    void replaceLatestLayer(QImage image,
                            const std::array<PreciseDecimal, 4>& axes);
    QImage fractal() const;
    bool hasSelection() const;
    FractalPaletteSettings paletteSettings() const;
    void setPaletteSettings(const FractalPaletteSettings& settings);
    void setPaletteControlsEnabled(bool enabled);

signals:
    void chosen(const QString& x, const QString& y);
    void pixelChosen(const QString& x, const QString& y, int expectedPeriod,
                     int precisionBits, int analysisBalls, int maxBalls,
                     int collisionBudget);
    void zoomRequested(const QString& left, const QString& right,
                       const QString& bottom, const QString& top);
    void paletteSettingsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    std::vector<Layer>* activeLayers();
    const std::vector<Layer>* activeLayers() const;
    std::optional<PixelSelection> snapToPixel(
        std::pair<PreciseDecimal, PreciseDecimal> world);
    QImage composite(QSize size, const PreciseDecimal& left,
                     const PreciseDecimal& right,
                     const PreciseDecimal& bottom,
                     const PreciseDecimal& top) const;
    void drawLegend(QPainter& painter) const;
    void recolorLayers();
    bool loadPeriods(Layer& layer);
    bool loadColorValues(Layer& layer);
    QPointF dragEnd(QPointF raw, bool preserveAspect) const;
    QRect canvasRect() const;
    std::pair<PreciseDecimal, PreciseDecimal> toWorld(QPointF point) const;
    QPointF toScreen(
        std::pair<PreciseDecimal, PreciseDecimal> point) const;

    std::map<QString, std::vector<Layer>> layerSets_;
    QString activeKey_;
    PreciseDecimal xmin_ = 0;
    PreciseDecimal xmax_ = 90;
    PreciseDecimal ymin_ = -90;
    PreciseDecimal ymax_ = 0;
    std::optional<std::pair<PreciseDecimal, PreciseDecimal>> selection_;
    QToolButton* resetButton_ = nullptr;
    QToolButton* fractalButton_ = nullptr;
    QCheckBox* ultraPalette_ = nullptr;
    QWidget* paletteControls_ = nullptr;
    QSlider* paletteCurve_ = nullptr;
    QSpinBox* paletteMaximum_ = nullptr;
    QRectF dragRect_;
    QPointF dragStart_;
    QPointF lastPan_;
    bool selecting_ = false;
    bool panning_ = false;
};

class SimulationView : public QWidget {
    Q_OBJECT

public:
    explicit SimulationView(QWidget* parent = nullptr);

    void setResult(Result result);
    void setPlaybackSpeed(double speed);
    void setPlaybackControl(QDoubleSpinBox* control);
    void setCameraState(double x, double y, double zoom);
    QPointF cameraPosition() const;
    double cameraZoom() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct DisplayBall {
        double x = 0;
        double y = 0;
        double vx = 0;
        double vy = 0;
    };

    struct DisplayFrame {
        std::vector<DisplayBall> balls;
        double duration = 0;
    };

    void advanceAnimation();

    Result result_;
    std::vector<DisplayFrame> frames_;
    std::vector<QLineF> segments_;
    size_t frameIndex_ = 0;
    size_t loopStart_ = 0;
    double frameTime_ = 0;
    double playbackSpeed_ = 1;
    QLabel* playbackLabel_ = nullptr;
    QDoubleSpinBox* playbackControl_ = nullptr;
    double zoom_ = 1;
    QPointF camera_{0, 220};
    QPointF lastDrag_;
    bool dragging_ = false;
    Qt::MouseButton dragButton_ = Qt::NoButton;
    QTimer timer_;
};
