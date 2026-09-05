#include "views.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

QColor periodColor(int period) {
    const double hue =
        55. + 245. * (1. - std::exp(-.24 * std::max(0, period - 1)));
    return QColor::fromHsv(int(hue) % 360, 210, 245);
}

QColor colorFor(const Result& result) {
    switch (result.outcome) {
    case Outcome::Periodic:
        return periodColor(result.period);
    case Outcome::CollisionBudget:
        return QColor(175, 35, 45);
    case Outcome::SpawnBlocked:
        return QColor(255, 140, 55);
    case Outcome::LiveCapacity:
        return QColor(245, 90, 90);
    case Outcome::Unresolved:
        return QColor(20, 20, 28);
    }
    return QColor(20, 20, 28);
}

ParameterView::ParameterView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 360);

    resetButton_ = new QToolButton(this);
    resetButton_->setText("Reset");
    resetButton_->setToolTip("Reset view to X=[0,90], Y=[-90,0]");
    connect(resetButton_, &QToolButton::clicked, this, [this] {
        emit zoomRequested("0", "90", "-90", "0");
    });

    fractalButton_ = new QToolButton(this);
    fractalButton_->setText("Fractal view");
    fractalButton_->setToolTip("Show the most recently generated fractal");
    connect(fractalButton_, &QToolButton::clicked, this, [this] {
        const auto* layers = activeLayers();
        if (!layers || layers->empty())
            return;
        const auto& layer = layers->back();
        emit zoomRequested(
            preciseString(layer.xmin), preciseString(layer.xmax),
            preciseString(layer.ymin), preciseString(layer.ymax));
    });
}

void ParameterView::setLayerKey(const QString& key) {
    if (activeKey_ == key)
        return;
    activeKey_ = key;
    update();
}

bool ParameterView::beginLayer(
    int width, int height, const PreciseDecimal& left,
    const PreciseDecimal& right, const PreciseDecimal& bottom,
    const PreciseDecimal& top, const Config& renderConfig) {
    PreciseDecimal area = abs((right - left) * (top - bottom));
    if (area == 0)
        return false;
    const double detail = preciseDouble(PreciseDecimal(width) * height / area);
    Layer layer{
        QImage(width, height, QImage::Format_RGB32),
        left, right, bottom, top, detail, renderConfig.precisionBits,
        renderConfig.analysisBalls, renderConfig.maxBalls,
        renderConfig.collisionBudget,
        std::make_shared<std::vector<int>>(size_t(width) * height)
    };
    if (layer.image.isNull())
        return false;
    layer.image.fill(QColor(15, 15, 20));
    layerSets_[activeKey_].push_back(std::move(layer));
    update();
    return true;
}

void ParameterView::addStoredLayer(
    const QString& key, QImage image, const QString& leftText,
    const QString& rightText, const QString& bottomText,
    const QString& topText, int precisionBits, int analysisBalls,
    int maxBalls, int collisionBudget, std::vector<int> periods) {
    if (image.isNull())
        return;
    try {
        const PreciseDecimal left = preciseDecimal(leftText);
        const PreciseDecimal right = preciseDecimal(rightText);
        const PreciseDecimal bottom = preciseDecimal(bottomText);
        const PreciseDecimal top = preciseDecimal(topText);
        const PreciseDecimal area = abs((right - left) * (top - bottom));
        if (area == 0)
            return;
        const double detail = preciseDouble(
            PreciseDecimal(image.width()) * image.height() / area);
        layerSets_[key].push_back(
            {std::move(image), left, right, bottom, top, detail,
             precisionBits, analysisBalls, maxBalls, collisionBudget, {}});
        auto& stored = layerSets_[key].back();
        if (periods.size() == size_t(stored.image.width()) * stored.image.height())
            stored.periods = std::make_shared<std::vector<int>>(std::move(periods));
    } catch (...) {
        return;
    }
    update();
}

std::optional<ParameterView::Layer> ParameterView::latestLayer() const {
    const auto* layers = activeLayers();
    if (!layers || layers->empty())
        return {};
    return layers->back();
}

void ParameterView::removeLatestLayer() {
    auto* layers = activeLayers();
    if (!layers || layers->empty())
        return;
    layers->pop_back();
    update();
}

