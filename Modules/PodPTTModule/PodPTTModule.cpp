/// @file   PodPTTModule.cpp
/// @path   Modules/PodPTTModule/PodPTTModule.cpp

#include "PodPTTModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDial>
#include <QComboBox>
#include <QGroupBox>
#include <QPainter>
#include <QTimer>
#include <QSettings>
#include <QMouseEvent>
#include <cmath>

namespace {

// ─── VADIndicator — voice activity LED ──────────────────────────────────────
class VADIndicator : public QWidget {
public:
    explicit VADIndicator(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(16, 16);
    }
    void setActive(bool on) { m_on = on; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(m_on ? QColor(0x22, 0xc5, 0x5e) : QColor(0x33, 0x33, 0x33));
        p.drawEllipse(rect().adjusted(2, 2, -2, -2));
    }
private:
    bool m_on = false;
};

// ─── LevelBar — horizontal input level meter ───────────────────────────────
class LevelBar : public QWidget {
public:
    explicit LevelBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(12);
        setMinimumWidth(120);
    }
    void setLevel(float v) { m_level = v; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(0x1a, 0x1a, 0x2e));
        const int w = static_cast<int>(width() * std::clamp(m_level, 0.0f, 1.0f));
        if (w > 0) {
            QColor c = m_level < 0.7f ? QColor(0x22, 0xc5, 0x5e)
                     : m_level < 0.9f ? QColor(0xf5, 0x9e, 0x0b)
                                      : QColor(0xef, 0x44, 0x44);
            p.fillRect(0, 0, w, height(), c);
        }
    }
private:
    float m_level = 0.0f;
};

// ─── TalkButton — press-and-hold or toggle talk button ──────────────────────
class TalkButton : public QPushButton {
    Q_OBJECT
public:
    explicit TalkButton(M1::PodPTTModule* mod, QWidget* parent = nullptr)
        : QPushButton("TALK", parent), m_mod(mod)
    {
        setObjectName("PodTalkBtn");
        setMinimumHeight(60);
        QFont f = font();
        f.setPixelSize(20);
        f.setBold(true);
        setFont(f);
        setCheckable(true);
    }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (m_mod->activationMode() == M1::PodPTTModule::ActivationMode::HoldToTalk) {
            m_mod->setState(M1::PodPTTModule::State::Live);
            setChecked(true);
        } else {
            QPushButton::mousePressEvent(e);
            if (isChecked())
                m_mod->setState(M1::PodPTTModule::State::Live);
            else
                m_mod->setState(M1::PodPTTModule::State::Off);
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_mod->activationMode() == M1::PodPTTModule::ActivationMode::HoldToTalk) {
            m_mod->setState(M1::PodPTTModule::State::Off);
            setChecked(false);
        } else {
            QPushButton::mouseReleaseEvent(e);
        }
    }
private:
    M1::PodPTTModule* m_mod;
};

