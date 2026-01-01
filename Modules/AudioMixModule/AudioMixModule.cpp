/// @file   AudioMixModule.cpp
/// @path   Modules/AudioMixModule/AudioMixModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-AudioMix — Live Service Audio Mixer Implementation
/// @purpose Implements the virtual mixing console with per-channel strip
///          processing, solo/mute logic, master bus, and main mix recording.
/// @reason  Church services need professional audio mixing with a visual
///          console interface for the sound engineer.
/// @changelog
///   2026-03-09  Initial implementation

#include "AudioMixModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QDial>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QProgressBar>
#include <QPainter>
#include <QSettings>
#include <QTemporaryFile>
#include <QFile>
#include <QDataStream>
#include <QDateTime>
#include <QScrollArea>
#include <QSplitter>
#include <cmath>

namespace {

// ─── VUBar — vertical level meter bar ───────────────────────────────────────
/// We paint a vertical gradient bar that fills from bottom to top based on
/// signal level. Green→Yellow→Red thresholds at 0.6 and 0.85.
class VUBar : public QWidget {
public:
    explicit VUBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("AudioMixVUBar");
        setMinimumSize(8, 60);
        setMaximumWidth(12);
    }
    void setLevel(float level) { m_level = qBound(0.0f, level, 1.0f); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(20, 20, 30));
        int h = height();
        int fillH = static_cast<int>(h * m_level);
        if (fillH <= 0) return;
        QRect bar(0, h - fillH, width(), fillH);
        QColor color;
        if (m_level < 0.6f) color = QColor(34, 197, 94);       // green
        else if (m_level < 0.85f) color = QColor(234, 179, 8);  // yellow
        else color = QColor(239, 68, 68);                        // red
        p.fillRect(bar, color);
    }
private:
    float m_level = 0.0f;
};

