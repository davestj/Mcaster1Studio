/// @file   PodMixerModule.cpp
/// @path   Modules/PodMixerModule/PodMixerModule.cpp

#include "PodMixerModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QDial>
#include <QPushButton>
#include <QScrollArea>
#include <QPainter>
#include <QTimer>
#include <QSettings>
#include <cmath>

namespace {

// ─── VUBar — vertical level meter ──────────────────────────────────────────
class VUBar : public QWidget {
public:
    explicit VUBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(8);
        setMinimumHeight(60);
        setObjectName("PodVUBar");
    }
    void setLevel(float v) { m_level = v; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));
        const int h = static_cast<int>(height() * std::clamp(m_level, 0.0f, 1.0f));
        if (h > 0) {
            QColor c = m_level < 0.7f ? QColor(0x22, 0xc5, 0x5e)
                     : m_level < 0.9f ? QColor(0xf5, 0x9e, 0x0b)
                                      : QColor(0xef, 0x44, 0x44);
            p.fillRect(0, height() - h, width(), h, c);
        }
    }
private:
    float m_level = 0.0f;
};

// ─── PodChannelStrip ────────────────────────────────────────────────────────
class PodChannelStrip : public QWidget {
    Q_OBJECT
public:
    explicit PodChannelStrip(M1::PodMixChannel& ch, int index, QWidget* parent = nullptr)
        : QWidget(parent), m_ch(ch), m_index(index)
    {
        setObjectName("PodChannelStrip");
        setFixedWidth(80);
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(3);

        m_nameLabel = new QLabel(ch.name.isEmpty() ? QString("Ch %1").arg(index + 1) : ch.name);
        m_nameLabel->setAlignment(Qt::AlignCenter);
        m_nameLabel->setObjectName("PodStripName");
        QFont f = m_nameLabel->font();
        f.setPixelSize(11);
        f.setBold(true);
        m_nameLabel->setFont(f);
        lay->addWidget(m_nameLabel);

        // Gain trim dial
        auto* gainDial = new QDial;
        gainDial->setRange(0, 200);
        gainDial->setValue(100);
        gainDial->setFixedSize(40, 40);
        gainDial->setToolTip("Gain Trim");
        connect(gainDial, &QDial::valueChanged, [this](int v) {
            m_ch.gainTrim.store(v / 100.0f, std::memory_order_relaxed);
        });
        lay->addWidget(gainDial, 0, Qt::AlignCenter);

        // EQ dials (H/M/L)
        for (int eq = 0; eq < 3; ++eq) {
            auto* dial = new QDial;
            dial->setRange(-120, 120);
            dial->setValue(0);
            dial->setFixedSize(32, 32);
            const char* labels[] = {"H", "M", "L"};
            dial->setToolTip(QString("EQ %1").arg(labels[eq]));
            std::atomic<float>* target = eq == 0 ? &m_ch.eqHigh
                                       : eq == 1 ? &m_ch.eqMid
                                                  : &m_ch.eqLow;
            connect(dial, &QDial::valueChanged, [target](int v) {
                target->store(v / 10.0f, std::memory_order_relaxed);
            });
            lay->addWidget(dial, 0, Qt::AlignCenter);
        }

        // Pan dial
        auto* panDial = new QDial;
        panDial->setRange(-100, 100);
        panDial->setValue(0);
        panDial->setFixedSize(32, 32);
        panDial->setToolTip("Pan");
        connect(panDial, &QDial::valueChanged, [this](int v) {
            m_ch.pan.store(v / 100.0f, std::memory_order_relaxed);
        });
        lay->addWidget(panDial, 0, Qt::AlignCenter);

        // VU + Fader row
        auto* faderRow = new QHBoxLayout;
        m_vuL = new VUBar;
        m_vuR = new VUBar;
        m_fader = new QSlider(Qt::Vertical);
        m_fader->setRange(0, 100);
        m_fader->setValue(100);
        m_fader->setMinimumHeight(80);
        m_fader->setToolTip("Volume");
        connect(m_fader, &QSlider::valueChanged, [this](int v) {
            m_ch.volume.store(v / 100.0f, std::memory_order_relaxed);
        });
        faderRow->addWidget(m_vuL);
        faderRow->addWidget(m_fader);
        faderRow->addWidget(m_vuR);
        lay->addLayout(faderRow, 1);

        // Mute / Solo buttons
        auto* btnRow = new QHBoxLayout;
        auto* muteBtn = new QPushButton("M");
        muteBtn->setCheckable(true);
        muteBtn->setFixedSize(28, 24);
        muteBtn->setObjectName("PodMuteBtn");
        muteBtn->setToolTip("Mute");
        connect(muteBtn, &QPushButton::toggled, [this](bool on) {
            m_ch.muted.store(on, std::memory_order_relaxed);
        });
        auto* soloBtn = new QPushButton("S");
        soloBtn->setCheckable(true);
        soloBtn->setFixedSize(28, 24);
        soloBtn->setObjectName("PodSoloBtn");
        soloBtn->setToolTip("Solo");
        connect(soloBtn, &QPushButton::toggled, [this](bool on) {
            m_ch.solo.store(on, std::memory_order_relaxed);
        });
        btnRow->addWidget(muteBtn);
        btnRow->addWidget(soloBtn);
        lay->addLayout(btnRow);
    }

