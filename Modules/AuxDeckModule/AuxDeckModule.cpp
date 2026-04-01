#include "AuxDeckModule.h"
#include "DeckPlayer.h"
#include "AudioDevice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QStyle>
#include <QSettings>
#include <cmath>

namespace M1 {

// ─── Construction ────────────────────────────────────────────────────────────

AuxDeckModule::AuxDeckModule(QObject* parent)
    : IModule(parent)
{
}

AuxDeckModule::~AuxDeckModule()
{
    shutdown();
}

QString AuxDeckModule::displayName() const
{
    return m_deckName.isEmpty() ? QStringLiteral("AUX Deck") : m_deckName;
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void AuxDeckModule::initialize()
{
    // DeckPlayer requires a deck index: use 99 to indicate auxiliary
    m_player = new DeckPlayer(99, this);

    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);
    connect(m_levelTimer, &QTimer::timeout, this, &AuxDeckModule::onPollLevels);
    m_levelTimer->start();
}

void AuxDeckModule::shutdown()
{
    if (m_levelTimer) {
        m_levelTimer->stop();
    }
    if (m_player) {
        m_player->stop();
    }
}

// ─── UI ──────────────────────────────────────────────────────────────────────

QWidget* AuxDeckModule::createWidget(QWidget* parent)
{
    m_widget = new QWidget(parent);
    m_widget->setObjectName("AuxDeckWidget");
    buildWidget(m_widget);
    return m_widget;
}

void AuxDeckModule::buildWidget(QWidget* container)
{
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // ── Deck name ────────────────────────────────────────────────────────
    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel("Deck Name:"));
    m_nameEdit = new QLineEdit(m_deckName);
    m_nameEdit->setPlaceholderText("Enter custom deck name...");
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        setDeckName(t);
    });
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    // ── Audio device routing ─────────────────────────────────────────────
    auto* devGroup = new QGroupBox("Audio Device Routing");
    devGroup->setObjectName("AuxDeckDeviceGroup");
    auto* devLayout = new QVBoxLayout(devGroup);

    auto* airRow = new QHBoxLayout;
    airRow->addWidget(new QLabel("AIR OUT:"));
    m_airOutCombo = new QComboBox;
    m_airOutCombo->setToolTip("Main on-air output device for this deck");
    connect(m_airOutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx >= 0)
            setAirOutDevice(m_airOutCombo->itemData(idx).toInt());
    });
    airRow->addWidget(m_airOutCombo, 1);
    devLayout->addLayout(airRow);

    auto* cueRow = new QHBoxLayout;
    cueRow->addWidget(new QLabel("CUE OUT:"));
    m_cueOutCombo = new QComboBox;
    m_cueOutCombo->setToolTip("Headphone/preview output device for this deck");
    connect(m_cueOutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx >= 0)
            setCueOutDevice(m_cueOutCombo->itemData(idx).toInt());
    });
    cueRow->addWidget(m_cueOutCombo, 1);
    devLayout->addLayout(cueRow);

    layout->addWidget(devGroup);

    // Populate device lists
    refreshDeviceList();

    // ── Track info ───────────────────────────────────────────────────────
    m_trackLabel = new QLabel("No track loaded");
    m_trackLabel->setObjectName("AuxDeckTrackLabel");
    m_trackLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_trackLabel);

    m_timeLabel = new QLabel("00:00 / 00:00");
    m_timeLabel->setObjectName("AuxDeckTimeLabel");
    m_timeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_timeLabel);

    // ── VU meters (simple bar indicators) ────────────────────────────────
    auto* vuRow = new QHBoxLayout;
    m_vuL = new QLabel;
    m_vuL->setObjectName("AuxDeckVU_L");
    m_vuL->setFixedHeight(8);
    m_vuL->setStyleSheet("background: #22c55e; border-radius: 2px;");
    m_vuR = new QLabel;
    m_vuR->setObjectName("AuxDeckVU_R");
    m_vuR->setFixedHeight(8);
    m_vuR->setStyleSheet("background: #22c55e; border-radius: 2px;");
    vuRow->addWidget(new QLabel("L"));
    vuRow->addWidget(m_vuL, 1);
    vuRow->addWidget(new QLabel("R"));
    vuRow->addWidget(m_vuR, 1);
    layout->addLayout(vuRow);

    // ── Volume slider ────────────────────────────────────────────────────
    auto* volRow = new QHBoxLayout;
    volRow->addWidget(new QLabel("Vol:"));
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        setVolume(v / 100.0f);
    });
    volRow->addWidget(m_volumeSlider, 1);
    layout->addLayout(volRow);

    // ── Transport controls ───────────────────────────────────────────────
    auto* transport = new QHBoxLayout;

    auto* loadBtn = new QPushButton("Load");
    loadBtn->setObjectName("AuxDeckLoad");
    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            m_widget, "Load Audio File", QString(),
            "Audio Files (*.mp3 *.wav *.ogg *.flac *.aac *.opus *.m4a);;All Files (*)");
        if (!file.isEmpty())
            loadFile(file);
    });
    transport->addWidget(loadBtn);

    auto* playBtn = new QPushButton("Play");
    playBtn->setObjectName("AuxDeckPlay");
    connect(playBtn, &QPushButton::clicked, this, &AuxDeckModule::play);
    transport->addWidget(playBtn);

    auto* pauseBtn = new QPushButton("Pause");
    pauseBtn->setObjectName("AuxDeckPause");
    connect(pauseBtn, &QPushButton::clicked, this, &AuxDeckModule::pause);
    transport->addWidget(pauseBtn);

    auto* stopBtn = new QPushButton("Stop");
    stopBtn->setObjectName("AuxDeckStop");
    connect(stopBtn, &QPushButton::clicked, this, &AuxDeckModule::stop);
    transport->addWidget(stopBtn);

    auto* cueBtn = new QPushButton("CUE");
    cueBtn->setObjectName("AuxDeckCue");
    cueBtn->setCheckable(true);
    connect(cueBtn, &QPushButton::toggled, this, &AuxDeckModule::setCueActive);
    transport->addWidget(cueBtn);

    layout->addLayout(transport);

    layout->addStretch();
}