// ─── ChannelStrip — single channel strip widget ─────────────────────────────
class ChannelStrip : public QWidget {
    Q_OBJECT
public:
    explicit ChannelStrip(M1::MixChannel& ch, int index, QWidget* parent = nullptr)
        : QWidget(parent), m_ch(ch), m_index(index)
    {
        setObjectName(QString("AudioMixChannel_%1").arg(index));
        setMinimumWidth(70);
        setMaximumWidth(100);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(2);

        // Channel name
        m_nameEdit = new QLineEdit(ch.name.isEmpty()
            ? QString("Ch %1").arg(index + 1) : ch.name);
        m_nameEdit->setObjectName("AudioMixChName");
        m_nameEdit->setMaximumHeight(22);
        lay->addWidget(m_nameEdit);

        // EQ knobs (compact row)
        auto* eqRow = new QHBoxLayout;
        eqRow->setSpacing(1);
        m_eqLow = createDial("L", -12, 12, 0);
        m_eqMid = createDial("M", -12, 12, 0);
        m_eqHigh = createDial("H", -12, 12, 0);
        eqRow->addWidget(m_eqLow);
        eqRow->addWidget(m_eqMid);
        eqRow->addWidget(m_eqHigh);
        lay->addLayout(eqRow);

        // Pan knob
        auto* panRow = new QHBoxLayout;
        panRow->addWidget(new QLabel("Pan"));
        m_panDial = new QDial;
        m_panDial->setObjectName("AudioMixPan");
        m_panDial->setRange(-100, 100);
        m_panDial->setValue(0);
        m_panDial->setMaximumSize(36, 36);
        panRow->addWidget(m_panDial);
        lay->addLayout(panRow);

        // Volume fader + VU meter
        auto* faderRow = new QHBoxLayout;
        m_fader = new QSlider(Qt::Vertical);
        m_fader->setObjectName("AudioMixFader");
        m_fader->setRange(0, 100);
        m_fader->setValue(100);
        m_fader->setMinimumHeight(80);
        faderRow->addWidget(m_fader);

        auto* vuCol = new QVBoxLayout;
        m_vuL = new VUBar;
        m_vuR = new VUBar;
        vuCol->addWidget(m_vuL);
        vuCol->addWidget(m_vuR);
        faderRow->addLayout(vuCol);
        lay->addLayout(faderRow, 1);

        // Mute / Solo buttons
        auto* msRow = new QHBoxLayout;
        m_muteBtn = new QPushButton("M");
        m_muteBtn->setObjectName("AudioMixMute");
        m_muteBtn->setCheckable(true);
        m_muteBtn->setMaximumWidth(30);
        m_muteBtn->setToolTip("Mute");
        m_soloBtn = new QPushButton("S");
        m_soloBtn->setObjectName("AudioMixSolo");
        m_soloBtn->setCheckable(true);
        m_soloBtn->setMaximumWidth(30);
        m_soloBtn->setToolTip("Solo");
        msRow->addWidget(m_muteBtn);
        msRow->addWidget(m_soloBtn);
        lay->addLayout(msRow);

        // Connections
        connect(m_fader, &QSlider::valueChanged, this, [this](int val) {
            m_ch.volume.store(val / 100.0f, std::memory_order_relaxed);
        });
        connect(m_panDial, &QDial::valueChanged, this, [this](int val) {
            m_ch.pan.store(val / 100.0f, std::memory_order_relaxed);
        });
        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool checked) {
            m_ch.muted.store(checked, std::memory_order_relaxed);
        });
        connect(m_soloBtn, &QPushButton::toggled, this, [this](bool checked) {
            m_ch.solo.store(checked, std::memory_order_relaxed);
        });
        connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
            m_ch.name = t;
        });
        connect(m_eqLow, &QDial::valueChanged, this, [this](int v) {
            m_ch.eqLow.store(static_cast<float>(v), std::memory_order_relaxed);
        });
        connect(m_eqMid, &QDial::valueChanged, this, [this](int v) {
            m_ch.eqMid.store(static_cast<float>(v), std::memory_order_relaxed);
        });
        connect(m_eqHigh, &QDial::valueChanged, this, [this](int v) {
            m_ch.eqHigh.store(static_cast<float>(v), std::memory_order_relaxed);
        });
    }

    void refreshMeters() {
        m_vuL->setLevel(m_ch.peakL.load(std::memory_order_relaxed));
        m_vuR->setLevel(m_ch.peakR.load(std::memory_order_relaxed));
    }

private:
    QDial* createDial(const QString& label, int min, int max, int val) {
        auto* d = new QDial;
        d->setRange(min, max);
        d->setValue(val);
        d->setMaximumSize(28, 28);
        d->setToolTip(label);
        return d;
    }

    M1::MixChannel& m_ch;
    int m_index;
    QLineEdit*   m_nameEdit = nullptr;
    QSlider*     m_fader    = nullptr;
    QDial*       m_panDial  = nullptr;
    QDial*       m_eqLow    = nullptr;
    QDial*       m_eqMid    = nullptr;
    QDial*       m_eqHigh   = nullptr;
    QPushButton* m_muteBtn  = nullptr;
    QPushButton* m_soloBtn  = nullptr;
    VUBar*       m_vuL      = nullptr;
    VUBar*       m_vuR      = nullptr;
};

