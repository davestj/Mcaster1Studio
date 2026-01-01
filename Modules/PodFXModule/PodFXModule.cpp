/// @file   PodFXModule.cpp
/// @path   Modules/PodFXModule/PodFXModule.cpp

#include "PodFXModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDial>
#include <QScrollArea>
#include <QSettings>
#include <QPainter>
#include <QTimer>
#include <cmath>

namespace {

// ─── FXStripWidget — one processor row in the rack ──────────────────────────
class FXStripWidget : public QWidget {
    Q_OBJECT
public:
    explicit FXStripWidget(M1::PodFXModule* mod, int index, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod), m_index(index)
    {
        setObjectName("PodFXStrip");
        setMinimumHeight(56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* root = new QHBoxLayout(this);
        root->setContentsMargins(6, 4, 6, 4);
        root->setSpacing(6);

        // Bypass toggle
        m_bypassBtn = new QPushButton;
        m_bypassBtn->setCheckable(true);
        m_bypassBtn->setFixedSize(28, 28);
        m_bypassBtn->setObjectName("PodFXBypassBtn");
        m_bypassBtn->setToolTip("Bypass processor");
        updateBypassButton();
        connect(m_bypassBtn, &QPushButton::toggled, this, [this](bool checked) {
            m_mod->setProcessorEnabled(m_index, !checked);
            updateBypassButton();
        });
        root->addWidget(m_bypassBtn);

        // Processor name
        const auto& proc = mod->processor(index);
        m_nameLabel = new QLabel(M1::FXProcessor::typeName(proc.type));
        m_nameLabel->setObjectName("PodFXStripName");
        m_nameLabel->setFixedWidth(90);
        QFont nf = m_nameLabel->font();
        nf.setPixelSize(12);
        nf.setBold(true);
        m_nameLabel->setFont(nf);
        root->addWidget(m_nameLabel);

        // Parameter dials
        m_dialLayout = new QHBoxLayout;
        m_dialLayout->setSpacing(4);
        rebuildDials();
        root->addLayout(m_dialLayout, 1);

        // Move up/down buttons
        auto* upBtn = new QPushButton("\xe2\x96\xb2");
        upBtn->setFixedSize(22, 22);
        upBtn->setObjectName("PodFXMoveBtn");
        upBtn->setToolTip("Move up");
        connect(upBtn, &QPushButton::clicked, this, [this]() {
            if (m_index > 0) {
                m_mod->moveProcessor(m_index, m_index - 1);
            }
        });
        root->addWidget(upBtn);

        auto* downBtn = new QPushButton("\xe2\x96\xbc");
        downBtn->setFixedSize(22, 22);
        downBtn->setObjectName("PodFXMoveBtn");
        downBtn->setToolTip("Move down");
        connect(downBtn, &QPushButton::clicked, this, [this]() {
            if (m_index < m_mod->processorCount() - 1) {
                m_mod->moveProcessor(m_index, m_index + 1);
            }
        });
        root->addWidget(downBtn);
    }

private:
    void updateBypassButton() {
        const auto& proc = m_mod->processor(m_index);
        m_bypassBtn->setChecked(!proc.enabled);
        m_bypassBtn->setText(proc.enabled ? "ON" : "BP");
    }

    void rebuildDials() {
        // Clear existing
        while (m_dialLayout->count() > 0) {
            auto* item = m_dialLayout->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }

        const auto& proc = m_mod->processor(m_index);

        // LUFS meter is display-only — show label instead of dials
        if (proc.type == M1::FXProcessorType::LUFSMeter) {
            m_lufsLabel = new QLabel("-70.0 LUFS");
            m_lufsLabel->setObjectName("PodFXLufsDisplay");
            m_lufsLabel->setAlignment(Qt::AlignCenter);
            QFont lf = m_lufsLabel->font();
            lf.setPixelSize(16);
            lf.setBold(true);
            lf.setFamily("Consolas");
            m_lufsLabel->setFont(lf);
            m_dialLayout->addWidget(m_lufsLabel);

            // Refresh timer for LUFS display
            auto* timer = new QTimer(this);
            connect(timer, &QTimer::timeout, this, [this]() {
                if (m_lufsLabel) {
                    const float lufs = m_mod->currentLUFS();
                    m_lufsLabel->setText(QString("%1 LUFS")
                        .arg(lufs, 0, 'f', 1));
                }
            });
            timer->start(100);
            return;
        }

        // Build dials for each parameter
        const auto keys = proc.params.keys();
        for (const QString& key : keys) {
            auto* dialContainer = new QWidget;
            dialContainer->setObjectName("PodFXDialContainer");
            auto* dLay = new QVBoxLayout(dialContainer);
            dLay->setContentsMargins(0, 0, 0, 0);
            dLay->setSpacing(1);

            auto* dial = new QDial;
            dial->setObjectName("PodFXDial");
            dial->setFixedSize(36, 36);
            dial->setToolTip(key);

            // Set dial range and value based on parameter semantics
            int minVal = 0, maxVal = 100;
            float scale = 100.0f;

            if (key.contains("threshold", Qt::CaseInsensitive)) {
                minVal = -60; maxVal = 0; scale = 1.0f;
            } else if (key.contains("ratio", Qt::CaseInsensitive)) {
                minVal = 10; maxVal = 200; scale = 10.0f;
            } else if (key.contains("attack", Qt::CaseInsensitive) ||
                       key.contains("release", Qt::CaseInsensitive) ||
                       key.contains("hold", Qt::CaseInsensitive)) {
                minVal = 1; maxVal = 500; scale = 1.0f;
            } else if (key.contains("makeup", Qt::CaseInsensitive) ||
                       key.contains("gain", Qt::CaseInsensitive) ||
                       key.contains("ceiling", Qt::CaseInsensitive)) {
                minVal = -24; maxVal = 24; scale = 1.0f;
            } else if (key.contains("freq", Qt::CaseInsensitive)) {
                minVal = 20; maxVal = 20000; scale = 1.0f;
            } else if (key.contains("q", Qt::CaseInsensitive)) {
                minVal = 1; maxVal = 100; scale = 10.0f;
            } else if (key.contains("amount", Qt::CaseInsensitive)) {
                minVal = 0; maxVal = 100; scale = 100.0f;
            }

            dial->setRange(minVal, maxVal);
            dial->setValue(static_cast<int>(proc.params[key] * scale));

            const int idx = m_index;
            connect(dial, &QDial::valueChanged, this, [this, key, scale, idx](int v) {
                m_mod->setProcessorParam(idx, key, v / scale);
            });
            dLay->addWidget(dial, 0, Qt::AlignCenter);

            // Short label below dial
            auto* label = new QLabel(key.left(6));
            label->setObjectName("PodFXDialLabel");
            label->setAlignment(Qt::AlignCenter);
            QFont lf = label->font();
            lf.setPixelSize(9);
            label->setFont(lf);
            dLay->addWidget(label);

            m_dialLayout->addWidget(dialContainer);
        }
    }

    M1::PodFXModule* m_mod;
    int              m_index;
    QPushButton*     m_bypassBtn;
    QLabel*          m_nameLabel;
    QHBoxLayout*     m_dialLayout;
    QLabel*          m_lufsLabel = nullptr;
};

// ─── PodFXWidget — main rack view ───────────────────────────────────────────
class PodFXWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodFXWidget(M1::PodFXModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodFXWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // Header
        auto* header = new QLabel("Podcast Effects Rack");
        header->setObjectName("PodFXHeader");
        header->setAlignment(Qt::AlignCenter);
        QFont hf = header->font();
        hf.setPixelSize(16);
        hf.setBold(true);
        header->setFont(hf);
        root->addWidget(header);

        // Scroll area for processor strips
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setObjectName("PodFXScroll");

        m_stripContainer = new QWidget;
        m_stripContainer->setObjectName("PodFXStripContainer");
        m_stripLayout = new QVBoxLayout(m_stripContainer);
        m_stripLayout->setContentsMargins(4, 4, 4, 4);
        m_stripLayout->setSpacing(4);

        rebuildStrips();

        scroll->setWidget(m_stripContainer);
        root->addWidget(scroll, 1);

        connect(mod, &M1::PodFXModule::chainChanged, this, &PodFXWidget::rebuildStrips);
    }

private slots:
    void rebuildStrips() {
        // Remove existing strips
        for (auto* s : m_strips) {
            m_stripLayout->removeWidget(s);
            delete s;
        }
        m_strips.clear();

        for (int i = 0; i < m_mod->processorCount(); ++i) {
            auto* strip = new FXStripWidget(m_mod, i);
            m_stripLayout->addWidget(strip);
            m_strips.append(strip);
        }
        m_stripLayout->addStretch();
    }

private:
    M1::PodFXModule*       m_mod;
    QVBoxLayout*           m_stripLayout;
    QWidget*               m_stripContainer;
    QList<FXStripWidget*>  m_strips;
};

} // anonymous namespace

