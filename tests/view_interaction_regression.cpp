#include "views.h"

#include <QApplication>
#include <QMouseEvent>

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