// ─── MasterStrip — master bus strip ─────────────────────────────────────────
class MasterStrip : public QWidget {
public:
    explicit MasterStrip(M1::AudioMixModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("AudioMixMaster");
        setMinimumWidth(80);
        setMaximumWidth(110);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(2);

        auto* label = new QLabel("MASTER");
        label->setObjectName("AudioMixMasterLabel");
        label->setAlignment(Qt::AlignCenter);
        QFont f;
        f.setBold(true);
        label->setFont(f);
        lay->addWidget(label);

        // Master fader + VU
        auto* faderRow = new QHBoxLayout;
        m_fader = new QSlider(Qt::Vertical);
        m_fader->setObjectName("AudioMixMasterFader");
        m_fader->setRange(0, 100);
        m_fader->setValue(100);
        m_fader->setMinimumHeight(120);
        faderRow->addWidget(m_fader);

        auto* vuCol = new QVBoxLayout;
        m_vuL = new VUBar;
        m_vuR = new VUBar;
        vuCol->addWidget(m_vuL);
        vuCol->addWidget(m_vuR);
        faderRow->addLayout(vuCol);
        lay->addLayout(faderRow, 1);

        // Record button
        m_recBtn = new QPushButton("REC");
        m_recBtn->setObjectName("AudioMixRecBtn");
        m_recBtn->setCheckable(true);
        m_recBtn->setToolTip("Record main mix to WAV");
        lay->addWidget(m_recBtn);

        m_recLabel = new QLabel("00:00:00");
        m_recLabel->setObjectName("AudioMixRecTime");
        m_recLabel->setAlignment(Qt::AlignCenter);
        lay->addWidget(m_recLabel);

        connect(m_fader, &QSlider::valueChanged, this, [this](int val) {
            m_mod->setMasterVolume(val / 100.0f);
        });
        connect(m_recBtn, &QPushButton::toggled, this, [this](bool checked) {
            if (checked) {
                QString path = QFileDialog::getSaveFileName(this,
                    "Save Recording", QString(), "WAV Files (*.wav)");
                if (path.isEmpty()) { m_recBtn->setChecked(false); return; }
                m_mod->startRecording(path);
            } else {
                m_mod->stopRecording();
            }
        });
    }

    void refresh() {
        m_vuL->setLevel(m_mod->masterPeakL());
        m_vuR->setLevel(m_mod->masterPeakR());
        if (m_mod->isRecording()) {
            qint64 ms = m_mod->recordedMs();
            int sec = static_cast<int>(ms / 1000);
            m_recLabel->setText(QString("%1:%2:%3")
                .arg(sec / 3600).arg((sec % 3600) / 60, 2, 10, QChar('0'))
                .arg(sec % 60, 2, 10, QChar('0')));
        }
    }

private:
    M1::AudioMixModule* m_mod;
    QSlider*     m_fader    = nullptr;
    VUBar*       m_vuL      = nullptr;
    VUBar*       m_vuR      = nullptr;
    QPushButton* m_recBtn   = nullptr;
    QLabel*      m_recLabel = nullptr;
};

// ─── AudioMixWidget — main composite widget ─────────────────────────────────
class AudioMixWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioMixWidget(M1::AudioMixModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("AudioMixWidget");
        auto* mainLay = new QHBoxLayout(this);
        mainLay->setContentsMargins(4, 4, 4, 4);
        mainLay->setSpacing(4);

        // Channel strips in a scroll area
        auto* scrollArea = new QScrollArea;
        scrollArea->setObjectName("AudioMixScrollArea");
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto* stripContainer = new QWidget;
        m_stripLayout = new QHBoxLayout(stripContainer);
        m_stripLayout->setContentsMargins(0, 0, 0, 0);
        m_stripLayout->setSpacing(2);

        for (int i = 0; i < mod->channelCount(); ++i) {
            auto* strip = new ChannelStrip(mod->channel(i), i);
            m_strips.append(strip);
            m_stripLayout->addWidget(strip);
        }
        m_stripLayout->addStretch(1);
        scrollArea->setWidget(stripContainer);
        mainLay->addWidget(scrollArea, 1);

        // Master strip
        m_master = new MasterStrip(mod);
        mainLay->addWidget(m_master);

        // Meter refresh timer (50ms = 20Hz)
        auto* meterTimer = new QTimer(this);
        meterTimer->setInterval(50);
        connect(meterTimer, &QTimer::timeout, this, &AudioMixWidget::refreshMeters);
        meterTimer->start();
    }

public slots:
    void refreshMeters() {
        for (auto* strip : m_strips) {
            strip->refreshMeters();
        }
        m_master->refresh();
    }

private:
    M1::AudioMixModule* m_mod;
    QList<ChannelStrip*> m_strips;
    MasterStrip*  m_master = nullptr;
    QHBoxLayout*  m_stripLayout = nullptr;
};

} // anonymous namespace

