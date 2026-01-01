#pragma once
#include <QWidget>
#include <QTimer>
#include <atomic>

namespace M1 {

/// VU/PPM stereo meter widget rendered with QPainter.
/// Displays two vertical bars (L/R) with dBFS scale markings.
/// Peak hold with 2-second decay (broadcast standard).
///
/// Thread-safe: levels are set via setLevels() from any thread using atomics.
/// The widget redraws on its own 20fps timer.
class VUMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit VUMeterWidget(QWidget* parent = nullptr);
    ~VUMeterWidget() override;

    /// Set current RMS levels (0.0 – 1.0 linear).
    /// Safe to call from any thread (uses atomics).
    void setLevels(float leftLinear, float rightLinear);

    /// Set display mode
    enum class Mode { VU, PPM };
    void setMode(Mode m) { m_mode = m; }

    /// Enable compact horizontal-bar mode (for ribbon boxes).
    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }

    /// Set the audio device name to display (surface mode: below bars; compact: truncated right).
    void setDeviceName(const QString& name);

    QSize sizeHint() const override { return m_compact ? QSize{120, 44} : QSize{220, 300}; }
    QSize minimumSizeHint() const override { return m_compact ? QSize{80, 36} : QSize{160, 200}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTimer();

private:
    float  linearToDb(float linear) const;
    QColor levelColor(float db) const;
    void   drawBar(QPainter& p, const QRectF& rect, float db, float peakDb, const QString& label);
    void   paintCompact(QPainter& p);
    void   paintFull(QPainter& p);

    std::atomic<float> m_leftLinear  {0.0f};
    std::atomic<float> m_rightLinear {0.0f};

    // Smoothed display values (Qt thread only)
    float m_dispL     = 0.0f;
    float m_dispR     = 0.0f;
    float m_peakL     = -60.0f;  ///< Peak hold in dBFS
    float m_peakR     = -60.0f;
    int   m_peakHoldL = 0;       ///< Frames since peak set (hold ~60 frames @ 20fps = 3s)
    int   m_peakHoldR = 0;

    Mode    m_mode    = Mode::VU;
    bool    m_compact = false;
    QString m_deviceName;
    QTimer  m_timer;

    // Colors (broadcast dark theme)
    static constexpr float DB_CLIP  = 0.0f;
    static constexpr float DB_RED   = -3.0f;
    static constexpr float DB_AMBER = -12.0f;
    static constexpr float DB_MIN   = -60.0f;
};

} // namespace M1
