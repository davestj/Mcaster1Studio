#pragma once
#include <QObject>
#include <QList>
#include <QSize>
#include <QRect>
#include <QString>

namespace M1 {

// ─── Monitor information ─────────────────────────────────────────────────────
struct MonitorInfo {
    int       index            = 0;     ///< QScreen index (0-based)
    QString   name;                     ///< "DELL U2722D", "\\.\DISPLAY1"
    QString   manufacturer;             ///< "Dell Inc."
    QSize     resolution;               ///< Native resolution (e.g. 2560x1440)
    QSize     physicalSizeMm;           ///< Physical size in millimeters
    double    refreshRate      = 60.0;  ///< Hz
    double    dpi              = 96.0;  ///< Effective DPI
    double    scaleFactor      = 1.0;   ///< Qt device pixel ratio
    QRect     geometry;                 ///< Virtual desktop position + size
    QRect     availableGeometry;        ///< Minus taskbar
    bool      isPrimary        = false;
    QString   gpuName;                  ///< GPU driving this display
    qint64    gpuVramBytes     = 0;     ///< GPU VRAM in bytes
};

// ─── Capture device information ──────────────────────────────────────────────
struct CaptureDeviceInfo {
    QString   deviceId;
    QString   displayName;              ///< "Logitech C920", "AVerMedia Live Gamer"
    QSize     maxResolution;
    double    maxFps           = 30.0;
    bool      isVirtualCamera  = false;
    enum class Type { Webcam, CaptureCard, ScreenCapture, VirtualCamera };
    Type      type             = Type::Webcam;
};

// ─── GPU information ─────────────────────────────────────────────────────────
struct GpuInfo {
    QString   name;                     ///< "NVIDIA GeForce RTX 4070"
    qint64    dedicatedVramBytes = 0;   ///< Dedicated VRAM
    qint64    sharedVramBytes    = 0;   ///< Shared system memory
    quint32   vendorId           = 0;   ///< 0x10DE=NVIDIA, 0x1002=AMD, 0x8086=Intel
};

// ─── MonitorManager (singleton) ──────────────────────────────────────────────
/// Enumerates monitors, GPUs, and video capture devices.
/// Tracks QScreen changes for hot-plug detection.
class MonitorManager : public QObject {
    Q_OBJECT
public:
    static MonitorManager& instance();

    QList<MonitorInfo>       monitors() const { return m_monitors; }
    QList<CaptureDeviceInfo> captureDevices() const { return m_captureDevices; }
    QList<GpuInfo>           gpus() const { return m_gpus; }

    const MonitorInfo* monitorAt(int index) const;
    const MonitorInfo* primaryMonitor() const;
    int monitorCount() const { return m_monitors.size(); }

    /// Refresh all enumerations
    void refresh();
    void refreshCaptureDevices();

signals:
    void monitorsChanged();
    void captureDevicesChanged();

private:
    explicit MonitorManager(QObject* parent = nullptr);
    void enumerateMonitors();
    void enumerateGpus();
    void enumerateCaptureDevices();
    void connectScreenSignals();

    QList<MonitorInfo>       m_monitors;
    QList<CaptureDeviceInfo> m_captureDevices;
    QList<GpuInfo>           m_gpus;
};

} // namespace M1
