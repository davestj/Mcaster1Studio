#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QPushButton>

class ScanProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScanProgressDialog(QWidget* parent = nullptr);

public slots:
    void onScanStarted(int estimatedCount);
    void onScanProgress(int done, int total);
    void onScanFileFound(const QString& path);
    void onScanFinished(int total);
    void onCancelClicked();

signals:
    void cancelRequested();

private:
    QLabel* m_phaseLabel = nullptr;       // "Discovering files..." / "Reading tags..."
    QLabel* m_percentLabel = nullptr;     // "47%"
    QLabel* m_countLabel = nullptr;       // "142 / 500 files"
    QProgressBar* m_progressBar = nullptr;
    QListWidget* m_fileList = nullptr;    // scrolling list of recent files
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QLabel* m_resultLabel = nullptr;      // "+42 added, 3 updated"
    int m_filesFound = 0;
};
