#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QTimeZone>

class QTimer;
class QLabel;
class QPushButton;

namespace M1 {

/// ClockModule — real-time clock with configurable timezone.
///
/// Displays current time in HH:MM:SS format plus the timezone abbreviation.
/// Right-clicking the clock widget opens a timezone picker dialog.
///
/// Designed to run in the surface ribbon, surface tray, or as a full dock.
/// Each surface can configure its own timezone for relay/simulcast in other
/// regions (e.g. US-East surface → UTC-5, UK relay surface → Europe/London).
class ClockModule : public IModule {
    Q_OBJECT

public:
    explicit ClockModule(QObject* parent = nullptr);
    ~ClockModule() override;

    QString moduleId()    const override { return "com.mcaster1.clock"; }
    QString displayName() const override { return "Clock"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {180, 60}; }
    QSize minimumModuleSize() const override { return {140, 48}; }

    void initialize() override;
    void shutdown()   override;

    QWidget* createWidget(QWidget* parent) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ClockModule produces no audio
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}

    /// Create a compact widget suitable for embedding in ribbon or tray (no title).
    QWidget* createCompactWidget(QWidget* parent);

    QTimeZone timezone() const { return m_timezone; }
    void setTimezone(const QTimeZone& tz);

signals:
    void timezoneChanged(const QTimeZone& tz);

private slots:
    void onTick();

private:
    QTimer*   m_timer    = nullptr;
    QTimeZone m_timezone { QTimeZone::systemTimeZone() };

    // (widgets connect to statusChanged signal directly — no external label lists needed)
};

} // namespace M1
