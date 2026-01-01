/// @file   PodPublisherModule.cpp
/// @path   Modules/PodPublisherModule/PodPublisherModule.cpp

#include "PodPublisherModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QMap>

namespace {

// ─── Helper: type enum to string ────────────────────────────────────────────
static QString typeToString(M1::PublishType t) {
    switch (t) {
    case M1::PublishType::SFTP:  return "SFTP";
    case M1::PublishType::FTP:   return "FTP";
    case M1::PublishType::HTTP:  return "HTTP";
    case M1::PublishType::Local: return "Local Copy";
    }
    return "Unknown";
}

static M1::PublishType stringToType(const QString& s) {
    if (s == "SFTP")       return M1::PublishType::SFTP;
    if (s == "FTP")        return M1::PublishType::FTP;
    if (s == "HTTP")       return M1::PublishType::HTTP;
    return M1::PublishType::Local;
}

static QString statusToString(M1::PublishStatus s) {
    switch (s) {
    case M1::PublishStatus::Queued:      return "Queued";
    case M1::PublishStatus::InProgress:  return "In Progress";
    case M1::PublishStatus::Completed:   return "Completed";
    case M1::PublishStatus::Failed:      return "Failed";
    case M1::PublishStatus::Cancelled:   return "Cancelled";
    case M1::PublishStatus::Unsupported: return "Coming in v2";
    }
    return "Unknown";
}

// ─── TargetDialog — modal dialog for adding/editing publish targets ─────────
class TargetDialog : public QDialog {
    Q_OBJECT
public:
    explicit TargetDialog(QWidget* parent = nullptr,
                          const M1::PublishTarget& target = {})
        : QDialog(parent)
    {
        setWindowTitle(target.name.isEmpty() ? "Add Target" : "Edit Target");
        setObjectName("PodPublisherTargetDialog");
        setMinimumWidth(400);

        auto* lay = new QFormLayout(this);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(6);

        m_nameEdit = new QLineEdit(target.name);
        m_nameEdit->setObjectName("PodPubTargetName");
        m_nameEdit->setPlaceholderText("My Server");
        m_nameEdit->setToolTip("Target profile name");
        lay->addRow("Name:", m_nameEdit);

        m_typeCombo = new QComboBox;
        m_typeCombo->setObjectName("PodPubTargetType");
        m_typeCombo->addItems({"Local Copy", "SFTP", "FTP", "HTTP"});
        m_typeCombo->setCurrentText(typeToString(target.type));
        m_typeCombo->setToolTip("Upload transport type");
        connect(m_typeCombo, &QComboBox::currentTextChanged,
                this, &TargetDialog::onTypeChanged);
        lay->addRow("Type:", m_typeCombo);

        m_hostEdit = new QLineEdit(target.host);
        m_hostEdit->setObjectName("PodPubTargetHost");
        m_hostEdit->setPlaceholderText("sftp.example.com");
        m_hostEdit->setToolTip("Server hostname or IP");
        lay->addRow("Host:", m_hostEdit);

        m_portSpin = new QSpinBox;
        m_portSpin->setObjectName("PodPubTargetPort");
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(target.port > 0 ? target.port : 22);
        m_portSpin->setToolTip("Server port number");
        lay->addRow("Port:", m_portSpin);

        m_userEdit = new QLineEdit(target.username);
        m_userEdit->setObjectName("PodPubTargetUser");
        m_userEdit->setToolTip("Login username");
        lay->addRow("Username:", m_userEdit);

        m_passEdit = new QLineEdit(target.password);
        m_passEdit->setObjectName("PodPubTargetPass");
        m_passEdit->setEchoMode(QLineEdit::Password);
        m_passEdit->setToolTip("Login password");
        lay->addRow("Password:", m_passEdit);

        m_remotePathEdit = new QLineEdit(target.remotePath);
        m_remotePathEdit->setObjectName("PodPubTargetRemotePath");
        m_remotePathEdit->setToolTip("Remote directory or local destination path");
        lay->addRow("Remote Path:", m_remotePathEdit);

        // Browse button for local copy
        m_browseBtn = new QPushButton("Browse...");
        m_browseBtn->setToolTip("Choose local destination directory");
        connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(this, "Destination Folder");
            if (!dir.isEmpty()) m_remotePathEdit->setText(dir);
        });
        lay->addRow("", m_browseBtn);