void AuxDeckModule::refreshDeviceList()
{
    if (!m_airOutCombo || !m_cueOutCombo) return;

    m_airOutCombo->blockSignals(true);
    m_cueOutCombo->blockSignals(true);

    m_airOutCombo->clear();
    m_cueOutCombo->clear();

    // Default = follow global
    m_airOutCombo->addItem("(Use Global Default)", -1);
    m_cueOutCombo->addItem("(Use Global Default)", -1);

    // Enumerate PortAudio output devices (free functions in M1:: namespace)
    auto devices = enumerateOutputDevices();
    for (const auto& dev : devices) {
        const QString label = QString("%1 — %2").arg(dev.name, dev.hostApi);
        m_airOutCombo->addItem(label, dev.index);
        m_cueOutCombo->addItem(label, dev.index);
    }

    // Restore selections
    for (int i = 0; i < m_airOutCombo->count(); ++i) {
        if (m_airOutCombo->itemData(i).toInt() == m_airOutDevice) {
            m_airOutCombo->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < m_cueOutCombo->count(); ++i) {
        if (m_cueOutCombo->itemData(i).toInt() == m_cueOutDevice) {
            m_cueOutCombo->setCurrentIndex(i);
            break;
        }
    }

    m_airOutCombo->blockSignals(false);
    m_cueOutCombo->blockSignals(false);
}

// ─── Playback ────────────────────────────────────────────────────────────────

void AuxDeckModule::loadFile(const QString& filePath)
{
    if (m_player)
        m_player->loadFile(filePath);

    if (m_trackLabel) {
        QFileInfo fi(filePath);
        m_trackLabel->setText(fi.baseName());
    }
    emit statusChanged("Loaded: " + QFileInfo(filePath).fileName());
}

void AuxDeckModule::play()
{
    if (m_player) m_player->play();
    emit playbackStateChanged(1);
}

void AuxDeckModule::pause()
{
    if (m_player) m_player->pause();
    emit playbackStateChanged(2);
}

void AuxDeckModule::stop()
{
    if (m_player) m_player->stop();
    emit playbackStateChanged(0);
}

void AuxDeckModule::setCueActive(bool active)
{
    m_cueActive.store(active);
}

void AuxDeckModule::setVolume(float v)
{
    m_volume.store(std::clamp(v, 0.0f, 1.0f));
}

void AuxDeckModule::setDeckName(const QString& name)
{
    if (m_deckName != name) {
        m_deckName = name;
        emit deckNameChanged(name);
        emit statusChanged(name);
    }
}

void AuxDeckModule::setAirOutDevice(int deviceIndex)
{
    m_airOutDevice = deviceIndex;
    emit deviceConfigChanged();
}

void AuxDeckModule::setCueOutDevice(int deviceIndex)
{
    m_cueOutDevice = deviceIndex;
    emit deviceConfigChanged();
}

// ─── Audio RT callback ──────────────────────────────────────────────────────

void AuxDeckModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out)
{
    (void)in;
    if (!m_player) return;

    // AudioBuffer is interleaved: [L0, R0, L1, R1, ...]
    const float vol = m_volume.load(std::memory_order_relaxed);
    const int frames = out.frames;
    const int ch = out.channels;
    float* d = out.data;
    if (!d) return;

    float pL = 0.0f, pR = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const int base = i * ch;
        d[base] *= vol;
        if (ch > 1) d[base + 1] *= vol;

        const float aL = fabsf(d[base]);
        if (aL > pL) pL = aL;
        if (ch > 1) {
            const float aR = fabsf(d[base + 1]);
            if (aR > pR) pR = aR;
        }
    }
    m_peakL.store(pL, std::memory_order_relaxed);
    m_peakR.store(pR, std::memory_order_relaxed);
}

