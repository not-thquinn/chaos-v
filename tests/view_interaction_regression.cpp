#include "views.h"
#include "sweep_dialog.h"
#include "sequence_fuser_dialog.h"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QTemporaryDir>

#include <cmath>
#include <iostream>

namespace {

void sendMouse(
    QWidget& widget, QEvent::Type type, QPointF position,
    Qt::MouseButton button, Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, position, button, buttons, modifiers);
    QApplication::sendEvent(&widget, &event);
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    bool passed = true;

    if (sweepFrameCount(30, 10) != 300 ||
        sweepFrameCount(.1, 1) != 2 ||
        sweepFrameCount(0, 10) != 0) {
        std::cerr << "parameter sweep frame count failed\n";
        passed = false;
    }

    SweepDefinition sweep;
    sweep.startConfig.gravity = 100;
    sweep.endConfig.gravity = 500;
    sweep.startConfig.radius = 10;
    sweep.endConfig.radius = 30;
    sweep.startConfig.gap = 100;
    sweep.endConfig.gap = 300;
    sweep.startConfig.precisionBits = 64;
    sweep.endConfig.precisionBits = 256;
    const Config midpoint = interpolateSweepConfig(sweep, .5);
    if (std::abs(midpoint.gravity - 300) > 1e-12 ||
        std::abs(midpoint.radius - 20) > 1e-12 ||
        std::abs(midpoint.gap - 200) > 1e-12 ||
        std::abs(midpoint.spawnX + 100) > 1e-12 ||
        midpoint.precisionBits != 64) {
        std::cerr << "multi-parameter sweep interpolation failed\n";
        passed = false;
    }

    QTemporaryDir sequenceDirectory;
    if (!sequenceDirectory.isValid()) {
        std::cerr << "could not create image-sequence test directory\n";
        passed = false;
    } else {
        const auto framePath = [&](int number) {
            return sequenceDirectory.filePath(
                QString("sample_%1.png").arg(number, 3, 10, QChar('0')));
        };
        QImage frame(2, 1, QImage::Format_RGB32);
        frame.fill(Qt::blue);
        const bool imagesSaved = frame.save(framePath(1)) &&
                                 frame.save(framePath(2)) &&
                                 frame.save(framePath(3));
        const QStringList discovered = discoverImageSequence(framePath(1));
        if (!imagesSaved || discovered.size() != 3 ||
            discovered.front() != framePath(1) ||
            discovered.back() != framePath(3)) {
            std::cerr << "image sequence discovery failed\n";
            passed = false;
        }
    }

    ParameterView panView;
    panView.resize(500, 500);
    panView.setAxes(0, 90, -90, 0);
    bool panned = false;
    double panLeft = 0;
    double panRight = 0;
    double panBottom = 0;
    double panTop = 0;
    QObject::connect(
        &panView, &ParameterView::zoomRequested,
        [&](const QString& leftText, const QString& rightText,
            const QString& bottomText, const QString& topText) {
            const double left = leftText.toDouble();
            const double right = rightText.toDouble();
            const double bottom = bottomText.toDouble();
            const double top = topText.toDouble();
            panned = true;
            panLeft = left;
            panRight = right;
            panBottom = bottom;
            panTop = top;
            panView.setAxes(preciseDecimal(leftText), preciseDecimal(rightText),
                            preciseDecimal(bottomText), preciseDecimal(topText));
        });
    sendMouse(panView, QEvent::MouseButtonPress, {250, 230},
              Qt::MiddleButton, Qt::MiddleButton);
    sendMouse(panView, QEvent::MouseMove, {270, 240},
              Qt::NoButton, Qt::MiddleButton);
    sendMouse(panView, QEvent::MouseButtonRelease, {270, 240},
              Qt::MiddleButton, Qt::NoButton);
    if (!panned || panLeft >= 0 || panBottom <= -90 ||
        std::abs((panRight - panLeft) - 90) > 1e-12 ||
        std::abs((panTop - panBottom) - 90) > 1e-12) {
        std::cerr << "parameter-view middle pan failed\n";
        passed = false;
    }

    ParameterView aspectView;
    aspectView.resize(500, 500);
    aspectView.setAxes(10, 70, -90, -30);
    bool zoomed = false;
    double zoomXSpan = 0;
    double zoomYSpan = 0;
    QObject::connect(
        &aspectView, &ParameterView::zoomRequested,
        [&](const QString& leftText, const QString& rightText,
            const QString& bottomText, const QString& topText) {
            zoomed = true;
            const double left = leftText.toDouble();
            const double right = rightText.toDouble();
            const double bottom = bottomText.toDouble();
            const double top = topText.toDouble();
            zoomXSpan = right - left;
            zoomYSpan = top - bottom;
        });
    sendMouse(aspectView, QEvent::MouseButtonPress, {200, 180},
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(aspectView, QEvent::MouseMove, {300, 230},
              Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
    sendMouse(aspectView, QEvent::MouseButtonRelease, {300, 230},
              Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    if (!zoomed || std::abs(std::abs(zoomXSpan / zoomYSpan) - 1) > 1e-12) {
        std::cerr << "shift-drag aspect preservation failed\n";
        passed = false;
    }

    SimulationView simulationView;
    simulationView.resize(500, 500);
    const QPointF initialCamera = simulationView.cameraPosition();
    sendMouse(simulationView, QEvent::MouseButtonPress, {250, 250},
              Qt::MiddleButton, Qt::MiddleButton);
    sendMouse(simulationView, QEvent::MouseMove, {270, 260},
              Qt::NoButton, Qt::MiddleButton);
    sendMouse(simulationView, QEvent::MouseButtonRelease, {270, 260},
              Qt::MiddleButton, Qt::NoButton);
    const QPointF movedCamera = simulationView.cameraPosition();
    if (movedCamera == initialCamera) {
        std::cerr << "simulation-view middle pan failed\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
