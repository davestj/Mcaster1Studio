/// @file   PodEncodeModule.cpp
/// @path   Modules/PodEncodeModule/PodEncodeModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodEncode — Podcast Encoding and ID3 Tagging Implementation
/// @purpose Implements the batch encode queue, ID3 metadata form, format
///          selection, and job progress tracking. Actual codec encoding is
///          stubbed for v1 — jobs transition Pending -> Running -> Complete
///          with a simulated progress timer.
/// @reason  Podcast producers need multi-format export with metadata.
/// @changelog
///   2026-03-09  Initial implementation

#include "PodEncodeModule.h"
#include "SurfaceThreadPool.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QScrollArea>
#include <QSplitter>
#include <QTextEdit>
#include <QToolButton>
#include <cmath>

namespace {

// ─── FormatCheckGroup — multi-format selector ────────────────────────────────
class FormatCheckGroup : public QGroupBox {
    Q_OBJECT
public:
    explicit FormatCheckGroup(QWidget* parent = nullptr)
        : QGroupBox("Output Formats", parent)
    {
        setObjectName("PodEncodeFormats");
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(6, 6, 6, 6);

        m_mp3   = new QCheckBox("MP3");   m_mp3->setObjectName("PodEncodeFmtMP3");
        m_aac   = new QCheckBox("AAC");   m_aac->setObjectName("PodEncodeFmtAAC");
        m_ogg   = new QCheckBox("OGG");   m_ogg->setObjectName("PodEncodeFmtOGG");
        m_flac  = new QCheckBox("FLAC");  m_flac->setObjectName("PodEncodeFmtFLAC");
        m_opus  = new QCheckBox("Opus");  m_opus->setObjectName("PodEncodeFmtOpus");

        m_mp3->setChecked(true); // default

        lay->addWidget(m_mp3);
        lay->addWidget(m_aac);
        lay->addWidget(m_ogg);
        lay->addWidget(m_flac);
        lay->addWidget(m_opus);
        lay->addStretch();
    }