// ─── PodPTTWidget ───────────────────────────────────────────────────────────
class PodPTTWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodPTTWidget(M1::PodPTTModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodPTTWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // Participant name + VAD
        auto* nameRow = new QHBoxLayout;
        m_nameLabel = new QLabel(mod->participantName().isEmpty() ? "Host" : mod->participantName());
        m_nameLabel->setObjectName("PodPTTName");
        QFont nf = m_nameLabel->font();
        nf.setPixelSize(16);
        nf.setBold(true);
        m_nameLabel->setFont(nf);
        nameRow->addWidget(m_nameLabel);
        nameRow->addStretch();
        auto* vadLabel = new QLabel("Voice:");
        nameRow->addWidget(vadLabel);
        m_vad = new VADIndicator;
        nameRow->addWidget(m_vad);
        root->addLayout(nameRow);

        // Level meter
        m_levelBar = new LevelBar;
        root->addWidget(m_levelBar);

        // Talk button
        m_talkBtn = new TalkButton(mod);
        root->addWidget(m_talkBtn);

        // Cough button
        auto* coughBtn = new QPushButton("COUGH");
        coughBtn->setObjectName("PodCoughBtn");
        coughBtn->setMinimumHeight(32);
        connect(coughBtn, &QPushButton::pressed, mod, &M1::PodPTTModule::coughStart);
        connect(coughBtn, &QPushButton::released, mod, &M1::PodPTTModule::coughEnd);
        root->addWidget(coughBtn);

        // Mode selector
        auto* modeRow = new QHBoxLayout;
        modeRow->addWidget(new QLabel("Mode:"));
        auto* modeCombo = new QComboBox;
        modeCombo->addItems({"Hold to Talk", "Toggle", "Always On"});
        modeCombo->setCurrentIndex(static_cast<int>(mod->activationMode()));
        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [mod](int i) {
            mod->setActivationMode(static_cast<M1::PodPTTModule::ActivationMode>(i));
        });
        modeRow->addWidget(modeCombo, 1);
        root->addLayout(modeRow);

        // DSP controls
        auto* dspGroup = new QGroupBox("Voice Processing");
        dspGroup->setObjectName("PodDSPGroup");
        auto* dspLay = new QGridLayout(dspGroup);
        int row = 0;

        auto addDspDial = [&](const QString& label, float min, float max, float val, auto setter) {
            dspLay->addWidget(new QLabel(label), row, 0);
            auto* dial = new QDial;
            dial->setRange(static_cast<int>(min * 100), static_cast<int>(max * 100));
            dial->setValue(static_cast<int>(val * 100));
            dial->setFixedSize(36, 36);
            connect(dial, &QDial::valueChanged, [mod, setter](int v) {
                (mod->*setter)(v / 100.0f);
            });
            dspLay->addWidget(dial, row, 1);
            row++;
        };

        addDspDial("Gate",     0.0f, 0.3f, 0.02f, &M1::PodPTTModule::setGateThreshold);
        addDspDial("De-Ess",   0.0f, 1.0f, 0.0f,  &M1::PodPTTModule::setDeEssAmount);
        addDspDial("Comp",     0.1f, 1.0f, 0.7f,   &M1::PodPTTModule::setCompThreshold);
        addDspDial("Gain",     0.0f, 3.0f, 1.0f,   &M1::PodPTTModule::setGain);

        root->addWidget(dspGroup);

        // Talkback toggle
        auto* tbBtn = new QPushButton("Talkback");
        tbBtn->setCheckable(true);
        tbBtn->setObjectName("PodTalkbackBtn");
        tbBtn->setToolTip("Talk to guests' headphones only (not recorded)");
        connect(tbBtn, &QPushButton::toggled, mod, &M1::PodPTTModule::setTalkback);
        root->addWidget(tbBtn);

        // Refresh timer
        auto* refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &PodPTTWidget::onRefresh);
        refreshTimer->start(50);
    }

private slots:
    void onRefresh() {
        m_levelBar->setLevel(m_mod->inputLevel());
        m_vad->setActive(m_mod->voiceActive());

        // Always-on mode auto-activates
        if (m_mod->activationMode() == M1::PodPTTModule::ActivationMode::AlwaysOn
            && m_mod->state() == M1::PodPTTModule::State::Off) {
            m_mod->setState(M1::PodPTTModule::State::Live);
            m_talkBtn->setChecked(true);
        }
    }

private:
    M1::PodPTTModule* m_mod;
    QLabel*       m_nameLabel;
    LevelBar*     m_levelBar;
    TalkButton*   m_talkBtn;
    VADIndicator* m_vad;
};

} // anonymous namespace

#include "PodPTTModule.moc"