        m_enabledCheck = new QCheckBox;
        m_enabledCheck->setChecked(target.enabled);
        m_enabledCheck->setToolTip("Enable this target for publishing");
        lay->addRow("Enabled:", m_enabledCheck);

        auto* btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addRow(btnBox);

        onTypeChanged(m_typeCombo->currentText());
    }

    M1::PublishTarget target() const {
        M1::PublishTarget t;
        t.name       = m_nameEdit->text();
        t.type       = stringToType(m_typeCombo->currentText());
        t.host       = m_hostEdit->text();
        t.port       = m_portSpin->value();
        t.username   = m_userEdit->text();
        t.password   = m_passEdit->text();
        t.remotePath = m_remotePathEdit->text();
        t.enabled    = m_enabledCheck->isChecked();
        return t;
    }

private slots:
    void onTypeChanged(const QString& type) {
        const bool isLocal = (type == "Local Copy");
        m_hostEdit->setEnabled(!isLocal);
        m_portSpin->setEnabled(!isLocal);
        m_userEdit->setEnabled(!isLocal);
        m_passEdit->setEnabled(!isLocal);
        m_browseBtn->setVisible(isLocal);
        if (isLocal) {
            m_remotePathEdit->setPlaceholderText("C:/Podcast/Episodes");
        } else {
            m_remotePathEdit->setPlaceholderText("/var/www/podcast/episodes");
        }
    }

private:
    QLineEdit* m_nameEdit;
    QComboBox* m_typeCombo;
    QLineEdit* m_hostEdit;
    QSpinBox*  m_portSpin;
    QLineEdit* m_userEdit;
    QLineEdit* m_passEdit;
    QLineEdit* m_remotePathEdit;
    QPushButton* m_browseBtn;
    QCheckBox* m_enabledCheck;
};