    QList<M1::PodEncodeFormat> checkedFormats() const {
        QList<M1::PodEncodeFormat> fmts;
        if (m_mp3->isChecked())   fmts << M1::PodEncodeFormat::MP3;
        if (m_aac->isChecked())   fmts << M1::PodEncodeFormat::AAC;
        if (m_ogg->isChecked())   fmts << M1::PodEncodeFormat::OGG;
        if (m_flac->isChecked())  fmts << M1::PodEncodeFormat::FLAC;
        if (m_opus->isChecked())  fmts << M1::PodEncodeFormat::Opus;
        return fmts;
    }

private:
    QCheckBox* m_mp3  = nullptr;
    QCheckBox* m_aac  = nullptr;
    QCheckBox* m_ogg  = nullptr;
    QCheckBox* m_flac = nullptr;
    QCheckBox* m_opus = nullptr;
};

// ─── Id3Panel — metadata entry form ─────────────────────────────────────────
class Id3Panel : public QGroupBox {
    Q_OBJECT
public:
    explicit Id3Panel(M1::PodEncodeModule* mod, QWidget* parent = nullptr)
        : QGroupBox("ID3 Metadata", parent), m_mod(mod)
    {
        setObjectName("PodEncodeId3");
        auto* grid = new QGridLayout(this);
        grid->setContentsMargins(6, 6, 6, 6);
        grid->setSpacing(4);

        int row = 0;

        // Title
        grid->addWidget(new QLabel("Title:"), row, 0);
        m_titleEdit = new QLineEdit(mod->id3Title());
        m_titleEdit->setObjectName("PodEncodeId3Title");
        m_titleEdit->setPlaceholderText("Episode title...");
        grid->addWidget(m_titleEdit, row++, 1, 1, 3);
        connect(m_titleEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3Title);

        // Episode / Season
        grid->addWidget(new QLabel("Episode #:"), row, 0);
        m_epSpin = new QSpinBox;
        m_epSpin->setObjectName("PodEncodeId3Ep");
        m_epSpin->setRange(0, 99999);
        m_epSpin->setValue(mod->id3EpisodeNumber());
        grid->addWidget(m_epSpin, row, 1);
        connect(m_epSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                mod, &M1::PodEncodeModule::setId3EpisodeNumber);

        grid->addWidget(new QLabel("Season #:"), row, 2);
        m_seasonSpin = new QSpinBox;
        m_seasonSpin->setObjectName("PodEncodeId3Season");
        m_seasonSpin->setRange(1, 999);
        m_seasonSpin->setValue(mod->id3SeasonNumber());
        grid->addWidget(m_seasonSpin, row++, 3);
        connect(m_seasonSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                mod, &M1::PodEncodeModule::setId3SeasonNumber);

        // Author
        grid->addWidget(new QLabel("Author:"), row, 0);
        m_authorEdit = new QLineEdit(mod->id3Author());
        m_authorEdit->setObjectName("PodEncodeId3Author");
        m_authorEdit->setPlaceholderText("Host name / podcast author...");
        grid->addWidget(m_authorEdit, row++, 1, 1, 3);
        connect(m_authorEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3Author);

        // Genre
        grid->addWidget(new QLabel("Genre:"), row, 0);
        m_genreEdit = new QLineEdit(mod->id3Genre());
        m_genreEdit->setObjectName("PodEncodeId3Genre");
        grid->addWidget(m_genreEdit, row++, 1, 1, 3);
        connect(m_genreEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3Genre);

        // Description
        grid->addWidget(new QLabel("Desc:"), row, 0, Qt::AlignTop);
        m_descEdit = new QTextEdit;
        m_descEdit->setObjectName("PodEncodeId3Desc");
        m_descEdit->setPlaceholderText("Episode description / show notes...");
        m_descEdit->setMaximumHeight(60);
        m_descEdit->setPlainText(mod->id3Description());
        grid->addWidget(m_descEdit, row++, 1, 1, 3);
        connect(m_descEdit, &QTextEdit::textChanged, this, [this]() {
            m_mod->setId3Description(m_descEdit->toPlainText());
        });

        // Cover Art
        grid->addWidget(new QLabel("Cover Art:"), row, 0);
        m_coverEdit = new QLineEdit(mod->id3CoverArtPath());
        m_coverEdit->setObjectName("PodEncodeId3Cover");
        m_coverEdit->setPlaceholderText("cover.jpg / cover.png");
        grid->addWidget(m_coverEdit, row, 1, 1, 2);
        connect(m_coverEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3CoverArtPath);

        auto* browseBtn = new QToolButton;
        browseBtn->setText("...");
        browseBtn->setObjectName("PodEncodeId3CoverBrowse");
        browseBtn->setToolTip("Browse for cover art image");
        connect(browseBtn, &QToolButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this,
                "Select Cover Art", {},
                "Images (*.jpg *.jpeg *.png *.bmp)");
            if (!path.isEmpty()) {
                m_coverEdit->setText(path);
            }
        });
        grid->addWidget(browseBtn, row++, 3);

        // Copyright
        grid->addWidget(new QLabel("Copyright:"), row, 0);
        m_copyEdit = new QLineEdit(mod->id3Copyright());
        m_copyEdit->setObjectName("PodEncodeId3Copy");
        grid->addWidget(m_copyEdit, row++, 1, 1, 3);
        connect(m_copyEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3Copyright);

        // Website
        grid->addWidget(new QLabel("Website:"), row, 0);
        m_webEdit = new QLineEdit(mod->id3WebsiteUrl());
        m_webEdit->setObjectName("PodEncodeId3Web");
        m_webEdit->setPlaceholderText("https://...");
        grid->addWidget(m_webEdit, row++, 1, 1, 3);
        connect(m_webEdit, &QLineEdit::textChanged,
                mod, &M1::PodEncodeModule::setId3WebsiteUrl);
    }

    void reload() {
        m_titleEdit->setText(m_mod->id3Title());
        m_epSpin->setValue(m_mod->id3EpisodeNumber());
        m_seasonSpin->setValue(m_mod->id3SeasonNumber());
        m_authorEdit->setText(m_mod->id3Author());
        m_genreEdit->setText(m_mod->id3Genre());
        m_descEdit->setPlainText(m_mod->id3Description());
        m_coverEdit->setText(m_mod->id3CoverArtPath());
        m_copyEdit->setText(m_mod->id3Copyright());
        m_webEdit->setText(m_mod->id3WebsiteUrl());
    }