void ParameterView::replaceLatestLayer(
    QImage image, const std::array<PreciseDecimal, 4>& axes) {
    auto* layers = activeLayers();
    if (!layers || layers->empty() || image.isNull())
        return;
    auto& layer = layers->back();
    layer.image = std::move(image);
    layer.xmin = axes[0]; layer.xmax = axes[1];
    layer.ymin = axes[2]; layer.ymax = axes[3];
    const PreciseDecimal area = abs((axes[1] - axes[0]) * (axes[3] - axes[2]));
    layer.detail = area == 0 ? 0 : preciseDouble(
        PreciseDecimal(layer.image.width()) * layer.image.height() / area);
    update();
}

void ParameterView::clearActiveLayers() {
    auto* layers = activeLayers();
    if (!layers)
        return;
    layers->clear();
    update();
}

void ParameterView::setAxes(
    const PreciseDecimal& left, const PreciseDecimal& right,
    const PreciseDecimal& bottom, const PreciseDecimal& top) {
    if (left == right || bottom == top)
        return;
    xmin_ = left;
    xmax_ = right;
    ymin_ = bottom;
    ymax_ = top;
    update();
}

void ParameterView::setSelection(
    const PreciseDecimal& x, const PreciseDecimal& y) {
    selection_ = {{x, y}};
    update();
}

std::array<PreciseDecimal, 4> ParameterView::axes() const {
    return {xmin_, xmax_, ymin_, ymax_};
}

void ParameterView::tile(
    int x, int y, const QImage& image, const std::vector<int>& periods) {
    auto* layers = activeLayers();
    if (!layers || layers->empty())
        return;
    QPainter painter(&layers->back().image);
    painter.drawImage(x, y, image);
    auto& layer = layers->back();
    if (!periods.empty() &&
        periods.size() == size_t(image.width()) * image.height() &&
        layer.periods &&
        layer.periods->size() == size_t(layer.image.width()) * layer.image.height()) {
        for (int row = 0; row < image.height(); ++row)
            std::copy_n(periods.begin() + size_t(row) * image.width(),
                        image.width(),
                        layer.periods->begin() + size_t(y + row) * layer.image.width() + x);
    }
    update();
}

QImage ParameterView::fractal() const {
    const auto* layers = activeLayers();
    if (!layers || layers->empty())
        return composite(QSize(1, 1), PreciseDecimal(0), PreciseDecimal(1),
                         PreciseDecimal(0), PreciseDecimal(1));
    const auto& latest = layers->back();
    return composite(latest.image.size(), latest.xmin, latest.xmax,
                     latest.ymin, latest.ymax);
}

bool ParameterView::hasSelection() const {
    return selection_.has_value();
}

void ParameterView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    const QRect target = canvasRect();
    painter.drawImage(target, composite(target.size(), xmin_, xmax_, ymin_, ymax_));

    painter.setPen(Qt::white);
    const auto axisText = [](const char* name, const PreciseDecimal& first,
                             const PreciseDecimal& last) {
        const PreciseDecimal center = (first + last) / 2;
        const PreciseDecimal radius = abs(last - first) / 2;
        const auto ordinary = [](const PreciseDecimal& value) {
            QString text = QString::fromStdString(
                value.str(12, std::ios_base::fixed));
            while (text.contains('.') && text.endsWith('0'))
                text.chop(1);
            if (text.endsWith('.'))
                text.chop(1);
            return text;
        };
        const auto scientific = [](const PreciseDecimal& value) {
            return QString::fromStdString(
                value.str(5, std::ios_base::scientific));
        };
        return QString("%1: %2 ± %3°")
            .arg(QString::fromLatin1(name), ordinary(center),
                 scientific(radius));
    };
    painter.drawText(8, 16, axisText("X", xmin_, xmax_));
    painter.drawText(8, 33, axisText("Y", ymin_, ymax_));

    if (selection_) {
        painter.setPen(QPen(Qt::white, 1));
        const QPointF point = toScreen(*selection_);
        painter.drawLine(int(point.x()) - 7, int(point.y()),
                         int(point.x()) + 7, int(point.y()));
        painter.drawLine(int(point.x()), int(point.y()) - 7,
                         int(point.x()), int(point.y()) + 7);
    }

    if (selecting_) {
        painter.setPen(QPen(QColor(230, 230, 255), 1, Qt::DashLine));
        painter.setBrush(QColor(180, 190, 255, 30));
        painter.drawRect(dragRect_.normalized());
    }
    drawLegend(painter);
}

