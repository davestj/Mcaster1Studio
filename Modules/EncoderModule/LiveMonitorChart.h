#pragma once
#include <QWidget>
#include <QList>
#include <QString>
#include <QColor>
#include <QPointF>

/// LiveMonitorChart — interactive QPainter time-series chart widget.
///
/// Stores a ring buffer of float samples. Draws polyline + gradient fill.
/// Features: hover crosshair with value tooltip, grid lines, auto-scaling Y axis,
/// smooth animations, theme-aware colors.
///
/// Used by LiveMonitorWindow (listeners, bandwidth, bitrate) and
/// HealthWidget (CPU, memory).
class LiveMonitorChart : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float animatedMax READ animatedMax WRITE setAnimatedMax)

public:
    explicit LiveMonitorChart(QWidget* parent = nullptr);

    /// Set chart title, Y-axis unit label, and line color per theme.
    /// @param darkColor   Line color for dark theme
    /// @param classicColor Line color for classic (brown) theme
    /// @param lightColor  Line color for light theme
    void configure(const QString& title,
                   const QString& yAxisLabel,
                   const QColor& darkColor,
                   const QColor& classicColor,
                   const QColor& lightColor,
                   int ringCapacity = 120);

    /// Push a new data point (newest = rightmost).
    void pushSample(float value);

    /// Clear all samples.
    void clearSamples();

    /// Current latest value.
    float lastValue() const { return m_samples.isEmpty() ? 0.0f : m_samples.last(); }

    QSize sizeHint()        const override { return {400, 120}; }
    QSize minimumSizeHint() const override { return {200, 60}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    float animatedMax() const { return m_animatedMax; }
    void  setAnimatedMax(float v) { m_animatedMax = v; update(); }

    QColor lineColor() const;
    QColor bgColor() const;
    QColor borderColor() const;
    QColor gridColor() const;
    QColor textColor() const;

    QString         m_title;
    QString         m_yLabel;
    QColor          m_darkColor    = QColor(14, 165, 233);  // #0ea5e9
    QColor          m_classicColor = QColor(212, 137, 30);  // #d4891e
    QColor          m_lightColor   = QColor(28, 92, 170);   // #1c5caa
    int             m_capacity     = 120;
    QList<float>    m_samples;

    // Hover state
    int             m_hoverX       = -1;

    // Animated max (smooth Y-axis rescale)
    float           m_animatedMax  = 10.0f;
};