private:
    M1::PodEncodeModule* m_mod;
    QLineEdit*    m_titleEdit   = nullptr;
    QSpinBox*     m_epSpin      = nullptr;
    QSpinBox*     m_seasonSpin  = nullptr;
    QLineEdit*    m_authorEdit  = nullptr;
    QLineEdit*    m_genreEdit   = nullptr;
    QTextEdit*    m_descEdit    = nullptr;
    QLineEdit*    m_coverEdit   = nullptr;
    QLineEdit*    m_copyEdit    = nullptr;
    QLineEdit*    m_webEdit     = nullptr;
};

// ─── JobTableWidget — encode job queue display ──────────────────────────────
class JobTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit JobTableWidget(QWidget* parent = nullptr) : QTableWidget(parent) {
        setObjectName("PodEncodeJobTable");
        setColumnCount(5);
        setHorizontalHeaderLabels({"Format", "Bitrate", "Output", "Status", "Progress"});
        horizontalHeader()->setStretchLastSection(true);
        horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        verticalHeader()->setVisible(false);
        setAlternatingRowColors(true);
    }

    void rebuild(const QList<M1::EncodeJob>& jobList) {
        setRowCount(jobList.size());
        for (int i = 0; i < jobList.size(); ++i) {
            const auto& job = jobList[i];

            setItem(i, 0, new QTableWidgetItem(M1::PodEncodeModule::formatName(job.format)));
            setItem(i, 1, new QTableWidgetItem(
                job.format == M1::PodEncodeFormat::FLAC
                    ? QString("lossless")
                    : QString("%1 kbps").arg(job.bitrate)));
            setItem(i, 2, new QTableWidgetItem(job.outputPath));

            QString statusStr;
            switch (job.status) {
            case M1::EncodeJobStatus::Pending:  statusStr = "Pending";  break;
            case M1::EncodeJobStatus::Running:  statusStr = "Running";  break;
            case M1::EncodeJobStatus::Complete: statusStr = "Complete"; break;
            case M1::EncodeJobStatus::Error:    statusStr = "Error";    break;
            }
            setItem(i, 3, new QTableWidgetItem(statusStr));

            // Progress bar in column 4
            auto* pb = new QProgressBar;
            pb->setRange(0, 100);
            pb->setValue(job.progress);
            pb->setTextVisible(true);
            setCellWidget(i, 4, pb);
        }
    }
};