#include "AudioMixModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
AudioMixModule::AudioMixModule(QObject* parent)
    : IModule(parent)
{
    // We set default channel names for a typical church service
    m_channels[0].name = "Pastor Mic";
    m_channels[1].name = "Vocals";
    m_channels[2].name = "Band";
    m_channels[3].name = "Media";

    m_auxNames[0] = "Stage Mon";
    m_auxNames[1] = "In-Ear Mon";
    m_auxNames[2] = "Aux 3";
    m_auxNames[3] = "Aux 4";
}

AudioMixModule::~AudioMixModule() {
    stopRecording();
}

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void AudioMixModule::initialize() {
    m_recDrainTimer = new QTimer(this);
    m_recDrainTimer->setInterval(50);  // 20Hz drain
    connect(m_recDrainTimer, &QTimer::timeout, this, &AudioMixModule::drainRecordingBuffer);
}

void AudioMixModule::shutdown() {
    stopRecording();
    if (m_recDrainTimer) m_recDrainTimer->stop();
}

QWidget* AudioMixModule::createWidget(QWidget* parent) {
    initialize();
    return new AudioMixWidget(this, parent);
}

// ─── RT audio processing ────────────────────────────────────────────────────
/// We process the incoming audio through each channel strip, applying gain,
/// EQ, compression, pan, mute/solo, then sum to the master bus. The master
/// bus output is mixed into the output buffer. If recording, we write the
/// master output to a ring buffer for the drain timer to flush to disk.
void AudioMixModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    if (!in.isValid || !out.isValid) return;

    const int frames   = in.frames;
    const int channels = in.channels;

    // We check if any channel has solo enabled
    bool anySolo = false;
    for (int ch = 0; ch < m_channelCount; ++ch) {
        if (m_channels[ch].solo.load(std::memory_order_relaxed)) {
            anySolo = true;
            break;
        }
    }

    float masterVol = m_masterVolume.load(std::memory_order_relaxed);
    float masterPeakL = 0.0f, masterPeakR = 0.0f;

    // Precompute per-channel EQ gains (dB → linear, once per block)
    struct ChEqGains { float low, mid, high; bool active; };
    ChEqGains eqGains[kMaxMixChannels];
    for (int ch = 0; ch < m_channelCount; ++ch) {
        float eL = m_channels[ch].eqLow.load(std::memory_order_relaxed);
        float eM = m_channels[ch].eqMid.load(std::memory_order_relaxed);
        float eH = m_channels[ch].eqHigh.load(std::memory_order_relaxed);
        eqGains[ch].active = (eL != 0.0f || eM != 0.0f || eH != 0.0f);
        if (eqGains[ch].active) {
            eqGains[ch].low  = powf(10.0f, eL / 20.0f);
            eqGains[ch].mid  = powf(10.0f, eM / 20.0f);
            eqGains[ch].high = powf(10.0f, eH / 20.0f);
        }
    }

    for (int f = 0; f < frames; ++f) {
        float mixL = 0.0f, mixR = 0.0f;

        // We sum channels based on the input interleaved layout
        // For simplicity, we treat channels as pairs mapped from the input
        for (int ch = 0; ch < m_channelCount && ch * 2 + 1 < channels * frames; ++ch) {
            auto& strip = m_channels[ch];

            // Check mute/solo
            bool isMuted = strip.muted.load(std::memory_order_relaxed);
            bool isSolo  = strip.solo.load(std::memory_order_relaxed);
            if (isMuted) continue;
            if (anySolo && !isSolo) continue;

            float vol  = strip.volume.load(std::memory_order_relaxed);
            float gain = strip.gain.load(std::memory_order_relaxed);
            float pan  = strip.pan.load(std::memory_order_relaxed);

            // We read the input sample (map channel index to input pairs)
            float sL = 0.0f, sR = 0.0f;
            if (f < in.frames) {
                int inIdx = f * in.channels;
                if (ch * 2 < in.channels) sL = in.data[inIdx + ch * 2];
                if (ch * 2 + 1 < in.channels) sR = in.data[inIdx + ch * 2 + 1];
                else sR = sL;  // mono input → duplicate
            }

            // Apply input gain
            sL *= gain;
            sR *= gain;

            // 3-band EQ (one-pole crossover network)
            if (eqGains[ch].active) {
                auto& eq = m_eqState[ch];
                // One-pole coefficients: ~300Hz low, ~2.7kHz high (at 48kHz)
                constexpr float alphaLo = 0.04f;
                constexpr float alphaHi = 0.35f;

                // Extract low band via lowpass
                eq.lpL += alphaLo * (sL - eq.lpL);
                eq.lpR += alphaLo * (sR - eq.lpR);
                float lowL = eq.lpL, lowR = eq.lpR;

                // Extract high band: input minus lowpass at high cutoff
                eq.hpL += alphaHi * (sL - eq.hpL);
                eq.hpR += alphaHi * (sR - eq.hpR);
                float highL = sL - eq.hpL;
                float highR = sR - eq.hpR;

                // Mid band is the remainder
                float midL = sL - lowL - highL;
                float midR = sR - lowR - highR;

                // Apply per-band gains and recombine
                sL = lowL * eqGains[ch].low + midL * eqGains[ch].mid + highL * eqGains[ch].high;
                sR = lowR * eqGains[ch].low + midR * eqGains[ch].mid + highR * eqGains[ch].high;
            }

            // Apply volume fader
            sL *= vol;
            sR *= vol;

            // Apply pan (constant power)
            float panL = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
            float panR = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
            sL *= panL;
            sR *= panR;

            // Simple compressor (per-channel)
            if (strip.compEnabled.load(std::memory_order_relaxed)) {
                float thresh = strip.compThreshold.load(std::memory_order_relaxed);
                float ratio  = strip.compRatio.load(std::memory_order_relaxed);
                float peak   = (fabsf(sL) > fabsf(sR)) ? fabsf(sL) : fabsf(sR);
                if (peak > thresh && ratio > 1.0f) {
                    float overDb = 20.0f * log10f(peak / thresh);
                    float reduction = overDb * (1.0f - 1.0f / ratio);
                    float attn = powf(10.0f, -reduction / 20.0f);
                    sL *= attn;
                    sR *= attn;
                }
            }

            // Update channel peak meters
            float chPeakL = fabsf(sL);
            float chPeakR = fabsf(sR);
            if (chPeakL > strip.peakL.load(std::memory_order_relaxed))
                strip.peakL.store(chPeakL, std::memory_order_relaxed);
            if (chPeakR > strip.peakR.load(std::memory_order_relaxed))
                strip.peakR.store(chPeakR, std::memory_order_relaxed);

            mixL += sL;
            mixR += sR;
        }

        // Apply master volume
        mixL *= masterVol;
        mixR *= masterVol;

        // Update master peaks
        if (fabsf(mixL) > masterPeakL) masterPeakL = fabsf(mixL);
        if (fabsf(mixR) > masterPeakR) masterPeakR = fabsf(mixR);

        // Mix into output
        if (f < out.frames) {
            int outIdx = f * out.channels;
            if (out.channels >= 2) {
                out.data[outIdx]     += mixL;
                out.data[outIdx + 1] += mixR;
            } else if (out.channels == 1) {
                out.data[outIdx] += (mixL + mixR) * 0.5f;
            }
        }

        // Write to recording ring buffer if active
        if (m_recording.load(std::memory_order_relaxed)) {
            int wi = m_recWriteIdx.load(std::memory_order_relaxed);
            int ri = m_recReadIdx.load(std::memory_order_acquire);
            int nextWi = (wi + 2) & (kRecRingSize - 1);
            // We skip if buffer is full (prefer dropping samples over blocking)
            if (nextWi != ri) {
                m_recRing[wi] = mixL;
                m_recRing[(wi + 1) & (kRecRingSize - 1)] = mixR;
                m_recWriteIdx.store((wi + 2) & (kRecRingSize - 1), std::memory_order_release);
            }
        }
    }

    // Store master peaks (decay applied on UI side)
    m_masterPeakL.store(masterPeakL, std::memory_order_relaxed);
    m_masterPeakR.store(masterPeakR, std::memory_order_relaxed);

    // Decay channel peak meters (simple exponential decay per block)
    for (int ch = 0; ch < m_channelCount; ++ch) {
        float pL = m_channels[ch].peakL.load(std::memory_order_relaxed) * 0.85f;
        float pR = m_channels[ch].peakR.load(std::memory_order_relaxed) * 0.85f;
        m_channels[ch].peakL.store(pL, std::memory_order_relaxed);
        m_channels[ch].peakR.store(pR, std::memory_order_relaxed);
    }
}