void ParameterView::resizeEvent(QResizeEvent*) {
    resetButton_->move(width() - resetButton_->sizeHint().width() - 6, 2);
    fractalButton_->move(
        width() - resetButton_->sizeHint().width() -
            fractalButton_->sizeHint().width() - 12,
        2);
}

void ParameterView::mousePressEvent(QMouseEvent* event) {
    if (!canvasRect().contains(event->pos()))
        return;
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPan_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    selecting_ = true;
    dragStart_ = event->position();
    dragRect_ = QRectF(dragStart_, QSizeF());
    update();
}

void ParameterView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const auto before = toWorld(lastPan_);
        const auto after = toWorld(event->position());
        const PreciseDecimal xShift = before.first - after.first;
        const PreciseDecimal yShift = before.second - after.second;
        emit zoomRequested(
            preciseString(xmin_ + xShift), preciseString(xmax_ + xShift),
            preciseString(ymin_ + yShift), preciseString(ymax_ + yShift));
        lastPan_ = event->position();
        event->accept();
        return;
    }
    if (!selecting_)
        return;

    dragRect_ = QRectF(
        dragStart_,
        dragEnd(event->position(), event->modifiers() & Qt::ShiftModifier));
    update();
}

void ParameterView::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && event->button() == Qt::MiddleButton) {
        panning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    if (!selecting_ || event->button() != Qt::LeftButton)
        return;
    dragRect_ = QRectF(
        dragStart_,
        dragEnd(event->position(), event->modifiers() & Qt::ShiftModifier));
    selecting_ = false;

    const QRectF selection = dragRect_.normalized();
    if (selection.width() >= 8 && selection.height() >= 8) {
        const auto first = toWorld(selection.topLeft());
        const auto second = toWorld(selection.bottomRight());
        emit zoomRequested(
            preciseString(first.first), preciseString(second.first),
            preciseString(second.second), preciseString(first.second));
    } else {
        auto point = toWorld(event->position());
        if (event->modifiers() & Qt::ShiftModifier) {
            if (const auto snapped = snapToPixel(point)) {
                point = {snapped->x, snapped->y};
                selection_ = point;
                emit pixelChosen(
                    preciseString(point.first), preciseString(point.second),
                    snapped->expectedPeriod, snapped->precisionBits,
                    snapped->analysisBalls, snapped->maxBalls,
                    snapped->collisionBudget);
                update();
                return;
            }
        }
        selection_ = point;
        emit chosen(preciseString(point.first), preciseString(point.second));
    }
    update();
}

void ParameterView::wheelEvent(QWheelEvent* event) {
    if (!canvasRect().contains(event->position().toPoint()))
        return;
    const double steps = event->angleDelta().y() / 120.;
    if (steps == 0)
        return;

    const auto focus = toWorld(event->position());
    const PreciseDecimal scale = preciseDecimal(std::pow(1.15, -steps));
    const PreciseDecimal left = focus.first + (xmin_ - focus.first) * scale;
    const PreciseDecimal right = focus.first + (xmax_ - focus.first) * scale;
    const PreciseDecimal bottom = focus.second + (ymin_ - focus.second) * scale;
    const PreciseDecimal top = focus.second + (ymax_ - focus.second) * scale;
    emit zoomRequested(preciseString(left), preciseString(right),
                       preciseString(bottom), preciseString(top));
    event->accept();
}

std::vector<ParameterView::Layer>* ParameterView::activeLayers() {
    const auto iterator = layerSets_.find(activeKey_);
    return iterator == layerSets_.end() ? nullptr : &iterator->second;
}

const std::vector<ParameterView::Layer>* ParameterView::activeLayers() const {
    const auto iterator = layerSets_.find(activeKey_);
    return iterator == layerSets_.end() ? nullptr : &iterator->second;
}

