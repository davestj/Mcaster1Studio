#include "ScanProgressDialog.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>

// ─── ScanProgressDialog ──────────────────────────────────────────────────────

ScanProgressDialog::ScanProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName("ScanProgressDialog");
    setWindowTitle(QStringLiteral("Library Scan Progress"));
    setFixedSize(500, 400);
    setModal(true);

    auto pal = ThemePalette::forCurrentTheme();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    // Phase label
    m_phaseLabel = new QLabel(QStringLiteral("Preparing scan..."), this);
    m_phaseLabel->setObjectName("ScanPhaseLabel");
    m_phaseLabel->setStyleSheet(
        QString("QLabel { font-size: 14px; font-weight: bold; color: %1; }")
            .arg(pal.text.name()));
    mainLayout->addWidget(m_phaseLabel);

    // Percentage + progress bar row
    auto* progressRow = new QHBoxLayout;
    progressRow->setSpacing(8);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName("ScanProgressBar");
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(20);
    m_progressBar->setStyleSheet(
        QString("QProgressBar { "
                "  background: %1; "
                "  border: 1px solid %2; "
                "  border-radius: 3px; "
                "} "
                "QProgressBar::chunk { "
                "  background: %3; "
                "  border-radius: 2px; "
                "}")
            .arg(pal.inputBg.name(), pal.border.name(), pal.accent.name()));
    progressRow->addWidget(m_progressBar, 1);

    m_percentLabel = new QLabel(QStringLiteral("0%"), this);
    m_percentLabel->setObjectName("ScanPercentLabel");
    m_percentLabel->setFixedWidth(40);
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_percentLabel->setStyleSheet(
        QString("QLabel { font-size: 13px; font-weight: bold; color: %1; }")
            .arg(pal.accent.name()));
    progressRow->addWidget(m_percentLabel);

    mainLayout->addLayout(progressRow);

    // Count label
    m_countLabel = new QLabel(QStringLiteral("0 / 0 files"), this);
    m_countLabel->setObjectName("ScanCountLabel");
    m_countLabel->setStyleSheet(
        QString("QLabel { font-size: 12px; color: %1; }")
            .arg(pal.textMuted.name()));
    mainLayout->addWidget(m_countLabel);

    // File list (scrolling list of recent files)
    m_fileList = new QListWidget(this);
    m_fileList->setObjectName("ScanFileList");
    m_fileList->setMaximumHeight(220);
    m_fileList->setStyleSheet(
        QString("QListWidget { "
                "  background: %1; "
                "  color: %2; "
                "  border: 1px solid %3; "
                "  font-size: 12px; "
                "  font-family: Consolas, monospace; "
                "} "
                "QListWidget::item { "
                "  padding: 1px 4px; "
                "}")
            .arg(pal.inputBg.name(), pal.textMuted.name(), pal.border.name()));
    mainLayout->addWidget(m_fileList, 1);

    // Result label (hidden initially)
    m_resultLabel = new QLabel(this);
    m_resultLabel->setObjectName("ScanResultLabel");
    m_resultLabel->setStyleSheet(
        QString("QLabel { font-size: 13px; font-weight: bold; color: %1; }")
            .arg(pal.success.name()));
    m_resultLabel->hide();
    mainLayout->addWidget(m_resultLabel);

    // Button row
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelBtn->setObjectName("ScanCancelBtn");
    m_cancelBtn->setFixedWidth(100);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &ScanProgressDialog::onCancelClicked);
    btnRow->addWidget(m_cancelBtn);

    m_closeBtn = new QPushButton(QStringLiteral("Close"), this);
    m_closeBtn->setObjectName("ScanCloseBtn");
    m_closeBtn->setFixedWidth(100);
    m_closeBtn->hide();
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(m_closeBtn);

    mainLayout->addLayout(btnRow);
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void ScanProgressDialog::onScanStarted(int estimatedCount) {
    m_filesFound = 0;
    m_phaseLabel->setText(QStringLiteral("Reading tags..."));
    m_progressBar->setRange(0, estimatedCount > 0 ? estimatedCount : 100);
    m_progressBar->setValue(0);
    m_percentLabel->setText(QStringLiteral("0%"));
    m_countLabel->setText(QString("0 / %1 files").arg(estimatedCount));
    m_fileList->clear();
    m_resultLabel->hide();
    m_cancelBtn->show();
    m_cancelBtn->setText(QStringLiteral("Cancel"));
    m_cancelBtn->setEnabled(true);
    m_closeBtn->hide();
}

void ScanProgressDialog::onScanProgress(int done, int total) {
    m_progressBar->setMaximum(total > 0 ? total : 100);
    m_progressBar->setValue(done);

    int pct = (total > 0) ? qBound(0, static_cast<int>(100.0 * done / total), 100) : 0;
    m_percentLabel->setText(QString("%1%").arg(pct));
    m_countLabel->setText(QString("%1 / %2 files").arg(done).arg(total));
}

void ScanProgressDialog::onScanFileFound(const QString& path) {
    m_filesFound++;

    // Show just the filename for readability, full path in tooltip
    QFileInfo fi(path);
    auto* item = new QListWidgetItem(fi.fileName());
    item->setToolTip(path);

    m_fileList->insertItem(0, item);  // most recent at top

    // Limit the list to 200 items to keep memory usage reasonable
    while (m_fileList->count() > 200) {
        delete m_fileList->takeItem(m_fileList->count() - 1);
    }
}

void ScanProgressDialog::onScanFinished(int total) {
    m_phaseLabel->setText(QStringLiteral("Scan complete"));

    m_progressBar->setMaximum(100);
    m_progressBar->setValue(100);
    m_percentLabel->setText(QStringLiteral("100%"));
    m_countLabel->setText(QString("%1 files processed").arg(total));

    m_resultLabel->setText(QString("+%1 files scanned").arg(total));
    m_resultLabel->show();

    m_cancelBtn->hide();
    m_closeBtn->show();
}

void ScanProgressDialog::onCancelClicked() {
    emit cancelRequested();
    m_cancelBtn->setText(QStringLiteral("Cancelling..."));
    m_cancelBtn->setEnabled(false);
}
