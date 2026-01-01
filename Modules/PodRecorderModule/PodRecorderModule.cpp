/// @file   PodRecorderModule.cpp
/// @path   Modules/PodRecorderModule/PodRecorderModule.cpp

#include "PodRecorderModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QTimer>
#include <QSettings>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QFileDialog>
#include <QPainter>
#include <cmath>
#include <cstring>

namespace {

class RecTimeDisplay : public QLabel {
public:
    explicit RecTimeDisplay(QWidget* parent = nullptr) : QLabel("00:00:00", parent) {
        setObjectName("PodRecTimeDisplay");
        setAlignment(Qt::AlignCenter);
        QFont f = font();
        f.setPixelSize(32);
        f.setBold(true);
        f.setFamily("Consolas");
        setFont(f);
    }
};

class PodRecorderWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodRecorderWidget(M1::PodRecorderModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodRecorderWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // Show / episode info
        auto* metaRow = new QHBoxLayout;
        metaRow->addWidget(new QLabel("Show:"));
        auto* showEdit = new QLineEdit(mod->showName());
        showEdit->setObjectName("PodRecShowEdit");
        showEdit->setPlaceholderText("My Podcast");
        connect(showEdit, &QLineEdit::textChanged, mod, &M1::PodRecorderModule::setShowName);
        metaRow->addWidget(showEdit, 1);
        metaRow->addWidget(new QLabel("Ep#"));
        auto* epSpin = new QSpinBox;
        epSpin->setRange(1, 9999);
        epSpin->setValue(mod->episodeNumber());
        connect(epSpin, QOverload<int>::of(&QSpinBox::valueChanged), mod, &M1::PodRecorderModule::setEpisodeNumber);
        metaRow->addWidget(epSpin);
        root->addLayout(metaRow);

        // Time display
        m_timeDisplay = new RecTimeDisplay;
        root->addWidget(m_timeDisplay);

        // Status label
        m_statusLabel = new QLabel("IDLE");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setObjectName("PodRecStatus");
        QFont sf = m_statusLabel->font();
        sf.setPixelSize(14);
        sf.setBold(true);
        m_statusLabel->setFont(sf);
        root->addWidget(m_statusLabel);

        // Transport buttons
        auto* transportRow = new QHBoxLayout;
        transportRow->setSpacing(6);

        m_recBtn = new QPushButton("REC");
        m_recBtn->setObjectName("PodRecBtn");
        m_recBtn->setMinimumHeight(40);
        QFont bf = m_recBtn->font();
        bf.setPixelSize(14);
        bf.setBold(true);
        m_recBtn->setFont(bf);
        connect(m_recBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->recordingState() == M1::PodRecorderModule::State::Idle)
                m_mod->startRecording();
            else
                m_mod->stopRecording();
        });
        transportRow->addWidget(m_recBtn);

        m_pauseBtn = new QPushButton("PAUSE");
        m_pauseBtn->setObjectName("PodRecPauseBtn");
        m_pauseBtn->setMinimumHeight(40);
        connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->recordingState() == M1::PodRecorderModule::State::Recording)
                m_mod->pauseRecording();
            else if (m_mod->recordingState() == M1::PodRecorderModule::State::Paused)
                m_mod->resumeRecording();
        });
        transportRow->addWidget(m_pauseBtn);

        auto* markerBtn = new QPushButton("+ Marker");
        markerBtn->setObjectName("PodRecMarkerBtn");
        markerBtn->setMinimumHeight(40);
        markerBtn->setToolTip("Drop a marker at current position");
        connect(markerBtn, &QPushButton::clicked, this, [this]() {
            m_mod->addMarker();
        });
        transportRow->addWidget(markerBtn);

        root->addLayout(transportRow);

        // Marker list
        auto* markerGroup = new QGroupBox("Markers");
        auto* mLay = new QVBoxLayout(markerGroup);
        m_markerList = new QListWidget;
        m_markerList->setObjectName("PodRecMarkerList");
        m_markerList->setMaximumHeight(100);
        mLay->addWidget(m_markerList);
        root->addWidget(markerGroup);

        // Export button
        auto* exportBtn = new QPushButton("Export WAV...");
        exportBtn->setObjectName("PodRecExportBtn");
        connect(exportBtn, &QPushButton::clicked, this, &PodRecorderWidget::onExport);
        root->addWidget(exportBtn);

        // Info line
        m_infoLabel = new QLabel;
        m_infoLabel->setObjectName("PodRecInfo");
        root->addWidget(m_infoLabel);

        // Refresh timer
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &PodRecorderWidget::onRefresh);
        timer->start(200);

        connect(mod, &M1::PodRecorderModule::markerAdded, this, &PodRecorderWidget::onRefresh);
    }