std::optional<ParameterView::PixelSelection>
ParameterView::snapToPixel(
    std::pair<PreciseDecimal, PreciseDecimal> world) const {
    const auto* layers = activeLayers();
    if (!layers)
        return {};

    const Layer* best = nullptr;
    for (const auto& layer : *layers) {
        const PreciseDecimal left = std::min(layer.xmin, layer.xmax);
        const PreciseDecimal right = std::max(layer.xmin, layer.xmax);
        const PreciseDecimal bottom = std::min(layer.ymin, layer.ymax);
        const PreciseDecimal top = std::max(layer.ymin, layer.ymax);
        if (world.first >= left && world.first <= right &&
            world.second >= bottom && world.second <= top &&
            (!best || layer.detail >= best->detail))
            best = &layer;
    }
    if (!best)
        return {};

    const double ux = preciseDouble(
        (world.first - best->xmin) / (best->xmax - best->xmin));
    const double uy = preciseDouble(
        (best->ymax - world.second) / (best->ymax - best->ymin));
    const int x = std::clamp(int(std::floor(ux * best->image.width())),
                             0, best->image.width() - 1);
    const int y = std::clamp(int(std::floor(uy * best->image.height())),
                             0, best->image.height() - 1);
    const PreciseDecimal snappedX =
        best->xmin + (PreciseDecimal(x) + PreciseDecimal("0.5")) /
                         best->image.width() * (best->xmax - best->xmin);
    const PreciseDecimal snappedY =
        best->ymax - (PreciseDecimal(y) + PreciseDecimal("0.5")) /
                         best->image.height() * (best->ymax - best->ymin);
    const QColor pixel = best->image.pixelColor(x, y);
    int expectedPeriod = best->periods && best->periods->size() ==
                                 size_t(best->image.width()) * best->image.height()
                             ? (*best->periods)[size_t(y) * best->image.width() + x]
                             : 0;
    for (int period = expectedPeriod ? 100001 : 1; period <= 100000; ++period) {
        if (periodColor(period).rgb() == pixel.rgb()) {
            expectedPeriod = period;
            break;
        }
        // The exponential palette converges; beyond this point distinct
        // integer periods no longer have distinct RGB values.
        if (period > 100 && periodColor(period) == periodColor(period + 1))
            break;
    }
    return PixelSelection{snappedX, snappedY, expectedPeriod,
                          best->precisionBits, best->analysisBalls,
                          best->maxBalls, best->collisionBudget};
}

QImage ParameterView::composite(
    QSize size, const PreciseDecimal& left, const PreciseDecimal& right,
    const PreciseDecimal& bottom, const PreciseDecimal& top) const {
    QImage output(size, QImage::Format_RGB32);
    output.fill(QColor(15, 15, 20));

    std::vector<const Layer*> ordered;
    if (const auto* layers = activeLayers())
        for (const auto& layer : *layers)
            ordered.push_back(&layer);
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Layer* first, const Layer* second) {
                         return first->detail < second->detail;
                     });

    // Let Qt's raster engine clip and scale each layer. The former code walked
    // every destination pixel once per layer on every paint, which made wheel
    // zoom progressively unusable as detailed layers accumulated.
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setClipRect(output.rect());
    for (const Layer* layer : ordered) {
        const double x0 = preciseDouble(
            (layer->xmin - left) / (right - left)) * size.width();
        const double x1 = preciseDouble(
            (layer->xmax - left) / (right - left)) * size.width();
        const double y0 = preciseDouble(
            (top - layer->ymax) / (top - bottom)) * size.height();
        const double y1 = preciseDouble(
            (top - layer->ymin) / (top - bottom)) * size.height();

        QTransform transform;
        transform.translate(x0, y0);
        transform.scale((x1 - x0) / layer->image.width(),
                        (y1 - y0) / layer->image.height());
        painter.save();
        painter.setTransform(transform);
        painter.drawImage(QPointF(0, 0), layer->image);
        painter.restore();
    }
    return output;
}