// ─── PodPublisherWidget — main publisher widget ─────────────────────────────
class PodPublisherWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodPublisherWidget(M1::PodPublisherModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodPublisherWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(6);

        // ── Target table ─────────────────────────────────────────────────
        auto* targetGroup = new QGroupBox("Publishing Targets");
        targetGroup->setObjectName("PodPublisherTargetGroup");
        auto* tgLay = new QVBoxLayout(targetGroup);

        m_targetTable = new QTableWidget(0, 4);
        m_targetTable->setObjectName("PodPublisherTargetTable");
        m_targetTable->setHorizontalHeaderLabels({"Name", "Type", "Destination", "Enabled"});
        m_targetTable->horizontalHeader()->setStretchLastSection(true);
        m_targetTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_targetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_targetTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_targetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_targetTable->setToolTip("Configured publishing targets");
        tgLay->addWidget(m_targetTable, 1);

        auto* targetBtnRow = new QHBoxLayout;
        targetBtnRow->setSpacing(4);

        auto* addTargetBtn = new QPushButton("Add Target");
        addTargetBtn->setObjectName("PodPublisherAddTargetBtn");
        addTargetBtn->setToolTip("Add a new publishing target");
        connect(addTargetBtn, &QPushButton::clicked, this, &PodPublisherWidget::onAddTarget);
        targetBtnRow->addWidget(addTargetBtn);

        auto* editTargetBtn = new QPushButton("Edit");
        editTargetBtn->setObjectName("PodPublisherEditTargetBtn");
        editTargetBtn->setToolTip("Edit selected target");
        connect(editTargetBtn, &QPushButton::clicked, this, &PodPublisherWidget::onEditTarget);
        targetBtnRow->addWidget(editTargetBtn);

        auto* removeTargetBtn = new QPushButton("Remove");
        removeTargetBtn->setObjectName("PodPublisherRemoveTargetBtn");
        removeTargetBtn->setToolTip("Remove selected target");
        connect(removeTargetBtn, &QPushButton::clicked, this, &PodPublisherWidget::onRemoveTarget);
        targetBtnRow->addWidget(removeTargetBtn);

        targetBtnRow->addStretch();
        tgLay->addLayout(targetBtnRow);

        root->addWidget(targetGroup, 1);

        // ── File selection ───────────────────────────────────────────────
        auto* fileRow = new QHBoxLayout;
        fileRow->setSpacing(4);

        fileRow->addWidget(new QLabel("File:"));
        m_filePathEdit = new QLineEdit;
        m_filePathEdit->setObjectName("PodPublisherFilePath");
        m_filePathEdit->setPlaceholderText("Select a file to publish...");
        m_filePathEdit->setToolTip("Source file to publish");
        fileRow->addWidget(m_filePathEdit, 1);

        auto* browseBtn = new QPushButton("Browse...");
        browseBtn->setObjectName("PodPublisherBrowseBtn");
        browseBtn->setToolTip("Choose a file to publish");
        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(this, "Select File to Publish",
                {}, "Audio Files (*.mp3 *.opus *.ogg *.flac *.aac *.wav *.m4a);;All Files (*)");
            if (!path.isEmpty()) m_filePathEdit->setText(path);
        });
        fileRow->addWidget(browseBtn);

        root->addLayout(fileRow);

        // ── Publish controls ─────────────────────────────────────────────
        auto* controlRow = new QHBoxLayout;
        controlRow->setSpacing(6);

        m_publishBtn = new QPushButton("Publish");
        m_publishBtn->setObjectName("PodPublisherPublishBtn");
        m_publishBtn->setMinimumHeight(36);
        QFont bf = m_publishBtn->font();
        bf.setPixelSize(14);
        bf.setBold(true);
        m_publishBtn->setFont(bf);
        m_publishBtn->setToolTip("Publish file to all enabled targets");
        connect(m_publishBtn, &QPushButton::clicked, this, &PodPublisherWidget::onPublish);
        controlRow->addWidget(m_publishBtn);

        m_cancelBtn = new QPushButton("Cancel");
        m_cancelBtn->setObjectName("PodPublisherCancelBtn");
        m_cancelBtn->setMinimumHeight(36);
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setToolTip("Cancel current publishing");
        connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
            m_mod->cancelPublish();
        });
        controlRow->addWidget(m_cancelBtn);

        root->addLayout(controlRow);

        // ── Progress area ────────────────────────────────────────────────
        m_progressGroup = new QGroupBox("Progress");
        m_progressGroup->setObjectName("PodPublisherProgressGroup");
        m_progressLay = new QVBoxLayout(m_progressGroup);
        m_progressLay->setContentsMargins(4, 4, 4, 4);
        m_progressLay->setSpacing(4);
        root->addWidget(m_progressGroup);

        // ── Status label ─────────────────────────────────────────────────
        m_statusLabel = new QLabel("Ready");
        m_statusLabel->setObjectName("PodPublisherStatus");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(m_statusLabel);

        // Connect signals
        connect(mod, &M1::PodPublisherModule::targetsChanged,
                this, &PodPublisherWidget::refreshTargetTable);
        connect(mod, &M1::PodPublisherModule::publishStarted,
                this, &PodPublisherWidget::onPublishStarted);
        connect(mod, &M1::PodPublisherModule::publishProgress,
                this, &PodPublisherWidget::onPublishProgress);
        connect(mod, &M1::PodPublisherModule::publishFinished,
                this, &PodPublisherWidget::onPublishFinished);
        connect(mod, &M1::PodPublisherModule::allPublishingComplete,
                this, &PodPublisherWidget::onAllComplete);

        refreshTargetTable();
    }