// ─── Level polling (Qt thread) ──────────────────────────────────────────────

void AuxDeckModule::onPollLevels()
{
    if (!m_vuL || !m_vuR) return;

    float pL = m_peakL.load(std::memory_order_relaxed);
    float pR = m_peakR.load(std::memory_order_relaxed);

    // Map to width percentage (max 200px)
    auto widthFor = [](float peak) -> int {
        return static_cast<int>(std::clamp(peak, 0.0f, 1.0f) * 200.0f);
    };

    auto colorFor = [](float peak) -> QString {
        if (peak > 0.9f) return "#ef4444"; // red
        if (peak > 0.7f) return "#f59e0b"; // amber
        return "#22c55e"; // green
    };

    m_vuL->setFixedWidth(std::max(2, widthFor(pL)));
    m_vuL->setStyleSheet(
        QString("background: %1; border-radius: 2px;").arg(colorFor(pL)));
    m_vuR->setFixedWidth(std::max(2, widthFor(pR)));
    m_vuR->setStyleSheet(
        QString("background: %1; border-radius: 2px;").arg(colorFor(pR)));

    // Update time display using DeckPlayer API
    if (m_player && m_timeLabel) {
        double posSec = m_player->positionSeconds();
        double durSec = m_player->durationSeconds();
        auto fmt = [](double sec) {
            int s = static_cast<int>(sec);
            if (s < 0) s = 0;
            return QString("%1:%2").arg(s / 60, 2, 10, QChar('0'))
                                   .arg(s % 60, 2, 10, QChar('0'));
        };
        m_timeLabel->setText(fmt(posSec) + " / " + fmt(durSec));
    }

    // Decay peaks
    m_peakL.store(pL * 0.85f, std::memory_order_relaxed);
    m_peakR.store(pR * 0.85f, std::memory_order_relaxed);
}

// ─── State persistence ──────────────────────────────────────────────────────

void AuxDeckModule::saveState(QSettings& s)
{
    s.setValue("auxdeck/name", m_deckName);
    s.setValue("auxdeck/airOutDevice", m_airOutDevice);
    s.setValue("auxdeck/cueOutDevice", m_cueOutDevice);
    s.setValue("auxdeck/volume", static_cast<double>(m_volume.load()));
}

void AuxDeckModule::loadState(QSettings& s)
{
    m_deckName     = s.value("auxdeck/name", "AUX Deck").toString();
    m_airOutDevice = s.value("auxdeck/airOutDevice", -1).toInt();
    m_cueOutDevice = s.value("auxdeck/cueOutDevice", -1).toInt();
    float vol      = s.value("auxdeck/volume", 1.0).toFloat();
    m_volume.store(vol);

    if (m_nameEdit)       m_nameEdit->setText(m_deckName);
    if (m_volumeSlider)   m_volumeSlider->setValue(static_cast<int>(vol * 100));
    refreshDeviceList();
}

} // namespace M1