    void refreshMeters() {
        m_vuL->setLevel(m_ch.peakL.load(std::memory_order_relaxed));
        m_vuR->setLevel(m_ch.peakR.load(std::memory_order_relaxed));
    }

private:
    M1::PodMixChannel& m_ch;
    int m_index;
    QLabel*  m_nameLabel;
    QSlider* m_fader;
    VUBar*   m_vuL;
    VUBar*   m_vuR;
};

// ─── PodMixerWidget ─────────────────────────────────────────────────────────
class PodMixerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodMixerWidget(M1::PodMixerModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodMixerWidget");
        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(2);

        // Channel strips in scroll area
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setObjectName("PodMixerScroll");

        auto* stripContainer = new QWidget;
        auto* stripLay = new QHBoxLayout(stripContainer);
        stripLay->setContentsMargins(0, 0, 0, 0);
        stripLay->setSpacing(2);

        for (int i = 0; i < mod->channelCount(); ++i) {
            auto* strip = new PodChannelStrip(mod->channel(i), i);
            stripLay->addWidget(strip);
            m_strips.append(strip);
        }
        stripLay->addStretch();

        scroll->setWidget(stripContainer);
        root->addWidget(scroll, 1);

        // Master section
        auto* masterGroup = new QGroupBox("Master");
        masterGroup->setObjectName("PodMasterGroup");
        masterGroup->setFixedWidth(100);
        auto* mLay = new QVBoxLayout(masterGroup);

        m_masterFader = new QSlider(Qt::Vertical);
        m_masterFader->setRange(0, 100);
        m_masterFader->setValue(100);
        m_masterFader->setToolTip("Master Volume");
        connect(m_masterFader, &QSlider::valueChanged, [mod](int v) {
            mod->setMasterVolume(v / 100.0f);
        });

        m_masterVuL = new VUBar;
        m_masterVuR = new VUBar;

        auto* vuRow = new QHBoxLayout;
        vuRow->addWidget(m_masterVuL);
        vuRow->addWidget(m_masterFader);
        vuRow->addWidget(m_masterVuR);
        mLay->addLayout(vuRow, 1);

        // Headphone volume
        auto* hpLabel = new QLabel("HP");
        hpLabel->setAlignment(Qt::AlignCenter);
        mLay->addWidget(hpLabel);
        auto* hpDial = new QDial;
        hpDial->setRange(0, 100);
        hpDial->setValue(80);
        hpDial->setFixedSize(40, 40);
        hpDial->setToolTip("Headphone Volume");
        connect(hpDial, &QDial::valueChanged, [mod](int v) {
            mod->setHeadphoneVolume(v / 100.0f);
        });
        mLay->addWidget(hpDial, 0, Qt::AlignCenter);

        root->addWidget(masterGroup);

        // Meter refresh
        connect(mod, &M1::PodMixerModule::channelLevelsUpdated, this, &PodMixerWidget::refreshMeters);
    }

private slots:
    void refreshMeters() {
        for (auto* s : m_strips) s->refreshMeters();
        m_masterVuL->setLevel(m_mod->masterPeakL());
        m_masterVuR->setLevel(m_mod->masterPeakR());
    }

private:
    M1::PodMixerModule* m_mod;
    QList<PodChannelStrip*> m_strips;
    QSlider* m_masterFader;
    VUBar*   m_masterVuL;
    VUBar*   m_masterVuR;
};

} // anonymous namespace

#include "PodMixerModule.moc"

namespace M1 {

PodMixerModule::PodMixerModule(QObject* parent) : IModule(parent) {
    // Default channel names
    m_channels[0].name = "Host";
    m_channels[1].name = "Guest 1";
    m_channels[2].name = "Guest 2";
    m_channels[3].name = "Soundboard";
    m_channels[4].name = "Music";
    m_channels[5].name = "Phone";
    m_auxNames[0] = "Aux 1";
    m_auxNames[1] = "Aux 2";
}

PodMixerModule::~PodMixerModule() = default;

void PodMixerModule::initialize() {
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(50);
    connect(m_meterTimer, &QTimer::timeout, this, &PodMixerModule::channelLevelsUpdated);
    m_meterTimer->start();
}

void PodMixerModule::shutdown() {
    if (m_meterTimer) m_meterTimer->stop();
}

QWidget* PodMixerModule::createWidget(QWidget* parent) {
    return new PodMixerWidget(this, parent);
}

void PodMixerModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    if (!out.isValid || out.frames <= 0) return;
    const int ch = out.channels;
    const int frames = out.frames;
    const float masterVol = m_masterVol.load(std::memory_order_relaxed);