private slots:
    void onRefresh() {
        const auto state = m_mod->recordingState();
        const qint64 ms = m_mod->elapsedMs();
        const int secs = static_cast<int>(ms / 1000);
        m_timeDisplay->setText(QString("%1:%2:%3")
            .arg(secs / 3600, 2, 10, QChar('0'))
            .arg((secs % 3600) / 60, 2, 10, QChar('0'))
            .arg(secs % 60, 2, 10, QChar('0')));

        switch (state) {
        case M1::PodRecorderModule::State::Idle:
            m_statusLabel->setText("IDLE");
            m_recBtn->setText("REC");
            m_pauseBtn->setText("PAUSE");
            break;
        case M1::PodRecorderModule::State::Recording:
            m_statusLabel->setText("RECORDING");
            m_recBtn->setText("STOP");
            m_pauseBtn->setText("PAUSE");
            break;
        case M1::PodRecorderModule::State::Paused:
            m_statusLabel->setText("PAUSED");
            m_recBtn->setText("STOP");
            m_pauseBtn->setText("RESUME");
            break;
        }

        // Update marker list
        const auto markers = m_mod->markers();
        if (m_markerList->count() != markers.size()) {
            m_markerList->clear();
            for (const auto& m : markers) {
                const int s = static_cast<int>(m.timestampMs / 1000);
                m_markerList->addItem(QString("[%1:%2] %3")
                    .arg(s / 60, 2, 10, QChar('0'))
                    .arg(s % 60, 2, 10, QChar('0'))
                    .arg(m.label.isEmpty() ? QString("Marker %1").arg(m_markerList->count() + 1) : m.label));
            }
        }

        m_infoLabel->setText(QString("Take %1 | %2-bit %3 Hz")
            .arg(m_mod->currentTake())
            .arg(m_mod->bitDepth())
            .arg(m_mod->sampleRateHz()));
    }

    void onExport() {
        const QString dir = QFileDialog::getExistingDirectory(this, "Export Recording");
        if (dir.isEmpty()) return;
        QString error;
        if (!m_mod->exportAll(dir, error))
            m_statusLabel->setText("Export failed: " + error);
        else
            m_statusLabel->setText("Exported to " + dir);
    }

private:
    M1::PodRecorderModule* m_mod;
    RecTimeDisplay* m_timeDisplay;
    QLabel*         m_statusLabel;
    QLabel*         m_infoLabel;
    QPushButton*    m_recBtn;
    QPushButton*    m_pauseBtn;
    QListWidget*    m_markerList;
};

} // anonymous namespace

#include "PodRecorderModule.moc"

namespace M1 {

PodRecorderModule::PodRecorderModule(QObject* parent) : IModule(parent) {
    for (auto& a : m_trackArms) a.store(true, std::memory_order_relaxed);
}

PodRecorderModule::~PodRecorderModule() { delete m_tempFile; }

void PodRecorderModule::initialize() {
    m_drainTimer = new QTimer(this);
    m_drainTimer->setInterval(50);
    connect(m_drainTimer, &QTimer::timeout, this, &PodRecorderModule::drainBuffers);
}

void PodRecorderModule::shutdown() {
    if (recordingState() != State::Idle) stopRecording();
    if (m_drainTimer) m_drainTimer->stop();
}

QWidget* PodRecorderModule::createWidget(QWidget* parent) {
    return new PodRecorderWidget(this, parent);
}

void PodRecorderModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    (void)out;
    if (!m_captureActive.load(std::memory_order_relaxed)) return;
    if (!in.isValid || in.frames <= 0) return;

