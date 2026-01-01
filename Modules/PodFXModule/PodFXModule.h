#pragma once
/// @file   PodFXModule.h
/// @path   Modules/PodFXModule/PodFXModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodFX — Podcast Voice Effects Rack
/// @purpose Chain of voice processors for podcast production: noise gate,
///          compressor, de-esser, parametric EQ, high-pass filter, limiter,
///          and LUFS metering. Each processor has bypass toggle and the chain
///          order is user-reorderable.
/// @reason  Podcasters need consistent, broadcast-quality voice processing
///          without configuring external DAW plugins.
/// @changelog
///   2026-03-09  Initial implementation — 7 processors, bypass, reorder, LUFS

#include "IModule.h"
#include <QList>
#include <QMap>
#include <atomic>

namespace M1 {

// ─── FXProcessorType ─────────────────────────────────────────────────────────
enum class FXProcessorType {
    NoiseGate,
    Compressor,
    DeEsser,
    ParametricEQ,
    HighPassFilter,
    Limiter,
    LUFSMeter
};

// ─── FXProcessor — one processor in the chain ───────────────────────────────
struct FXProcessor {
    FXProcessorType          type     = FXProcessorType::NoiseGate;
    bool                     enabled  = true;
    QMap<QString, float>     params;

    /// Display name for the processor type
    static QString typeName(FXProcessorType t);

    /// Create a processor with sensible defaults for the given type
    static FXProcessor createDefault(FXProcessorType t);
};

// ─── PodFXModule ─────────────────────────────────────────────────────────────
class PodFXModule : public IModule {
    Q_OBJECT

public:
    explicit PodFXModule(QObject* parent = nullptr);
    ~PodFXModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.fx"; }
    QString displayName() const override { return "Podcast Effects"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {400, 450}; }
    QSize minimumModuleSize() const override { return {300, 350}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Chain access
    int processorCount() const { return m_chain.size(); }
    const FXProcessor& processor(int index) const { return m_chain[index]; }
    FXProcessor& processor(int index) { return m_chain[index]; }
    const QList<FXProcessor>& chain() const { return m_chain; }

    // Chain manipulation
    void setProcessorEnabled(int index, bool on);
    void setProcessorParam(int index, const QString& key, float value);
    void moveProcessor(int fromIndex, int toIndex);

    // LUFS reading (RT → UI via atomic)
    float currentLUFS() const { return m_lufs.load(std::memory_order_relaxed); }

signals:
    void chainChanged();
    void processorParamChanged(int index, const QString& key, float value);

private:
    QList<FXProcessor> m_chain;

    // LUFS measurement (integrated loudness approximation)
    std::atomic<float> m_lufs{-70.0f};

    // RT processing helpers (no alloc, no Qt)
    void applyNoiseGate(float* data, int frames, int channels, const FXProcessor& proc);
    void applyCompressor(float* data, int frames, int channels, const FXProcessor& proc);
    void applyDeEsser(float* data, int frames, int channels, const FXProcessor& proc);
    void applyParametricEQ(float* data, int frames, int channels, const FXProcessor& proc);
    void applyHighPass(float* data, int frames, int channels, const FXProcessor& proc);
    void applyLimiter(float* data, int frames, int channels, const FXProcessor& proc);
    void measureLUFS(const float* data, int frames, int channels);

    // RT state for processors
    float m_gateEnvelope     = 0.0f;
    float m_compEnvelope     = 0.0f;
    float m_hpPrevIn[2]      = {0.0f, 0.0f};  // per-channel HP previous input
    float m_hpPrevOut[2]     = {0.0f, 0.0f};  // per-channel HP previous output
    double m_lufsAccum       = 0.0;
    int    m_lufsBlockCount  = 0;
};

} // namespace M1