void ParameterView::drawLegend(QPainter& painter) const {
    const int x = 8;
    const int y = height() - 55;
    const int width = std::max(32, this->width() - 16);
    constexpr int height = 10;

    QImage gradient(width, height, QImage::Format_RGB32);
    for (int column = 0; column < width; ++column) {
        const double u = double(column) / std::max(1, width - 1);
        const double period = 1 - std::log(std::max(1e-6, 1 - u)) / .24;
        for (int row = 0; row < height; ++row)
            gradient.setPixelColor(
                column, row, periodColor(int(std::round(period))));
    }
    painter.drawImage(x, y, gradient);

    painter.setPen(QColor(220, 220, 220));
    painter.setFont(QFont(painter.font().family(), 8));
    for (const int period : {1, 2, 3, 4, 5, 10, 20}) {
        const double u = 1 - std::exp(-.24 * (period - 1));
        const int tick = std::clamp(
            x + int(u * (width - 1)), x, x + width - 1);
        painter.drawLine(tick, y + height, tick, y + height + 3);
        painter.drawText(QRect(tick - 12, y + height + 3, 24, 12),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QString::number(period));
    }

    const QStringList labels{"unresolved", "budget", "blocked", "capacity"};
    int totalWidth = 0;
    for (const auto& label : labels)
        totalWidth += 17 + painter.fontMetrics().horizontalAdvance(label);
    int itemX = std::max(x, x + (width - totalWidth) / 2);
    const int itemY = this->height() - 17;
    auto item = [&](QColor color, const QString& text) {
        painter.fillRect(itemX, itemY, 10, 10, color);
        painter.drawText(itemX + 13, itemY + 9, text);
        itemX += 17 + painter.fontMetrics().horizontalAdvance(text);
    };
    item(QColor(20, 20, 28), labels[0]);
    item(QColor(175, 35, 45), labels[1]);
    item(QColor(255, 140, 55), labels[2]);
    item(QColor(245, 90, 90), labels[3]);
}

QPointF ParameterView::dragEnd(QPointF raw, bool preserveAspect) const {
    const QRect canvas = canvasRect();
    // toWorld maps pixel centers; these half-pixel boundaries therefore map
    // exactly to the current axis limits.
    const QRectF bounds(
        canvas.left() - .5, canvas.top() - .5,
        canvas.width(), canvas.height());
    QPointF end{
        std::clamp(raw.x(), bounds.left(), bounds.right()),
        std::clamp(raw.y(), bounds.top(), bounds.bottom())
    };
    if (!preserveAspect)
        return end;

    const double dx = end.x() - dragStart_.x();
    const double dy = end.y() - dragStart_.y();
    const double xDirection = dx < 0 ? -1. : 1.;
    const double yDirection = dy < 0 ? -1. : 1.;
    double scale = std::max(
        std::abs(dx) / bounds.width(),
        std::abs(dy) / bounds.height());
    const double xLimit =
        (xDirection < 0 ? dragStart_.x() - bounds.left()
                        : bounds.right() - dragStart_.x()) /
        bounds.width();
    const double yLimit =
        (yDirection < 0 ? dragStart_.y() - bounds.top()
                        : bounds.bottom() - dragStart_.y()) /
        bounds.height();
    scale = std::min(scale, std::min(xLimit, yLimit));
    return {
        dragStart_.x() + xDirection * scale * bounds.width(),
        dragStart_.y() + yDirection * scale * bounds.height()
    };
}

QRect ParameterView::canvasRect() const {
    const QRect available = rect().adjusted(8, 44, -8, -68);
    const int side = std::min(available.width(), available.height());
    return QRect(available.center().x() - side / 2,
                 available.center().y() - side / 2, side, side);
}

std::pair<PreciseDecimal, PreciseDecimal> ParameterView::toWorld(
    QPointF point) const {
    const QRect target = canvasRect();
    const double width = std::max(1, target.width());
    const double height = std::max(1, target.height());
    const PreciseDecimal xRatio = preciseDecimal(
        (point.x() - target.left() + .5) / width);
    const PreciseDecimal yRatio = preciseDecimal(
        (point.y() - target.top() + .5) / height);
    return {xmin_ + xRatio * (xmax_ - xmin_),
            ymax_ - yRatio * (ymax_ - ymin_)};
}

QPointF ParameterView::toScreen(
    std::pair<PreciseDecimal, PreciseDecimal> point) const {
    const QRect target = canvasRect();
    const double width = std::max(1, target.width());
    const double height = std::max(1, target.height());
    return {
        target.left() + preciseDouble(
            (point.first - xmin_) / (xmax_ - xmin_)) * width - .5,
        target.top() + preciseDouble(
            (ymax_ - point.second) / (ymax_ - ymin_)) * height - .5
    };
}