// ─── PodEncodeWidget — main composite widget ────────────────────────────────
class PodEncodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodEncodeWidget(M1::PodEncodeModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodEncodeWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->setSpacing(4);

        // ── Input file selector ──────────────────────────────────────────────
        auto* inputRow = new QHBoxLayout;
        inputRow->addWidget(new QLabel("Input File:"));
        m_inputEdit = new QLineEdit(mod->inputFile());
        m_inputEdit->setObjectName("PodEncodeInput");
        m_inputEdit->setPlaceholderText("Select audio file...");
        m_inputEdit->setReadOnly(true);
        inputRow->addWidget(m_inputEdit, 1);

        auto* browseBtn = new QPushButton("Browse...");
        browseBtn->setObjectName("PodEncodeBrowse");
        browseBtn->setToolTip("Select input audio file for encoding");
        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            QString path = QFileDialog::getOpenFileName(this,
                "Select Audio File", {},
                "Audio Files (*.wav *.mp3 *.ogg *.flac *.opus *.aac *.m4a);;All Files (*)");
            if (!path.isEmpty()) {
                m_mod->setInputFile(path);
                m_inputEdit->setText(path);
            }
        });
        inputRow->addWidget(browseBtn);
        root->addLayout(inputRow);

        // ── Scrollable content area ──────────────────────────────────────────
        auto* splitter = new QSplitter(Qt::Vertical);

        // Top: format + encoding config + ID3
        auto* topWidget = new QWidget;
        auto* topLay = new QVBoxLayout(topWidget);
        topLay->setContentsMargins(0, 0, 0, 0);
        topLay->setSpacing(4);

        // Format checkboxes
        m_formatGroup = new FormatCheckGroup;
        topLay->addWidget(m_formatGroup);

        // Encoding parameters row
        auto* paramRow = new QHBoxLayout;
        paramRow->addWidget(new QLabel("Bitrate:"));
        m_bitrateSpin = new QSpinBox;
        m_bitrateSpin->setObjectName("PodEncodeBitrate");
        m_bitrateSpin->setRange(32, 512);
        m_bitrateSpin->setValue(192);
        m_bitrateSpin->setSuffix(" kbps");
        m_bitrateSpin->setToolTip("Encoding bitrate (ignored for FLAC)");
        paramRow->addWidget(m_bitrateSpin);

        paramRow->addWidget(new QLabel("Sample Rate:"));
        m_sampleRateCombo = new QComboBox;
        m_sampleRateCombo->setObjectName("PodEncodeSampleRate");
        m_sampleRateCombo->addItem("44100 Hz", 44100);
        m_sampleRateCombo->addItem("48000 Hz", 48000);
        m_sampleRateCombo->setCurrentIndex(1); // default 48000
        m_sampleRateCombo->setToolTip("Output sample rate");
        paramRow->addWidget(m_sampleRateCombo);

        paramRow->addWidget(new QLabel("Channels:"));
        m_channelsCombo = new QComboBox;
        m_channelsCombo->setObjectName("PodEncodeChannels");
        m_channelsCombo->addItem("Mono", 1);
        m_channelsCombo->addItem("Stereo", 2);
        m_channelsCombo->setCurrentIndex(1); // default stereo
        m_channelsCombo->setToolTip("Output channel configuration");
        paramRow->addWidget(m_channelsCombo);

        paramRow->addWidget(new QLabel("LUFS:"));
        m_lufsSpin = new QDoubleSpinBox;
        m_lufsSpin->setObjectName("PodEncodeLufs");
        m_lufsSpin->setRange(-30.0, -6.0);
        m_lufsSpin->setValue(mod->lufsTarget());
        m_lufsSpin->setSingleStep(0.5);
        m_lufsSpin->setDecimals(1);
        m_lufsSpin->setToolTip("LUFS loudness normalization target (-16 LUFS recommended)");
        connect(m_lufsSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                mod, &M1::PodEncodeModule::setLufsTarget);
        paramRow->addWidget(m_lufsSpin);
        paramRow->addStretch();

        topLay->addLayout(paramRow);

        // ID3 metadata panel
        m_id3Panel = new Id3Panel(mod);
        topLay->addWidget(m_id3Panel);

        splitter->addWidget(topWidget);

        // Bottom: job table + controls
        auto* bottomWidget = new QWidget;
        auto* bottomLay = new QVBoxLayout(bottomWidget);
        bottomLay->setContentsMargins(0, 0, 0, 0);
        bottomLay->setSpacing(4);

        auto* jobLabel = new QLabel("Encode Jobs");
        jobLabel->setObjectName("PodEncodeJobHeader");
        QFont hf;
        hf.setBold(true);
        jobLabel->setFont(hf);
        bottomLay->addWidget(jobLabel);

        m_jobTable = new JobTableWidget;
        bottomLay->addWidget(m_jobTable, 1);

        // Job control buttons
        auto* jobBtnRow = new QHBoxLayout;
        auto* addJobBtn = new QPushButton("Add Jobs");
        addJobBtn->setObjectName("PodEncodeAddJob");
        addJobBtn->setToolTip("Add encode jobs for all checked formats");
        connect(addJobBtn, &QPushButton::clicked, this, &PodEncodeWidget::onAddJobs);
        jobBtnRow->addWidget(addJobBtn);

        auto* removeJobBtn = new QPushButton("Remove");
        removeJobBtn->setObjectName("PodEncodeRemoveJob");
        removeJobBtn->setToolTip("Remove selected job from queue");
        connect(removeJobBtn, &QPushButton::clicked, this, [this]() {
            int row = m_jobTable->currentRow();
            if (row < 0) return;
            auto jobList = m_mod->jobs();
            if (row < jobList.size()) {
                m_mod->removeJob(jobList[row].id);
                rebuildJobTable();
            }
        });
        jobBtnRow->addWidget(removeJobBtn);

        auto* clearBtn = new QPushButton("Clear All");
        clearBtn->setObjectName("PodEncodeClearJobs");
        clearBtn->setToolTip("Remove all jobs from the queue");
        connect(clearBtn, &QPushButton::clicked, this, [this]() {
            m_mod->clearJobs();
            rebuildJobTable();
        });
        jobBtnRow->addWidget(clearBtn);

        jobBtnRow->addStretch();

        m_encodeBtn = new QPushButton("Encode");
        m_encodeBtn->setObjectName("PodEncodeStartBtn");
        m_encodeBtn->setMinimumWidth(100);
        m_encodeBtn->setToolTip("Start encoding all pending jobs");
        QFont bf = m_encodeBtn->font();
        bf.setBold(true);
        m_encodeBtn->setFont(bf);
        connect(m_encodeBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->isEncoding()) {
                m_mod->cancelEncoding();
                m_encodeBtn->setText("Encode");
            } else {
                m_mod->startEncoding();
                m_encodeBtn->setText("Cancel");
            }
        });
        jobBtnRow->addWidget(m_encodeBtn);

        bottomLay->addLayout(jobBtnRow);

        // Status label
        m_statusLabel = new QLabel("Ready");
        m_statusLabel->setObjectName("PodEncodeStatus");
        bottomLay->addWidget(m_statusLabel);

        splitter->addWidget(bottomWidget);
        splitter->setStretchFactor(0, 2);
        splitter->setStretchFactor(1, 3);

        root->addWidget(splitter, 1);

        // Connect module signals
        connect(mod, &M1::PodEncodeModule::inputFileChanged, this, [this](const QString& path) {
            m_inputEdit->setText(path);
        });
        connect(mod, &M1::PodEncodeModule::jobAdded, this, [this](int /*id*/) {
            rebuildJobTable();
        });
        connect(mod, &M1::PodEncodeModule::jobRemoved, this, [this](int /*id*/) {
            rebuildJobTable();
        });
        connect(mod, &M1::PodEncodeModule::jobProgressChanged, this, [this](int /*id*/, int /*pct*/) {
            rebuildJobTable();
        });
        connect(mod, &M1::PodEncodeModule::jobStatusChanged, this, [this](int /*id*/, int /*status*/) {
            rebuildJobTable();
        });
        connect(mod, &M1::PodEncodeModule::encodingFinished, this, [this]() {
            m_encodeBtn->setText("Encode");
            m_statusLabel->setText("Encoding complete");
        });
        connect(mod, &M1::PodEncodeModule::encodingStarted, this, [this]() {
            m_statusLabel->setText("Encoding...");
        });
        connect(mod, &M1::PodEncodeModule::encodeError, this, [this](const QString& msg) {
            m_statusLabel->setText("Error: " + msg);
        });
    }

