#include "sequence_fuser_dialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <limits>

QStringList discoverImageSequence(const QString& firstFrame) {
    const QFileInfo first(firstFrame);
    if (!first.isFile())
        return {};

    const QRegularExpression expression("^(.*?)(\\d+)$");
    const auto match = expression.match(first.completeBaseName());
    if (!match.hasMatch())
        return {first.absoluteFilePath()};

    const QString prefix = match.captured(1);
    const QString digits = match.captured(2);
    bool validNumber = false;
    qlonglong number = digits.toLongLong(&validNumber);
    if (!validNumber)
        return {first.absoluteFilePath()};

    QStringList result;
    for (;;) {
        const QString name = QString("%1%2.%3")
            .arg(prefix, QString::number(number).rightJustified(
                             digits.size(), '0'), first.suffix());
        const QString path = first.dir().filePath(name);
        if (!QFileInfo(path).isFile())
            break;
        result.append(QFileInfo(path).absoluteFilePath());
        if (number == std::numeric_limits<qlonglong>::max())
            break;
        ++number;
    }
    return result;
}

SequenceFuserDialog::SequenceFuserDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Fuse image sequences");
    auto* form = new QFormLayout;
    inputs_.append(addSequenceRow("Sequence 1", false));
    inputs_.append(addSequenceRow("Sequence 2", false));
    inputs_.append(addSequenceRow("Sequence 3", true));
    for (int index = 0; index < inputs_.size(); ++index)
        form->addRow(index == 2 ? "Sequence 3 (optional)" :
                                 QString("Sequence %1").arg(index + 1),
                     inputs_[index]->parentWidget());

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &SequenceFuserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    setMinimumWidth(620);
}

QLineEdit* SequenceFuserDialog::addSequenceRow(
    const QString&, bool optional) {
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit(row);
    edit->setPlaceholderText(optional ? "Optional" : "Choose the first frame");
    auto* browse = new QPushButton("Browse…", row);
    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this, edit] {
        const QString current = edit->text();
        const QString initial = current.isEmpty()
            ? settings_.value("lastImageSaveDirectory").toString()
            : current;
        const QString path = QFileDialog::getOpenFileName(
            this, "Choose first sequence frame", initial,
            "Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)");
        if (!path.isEmpty()) {
            edit->setText(path);
            settings_.setValue(
                "lastImageSaveDirectory", QFileInfo(path).absolutePath());
        }
    });
    return edit;
}

QStringList SequenceFuserDialog::firstFrames() const {
    QStringList result;
    for (auto* input : inputs_)
        if (!input->text().trimmed().isEmpty())
            result.append(input->text().trimmed());
    return result;
}

void SequenceFuserDialog::accept() {
    const QStringList frames = firstFrames();
    if (frames.size() < 2 || frames.size() > 3) {
        QMessageBox::warning(
            this, "Choose sequences", "Choose two or three image sequences.");
        return;
    }
    for (const QString& frame : frames) {
        if (!QFileInfo(frame).isFile()) {
            QMessageBox::warning(
                this, "Missing image", "An input image does not exist:\n" + frame);
            return;
        }
    }
    settings_.sync();
    QDialog::accept();
}