SimulationView::SimulationView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 360);
    setMouseTracking(true);
    connect(&timer_, &QTimer::timeout, this, &SimulationView::advanceAnimation);
    timer_.start(1000 / 60);
}

void SimulationView::setResult(Result result) {
    result_ = std::move(result);
    frames_.clear();
    segments_.clear();
    for (const auto& segment : result_.segments) {
        segments_.push_back({
            QPointF(toDouble(segment.a.x), toDouble(segment.a.y)),
            QPointF(toDouble(segment.b.x), toDouble(segment.b.y))
        });
    }

    const auto appendFrame = [this](
        const AnimFrame& frame, double offset, double duration) {
        DisplayFrame display;
        display.duration = duration;
        display.balls.reserve(frame.balls.size());
        const Real realOffset = makeReal(offset);
        for (auto ball : frame.balls) {
            ball.p = position(ball, realOffset, makeReal(result_.gravity));
            ball.v.y += makeReal(result_.gravity) * realOffset;
            display.balls.push_back({
                toDouble(ball.p.x), toDouble(ball.p.y),
                toDouble(ball.v.x), toDouble(ball.v.y)
            });
        }
        frames_.push_back(std::move(display));
    };

    if (result_.outcome == Outcome::Periodic && !result_.frames.empty()) {
        const double end =
            result_.frames.back().start + result_.frames.back().duration;
        const double start = end - result_.period * result_.spawnInterval;
        for (const auto& frame : result_.frames) {
            const double frameStart = std::max(start, frame.start);
            const double frameEnd = std::min(end, frame.start + frame.duration);
            if (frameEnd <= frameStart)
                continue;

            appendFrame(frame, frameStart - frame.start, frameEnd - frameStart);
        }
    } else {
        for (const auto& frame : result_.frames)
            appendFrame(frame, 0, frame.duration);
    }

    // Painting no longer needs the arbitrary-precision trajectory after the
    // compact display copy has been prepared.
    result_.frames.clear();
    result_.exits.clear();
    result_.exitIds.clear();

    frameIndex_ = 0;
    frameTime_ = 0;
    loopStart_ = 0;
    update();
}

void SimulationView::setPlaybackSpeed(double speed) {
    playbackSpeed_ = speed;
}

void SimulationView::setPlaybackControl(QDoubleSpinBox* control) {
    playbackControl_ = control;
    playbackControl_->setParent(this);
    playbackLabel_ = new QLabel("Playback speed", this);
    playbackLabel_->show();
    playbackControl_->show();
    resizeEvent(nullptr);
}

void SimulationView::resizeEvent(QResizeEvent*) {
    if (!playbackControl_ || !playbackLabel_)
        return;
    playbackLabel_->adjustSize();
    const int controlWidth = 114;
    playbackControl_->setGeometry(
        width() - controlWidth - 10, 8, controlWidth,
        playbackControl_->sizeHint().height());
    playbackLabel_->move(
        playbackControl_->x() - playbackLabel_->width() - 7,
        11);
}

void SimulationView::setCameraState(double x, double y, double zoom) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(zoom))
        return;
    camera_ = QPointF(x, y);
    zoom_ = std::clamp(zoom, .08, 20.);
    update();
}

QPointF SimulationView::cameraPosition() const {
    return camera_;
}

double SimulationView::cameraZoom() const {
    return zoom_;
}