private slots:
    void onAddJobs() {
        auto formats = m_formatGroup->checkedFormats();
        if (formats.isEmpty()) {
            QMessageBox::information(this, "No Formats",
                "Please check at least one output format.");
            return;
        }

        if (m_mod->inputFile().isEmpty()) {
            QMessageBox::information(this, "No Input",
                "Please select an input audio file first.");
            return;
        }

        // Choose output directory
        QString outDir = QFileDialog::getExistingDirectory(this,
            "Select Output Directory");
        if (outDir.isEmpty()) return;

        int bitrate    = m_bitrateSpin->value();
        QString baseName = QFileInfo(m_mod->inputFile()).completeBaseName();

        for (auto fmt : formats) {
            QString ext = M1::PodEncodeModule::formatExtension(fmt);
            QString outPath = outDir + "/" + baseName + "." + ext;
            m_mod->addJob(fmt, bitrate, outPath);
        }

        rebuildJobTable();
        m_statusLabel->setText(QString("%1 job(s) added").arg(formats.size()));
    }

private:
    void rebuildJobTable() {
        m_jobTable->rebuild(m_mod->jobs());
    }

    M1::PodEncodeModule* m_mod;
    QLineEdit*       m_inputEdit      = nullptr;
    FormatCheckGroup* m_formatGroup   = nullptr;
    QSpinBox*        m_bitrateSpin    = nullptr;
    QComboBox*       m_sampleRateCombo = nullptr;
    QComboBox*       m_channelsCombo  = nullptr;
    QDoubleSpinBox*  m_lufsSpin       = nullptr;
    Id3Panel*        m_id3Panel       = nullptr;
    JobTableWidget*  m_jobTable       = nullptr;
    QPushButton*     m_encodeBtn      = nullptr;
    QLabel*          m_statusLabel    = nullptr;
};

} // anonymous namespace