private slots:
    void refreshTargetTable() {
        const auto targets = m_mod->targets();
        m_targetTable->setRowCount(targets.size());
        for (int i = 0; i < targets.size(); ++i) {
            const auto& t = targets[i];
            m_targetTable->setItem(i, 0, new QTableWidgetItem(t.name));
            m_targetTable->setItem(i, 1, new QTableWidgetItem(typeToString(t.type)));
            const QString dest = (t.type == M1::PublishType::Local)
                ? t.remotePath
                : QString("%1:%2%3").arg(t.host).arg(t.port).arg(t.remotePath);
            m_targetTable->setItem(i, 2, new QTableWidgetItem(dest));
            m_targetTable->setItem(i, 3, new QTableWidgetItem(t.enabled ? "Yes" : "No"));
        }
    }

    void onAddTarget() {
        TargetDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            m_mod->addTarget(dlg.target());
        }
    }

    void onEditTarget() {
        const int row = m_targetTable->currentRow();
        if (row < 0) return;
        const auto targets = m_mod->targets();
        if (row >= targets.size()) return;

        TargetDialog dlg(this, targets[row]);
        if (dlg.exec() == QDialog::Accepted) {
            m_mod->updateTarget(targets[row].id, dlg.target());
        }
    }

    void onRemoveTarget() {
        const int row = m_targetTable->currentRow();
        if (row < 0) return;
        const auto targets = m_mod->targets();
        if (row >= targets.size()) return;
        m_mod->removeTarget(targets[row].id);
    }

    void onPublish() {
        const QString filePath = m_filePathEdit->text();
        if (filePath.isEmpty()) {
            QMessageBox::warning(this, "Publish", "No file selected.");
            return;
        }
        if (!QFile::exists(filePath)) {
            QMessageBox::warning(this, "Publish", "File does not exist.");
            return;
        }
        m_mod->publish(filePath);
    }

    void onPublishStarted() {
        m_publishBtn->setEnabled(false);
        m_cancelBtn->setEnabled(true);
        m_statusLabel->setText("Publishing...");

        // Clear old progress bars
        QLayoutItem* item;
        while ((item = m_progressLay->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        m_progressBars.clear();

        // Create progress bars per job
        const auto jobs = m_mod->jobs();
        const auto targets = m_mod->targets();
        for (const auto& job : jobs) {
            // Find target name
            QString targetName = "Unknown";
            for (const auto& t : targets) {
                if (t.id == job.targetId) {
                    targetName = t.name;
                    break;
                }
            }

            auto* row = new QWidget;
            row->setObjectName("PodPublisherProgressRow");
            auto* rLay = new QHBoxLayout(row);
            rLay->setContentsMargins(0, 0, 0, 0);
            rLay->setSpacing(4);

            auto* label = new QLabel(targetName);
            label->setFixedWidth(120);
            rLay->addWidget(label);

            auto* bar = new QProgressBar;
            bar->setObjectName("PodPublisherProgressBar");
            bar->setRange(0, 100);
            bar->setValue(0);
            bar->setToolTip(QString("Upload progress for %1").arg(targetName));
            rLay->addWidget(bar, 1);

            auto* statusLbl = new QLabel(statusToString(job.status));
            statusLbl->setObjectName("PodPublisherJobStatus");
            statusLbl->setFixedWidth(100);
            rLay->addWidget(statusLbl);

            m_progressLay->addWidget(row);
            m_progressBars[job.targetId] = {bar, statusLbl};
        }
    }

    void onPublishProgress(int targetId, int percent) {
        if (m_progressBars.contains(targetId)) {
            m_progressBars[targetId].first->setValue(percent);
        }
    }

    void onPublishFinished(int targetId, bool success, const QString& message) {
        if (m_progressBars.contains(targetId)) {
            auto& pair = m_progressBars[targetId];
            pair.first->setValue(success ? 100 : pair.first->value());
            pair.second->setText(success ? "Completed" : "Failed");
            pair.second->setToolTip(message);
        }
    }

    void onAllComplete() {
        m_publishBtn->setEnabled(true);
        m_cancelBtn->setEnabled(false);
        m_statusLabel->setText("Publishing complete.");
    }

private:
    M1::PodPublisherModule* m_mod;
    QTableWidget*           m_targetTable;
    QLineEdit*              m_filePathEdit;
    QPushButton*            m_publishBtn;
    QPushButton*            m_cancelBtn;
    QGroupBox*              m_progressGroup;
    QVBoxLayout*            m_progressLay;
    QLabel*                 m_statusLabel;

    /// targetId → (progress bar, status label)
    QMap<int, QPair<QProgressBar*, QLabel*>> m_progressBars;
};

} // anonymous namespace

