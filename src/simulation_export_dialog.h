#pragma once

#include <QColor>
#include <QDialog>
#include <QSettings>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

struct SimulationExportDefinition {
    int framesPerSpawn = 30;
    int width = 1920;
    int height = 1080;
    bool overridePeriod = false;
    int period = 1;
    double centerX = 0;
    double centerY = 220;
    double pixelsPerUnit = 1;
    double segmentWidth = 3;
    QColor background{18, 22, 30};
    QColor balls{100, 190, 255};
    QColor segments{230, 180, 80};
    bool antialias = true;
};

class SimulationExportDialog : public QDialog {
public:
    SimulationExportDialog(double cameraX, double cameraY, double cameraZoom,
                           QWidget* parent = nullptr);
    SimulationExportDefinition definition() const;

protected:
    void accept() override;

private:
    QPushButton* colorButton(const QString& key, const QColor& fallback);
    QColor buttonColor(QPushButton* button) const;

    QSpinBox* framesPerSpawn_ = nullptr;
    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QCheckBox* overridePeriod_ = nullptr;
    QSpinBox* period_ = nullptr;
    QDoubleSpinBox* centerX_ = nullptr;
    QDoubleSpinBox* centerY_ = nullptr;
    QDoubleSpinBox* zoom_ = nullptr;
    QDoubleSpinBox* segmentWidth_ = nullptr;
    QPushButton* background_ = nullptr;
    QPushButton* balls_ = nullptr;
    QPushButton* segments_ = nullptr;
    QCheckBox* antialias_ = nullptr;
    QSettings settings_{"ChaosV", "ChaosV"};
};