    m_sampleRate.store(in.sampleRate, std::memory_order_relaxed);
    m_channels.store(in.channels, std::memory_order_relaxed);

    const int total = in.frames * in.channels;
    int wr = m_writeIdx.load(std::memory_order_relaxed);
    for (int i = 0; i < total; ++i) {
        m_ring[wr] = in.data[i];
        wr = (wr + 1) % kRingSize;
    }
    m_writeIdx.store(wr, std::memory_order_release);
}

void PodRecorderModule::startRecording() {
    if (recordingState() != State::Idle) return;
    openTempFile();
    if (!m_tempFile) { emit recordingError("Failed to create temp file"); return; }
    m_sampleCount = 0;
    m_pausedMs = 0;
    m_elapsed.start();
    m_writeIdx.store(0, std::memory_order_relaxed);
    m_readIdx.store(0, std::memory_order_relaxed);
    m_captureActive.store(true, std::memory_order_relaxed);
    m_state.store(static_cast<int>(State::Recording), std::memory_order_relaxed);
    m_drainTimer->start();
    emit stateChanged(static_cast<int>(State::Recording));
}

void PodRecorderModule::pauseRecording() {
    if (recordingState() != State::Recording) return;
    m_captureActive.store(false, std::memory_order_relaxed);
    m_pausedMs += m_elapsed.elapsed() - m_pausedMs;
    m_state.store(static_cast<int>(State::Paused), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(State::Paused));
}

void PodRecorderModule::resumeRecording() {
    if (recordingState() != State::Paused) return;
    m_captureActive.store(true, std::memory_order_relaxed);
    m_state.store(static_cast<int>(State::Recording), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(State::Recording));
}

void PodRecorderModule::stopRecording() {
    m_captureActive.store(false, std::memory_order_relaxed);
    m_drainTimer->stop();
    drainBuffers(); // final drain
    closeTempFile();
    m_state.store(static_cast<int>(State::Idle), std::memory_order_relaxed);
    m_takeNumber++;
    emit stateChanged(static_cast<int>(State::Idle));
}

qint64 PodRecorderModule::elapsedMs() const {
    if (recordingState() == State::Idle) return 0;
    return m_elapsed.elapsed() - m_pausedMs;
}

void PodRecorderModule::addMarker(const QString& label) {
    RecordMarker m;
    m.timestampMs = elapsedMs();
    m.label = label;
    m.color = QColor(0x0e, 0xa5, 0xe9);
    m_markers.append(m);
    emit markerAdded(m_markers.size() - 1);
}

void PodRecorderModule::removeMarker(int index) {
    if (index >= 0 && index < m_markers.size()) m_markers.removeAt(index);
}

void PodRecorderModule::setTrackArm(int track, bool arm) {
    if (track >= 0 && track < kMaxRecTracks)
        m_trackArms[track].store(arm, std::memory_order_relaxed);
}

bool PodRecorderModule::trackArm(int track) const {
    if (track >= 0 && track < kMaxRecTracks)
        return m_trackArms[track].load(std::memory_order_relaxed);
    return false;
}

void PodRecorderModule::setTrackCount(int count) {
    m_trackCount = std::clamp(count, 1, kMaxRecTracks);
}