#include "PodPublisherModule.moc"

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PodPublisherModule::PodPublisherModule(QObject* parent) : IModule(parent) {}
PodPublisherModule::~PodPublisherModule() = default;

// ─── IModule lifecycle ────────────────────────────────────────────────────────

void PodPublisherModule::initialize() {
    qInfo() << "[PodPublisherModule] Initialized.";
}

void PodPublisherModule::shutdown() {
    if (m_publishing) cancelPublish();
    qInfo() << "[PodPublisherModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────

QWidget* PodPublisherModule::createWidget(QWidget* parent) {
    return new PodPublisherWidget(this, parent);
}

// ─── Target management ───────────────────────────────────────────────────────

int PodPublisherModule::addTarget(const PublishTarget& target) {
    PublishTarget t = target;
    t.id = m_nextId++;
    m_targets.append(t);
    emit targetsChanged();
    return t.id;
}

void PodPublisherModule::removeTarget(int id) {
    for (int i = 0; i < m_targets.size(); ++i) {
        if (m_targets[i].id == id) {
            m_targets.removeAt(i);
            emit targetsChanged();
            return;
        }
    }
}

void PodPublisherModule::updateTarget(int id, const PublishTarget& target) {
    for (auto& t : m_targets) {
        if (t.id == id) {
            const int savedId = t.id;
            t = target;
            t.id = savedId;
            emit targetsChanged();
            return;
        }
    }
}

// ─── Publishing ──────────────────────────────────────────────────────────────

void PodPublisherModule::publish(const QString& localFilePath) {
    if (m_publishing) return;
    if (!QFile::exists(localFilePath)) {
        qWarning() << "[PodPublisherModule] File does not exist:" << localFilePath;
        return;
    }

    m_jobs.clear();
    m_publishing = true;

    // Create jobs for all enabled targets
    for (const auto& target : m_targets) {
        if (!target.enabled) continue;

        PublishJob job;
        job.targetId   = target.id;
        job.localPath  = localFilePath;
        job.progress   = 0;

        // Build remote path
        const QFileInfo fi(localFilePath);
        if (target.type == PublishType::Local) {
            job.remotePath = target.remotePath + "/" + fi.fileName();
        } else {
            job.remotePath = target.remotePath + "/" + fi.fileName();
        }

        // Check if target type is implemented
        if (target.type != PublishType::Local) {
            job.status     = PublishStatus::Unsupported;
            job.statusText = "Coming in v2";
        } else {
            job.status     = PublishStatus::Queued;
            job.statusText = "Queued";
        }

        m_jobs.append(job);
    }

    if (m_jobs.isEmpty()) {
        m_publishing = false;
        qWarning() << "[PodPublisherModule] No enabled targets.";
        return;
    }

    emit publishStarted();

    // Process all jobs
    // Use QTimer::singleShot to keep UI responsive
    QTimer::singleShot(100, this, [this]() {
        for (auto& job : m_jobs) {
            if (!m_publishing) {
                job.status     = PublishStatus::Cancelled;
                job.statusText = "Cancelled";
                emit publishFinished(job.targetId, false, "Cancelled");
                continue;
            }

            if (job.status == PublishStatus::Unsupported) {
                emit publishProgress(job.targetId, 0);
                emit publishFinished(job.targetId, false, "Coming in v2");
                continue;
            }

            // Find target
            PublishType targetType = PublishType::Local;
            for (const auto& t : m_targets) {
                if (t.id == job.targetId) {
                    targetType = t.type;
                    break;
                }
            }

            if (targetType == PublishType::Local) {
                doLocalCopy(job);
            } else {
                job.status     = PublishStatus::Unsupported;
                job.statusText = "Coming in v2";
                emit publishFinished(job.targetId, false, "Coming in v2");
            }
        }

        m_publishing = false;
        emit allPublishingComplete();
    });
}

void PodPublisherModule::cancelPublish() {
    m_publishing = false;
}

// ─── Local copy implementation ───────────────────────────────────────────────

void PodPublisherModule::doLocalCopy(PublishJob& job) {
    job.status     = PublishStatus::InProgress;
    job.statusText = "Copying...";
    emit publishProgress(job.targetId, 10);

    // Ensure destination directory exists
    const QFileInfo dstInfo(job.remotePath);
    QDir().mkpath(dstInfo.absolutePath());

    // Remove existing file if present
    if (QFile::exists(job.remotePath))
        QFile::remove(job.remotePath);

    emit publishProgress(job.targetId, 30);

    // Perform copy
    if (QFile::copy(job.localPath, job.remotePath)) {
        emit publishProgress(job.targetId, 80);

        // Post-upload verification
        if (verifyFile(job.localPath, job.remotePath)) {
            job.progress   = 100;
            job.status     = PublishStatus::Completed;
            job.statusText = "Completed successfully";
            emit publishProgress(job.targetId, 100);
            emit publishFinished(job.targetId, true, "File copied and verified.");
        } else {
            job.status     = PublishStatus::Failed;
            job.statusText = "Verification failed — size mismatch";
            emit publishFinished(job.targetId, false, "File size verification failed.");
        }
    } else {
        job.status     = PublishStatus::Failed;
        job.statusText = "Copy failed";
        emit publishFinished(job.targetId, false,
            QString("Failed to copy file to %1").arg(job.remotePath));
    }
}

bool PodPublisherModule::verifyFile(const QString& srcPath, const QString& dstPath) {
    const QFileInfo src(srcPath);
    const QFileInfo dst(dstPath);
    return dst.exists() && dst.size() == src.size();
}

// ─── State persistence ───────────────────────────────────────────────────────

void PodPublisherModule::saveState(QSettings& s) {
    s.setValue("nextId", m_nextId);

    s.beginWriteArray("targets", m_targets.size());
    for (int i = 0; i < m_targets.size(); ++i) {
        s.setArrayIndex(i);
        const auto& t = m_targets[i];
        s.setValue("id", t.id);
        s.setValue("name", t.name);
        s.setValue("type", static_cast<int>(t.type));
        s.setValue("host", t.host);
        s.setValue("port", t.port);
        s.setValue("username", t.username);
        // NOTE: Password stored in plain text — production should use
        // a secure credential store (e.g., Windows Credential Manager).
        s.setValue("password", t.password);
        s.setValue("remotePath", t.remotePath);
        s.setValue("enabled", t.enabled);
    }
    s.endArray();
}

void PodPublisherModule::loadState(QSettings& s) {
    m_nextId = s.value("nextId", 1).toInt();

    m_targets.clear();
    const int count = s.beginReadArray("targets");
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        PublishTarget t;
        t.id         = s.value("id", 0).toInt();
        t.name       = s.value("name").toString();
        t.type       = static_cast<PublishType>(s.value("type", 3).toInt());
        t.host       = s.value("host").toString();
        t.port       = s.value("port", 22).toInt();
        t.username   = s.value("username").toString();
        t.password   = s.value("password").toString();
        t.remotePath = s.value("remotePath").toString();
        t.enabled    = s.value("enabled", true).toBool();
        m_targets.append(t);

        if (t.id >= m_nextId)
            m_nextId = t.id + 1;
    }
    s.endArray();
}

} // namespace M1