void AudioMixModule::saveState(QSettings& s) {
    s.beginGroup("AudioMix");
    s.setValue("channelCount", m_channelCount);
    s.setValue("masterVolume", m_masterVolume.load());
    for (int i = 0; i < m_channelCount; ++i) {
        s.beginGroup(QString("ch_%1").arg(i));
        s.setValue("name", m_channels[i].name);
        s.setValue("volume", m_channels[i].volume.load());
        s.setValue("pan", m_channels[i].pan.load());
        s.setValue("muted", m_channels[i].muted.load());
        s.setValue("gain", m_channels[i].gain.load());
        s.setValue("eqLow", m_channels[i].eqLow.load());
        s.setValue("eqMid", m_channels[i].eqMid.load());
        s.setValue("eqHigh", m_channels[i].eqHigh.load());
        s.endGroup();
    }
    for (int i = 0; i < kMaxAuxBuses; ++i)
        s.setValue(QString("auxName_%1").arg(i), m_auxNames[i]);
    s.endGroup();
}

void AudioMixModule::loadState(QSettings& s) {
    s.beginGroup("AudioMix");
    m_channelCount = s.value("channelCount", 4).toInt();
    m_masterVolume.store(s.value("masterVolume", 1.0).toFloat());
    for (int i = 0; i < m_channelCount; ++i) {
        s.beginGroup(QString("ch_%1").arg(i));
        m_channels[i].name = s.value("name", QString("Ch %1").arg(i + 1)).toString();
        m_channels[i].volume.store(s.value("volume", 1.0).toFloat());
        m_channels[i].pan.store(s.value("pan", 0.0).toFloat());
        m_channels[i].muted.store(s.value("muted", false).toBool());
        m_channels[i].gain.store(s.value("gain", 1.0).toFloat());
        m_channels[i].eqLow.store(s.value("eqLow", 0.0).toFloat());
        m_channels[i].eqMid.store(s.value("eqMid", 0.0).toFloat());
        m_channels[i].eqHigh.store(s.value("eqHigh", 0.0).toFloat());
        s.endGroup();
    }
    for (int i = 0; i < kMaxAuxBuses; ++i)
        m_auxNames[i] = s.value(QString("auxName_%1").arg(i), QString("Aux %1").arg(i + 1)).toString();
    s.endGroup();
}

