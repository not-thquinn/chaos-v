#pragma once

#include <QDialog>
#include <QSettings>
#include <QStringList>

class QLineEdit;

QStringList discoverImageSequence(const QString& firstFrame);

class SequenceFuserDialog : public QDialog {
public:
    explicit SequenceFuserDialog(QWidget* parent = nullptr);

    QStringList firstFrames() const;

protected:
    void accept() override;

private:
    QLineEdit* addSequenceRow(const QString& label, bool optional);

    QList<QLineEdit*> inputs_;
    QSettings settings_{"ChaosV", "ChaosV"};
};