void SimulationView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(18, 22, 30));
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = rect().adjusted(10, 62, -10, -10);
    const auto map = [&](double x, double y) {
        return QPointF(
            area.center().x() + (x - camera_.x()) * zoom_,
            area.center().y() + (y - camera_.y()) * zoom_);
    };

    double target = 80 / zoom_;
    double unit = 1;
    while (unit < target)
        unit *= 10;
    if (unit / 5 >= target)
        unit /= 5;
    else if (unit / 2 >= target)
        unit /= 2;

    const double low = camera_.y() - area.height() / (2 * zoom_);
    const double high = camera_.y() + area.height() / (2 * zoom_);
    const double first = std::ceil(low / unit) * unit;
    painter.setPen(QPen(QColor(110, 130, 150, 55), 1, Qt::DashLine));
    painter.setFont(QFont(painter.font().family(), 8));
    for (double y = first; y <= high; y += unit) {
        const double screenY = area.center().y() + (y - camera_.y()) * zoom_;
        painter.drawLine(QPointF(area.left(), screenY),
                         QPointF(area.right(), screenY));
        painter.setPen(QColor(165, 180, 195, 115));
        painter.drawText(QPointF(area.left() + 4, screenY - 3),
                         QString::number(y, 'f', 0));
        painter.setPen(QPen(QColor(110, 130, 150, 55), 1, Qt::DashLine));
    }

    painter.setPen(QPen(QColor(230, 180, 80), 3));
    for (const auto& segment : segments_)
        painter.drawLine(map(segment.x1(), segment.y1()),
                         map(segment.x2(), segment.y2()));

    if (!frames_.empty()) {
        painter.setBrush(QColor(100, 190, 255));
        painter.setPen(Qt::NoPen);
        const double radius = result_.radius * zoom_;
        for (const auto& ball : frames_[frameIndex_].balls) {
            const double x = ball.x + ball.vx * frameTime_;
            const double y = ball.y + ball.vy * frameTime_ +
                             result_.gravity * frameTime_ * frameTime_ / 2;
            painter.drawEllipse(map(x, y), radius, radius);
        }
    }

    painter.setPen(Qt::white);
    QString status;
    switch (result_.outcome) {
    case Outcome::Periodic:
        status = result_.periodFromRenderedPixel
                     ? QString("Rendered pixel period: %1").arg(result_.period)
                     : QString("Detected period: %1").arg(result_.period);
        break;
    case Outcome::CollisionBudget:
        status = "Undefined: collision budget exceeded";
        break;
    case Outcome::SpawnBlocked:
        status = "Undefined: spawn point blocked";
        break;
    case Outcome::LiveCapacity:
        status = "Undefined: max live balls reached";
        break;
    case Outcome::Unresolved:
        status = "Period: unresolved";
        break;
    }
    painter.drawText(10, 18, status);
    if (result_.outcome == Outcome::Periodic &&
        !result_.periodFromRenderedPixel) {
        painter.drawText(
            10, 34,
            QString("Balls spawned before detection: %1")
                .arg(result_.ballsSpawnedAtDetection));
        painter.drawText(
            10, 50,
            QString("Collision events before detection: %1")
                .arg(result_.collisionsAtDetection));
    }

}

void SimulationView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton &&
        event->button() != Qt::MiddleButton)
        return;
    dragging_ = true;
    dragButton_ = event->button();
    lastDrag_ = event->position();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void SimulationView::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_)
        return;
    const QPointF delta = event->position() - lastDrag_;
    camera_.setX(camera_.x() - delta.x() / zoom_);
    camera_.setY(camera_.y() - delta.y() / zoom_);
    lastDrag_ = event->position();
    update();
}

void SimulationView::mouseReleaseEvent(QMouseEvent* event) {
    if (dragging_ && event->button() == dragButton_) {
        dragging_ = false;
        dragButton_ = Qt::NoButton;
        unsetCursor();
        event->accept();
    }
}

void SimulationView::wheelEvent(QWheelEvent* event) {
    const QRectF area = rect().adjusted(10, 45, -10, -10);
    if (!area.contains(event->position()))
        return;
    const double steps = event->angleDelta().y() / 120.;
    if (steps == 0)
        return;

    const double oldZoom = zoom_;
    zoom_ = std::clamp(zoom_ * std::pow(1.15, steps), .08, 20.);
    const QPointF delta = event->position() - area.center();
    camera_.setX(camera_.x() + delta.x() * (1 / oldZoom - 1 / zoom_));
    camera_.setY(camera_.y() + delta.y() * (1 / oldZoom - 1 / zoom_));
    update();
    event->accept();
}

void SimulationView::advanceAnimation() {
    if (frames_.empty())
        return;
    frameTime_ += playbackSpeed_ / 60.;
    while (frameTime_ >= frames_[frameIndex_].duration &&
           frames_[frameIndex_].duration > 0) {
        frameTime_ -= frames_[frameIndex_].duration;
        ++frameIndex_;
        if (frameIndex_ >= frames_.size())
            frameIndex_ = loopStart_;
    }
    update();
}