// ─── Channel management ────────────────────────────────────────────────────
void AudioMixModule::setChannelCount(int count) {
    m_channelCount = qBound(1, count, kMaxMixChannels);
    emit channelCountChanged(m_channelCount);
}

void AudioMixModule::setMasterVolume(float vol) {
    m_masterVolume.store(qBound(0.0f, vol, 1.0f), std::memory_order_relaxed);
}

// ─── Recording ──────────────────────────────────────────────────────────────
void AudioMixModule::startRecording(const QString& outputPath) {
    if (m_recording.load()) return;

    m_recTempFile = new QTemporaryFile(this);
    m_recTempFile->setAutoRemove(true);
    if (!m_recTempFile->open()) {
        emit recordingError("Failed to create temp file for recording");
        delete m_recTempFile;
        m_recTempFile = nullptr;
        return;
    }

    // We store the output path for later WAV finalization
    m_recTempFile->setProperty("_m1_outPath", outputPath);

    m_recSampleCount = 0;
    m_recWriteIdx.store(0, std::memory_order_relaxed);
    m_recReadIdx.store(0, std::memory_order_relaxed);
    m_recChannels.store(2, std::memory_order_relaxed);
    m_recSampleRate.store(48000.0, std::memory_order_relaxed);

    m_recording.store(true, std::memory_order_release);
    m_recDrainTimer->start();
    emit recordingStateChanged(true);
}

