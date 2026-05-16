#include "LiveMonitorChart.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QToolTip>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
LiveMonitorChart::LiveMonitorChart(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setObjectName("LiveMonitorChart");

    // Respond to theme changes
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { update(); });
}

void LiveMonitorChart::configure(const QString& title,
                                  const QString& yAxisLabel,
                                  const QColor& darkColor,
                                  const QColor& classicColor,
                                  const QColor& lightColor,
                                  int ringCapacity)
{
    m_title        = title;
    m_yLabel       = yAxisLabel;
    m_darkColor    = darkColor;
    m_classicColor = classicColor;
    m_lightColor   = lightColor;
    m_capacity     = ringCapacity;
    update();
}

void LiveMonitorChart::pushSample(float value)
{
    m_samples.append(value);
    while (m_samples.size() > m_capacity)
        m_samples.removeFirst();

    // Smooth Y-axis rescale
    float maxVal = 10.0f;
    for (float v : m_samples)
        if (v > maxVal) maxVal = v;
    maxVal = std::ceil(maxVal * 1.1f);  // 10% headroom

    if (std::abs(maxVal - m_animatedMax) > 0.5f) {
        auto* anim = new QPropertyAnimation(this, "animatedMax");
        anim->setDuration(200);
        anim->setStartValue(m_animatedMax);
        anim->setEndValue(maxVal);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    update();
}

void LiveMonitorChart::clearSamples()
{
    m_samples.clear();
    m_animatedMax = 10.0f;
    update();
}

// ─── Theme helpers ───────────────────────────────────────────────────────────
QColor LiveMonitorChart::lineColor() const
{
    switch (ThemeManager::instance()->currentTheme()) {
        case ThemeManager::Theme::Classic: return m_classicColor;
        case ThemeManager::Theme::EnterprisePro:   return m_lightColor;
        default:                           return m_darkColor;
    }
}

QColor LiveMonitorChart::bgColor() const
{
    switch (ThemeManager::instance()->currentTheme()) {
        case ThemeManager::Theme::Classic: return QColor("#2a1e14");
        case ThemeManager::Theme::EnterprisePro:   return QColor("#f5f3f0");
        default:                           return QColor("#0a1628");
    }
}

QColor LiveMonitorChart::borderColor() const
{
    switch (ThemeManager::instance()->currentTheme()) {
        case ThemeManager::Theme::Classic: return QColor("#5a4030");
        case ThemeManager::Theme::EnterprisePro:   return QColor("#d8d4ce");
        default:                           return ThemePalette::forCurrentTheme().border;
    }
}

QColor LiveMonitorChart::gridColor() const
{
    QColor c = borderColor();
    c.setAlpha(64);
    return c;
}

QColor LiveMonitorChart::textColor() const
{
    switch (ThemeManager::instance()->currentTheme()) {
        case ThemeManager::Theme::Classic: return QColor("#a08870");
        case ThemeManager::Theme::EnterprisePro:   return QColor("#6b6560");
        default:                           return QColor("#94a3b8");
    }
}

// ─── Mouse tracking ─────────────────────────────────────────────────────────
void LiveMonitorChart::mouseMoveEvent(QMouseEvent* event)
{
    m_hoverX = event->pos().x();
    update();

    // Show tooltip with interpolated value
    if (!m_samples.isEmpty()) {
        const int leftMargin  = 40;
        const int rightMargin = 8;
        const int chartW = width() - leftMargin - rightMargin;
        const int n = m_samples.size();

        if (chartW > 0 && m_hoverX >= leftMargin && m_hoverX <= width() - rightMargin) {
            const float frac = float(m_hoverX - leftMargin) / chartW;
            const int idx = qBound(0, int(frac * (n - 1)), n - 1);
            const float val = m_samples[idx];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1: %2 %3").arg(m_title).arg(val, 0, 'f', 1).arg(m_yLabel));
        }
    }

    QWidget::mouseMoveEvent(event);
}

void LiveMonitorChart::leaveEvent(QEvent* event)
{
    m_hoverX = -1;
    update();
    QWidget::leaveEvent(event);
}

// ─── Paint ──────────────────────────────────────────────────────────────────
void LiveMonitorChart::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const int leftMargin  = 40;
    const int rightMargin = 8;
    const int topMargin   = 20;
    const int bottomMargin = 16;

    // Background
    p.fillRect(r, bgColor());

    // Border
    p.setPen(borderColor());
    p.drawRect(r.adjusted(0, 0, -1, -1));

    const int chartX = leftMargin;
    const int chartY = topMargin;
    const int chartW = r.width()  - leftMargin - rightMargin;
    const int chartH = r.height() - topMargin  - bottomMargin;

    if (chartW < 10 || chartH < 10) return;

    // Grid lines at 25%, 50%, 75%
    p.setPen(QPen(gridColor(), 1.0, Qt::DotLine));
    for (int pct : {25, 50, 75}) {
        const int y = chartY + chartH - (chartH * pct / 100);
        p.drawLine(chartX, y, chartX + chartW, y);
    }

    // Title (top-left)
    QFont titleFont = font();
    titleFont.setPixelSize(11);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(textColor());
    p.drawText(chartX + 4, chartY - 4, m_title);

    // Current value (top-right)
    if (!m_samples.isEmpty()) {
        const QString valText = QString::number(m_samples.last(), 'f', 1) + " " + m_yLabel;
        QFont valFont = font();
        valFont.setPixelSize(11);
        p.setFont(valFont);
        const QColor lc = lineColor();
        p.setPen(lc);
        const int tw = p.fontMetrics().horizontalAdvance(valText);
        p.drawText(chartX + chartW - tw - 4, chartY - 4, valText);
    }

    // Y-axis labels
    QFont axisFont = font();
    axisFont.setPixelSize(10);
    p.setFont(axisFont);
    p.setPen(textColor());
    const float maxY = std::max(m_animatedMax, 1.0f);
    p.drawText(2, chartY + 10, QString::number(maxY, 'f', 0));
    p.drawText(2, chartY + chartH, "0");

    // Polyline
    const int n = m_samples.size();
    if (n < 2) return;

    QVector<QPointF> points;
    points.reserve(n);
    for (int i = 0; i < n; ++i) {
        const float x = chartX + float(i) / (m_capacity - 1) * chartW;
        const float y = chartY + chartH - (m_samples[i] / maxY) * chartH;
        points.append(QPointF(x, qBound(float(chartY), y, float(chartY + chartH))));
    }

    // Gradient fill below polyline
    QPainterPath fillPath;
    fillPath.moveTo(points.first().x(), chartY + chartH);
    for (const auto& pt : points)
        fillPath.lineTo(pt);
    fillPath.lineTo(points.last().x(), chartY + chartH);
    fillPath.closeSubpath();

    QColor fillTop = lineColor();
    fillTop.setAlpha(120);
    QColor fillBot = lineColor();
    fillBot.setAlpha(20);
    QLinearGradient grad(0, chartY, 0, chartY + chartH);
    grad.setColorAt(0.0, fillTop);
    grad.setColorAt(1.0, fillBot);
    p.fillPath(fillPath, grad);

    // Polyline stroke
    p.setPen(QPen(lineColor(), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(points.data(), points.size());

    // Hover crosshair
    if (m_hoverX >= chartX && m_hoverX <= chartX + chartW) {
        QColor crossColor = lineColor();
        crossColor.setAlpha(150);
        p.setPen(QPen(crossColor, 1.0, Qt::DashLine));
        p.drawLine(m_hoverX, chartY, m_hoverX, chartY + chartH);

        // Dot on the line at hover position
        const float frac = float(m_hoverX - chartX) / chartW;
        const int idx = qBound(0, int(frac * (n - 1)), n - 1);
        if (idx < points.size()) {
            p.setPen(Qt::NoPen);
            p.setBrush(lineColor());
            p.drawEllipse(points[idx], 4.0, 4.0);
        }
    }
}