namespace M1 {

PodPTTModule::PodPTTModule(QObject* parent) : IModule(parent) {
    for (auto& a : std::array<std::atomic<bool>*, 0>{}) (void)a; // suppress unused
}

PodPTTModule::~PodPTTModule() = default;

void PodPTTModule::initialize() {
    m_meterTimer = new QTimer(this);
    m_meterTimer->setInterval(50);
    m_meterTimer->start();
}

void PodPTTModule::shutdown() {
    if (m_meterTimer) m_meterTimer->stop();
}

QWidget* PodPTTModule::createWidget(QWidget* parent) {
    return new PodPTTWidget(this, parent);
}

void PodPTTModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
    if (!in.isValid || in.frames <= 0) return;
    const auto st = static_cast<State>(m_state.load(std::memory_order_relaxed));
    const float gain = m_gain.load(std::memory_order_relaxed);
    const float coughGain = m_coughGain.load(std::memory_order_relaxed);
    const float gateThresh = m_gateThresh.load(std::memory_order_relaxed);
    const int total = in.frames * in.channels;

    float peak = 0.0f;
    bool voice = false;

    for (int i = 0; i < total; ++i) {
        float s = in.data[i];

        // Gate
        if (fabsf(s) < gateThresh) s = 0.0f;

        // Apply gain
        s *= gain;

        // Cough fade
        s *= coughGain;

        // Only pass through when Live
        if (st == State::Live || st == State::Cough) {
            // Mix into output (additive)
            if (i < out.frames * out.channels)
                out.data[i] += s;
        }

        if (fabsf(s) > peak) peak = fabsf(s);
        if (fabsf(s) > gateThresh) voice = true;
    }

    m_level.store(peak, std::memory_order_relaxed);
    m_vad.store(voice, std::memory_order_relaxed);
}

void PodPTTModule::setState(State s) {
    m_state.store(static_cast<int>(s), std::memory_order_relaxed);
    emit stateChanged(static_cast<int>(s));
}

void PodPTTModule::setActivationMode(ActivationMode mode) {
    m_actMode = mode;
    if (mode == ActivationMode::AlwaysOn)
        setState(State::Live);
}

void PodPTTModule::coughStart() {
    m_prevState = state();
    m_state.store(static_cast<int>(State::Cough), std::memory_order_relaxed);
    m_coughGain.store(0.0f, std::memory_order_relaxed); // instant mute for now
    emit stateChanged(static_cast<int>(State::Cough));
}

void PodPTTModule::coughEnd() {
    m_coughGain.store(1.0f, std::memory_order_relaxed);
    setState(m_prevState);
}

void PodPTTModule::saveState(QSettings& s) {
    s.setValue("activationMode", static_cast<int>(m_actMode));
    s.setValue("participantName", m_participantName);
    s.setValue("inputDevice", m_inputDevice);
    s.setValue("gain", m_gain.load(std::memory_order_relaxed));
    s.setValue("gateThreshold", m_gateThresh.load(std::memory_order_relaxed));
    s.setValue("deEss", m_deEss.load(std::memory_order_relaxed));
    s.setValue("compThreshold", m_compThresh.load(std::memory_order_relaxed));
    s.setValue("compRatio", m_compRatio.load(std::memory_order_relaxed));
    s.setValue("hpfFreq", m_hpfFreq.load(std::memory_order_relaxed));
}

void PodPTTModule::loadState(QSettings& s) {
    m_actMode = static_cast<ActivationMode>(s.value("activationMode", 0).toInt());
    m_participantName = s.value("participantName").toString();
    m_inputDevice = s.value("inputDevice").toString();
    m_gain.store(s.value("gain", 1.0f).toFloat(), std::memory_order_relaxed);
    m_gateThresh.store(s.value("gateThreshold", 0.02f).toFloat(), std::memory_order_relaxed);
    m_deEss.store(s.value("deEss", 0.0f).toFloat(), std::memory_order_relaxed);
    m_compThresh.store(s.value("compThreshold", 0.7f).toFloat(), std::memory_order_relaxed);
    m_compRatio.store(s.value("compRatio", 3.0f).toFloat(), std::memory_order_relaxed);
    m_hpfFreq.store(s.value("hpfFreq", 80.0f).toFloat(), std::memory_order_relaxed);
}

} // namespace M1