bool PodRecorderModule::exportAll(const QString& outputDir, QString& errorOut) {
    if (!m_tempFile || m_sampleCount == 0) {
        errorOut = "No recording data";
        return false;
    }

    const int ch = m_channels.load(std::memory_order_relaxed);
    const double sr = m_sampleRate.load(std::memory_order_relaxed);
    const QString fileName = autoFileName();
    const QString outPath = outputDir + "/" + fileName + ".wav";

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) {
        errorOut = "Cannot open " + outPath;
        return false;
    }

    writeWavHeader(out, ch, sr, m_sampleCount);

    // Copy PCM data from temp file
    m_tempFile->seek(0);
    char buf[8192];
    while (!m_tempFile->atEnd()) {
        const qint64 n = m_tempFile->read(buf, sizeof(buf));
        if (n > 0) out.write(buf, n);
    }
    return true;
}

QString PodRecorderModule::autoFileName() const {
    return QString("%1_EP%2_%3_Take%4")
        .arg(m_showName.isEmpty() ? "Podcast" : m_showName)
        .arg(m_episodeNum, 3, 10, QChar('0'))
        .arg(QDate::currentDate().toString("yyyy-MM-dd"))
        .arg(m_takeNumber);
}

qint64 PodRecorderModule::estimatedFileSizeBytes() const {
    const double sr = m_sampleRate.load(std::memory_order_relaxed);
    const int ch = m_channels.load(std::memory_order_relaxed);
    const double bytesPerSample = (m_bitDepth == 24) ? 3.0 : 2.0;
    return static_cast<qint64>(sr * ch * bytesPerSample * (elapsedMs() / 1000.0));
}

void PodRecorderModule::saveState(QSettings& s) {
    s.setValue("showName", m_showName);
    s.setValue("episodeNumber", m_episodeNum);
    s.setValue("bitDepth", m_bitDepth);
}

void PodRecorderModule::loadState(QSettings& s) {
    m_showName = s.value("showName").toString();
    m_episodeNum = s.value("episodeNumber", 1).toInt();
    m_bitDepth = s.value("bitDepth", 24).toInt();
}

void PodRecorderModule::drainBuffers() {
    if (!m_tempFile) return;
    int rd = m_readIdx.load(std::memory_order_relaxed);
    const int wr = m_writeIdx.load(std::memory_order_acquire);

    while (rd != wr) {
        int count = 0;
        while (rd != wr && count < kDrainBuf) {
            m_drainScratch[count++] = m_ring[rd];
            rd = (rd + 1) % kRingSize;
        }
        // Write as 32-bit float PCM
        m_tempFile->write(reinterpret_cast<const char*>(m_drainScratch),
                          count * sizeof(float));
        m_sampleCount += count;
    }
    m_readIdx.store(rd, std::memory_order_relaxed);
}

void PodRecorderModule::openTempFile() {
    m_tempFile = new QTemporaryFile(QDir::tempPath() + "/m1_podrec_XXXXXX.raw");
    m_tempFile->setAutoRemove(true);
    if (!m_tempFile->open()) {
        delete m_tempFile;
        m_tempFile = nullptr;
    }
}

void PodRecorderModule::closeTempFile() {
    // keep temp file for export — will be deleted with module
}

bool PodRecorderModule::writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount) {
    const int bytesPerSample = 4; // 32-bit float
    const qint64 dataSize = sampleCount * bytesPerSample;
    const qint64 fileSize = 36 + dataSize;

    auto write16 = [&](quint16 v) { file.write(reinterpret_cast<const char*>(&v), 2); };
    auto write32 = [&](quint32 v) { file.write(reinterpret_cast<const char*>(&v), 4); };

    file.write("RIFF", 4);
    write32(static_cast<quint32>(fileSize));
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    write32(16);
    write16(3); // IEEE float
    write16(static_cast<quint16>(channels));
    write32(static_cast<quint32>(sampleRate));
    write32(static_cast<quint32>(sampleRate * channels * bytesPerSample));
    write16(static_cast<quint16>(channels * bytesPerSample));
    write16(static_cast<quint16>(bytesPerSample * 8));
    file.write("data", 4);
    write32(static_cast<quint32>(dataSize));
    return true;
}

} // namespace M1
