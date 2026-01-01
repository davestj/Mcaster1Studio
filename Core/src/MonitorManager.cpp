#include "MonitorManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
// MediaFoundation for capture devices
#include <mfapi.h>
#include <mfidl.h>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#endif

namespace M1 {

// ─── Singleton ───────────────────────────────────────────────────────────────
MonitorManager& MonitorManager::instance() {
    static MonitorManager s(nullptr);
    return s;
}

MonitorManager::MonitorManager(QObject* parent)
    : QObject(parent)
{
    connectScreenSignals();
    refresh();
}

// ─── Refresh all ─────────────────────────────────────────────────────────────
void MonitorManager::refresh() {
    enumerateGpus();
    enumerateMonitors();
    enumerateCaptureDevices();
}

// ─── Lookup ──────────────────────────────────────────────────────────────────
const MonitorInfo* MonitorManager::monitorAt(int index) const {
    for (const auto& m : m_monitors)
        if (m.index == index) return &m;
    return nullptr;
}

const MonitorInfo* MonitorManager::primaryMonitor() const {
    for (const auto& m : m_monitors)
        if (m.isPrimary) return &m;
    return m_monitors.isEmpty() ? nullptr : &m_monitors.first();
}

// ─── QScreen-based monitor enumeration ───────────────────────────────────────
void MonitorManager::enumerateMonitors() {
    m_monitors.clear();

    const auto screens = QGuiApplication::screens();
    const QScreen* primary = QGuiApplication::primaryScreen();

    // Build GPU name map from DXGI (best effort — index-matched)
    // GPUs are enumerated separately; we match by index heuristic
    for (int i = 0; i < screens.size(); ++i) {
        const QScreen* scr = screens[i];
        MonitorInfo info;
        info.index             = i;
        info.name              = scr->name();
        info.manufacturer      = scr->manufacturer();
        info.resolution        = scr->size();
        info.physicalSizeMm    = scr->physicalSize().toSize();
        info.refreshRate       = scr->refreshRate();
        info.dpi               = scr->logicalDotsPerInch();
        info.scaleFactor       = scr->devicePixelRatio();
        info.geometry          = scr->geometry();
        info.availableGeometry = scr->availableGeometry();
        info.isPrimary         = (scr == primary);

        // Assign GPU name from DXGI enumeration (heuristic: first GPU for all screens)
        if (!m_gpus.isEmpty()) {
            info.gpuName      = m_gpus.first().name;
            info.gpuVramBytes = m_gpus.first().dedicatedVramBytes;
        }

        m_monitors.append(info);
    }

    qInfo() << "[MonitorManager] Detected" << m_monitors.size() << "monitor(s).";
    emit monitorsChanged();
}

// ─── DXGI GPU enumeration (Windows) ──────────────────────────────────────────
void MonitorManager::enumerateGpus() {
    m_gpus.clear();

#ifdef Q_OS_WIN
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                    reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) {
        qWarning() << "[MonitorManager] CreateDXGIFactory1 failed.";
        return;
    }

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software/basic render drivers
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }

        GpuInfo gpu;
        gpu.name               = QString::fromWCharArray(desc.Description);
        gpu.dedicatedVramBytes = static_cast<qint64>(desc.DedicatedVideoMemory);
        gpu.sharedVramBytes    = static_cast<qint64>(desc.SharedSystemMemory);
        gpu.vendorId           = desc.VendorId;
        m_gpus.append(gpu);

        adapter->Release();
    }
    factory->Release();

    qInfo() << "[MonitorManager] Detected" << m_gpus.size() << "GPU(s).";
    for (const auto& gpu : m_gpus) {
        qInfo() << "  -" << gpu.name
                << "VRAM:" << (gpu.dedicatedVramBytes / (1024*1024)) << "MB";
    }
#endif
}

// ─── Capture device enumeration (Windows MediaFoundation) ────────────────────
void MonitorManager::enumerateCaptureDevices() {
    m_captureDevices.clear();

#ifdef Q_OS_WIN
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        qWarning() << "[MonitorManager] MFStartup failed.";
        return;
    }

    IMFAttributes* attrs = nullptr;
    hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr) || !attrs) {
        MFShutdown();
        return;
    }

    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** devices = nullptr;
    UINT32 deviceCount = 0;
    hr = MFEnumDeviceSources(attrs, &devices, &deviceCount);
    if (SUCCEEDED(hr) && devices) {
        for (UINT32 i = 0; i < deviceCount; ++i) {
            WCHAR* friendlyName = nullptr;
            UINT32 nameLen = 0;
            devices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                &friendlyName, &nameLen);

            WCHAR* symLink = nullptr;
            UINT32 linkLen = 0;
            devices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                &symLink, &linkLen);

            CaptureDeviceInfo dev;
            dev.displayName = friendlyName ? QString::fromWCharArray(friendlyName) : "Unknown";
            dev.deviceId    = symLink ? QString::fromWCharArray(symLink) : "";

            // Detect virtual cameras by name
            const QString lower = dev.displayName.toLower();
            if (lower.contains("virtual") || lower.contains("obs") ||
                lower.contains("manycam") || lower.contains("snap camera") ||
                lower.contains("xsplit")) {
                dev.type = CaptureDeviceInfo::Type::VirtualCamera;
                dev.isVirtualCamera = true;
            } else if (lower.contains("capture") || lower.contains("avermedia") ||
                       lower.contains("elgato") || lower.contains("magewell")) {
                dev.type = CaptureDeviceInfo::Type::CaptureCard;
            }

            m_captureDevices.append(dev);

            if (friendlyName) CoTaskMemFree(friendlyName);
            if (symLink) CoTaskMemFree(symLink);
            devices[i]->Release();
        }
        CoTaskMemFree(devices);
    }
    attrs->Release();
    MFShutdown();

    qInfo() << "[MonitorManager] Detected" << m_captureDevices.size() << "capture device(s).";
    for (const auto& d : m_captureDevices) {
        qInfo() << "  -" << d.displayName
                << (d.isVirtualCamera ? "(virtual)" : "");
    }
#endif
}

// ─── QScreen hot-plug tracking ───────────────────────────────────────────────
void MonitorManager::connectScreenSignals() {
    auto* app = QGuiApplication::instance();
    if (!app) return;

    connect(qApp, &QGuiApplication::screenAdded,
            this, [this](QScreen*) {
                qInfo() << "[MonitorManager] Screen added — refreshing.";
                enumerateMonitors();
            });

    connect(qApp, &QGuiApplication::screenRemoved,
            this, [this](QScreen*) {
                qInfo() << "[MonitorManager] Screen removed — refreshing.";
                enumerateMonitors();
            });
}

} // namespace M1