#include "PodEncodeModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
PodEncodeModule::PodEncodeModule(QObject* parent)
    : IModule(parent)
{
}

PodEncodeModule::~PodEncodeModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void PodEncodeModule::initialize() {
    // No background threads needed for v1 stub
}

void PodEncodeModule::shutdown() {
    cancelEncoding();
}

QWidget* PodEncodeModule::createWidget(QWidget* parent) {
    return new PodEncodeWidget(this, parent);
}

// ─── Input file ─────────────────────────────────────────────────────────────
void PodEncodeModule::setInputFile(const QString& path) {
    if (m_inputPath != path) {
        m_inputPath = path;
        emit inputFileChanged(path);
    }
}

// ─── Job management ─────────────────────────────────────────────────────────
int PodEncodeModule::addJob(PodEncodeFormat format, int bitrate, const QString& outputPath) {
    EncodeJob job;
    job.id         = m_nextJobId++;
    job.inputPath  = m_inputPath;
    job.outputPath = outputPath;
    job.format     = format;
    job.bitrate    = bitrate;
    job.status     = EncodeJobStatus::Pending;
    job.progress   = 0;
    applyId3ToJob(job);

    m_jobs.append(job);
    emit jobAdded(job.id);
    return job.id;
}

void PodEncodeModule::removeJob(int id) {
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i].id == id) {
            m_jobs.removeAt(i);
            emit jobRemoved(id);
            return;
        }
    }
}

void PodEncodeModule::clearJobs() {
    QList<int> ids;
    for (const auto& j : m_jobs) ids << j.id;
    m_jobs.clear();
    for (int id : ids) emit jobRemoved(id);
}

// ─── Encoding control ──────────────────────────────────────────────────────
void PodEncodeModule::startEncoding() {
    if (m_encoding) return;
    if (m_jobs.isEmpty()) {
        emit encodeError("No jobs in queue");
        return;
    }

    m_encoding = true;
    emit encodingStarted();

    // Use surface thread pool if available (non-blocking)
    if (m_pool) {
        m_pool->submitTask([this]() {
            // Runs in pool thread — NO Qt widget calls
            for (auto& job : m_jobs) {
                if (job.status == EncodeJobStatus::Complete) continue;
                encodeJobStub(job);
                if (!m_encoding) break;
            }
            // Signal completion back to GUI thread
            QMetaObject::invokeMethod(this, [this]() {
                m_encoding = false;
                emit encodingFinished();
            }, Qt::QueuedConnection);
        });
        return;
    }

    // Fallback: synchronous encoding (no pool available)
    for (auto& job : m_jobs) {
        if (job.status == EncodeJobStatus::Complete) continue;
        encodeJobStub(job);
        if (!m_encoding) break;
    }

    m_encoding = false;
    emit encodingFinished();
}

void PodEncodeModule::cancelEncoding() {
    m_encoding = false;
}

