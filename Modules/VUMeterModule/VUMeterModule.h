#pragma once
#include "IModule.h"
#include "IAudioEngine.h"
#include <QObject>

namespace M1 {

/// VUMeterModule — Phase 1 first working module.
/// Displays live stereo VU/PPM meters fed from the AudioEngine input levels.
/// Registers with the EventBus to receive audioLevelsChanged signals.
class VUMeterModule : public IModule {
    Q_OBJECT

public:
    explicit VUMeterModule(QObject* parent = nullptr);
    ~VUMeterModule() override;

    // IModule
    QString  moduleId()    const override { return "com.mcaster1.vumeter"; }
    QString  displayName() const override { return "VU Meter"; }
    QString  version()     const override { return "1.0.0"; }
    QSize    preferredSize() const override { return {220, 300}; }
    QSize    minimumModuleSize() const override { return {160, 200}; }

    void     initialize() override;
    void     shutdown()   override;

    QWidget* createWidget(QWidget* parent) override;

    /// Create a compact horizontal ribbon version of the VU meter.
    QWidget* createCompactWidget(QWidget* parent);

    void     saveState(QSettings& s) override;
    void     loadState(QSettings& s) override;

    /// Set the audio engine source for levels (called by MainWindow on engine start)
    void setAudioEngine(IAudioEngine* engine);

private slots:
    void onLevelsChanged(float inL, float inR, float outL, float outR);

private:
    class VUMeterWidget* m_widget = nullptr;
    IAudioEngine*        m_engine = nullptr;
};

} // namespace M1