#include "PodFXModule.moc"

namespace M1 {

// ─── FXProcessor static helpers ──────────────────────────────────────────────

QString FXProcessor::typeName(FXProcessorType t) {
    switch (t) {
    case FXProcessorType::NoiseGate:     return "Noise Gate";
    case FXProcessorType::Compressor:    return "Compressor";
    case FXProcessorType::DeEsser:       return "De-Esser";
    case FXProcessorType::ParametricEQ:  return "Parametric EQ";
    case FXProcessorType::HighPassFilter: return "High-Pass";
    case FXProcessorType::Limiter:       return "Limiter";
    case FXProcessorType::LUFSMeter:     return "LUFS Meter";
    }
    return "Unknown";
}

FXProcessor FXProcessor::createDefault(FXProcessorType t) {
    FXProcessor p;
    p.type    = t;
    p.enabled = true;

    switch (t) {
    case FXProcessorType::NoiseGate:
        p.params["threshold"] = -40.0f;  // dB
        p.params["attack"]    = 1.0f;    // ms
        p.params["hold"]      = 50.0f;   // ms
        p.params["release"]   = 100.0f;  // ms
        break;

    case FXProcessorType::Compressor:
        p.params["threshold"] = -18.0f;  // dB
        p.params["ratio"]     = 4.0f;    // :1
        p.params["attack"]    = 10.0f;   // ms
        p.params["release"]   = 100.0f;  // ms
        p.params["makeup"]    = 0.0f;    // dB
        break;

    case FXProcessorType::DeEsser:
        p.params["amount"]    = 0.5f;    // 0–1
        break;

    case FXProcessorType::ParametricEQ:
        // 4 bands: freq (Hz) + gain (dB) + Q
        p.params["freq1"]     = 100.0f;
        p.params["gain1"]     = 0.0f;
        p.params["q1"]        = 1.0f;
        p.params["freq2"]     = 500.0f;
        p.params["gain2"]     = 0.0f;
        p.params["q2"]        = 1.0f;
        p.params["freq3"]     = 2000.0f;
        p.params["gain3"]     = 0.0f;
        p.params["q3"]        = 1.0f;
        p.params["freq4"]     = 8000.0f;
        p.params["gain4"]     = 0.0f;
        p.params["q4"]        = 1.0f;
        break;

    case FXProcessorType::HighPassFilter:
        p.params["freq"]      = 80.0f;   // Hz
        break;

    case FXProcessorType::Limiter:
        p.params["ceiling"]   = -1.0f;   // dB
        break;

    case FXProcessorType::LUFSMeter:
        // No user-adjustable parameters — display only
        break;
    }

    return p;
}

// ─── PodFXModule ─────────────────────────────────────────────────────────────

PodFXModule::PodFXModule(QObject* parent) : IModule(parent) {}
PodFXModule::~PodFXModule() = default;

void PodFXModule::initialize() {
    // Build default chain if empty (no saved state loaded)
    if (m_chain.isEmpty()) {
        m_chain.append(FXProcessor::createDefault(FXProcessorType::HighPassFilter));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::NoiseGate));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::Compressor));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::DeEsser));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::ParametricEQ));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::Limiter));
        m_chain.append(FXProcessor::createDefault(FXProcessorType::LUFSMeter));
    }
}