// ─── Helpers ────────────────────────────────────────────────────────────────
void PodEncodeModule::applyId3ToJob(EncodeJob& job) {
    job.title         = m_id3Title;
    job.episodeNumber = m_id3Episode;
    job.seasonNumber  = m_id3Season;
    job.author        = m_id3Author;
    job.description   = m_id3Description;
    job.genre         = m_id3Genre;
    job.coverArtPath  = m_id3CoverArt;
    job.copyright     = m_id3Copyright;
    job.websiteUrl    = m_id3Website;
}

void PodEncodeModule::encodeJobStub(EncodeJob& job) {
    // v1 stub: immediately mark as complete with simulated progress
    job.status   = EncodeJobStatus::Running;
    job.progress = 0;
    emit jobStatusChanged(job.id, static_cast<int>(EncodeJobStatus::Running));

    // Simulate progress steps (synchronous for v1 stub)
    for (int pct = 0; pct <= 100; pct += 25) {
        job.progress = pct;
        emit jobProgressChanged(job.id, pct);
        if (!m_encoding) {
            job.status = EncodeJobStatus::Error;
            job.errorMsg = "Cancelled";
            emit jobStatusChanged(job.id, static_cast<int>(EncodeJobStatus::Error));
            return;
        }
    }

    job.progress = 100;
    job.status   = EncodeJobStatus::Complete;
    emit jobStatusChanged(job.id, static_cast<int>(EncodeJobStatus::Complete));
    qInfo() << "[PodEncodeModule] Stub encode complete:"
            << formatName(job.format) << "->" << job.outputPath;
}

// ─── Format helpers ─────────────────────────────────────────────────────────
QString PodEncodeModule::formatName(PodEncodeFormat fmt) {
    switch (fmt) {
    case PodEncodeFormat::MP3:  return "MP3";
    case PodEncodeFormat::AAC:  return "AAC";
    case PodEncodeFormat::OGG:  return "OGG Vorbis";
    case PodEncodeFormat::FLAC: return "FLAC";
    case PodEncodeFormat::Opus: return "Opus";
    }
    return "Unknown";
}

QString PodEncodeModule::formatExtension(PodEncodeFormat fmt) {
    switch (fmt) {
    case PodEncodeFormat::MP3:  return "mp3";
    case PodEncodeFormat::AAC:  return "m4a";
    case PodEncodeFormat::OGG:  return "ogg";
    case PodEncodeFormat::FLAC: return "flac";
    case PodEncodeFormat::Opus: return "opus";
    }
    return "bin";
}

// ─── State persistence ──────────────────────────────────────────────────────
void PodEncodeModule::saveState(QSettings& s) {
    s.beginGroup("PodEncode");
    s.setValue("inputPath",   m_inputPath);
    s.setValue("id3Title",    m_id3Title);
    s.setValue("id3Episode",  m_id3Episode);
    s.setValue("id3Season",   m_id3Season);
    s.setValue("id3Author",   m_id3Author);
    s.setValue("id3Desc",     m_id3Description);
    s.setValue("id3Genre",    m_id3Genre);
    s.setValue("id3CoverArt", m_id3CoverArt);
    s.setValue("id3Copyright",m_id3Copyright);
    s.setValue("id3Website",  m_id3Website);
    s.setValue("lufsTarget",  m_lufsTarget);
    s.endGroup();
}

void PodEncodeModule::loadState(QSettings& s) {
    s.beginGroup("PodEncode");
    m_inputPath     = s.value("inputPath").toString();
    m_id3Title      = s.value("id3Title").toString();
    m_id3Episode    = s.value("id3Episode", 0).toInt();
    m_id3Season     = s.value("id3Season", 1).toInt();
    m_id3Author     = s.value("id3Author").toString();
    m_id3Description = s.value("id3Desc").toString();
    m_id3Genre      = s.value("id3Genre", "Podcast").toString();
    m_id3CoverArt   = s.value("id3CoverArt").toString();
    m_id3Copyright  = s.value("id3Copyright").toString();
    m_id3Website    = s.value("id3Website").toString();
    m_lufsTarget    = s.value("lufsTarget", -16.0).toDouble();
    s.endGroup();
}

} // namespace M1