    // Check for solo
    bool anySolo = false;
    for (int c = 0; c < kPodMixChannels; ++c) {
        if (m_channels[c].solo.load(std::memory_order_relaxed)) {
            anySolo = true;
            break;
        }
    }

    float peakL = 0.0f, peakR = 0.0f;

    // Mix channels (simplified — in production each channel would have its own input)
    // For now, each channel processes the same input with its own settings
    for (int c = 0; c < kPodMixChannels; ++c) {
        auto& mc = m_channels[c];
        const bool muted = mc.muted.load(std::memory_order_relaxed);
        const bool solo  = mc.solo.load(std::memory_order_relaxed);
        if (muted || (anySolo && !solo)) {
            mc.peakL.store(0.0f, std::memory_order_relaxed);
            mc.peakR.store(0.0f, std::memory_order_relaxed);
            continue;
        }

        const float vol  = mc.volume.load(std::memory_order_relaxed);
        const float gain = mc.gainTrim.load(std::memory_order_relaxed);
        const float pan  = mc.pan.load(std::memory_order_relaxed);
        const float finalGain = vol * gain;
        const float panL = std::cos((pan + 1.0f) * 0.25f * 3.14159f);
        const float panR = std::sin((pan + 1.0f) * 0.25f * 3.14159f);

        float chPeakL = 0.0f, chPeakR = 0.0f;
        for (int f = 0; f < frames; ++f) {
            const float sL = (ch >= 1) ? in.data[f * ch] * finalGain : 0.0f;
            const float sR = (ch >= 2) ? in.data[f * ch + 1] * finalGain : sL;
            const float oL = sL * panL;
            const float oR = sR * panR;
            if (fabsf(oL) > chPeakL) chPeakL = fabsf(oL);
            if (fabsf(oR) > chPeakR) chPeakR = fabsf(oR);
        }
        mc.peakL.store(chPeakL, std::memory_order_relaxed);
        mc.peakR.store(chPeakR, std::memory_order_relaxed);
        if (chPeakL > peakL) peakL = chPeakL;
        if (chPeakR > peakR) peakR = chPeakR;
    }

    // Apply master volume
    for (int i = 0; i < frames * ch; ++i)
        out.data[i] *= masterVol;

    m_masterPeakL.store(peakL * masterVol, std::memory_order_relaxed);
    m_masterPeakR.store(peakR * masterVol, std::memory_order_relaxed);
}

void PodMixerModule::saveState(QSettings& s) {
    for (int i = 0; i < kPodMixChannels; ++i) {
        const QString prefix = QString("ch%1/").arg(i);
        s.setValue(prefix + "name", m_channels[i].name);
        s.setValue(prefix + "volume", m_channels[i].volume.load(std::memory_order_relaxed));
        s.setValue(prefix + "pan", m_channels[i].pan.load(std::memory_order_relaxed));
        s.setValue(prefix + "muted", m_channels[i].muted.load(std::memory_order_relaxed));
        s.setValue(prefix + "gain", m_channels[i].gainTrim.load(std::memory_order_relaxed));
    }
    s.setValue("masterVolume", m_masterVol.load(std::memory_order_relaxed));
    s.setValue("hpVolume", m_hpVol.load(std::memory_order_relaxed));
}

void PodMixerModule::loadState(QSettings& s) {
    for (int i = 0; i < kPodMixChannels; ++i) {
        const QString prefix = QString("ch%1/").arg(i);
        m_channels[i].name = s.value(prefix + "name", m_channels[i].name).toString();
        m_channels[i].volume.store(s.value(prefix + "volume", 1.0f).toFloat(), std::memory_order_relaxed);
        m_channels[i].pan.store(s.value(prefix + "pan", 0.0f).toFloat(), std::memory_order_relaxed);
        m_channels[i].muted.store(s.value(prefix + "muted", false).toBool(), std::memory_order_relaxed);
        m_channels[i].gainTrim.store(s.value(prefix + "gain", 1.0f).toFloat(), std::memory_order_relaxed);
    }
    m_masterVol.store(s.value("masterVolume", 1.0f).toFloat(), std::memory_order_relaxed);
    m_hpVol.store(s.value("hpVolume", 0.8f).toFloat(), std::memory_order_relaxed);
}

void PodMixerModule::setAuxName(int bus, const QString& name) {
    if (bus >= 0 && bus < kPodAuxBuses) m_auxNames[bus] = name;
}

QString PodMixerModule::auxName(int bus) const {
    if (bus >= 0 && bus < kPodAuxBuses) return m_auxNames[bus];
    return {};
}

} // namespace M1