void PodFXModule::shutdown() {}

QWidget* PodFXModule::createWidget(QWidget* parent) {
    return new PodFXWidget(this, parent);
}

// ─── RT audio processing ─────────────────────────────────────────────────────

void PodFXModule::onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& out) {
    if (!out.isValid || out.frames <= 0) return;

    // Process chain in order on the accumulated mix (out buffer, in-place)
    for (const auto& proc : m_chain) {
        if (!proc.enabled) continue;

        switch (proc.type) {
        case FXProcessorType::NoiseGate:
            applyNoiseGate(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::Compressor:
            applyCompressor(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::DeEsser:
            applyDeEsser(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::ParametricEQ:
            applyParametricEQ(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::HighPassFilter:
            applyHighPass(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::Limiter:
            applyLimiter(out.data, out.frames, out.channels, proc);
            break;
        case FXProcessorType::LUFSMeter:
            measureLUFS(out.data, out.frames, out.channels);
            break;
        }
    }
}

// ─── RT processor implementations ────────────────────────────────────────────
// NOTE: These are simplified implementations suitable for v1.
// Full production versions would use proper DSP coefficient calculation.

void PodFXModule::applyNoiseGate(float* data, int frames, int channels,
                                  const FXProcessor& proc)
{
    // Simple envelope-follower gate
    const float threshLin = std::pow(10.0f, proc.params.value("threshold", -40.0f) / 20.0f);
    const float attackCoeff  = 1.0f - std::exp(-1.0f / (proc.params.value("attack", 1.0f) * 48.0f));
    const float releaseCoeff = 1.0f - std::exp(-1.0f / (proc.params.value("release", 100.0f) * 48.0f));

    for (int f = 0; f < frames; ++f) {
        // Peak across channels
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            const float s = fabsf(data[f * channels + c]);
            if (s > peak) peak = s;
        }

        // Envelope follower
        const float coeff = (peak > m_gateEnvelope) ? attackCoeff : releaseCoeff;
        m_gateEnvelope += coeff * (peak - m_gateEnvelope);

        // Gate gain
        const float gateGain = (m_gateEnvelope >= threshLin) ? 1.0f : 0.0f;

        for (int c = 0; c < channels; ++c)
            data[f * channels + c] *= gateGain;
    }
}

void PodFXModule::applyCompressor(float* data, int frames, int channels,
                                   const FXProcessor& proc)
{
    const float threshDb  = proc.params.value("threshold", -18.0f);
    const float ratio     = proc.params.value("ratio", 4.0f);
    const float makeupDb  = proc.params.value("makeup", 0.0f);
    const float attackCoeff  = 1.0f - std::exp(-1.0f / (proc.params.value("attack", 10.0f) * 48.0f));
    const float releaseCoeff = 1.0f - std::exp(-1.0f / (proc.params.value("release", 100.0f) * 48.0f));
    const float makeupLin = std::pow(10.0f, makeupDb / 20.0f);

    for (int f = 0; f < frames; ++f) {
        // Peak detect across channels
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) {
            const float s = fabsf(data[f * channels + c]);
            if (s > peak) peak = s;
        }

        // Envelope
        const float coeff = (peak > m_compEnvelope) ? attackCoeff : releaseCoeff;
        m_compEnvelope += coeff * (peak - m_compEnvelope);

        // Gain computation
        float gainDb = 0.0f;
        if (m_compEnvelope > 0.0f) {
            const float envDb = 20.0f * std::log10(m_compEnvelope + 1e-30f);
            if (envDb > threshDb) {
                const float overDb = envDb - threshDb;
                gainDb = overDb * (1.0f / ratio - 1.0f);
            }
        }

        const float gainLin = std::pow(10.0f, gainDb / 20.0f) * makeupLin;

        for (int c = 0; c < channels; ++c)
            data[f * channels + c] *= gainLin;
    }
}

void PodFXModule::applyDeEsser(float* data, int frames, int channels,
                                const FXProcessor& proc)
{
    // Simplified de-esser: attenuate high-frequency content when sibilance detected
    // Uses a basic one-pole high-pass to detect sibilance energy
    const float amount = proc.params.value("amount", 0.5f);
    const float detection = 0.15f; // high-pass coefficient for ~4kHz at 48kHz

    for (int f = 0; f < frames; ++f) {
        for (int c = 0; c < channels; ++c) {
            const int idx = f * channels + c;
            const float sample = data[idx];

            // One-pole HP for sibilance detection
            const float hp = sample - (1.0f - detection) * sample;
            const float sibilance = fabsf(hp);

            // Reduce gain in proportion to sibilance and amount
            if (sibilance > 0.05f) {
                const float reduction = 1.0f - amount * std::min(sibilance * 4.0f, 1.0f);
                data[idx] *= reduction;
            }
        }
    }
}

void PodFXModule::applyParametricEQ(float* data, int frames, int channels,
                                     const FXProcessor& proc)
{
    // Simplified parametric EQ v1 — applies gain per-band as a rough approximation.
    // A full biquad filter bank would be used in production.
    // For now, apply the combined gain curve as a simple level adjustment.
    float totalGainDb = 0.0f;
    for (int b = 1; b <= 4; ++b) {
        totalGainDb += proc.params.value(QString("gain%1").arg(b), 0.0f);
    }
    // Average gain across bands
    totalGainDb /= 4.0f;

    if (fabsf(totalGainDb) < 0.1f) return; // Skip if negligible

    const float gainLin = std::pow(10.0f, totalGainDb / 20.0f);
    for (int i = 0; i < frames * channels; ++i)
        data[i] *= gainLin;
}

void PodFXModule::applyHighPass(float* data, int frames, int channels,
                                 const FXProcessor& proc)
{
    // First-order RC high-pass filter: y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    const float freq = proc.params.value("freq", 80.0f);
    const float rc   = 1.0f / (2.0f * 3.14159265f * freq);
    const float dt   = 1.0f / 48000.0f; // TODO: use actual sample rate
    const float alpha = rc / (rc + dt);

    for (int f = 0; f < frames; ++f) {
        for (int c = 0; c < channels && c < 2; ++c) {
            const int idx = f * channels + c;
            const float input = data[idx];
            const float output = alpha * (m_hpPrevOut[c] + input - m_hpPrevIn[c]);
            m_hpPrevIn[c]  = input;
            m_hpPrevOut[c] = output;
            data[idx] = output;
        }
    }
}

void PodFXModule::applyLimiter(float* data, int frames, int channels,
                                const FXProcessor& proc)
{
    const float ceilingDb  = proc.params.value("ceiling", -1.0f);
    const float ceilingLin = std::pow(10.0f, ceilingDb / 20.0f);

    for (int i = 0; i < frames * channels; ++i) {
        if (data[i] > ceilingLin)       data[i] = ceilingLin;
        else if (data[i] < -ceilingLin) data[i] = -ceilingLin;
    }
}

void PodFXModule::measureLUFS(const float* data, int frames, int channels) {
    // Simplified short-term LUFS approximation (EBU R128 simplified)
    // Measures RMS of the block and converts to LUFS
    double sumSq = 0.0;
    for (int i = 0; i < frames * channels; ++i) {
        const double s = static_cast<double>(data[i]);
        sumSq += s * s;
    }
    const int totalSamples = frames * channels;
    if (totalSamples <= 0) return;

    const double meanSq = sumSq / totalSamples;

    // Accumulate for integrated measurement
    m_lufsAccum += meanSq;
    m_lufsBlockCount++;

    // Update LUFS every ~10 blocks (~100ms at 512-sample blocks)
    if (m_lufsBlockCount >= 10) {
        const double avgMeanSq = m_lufsAccum / m_lufsBlockCount;
        const float lufs = (avgMeanSq > 1e-10)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(avgMeanSq))
            : -70.0f;
        m_lufs.store(lufs, std::memory_order_relaxed);
        m_lufsAccum = 0.0;
        m_lufsBlockCount = 0;
    }
}

// ─── Chain manipulation ──────────────────────────────────────────────────────

void PodFXModule::setProcessorEnabled(int index, bool on) {
    if (index >= 0 && index < m_chain.size()) {
        m_chain[index].enabled = on;
    }
}

void PodFXModule::setProcessorParam(int index, const QString& key, float value) {
    if (index >= 0 && index < m_chain.size()) {
        m_chain[index].params[key] = value;
        emit processorParamChanged(index, key, value);
    }
}

void PodFXModule::moveProcessor(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= m_chain.size()) return;
    if (toIndex < 0 || toIndex >= m_chain.size()) return;
    if (fromIndex == toIndex) return;

    auto proc = m_chain.takeAt(fromIndex);
    m_chain.insert(toIndex, proc);
    emit chainChanged();
}

// ─── State persistence ───────────────────────────────────────────────────────

void PodFXModule::saveState(QSettings& s) {
    s.setValue("chainCount", m_chain.size());
    for (int i = 0; i < m_chain.size(); ++i) {
        const QString prefix = QString("proc%1/").arg(i);
        const auto& proc = m_chain[i];
        s.setValue(prefix + "type", static_cast<int>(proc.type));
        s.setValue(prefix + "enabled", proc.enabled);

        // Save parameters
        const auto keys = proc.params.keys();
        s.setValue(prefix + "paramCount", keys.size());
        for (int k = 0; k < keys.size(); ++k) {
            s.setValue(prefix + QString("paramKey%1").arg(k), keys[k]);
            s.setValue(prefix + QString("paramVal%1").arg(k),
                       static_cast<double>(proc.params[keys[k]]));
        }
    }
}

void PodFXModule::loadState(QSettings& s) {
    const int count = s.value("chainCount", 0).toInt();
    if (count <= 0) return;

    m_chain.clear();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("proc%1/").arg(i);
        FXProcessor proc;
        proc.type    = static_cast<FXProcessorType>(s.value(prefix + "type", 0).toInt());
        proc.enabled = s.value(prefix + "enabled", true).toBool();

        const int paramCount = s.value(prefix + "paramCount", 0).toInt();
        for (int k = 0; k < paramCount; ++k) {
            const QString key = s.value(prefix + QString("paramKey%1").arg(k)).toString();
            const float val   = s.value(prefix + QString("paramVal%1").arg(k), 0.0).toFloat();
            if (!key.isEmpty())
                proc.params[key] = val;
        }

        m_chain.append(proc);
    }
}

} // namespace M1