void AudioMixModule::stopRecording() {
    if (!m_recording.load()) return;
    m_recording.store(false, std::memory_order_release);
    m_recDrainTimer->stop();

    // Drain any remaining samples
    drainRecordingBuffer();

    if (m_recTempFile) {
        QString outPath = m_recTempFile->property("_m1_outPath").toString();
        if (!outPath.isEmpty()) {
            // Write final WAV file
            QFile outFile(outPath);
            if (outFile.open(QIODevice::WriteOnly)) {
                writeWavHeader(outFile, 2, 48000.0, m_recSampleCount);
                // Copy PCM data from temp file
                m_recTempFile->seek(0);
                while (!m_recTempFile->atEnd()) {
                    QByteArray chunk = m_recTempFile->read(65536);
                    outFile.write(chunk);
                }
                outFile.close();
            }
        }
        delete m_recTempFile;
        m_recTempFile = nullptr;
    }

    emit recordingStateChanged(false);
}

qint64 AudioMixModule::recordedMs() const {
    double sr = m_recSampleRate.load(std::memory_order_relaxed);
    int ch = m_recChannels.load(std::memory_order_relaxed);
    if (sr <= 0 || ch <= 0) return 0;
    return static_cast<qint64>((m_recSampleCount / ch / sr) * 1000.0);
}

void AudioMixModule::drainRecordingBuffer() {
    if (!m_recTempFile || !m_recTempFile->isOpen()) return;

    int ri = m_recReadIdx.load(std::memory_order_relaxed);
    int wi = m_recWriteIdx.load(std::memory_order_acquire);

    while (ri != wi) {
        int available = (wi - ri + kRecRingSize) & (kRecRingSize - 1);
        int toRead = qMin(available, 4096);

        for (int i = 0; i < toRead; ++i) {
            float sample = m_recRing[(ri + i) & (kRecRingSize - 1)];
            m_recTempFile->write(reinterpret_cast<const char*>(&sample), sizeof(float));
            m_recSampleCount++;
        }

        ri = (ri + toRead) & (kRecRingSize - 1);
        m_recReadIdx.store(ri, std::memory_order_release);
        wi = m_recWriteIdx.load(std::memory_order_acquire);
    }
}

bool AudioMixModule::writeWavHeader(QFile& file, int channels, double sampleRate, qint64 sampleCount) {
    // We write a standard WAV header for 32-bit float PCM
    qint64 dataSize = sampleCount * sizeof(float);
    qint64 fileSize = 36 + dataSize;

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    // RIFF header
    file.write("RIFF", 4);
    ds << static_cast<quint32>(fileSize);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    ds << quint32(16);             // chunk size
    ds << quint16(3);              // format: IEEE float
    ds << quint16(channels);
    ds << quint32(static_cast<quint32>(sampleRate));
    ds << quint32(static_cast<quint32>(sampleRate * channels * sizeof(float)));  // byte rate
    ds << quint16(channels * sizeof(float));  // block align
    ds << quint16(32);             // bits per sample

    // data chunk
    file.write("data", 4);
    ds << quint32(dataSize);

    return true;
}

void AudioMixModule::setAuxBusName(int bus, const QString& name) {
    if (bus >= 0 && bus < kMaxAuxBuses) m_auxNames[bus] = name;
}

QString AudioMixModule::auxBusName(int bus) const {
    if (bus >= 0 && bus < kMaxAuxBuses) return m_auxNames[bus];
    return {};
}

} // namespace M1
