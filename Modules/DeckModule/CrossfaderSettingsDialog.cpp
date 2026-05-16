#include "CrossfaderSettingsDialog.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QSettings>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
#include <algorithm>

#ifndef M_PI_2
#  define M_PI_2 1.57079632679489661923
#endif

// ─── Theme-aware color helpers ────────────────────────────────────────────────
namespace {
    struct DlgColors {
        QString bg, panel, border, textPrimary, textMuted, accent;
        QString chartBg, chartGrid;
    };

    DlgColors dlgColors() {
        using T = ThemeManager::Theme;
        switch (ThemeManager::instance()->currentTheme()) {
        case T::Classic:
            return { "#e8e0d0","#f0e8d8","#bfb090","#2a1810","#806040","#8b3a0a",
                     "#e0d8c8","#ccbfa8" };
        default: { // EnterprisePro (via ThemePalette)
            auto p = ThemePalette::forCurrentTheme();
            return { p.bg.name(), p.panelBg.name(), p.border.name(),
                     p.text.name(), p.textMuted.name(), p.accent.name(),
                     p.cardBg.name(), p.border.lighter(110).name() };
        }
        }
    }

    QString groupBoxQss() {
        const auto c = dlgColors();
        return QString(
            "QGroupBox {"
            "  background:%1; border:1px solid %2; border-radius:4px;"
            "  color:%3; font-size:12px; font-weight:700;"
            "  margin-top:12px; padding:6px 4px 4px 4px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin:margin; left:8px;"
            "  background:transparent; color:%4;"
            "}"
        ).arg(c.panel, c.border, c.textPrimary, c.accent);
    }

    QString labelQss(bool muted = false) {
        const auto c = dlgColors();
        return QString("QLabel { color:%1; font-size:12px; background:transparent; }")
               .arg(muted ? c.textMuted : c.textPrimary);
    }

    QString sliderQss() {
        const auto c = dlgColors();
        return QString(
            "QSlider::groove:horizontal {"
            "  background:%1; height:5px; border-radius:2px;"
            "}"
            "QSlider::handle:horizontal {"
            "  background:%3; border:1px solid %4;"
            "  width:12px; height:16px; margin:-6px 0; border-radius:2px;"
            "}"
            "QSlider::sub-page:horizontal { background:%2; border-radius:2px; }"
        ).arg(c.border, c.accent, c.panel,
              QColor(c.border).darker(120).name());
    }

    QString comboQss() {
        const auto c = dlgColors();
        return QString(
            "QComboBox {"
            "  background:%1; color:%2; border:1px solid %3; border-radius:3px;"
            "  font-size:12px; padding:2px 6px; min-height:20px;"
            "}"
            "QComboBox:hover { border-color:%4; }"
            "QComboBox::drop-down { border:none; width:18px; }"
            "QComboBox QAbstractItemView {"
            "  background:%1; color:%2; border:1px solid %3; selection-background-color:%4;"
            "  outline:none;"
            "}"
        ).arg(c.panel, c.textPrimary, c.border, c.accent);
    }

    QString spinQss() {
        const auto c = dlgColors();
        return QString(
            "QSpinBox, QDoubleSpinBox {"
            "  background:%1; color:%2; border:1px solid %3; border-radius:3px;"
            "  font-size:12px; padding:1px 4px;"
            "}"
            "QSpinBox::up-button, QDoubleSpinBox::up-button {"
            "  background:%3; border:none; width:14px;"
            "}"
            "QSpinBox::down-button, QDoubleSpinBox::down-button {"
            "  background:%3; border:none; width:14px;"
            "}"
            "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { color:%2; }"
            "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { color:%2; }"
        ).arg(c.panel, c.textPrimary, c.border);
    }

    QString checkQss() {
        const auto c = dlgColors();
        return QString(
            "QCheckBox { color:%1; font-size:12px; font-weight:700; background:transparent; }"
            "QCheckBox::indicator { width:13px; height:13px; border:1px solid %2; border-radius:2px; background:%3; }"
            "QCheckBox::indicator:checked { background:%4; border-color:%4; }"
        ).arg(c.textPrimary, c.border, c.panel, c.accent);
    }

    QString btnQss(const QString& bg = QString()) {
        const auto c = dlgColors();
        const QString bgColor = bg.isEmpty() ? c.panel : bg;
        return QString(
            "QPushButton {"
            "  background:%1; color:%2; border:1px solid %3; border-radius:3px;"
            "  font-size:12px; font-weight:700; padding:4px 12px; min-height:24px;"
            "}"
            "QPushButton:hover  { background:%4; border-color:%5; }"
            "QPushButton:pressed{ background:%6; }"
        ).arg(bgColor, c.textPrimary, c.border,
              QColor(bgColor).lighter(130).name(), c.accent,
              QColor(bgColor).darker(120).name());
    }
} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// CrossfadeSettings — save / load
// ═════════════════════════════════════════════════════════════════════════════
void CrossfadeSettings::saveToSettings(QSettings& s, const QString& prefix) const {
    s.beginGroup(prefix);
    s.setValue("fadeOutEnabled",  fadeOutEnabled);
    s.setValue("fadeOutCurve",    (int)fadeOutCurve);
    s.setValue("fadeOutTime",     fadeOutTime);
    s.setValue("fadeOutLevel",    fadeOutLevel);
    s.setValue("fadeInEnabled",   fadeInEnabled);
    s.setValue("fadeInCurve",     (int)fadeInCurve);
    s.setValue("fadeInTime",      fadeInTime);
    s.setValue("fadeInLevel",     fadeInLevel);
    s.setValue("triggerMode",     (int)triggerMode);
    s.setValue("fixedFadeTime",   fixedFadeTime);
    s.setValue("triggerDbLevel",  triggerDbLevel);
    s.setValue("minFadeTimeMs",   minFadeTimeMs);
    s.setValue("maxFadeTimeMs",   maxFadeTimeMs);
    s.setValue("skipShortTracks", skipShortTracks);
    s.setValue("skipDurationSecs",skipDurationSecs);
    // Custom curve points
    QStringList outPts, inPts;
    for (const auto& p : fadeOutPoints) outPts << QString("%1,%2").arg(p.x()).arg(p.y());
    for (const auto& p : fadeInPoints)  inPts  << QString("%1,%2").arg(p.x()).arg(p.y());
    s.setValue("fadeOutPoints", outPts);
    s.setValue("fadeInPoints",  inPts);
    s.endGroup();
}

void CrossfadeSettings::loadFromSettings(QSettings& s, const QString& prefix) {
    s.beginGroup(prefix);
    fadeOutEnabled  = s.value("fadeOutEnabled",  true).toBool();
    fadeOutCurve    = (CurveType)s.value("fadeOutCurve",    (int)SCurve).toInt();
    fadeOutTime     = s.value("fadeOutTime",     5.0f).toFloat();
    fadeOutLevel    = s.value("fadeOutLevel",    0.90f).toFloat();
    fadeInEnabled   = s.value("fadeInEnabled",   true).toBool();
    fadeInCurve     = (CurveType)s.value("fadeInCurve",     (int)SCurve).toInt();
    fadeInTime      = s.value("fadeInTime",      3.2f).toFloat();
    fadeInLevel     = s.value("fadeInLevel",     0.80f).toFloat();
    triggerMode     = (TriggerMode)s.value("triggerMode", (int)AutoDetect).toInt();
    fixedFadeTime   = s.value("fixedFadeTime",   3.0f).toFloat();
    triggerDbLevel  = s.value("triggerDbLevel",  -7.0f).toFloat();
    minFadeTimeMs   = s.value("minFadeTimeMs",   100).toInt();
    maxFadeTimeMs   = s.value("maxFadeTimeMs",   6000).toInt();
    skipShortTracks = s.value("skipShortTracks", true).toBool();
    skipDurationSecs= s.value("skipDurationSecs",65).toInt();
    // Custom points
    fadeOutPoints.clear();
    fadeInPoints.clear();
    for (const QString& str : s.value("fadeOutPoints").toStringList()) {
        const QStringList p = str.split(',');
        if (p.size() == 2) fadeOutPoints.append({p[0].toDouble(), p[1].toDouble()});
    }
    for (const QString& str : s.value("fadeInPoints").toStringList()) {
        const QStringList p = str.split(',');
        if (p.size() == 2) fadeInPoints.append({p[0].toDouble(), p[1].toDouble()});
    }
    s.endGroup();
}

// ═════════════════════════════════════════════════════════════════════════════
// CrossfadeCurveView
// ═════════════════════════════════════════════════════════════════════════════
CrossfadeCurveView::CrossfadeCurveView(QWidget* parent)
    : QChartView(parent)
{
    setMinimumSize(300, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
    setDragMode(QGraphicsView::NoDrag);
    buildChart();
    refreshTheme();
}

void CrossfadeCurveView::buildChart() {
    const auto col = dlgColors();
    auto* c = new QChart;
    c->setBackgroundBrush(QBrush(QColor(col.chartBg)));
    c->setPlotAreaBackgroundBrush(QBrush(QColor(col.chartBg)));
    c->setPlotAreaBackgroundVisible(true);
    c->setBackgroundRoundness(0);
    c->legend()->hide();
    c->setMargins({4, 4, 4, 4});

    // Axes
    m_axisX = new QValueAxis;
    m_axisX->setRange(0, 10);
    m_axisX->setTickCount(6);
    m_axisX->setLabelFormat("%.0fs");
    m_axisX->setTitleText("");
    m_axisX->setLabelsColor(QColor(col.textMuted));
    m_axisX->setGridLineColor(QColor(col.chartGrid));
    m_axisX->setLinePen(QPen(QColor(col.chartGrid)));
    m_axisX->setTitleBrush(QBrush(QColor(col.textMuted)));

    m_axisY = new QValueAxis;
    m_axisY->setRange(0, 1.05);
    m_axisY->setTickCount(6);
    m_axisY->setLabelFormat("%.0f%%");
    m_axisY->setLabelsColor(QColor(col.textMuted));
    m_axisY->setGridLineColor(QColor(col.chartGrid));
    m_axisY->setLinePen(QPen(QColor(col.chartGrid)));

    // ── Green fade-out area ───────────────────────────────────────────────
    m_outLine     = new QLineSeries;
    m_outBaseLine = new QLineSeries;
    m_outArea     = new QAreaSeries(m_outLine, m_outBaseLine);
    m_outArea->setName("Fade Out");
    m_outArea->setBrush(QBrush(QColor(30, 160, 70, 120)));
    m_outArea->setPen(QPen(QColor(40, 220, 90), 2.0));

    // ── Blue fade-in area ─────────────────────────────────────────────────
    m_inLine     = new QLineSeries;
    m_inBaseLine = new QLineSeries;
    m_inArea     = new QAreaSeries(m_inLine, m_inBaseLine);
    m_inArea->setName("Fade In");
    m_inArea->setBrush(QBrush(QColor(30, 80, 200, 120)));
    m_inArea->setPen(QPen(QColor(50, 130, 255), 2.0));

    // ── Red threshold dashed line ─────────────────────────────────────────
    m_threshold = new QLineSeries;
    QPen threshPen(QColor(200, 50, 50));
    threshPen.setStyle(Qt::DashLine);
    threshPen.setWidth(1);
    m_threshold->setPen(threshPen);

    // ── Custom control points (scatter) ───────────────────────────────────
    m_outCtrlPts = new QScatterSeries;
    m_outCtrlPts->setColor(QColor(50, 200, 100));
    m_outCtrlPts->setBorderColor(Qt::white);
    m_outCtrlPts->setMarkerSize(10);
    m_outCtrlPts->setMarkerShape(QScatterSeries::MarkerShapeCircle);

    m_inCtrlPts = new QScatterSeries;
    m_inCtrlPts->setColor(QColor(80, 140, 255));
    m_inCtrlPts->setBorderColor(Qt::white);
    m_inCtrlPts->setMarkerSize(10);
    m_inCtrlPts->setMarkerShape(QScatterSeries::MarkerShapeCircle);

    c->addSeries(m_outArea);
    c->addSeries(m_inArea);
    c->addSeries(m_threshold);
    c->addSeries(m_outCtrlPts);
    c->addSeries(m_inCtrlPts);

    c->addAxis(m_axisX, Qt::AlignBottom);
    c->addAxis(m_axisY, Qt::AlignLeft);

    const QList<QAbstractSeries*> allSeries = {
        m_outArea, m_inArea, m_threshold, m_outCtrlPts, m_inCtrlPts
    };
    for (QAbstractSeries* s : allSeries) {
        s->attachAxis(m_axisX);
        s->attachAxis(m_axisY);
    }

    setChart(c);
}

float CrossfadeCurveView::evalCurve(CrossfadeSettings::CurveType type,
                                     float t, float level, bool fadeOut) const {
    switch (type) {
    case CrossfadeSettings::Linear:
        return fadeOut ? (1.0f - t) * level : t * level;
    case CrossfadeSettings::SCurve:
        return fadeOut
            ? std::cos(t * (float)M_PI_2) * level
            : std::cos((1.0f - t) * (float)M_PI_2) * level;
    case CrossfadeSettings::Exponential:
        return fadeOut
            ? (1.0f - t) * (1.0f - t) * level
            : t * t * level;
    case CrossfadeSettings::Custom: {
        const QList<QPointF>& pts = fadeOut ? m_s.fadeOutPoints : m_s.fadeInPoints;
        if (pts.size() < 2)
            return fadeOut
                ? std::cos(t * (float)M_PI_2) * level
                : std::cos((1.0f - t) * (float)M_PI_2) * level;
        float result = 0.0f;
        for (int i = 0; i + 1 < pts.size(); ++i) {
            if (t >= (float)pts[i].x() && t <= (float)pts[i+1].x()) {
                float span   = (float)(pts[i+1].x() - pts[i].x());
                float localT = span > 0.0f ? (t - (float)pts[i].x()) / span : 0.0f;
                result = (float)(pts[i].y() + localT * (pts[i+1].y() - pts[i].y()));
                break;
            }
        }
        return result * level;
    }
    }
    return 0.0f;
}

QVector<QPointF> CrossfadeCurveView::calcCurve(bool fadeOut) const {
    const float dur    = fadeOut ? m_s.fadeOutTime  : m_s.fadeInTime;
    const float level  = fadeOut ? m_s.fadeOutLevel : m_s.fadeInLevel;
    const auto  type   = fadeOut ? m_s.fadeOutCurve : m_s.fadeInCurve;
    const float startX = fadeOut ? 0.0f : m_s.fadeOutTime * 0.5f;

    QVector<QPointF> pts;
    pts.reserve(101);
    for (int i = 0; i <= 100; ++i) {
        float t   = (float)i / 100.0f;
        float x   = startX + t * dur;
        float vol = evalCurve(type, t, level, fadeOut);
        pts.append({(double)x, (double)vol});
    }
    return pts;
}

void CrossfadeCurveView::rebuildSeries() {
    const float maxTime = m_s.fadeOutTime + m_s.fadeInTime * 0.8f;
    m_axisX->setRange(0, std::max(10.0f, maxTime));

    m_outLine->clear();
    m_outBaseLine->clear();
    if (m_s.fadeOutEnabled) {
        const auto pts = calcCurve(true);
        for (const auto& p : pts)     m_outLine->append(p);
        m_outBaseLine->append(pts.first().x(), 0);
        m_outBaseLine->append(pts.last().x(),  0);
    }

    m_inLine->clear();
    m_inBaseLine->clear();
    if (m_s.fadeInEnabled) {
        const auto pts = calcCurve(false);
        for (const auto& p : pts)    m_inLine->append(p);
        m_inBaseLine->append(pts.first().x(), 0);
        m_inBaseLine->append(pts.last().x(),  0);
    }

    m_threshold->clear();
    if (m_s.triggerMode == CrossfadeSettings::AutoDetect) {
        float linLevel = std::pow(10.0f, m_s.triggerDbLevel / 20.0f);
        m_threshold->append(0,       linLevel);
        m_threshold->append(maxTime, linLevel);
    }

    m_outCtrlPts->clear();
    m_inCtrlPts->clear();
    const bool showCtrl = (m_s.fadeOutCurve == CrossfadeSettings::Custom ||
                           m_s.fadeInCurve  == CrossfadeSettings::Custom);
    if (showCtrl) {
        for (const auto& p : m_s.fadeOutPoints)
            m_outCtrlPts->append(p.x() * m_s.fadeOutTime, p.y() * m_s.fadeOutLevel);
        const float startX = m_s.fadeOutTime * 0.5f;
        for (const auto& p : m_s.fadeInPoints)
            m_inCtrlPts->append(startX + p.x() * m_s.fadeInTime, p.y() * m_s.fadeInLevel);
    }
}

void CrossfadeCurveView::updateCurves(const CrossfadeSettings& s) {
    m_s = s;
    rebuildSeries();
}

// ── Mouse interaction ─────────────────────────────────────────────────────────
void CrossfadeCurveView::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QChartView::mousePressEvent(e); return; }
    const QPointF val = chart()->mapToValue(e->pos(), m_threshold);

    if (m_s.triggerMode == CrossfadeSettings::AutoDetect && !m_threshold->points().isEmpty()) {
        float linLevel = std::pow(10.0f, m_s.triggerDbLevel / 20.0f);
        const QPointF screenPt = chart()->mapToPosition({0, (double)linLevel}, m_threshold);
        if (std::abs(e->pos().y() - screenPt.y()) < 10) {
            m_draggingThreshold = true;
            return;
        }
    }

    auto checkPts = [&](QScatterSeries* series, bool isFadeOut) {
        for (int i = 0; i < series->count(); ++i) {
            QPointF sp = chart()->mapToPosition(series->at(i), series);
            if ((e->pos() - sp.toPoint()).manhattanLength() < 14) {
                m_draggingCtrl  = i;
                m_ctrlIsFadeOut = isFadeOut;
                return true;
            }
        }
        return false;
    };

    if (m_s.fadeOutCurve == CrossfadeSettings::Custom && checkPts(m_outCtrlPts, true))  return;
    if (m_s.fadeInCurve  == CrossfadeSettings::Custom && checkPts(m_inCtrlPts,  false)) return;

    if (e->modifiers() & Qt::ControlModifier) {
        const bool inFadeOutZone = val.x() <= m_s.fadeOutTime;
        if (inFadeOutZone && m_s.fadeOutCurve == CrossfadeSettings::Custom) {
            float nx = std::clamp((float)val.x() / m_s.fadeOutTime, 0.0f, 1.0f);
            float ny = std::clamp((float)val.y() / m_s.fadeOutLevel, 0.0f, 1.0f);
            m_s.fadeOutPoints.append({nx, ny});
            std::sort(m_s.fadeOutPoints.begin(), m_s.fadeOutPoints.end(),
                      [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
        } else if (!inFadeOutZone && m_s.fadeInCurve == CrossfadeSettings::Custom) {
            float startX = m_s.fadeOutTime * 0.5f;
            float nx = std::clamp((float)(val.x() - startX) / m_s.fadeInTime, 0.0f, 1.0f);
            float ny = std::clamp((float)val.y() / m_s.fadeInLevel, 0.0f, 1.0f);
            m_s.fadeInPoints.append({nx, ny});
            std::sort(m_s.fadeInPoints.begin(), m_s.fadeInPoints.end(),
                      [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
        }
        rebuildSeries();
    }

    m_lastMouse = e->pos();
    QChartView::mousePressEvent(e);
}

void CrossfadeCurveView::mouseMoveEvent(QMouseEvent* e) {
    const QPointF val = chart()->mapToValue(e->pos(), m_threshold);

    if (m_draggingThreshold) {
        const float db = 20.0f * std::log10(std::max(0.001f, (float)val.y()));
        m_s.triggerDbLevel = std::clamp(db, -40.0f, 0.0f);
        rebuildSeries();
        emit thresholdDbChanged(m_s.triggerDbLevel);
    } else if (m_draggingCtrl >= 0) {
        QList<QPointF>& pts = m_ctrlIsFadeOut ? m_s.fadeOutPoints : m_s.fadeInPoints;
        if (m_draggingCtrl < pts.size()) {
            const float dur    = m_ctrlIsFadeOut ? m_s.fadeOutTime  : m_s.fadeInTime;
            const float lev    = m_ctrlIsFadeOut ? m_s.fadeOutLevel : m_s.fadeInLevel;
            const float startX = m_ctrlIsFadeOut ? 0.0f : m_s.fadeOutTime * 0.5f;
            float nx = std::clamp((float)(val.x() - startX) / dur, 0.0f, 1.0f);
            float ny = std::clamp((float)val.y() / lev, 0.0f, 1.0f);
            pts[m_draggingCtrl] = {nx, ny};
            rebuildSeries();
            emit customPointMoved(m_ctrlIsFadeOut, m_draggingCtrl, {nx, ny});
        }
    }

    setCursor(Qt::ArrowCursor);
    if (!m_threshold->points().isEmpty() && m_s.triggerMode == CrossfadeSettings::AutoDetect) {
        float linLevel = std::pow(10.0f, m_s.triggerDbLevel / 20.0f);
        QPointF sp = chart()->mapToPosition({0, (double)linLevel}, m_threshold);
        if (std::abs(e->pos().y() - sp.y()) < 10) setCursor(Qt::SizeVerCursor);
    }
    for (int i = 0; i < m_outCtrlPts->count(); ++i) {
        QPointF sp = chart()->mapToPosition(m_outCtrlPts->at(i), m_outCtrlPts);
        if ((e->pos() - sp.toPoint()).manhattanLength() < 14) { setCursor(Qt::OpenHandCursor); break; }
    }
    for (int i = 0; i < m_inCtrlPts->count(); ++i) {
        QPointF sp = chart()->mapToPosition(m_inCtrlPts->at(i), m_inCtrlPts);
        if ((e->pos() - sp.toPoint()).manhattanLength() < 14) { setCursor(Qt::OpenHandCursor); break; }
    }

    m_lastMouse = e->pos();
    QChartView::mouseMoveEvent(e);
}

void CrossfadeCurveView::mouseReleaseEvent(QMouseEvent* e) {
    m_draggingThreshold = false;
    m_draggingCtrl      = -1;
    setCursor(Qt::ArrowCursor);
    QChartView::mouseReleaseEvent(e);
}

// ═════════════════════════════════════════════════════════════════════════════
// CrossfaderSettingsDialog — helpers
// ═════════════════════════════════════════════════════════════════════════════
static QGroupBox* makeGroup(const QString& title, QWidget* parent) {
    auto* g = new QGroupBox(title, parent);
    g->setStyleSheet(groupBoxQss());
    return g;
}

static QLabel* makeLbl(const QString& text, QWidget* parent, bool muted = false) {
    auto* l = new QLabel(text, parent);
    l->setStyleSheet(labelQss(muted));
    return l;
}

static QSlider* makeSlider(int min, int max, int val, QWidget* parent) {
    auto* s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    s->setValue(val);
    s->setStyleSheet(sliderQss());
    return s;
}

// ═════════════════════════════════════════════════════════════════════════════
// CrossfaderSettingsDialog
// ═════════════════════════════════════════════════════════════════════════════
CrossfaderSettingsDialog::CrossfaderSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Cross-Fading Settings");
    setMinimumSize(700, 600);

    buildUi();
    applyDialogColors();
    applyChartColors();
    loadSettings();

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &CrossfaderSettingsDialog::onThemeChanged);
}

void CrossfaderSettingsDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 8);
    root->setSpacing(8);

    // ── Title ─────────────────────────────────────────────────────────────
    {
        auto* titleLbl = new QLabel("⚙  Cross-Fading Settings", this);
        titleLbl->setObjectName("DlgTitle");
        root->addWidget(titleLbl);
    }

    // ── Top row: Fade Out | Fade In panels ────────────────────────────────
    {
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(8);

        auto makeFadePanel = [&](const QString& title,
                                  QCheckBox*&   enableCb,
                                  QComboBox*&   curveCb,
                                  QSlider*&     timeSl, QLabel*& timeLbl,
                                  QSlider*&     levelSl, QLabel*& levelLbl) -> QGroupBox*
        {
            auto* g    = makeGroup(title, this);
            auto* glay = new QGridLayout(g);
            glay->setContentsMargins(8, 14, 8, 6);
            glay->setSpacing(5);
            glay->setColumnStretch(1, 1);

            enableCb = new QCheckBox("Enable", g);
            enableCb->setStyleSheet(checkQss());
            enableCb->setChecked(true);
            glay->addWidget(enableCb, 0, 0, 1, 3);

            glay->addWidget(makeLbl("Curve:", g, true), 1, 0);
            curveCb = new QComboBox(g);
            curveCb->setStyleSheet(comboQss());
            curveCb->addItems({"Linear", "S-Curve", "Exponential", "Custom"});
            curveCb->setCurrentIndex(1);
            glay->addWidget(curveCb, 1, 1, 1, 2);

            glay->addWidget(makeLbl("Time:", g, true), 2, 0);
            timeSl = makeSlider(5, 300, 50, g);
            glay->addWidget(timeSl, 2, 1);
            timeLbl = makeLbl("5.0s", g);
            timeLbl->setFixedWidth(36);
            timeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            glay->addWidget(timeLbl, 2, 2);

            glay->addWidget(makeLbl("Level:", g, true), 3, 0);
            levelSl = makeSlider(0, 100, 90, g);
            glay->addWidget(levelSl, 3, 1);
            levelLbl = makeLbl("90%", g);
            levelLbl->setFixedWidth(36);
            levelLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            glay->addWidget(levelLbl, 3, 2);

            return g;
        };

        auto* outGroup = makeFadePanel("Fade Out  (outgoing track)",
            m_outEnable, m_outCurve, m_outTimeSl, m_outTimeLbl, m_outLevelSl, m_outLevelLbl);
        auto* inGroup  = makeFadePanel("Fade In  (incoming track)",
            m_inEnable,  m_inCurve,  m_inTimeSl,  m_inTimeLbl,  m_inLevelSl,  m_inLevelLbl);

        topRow->addWidget(outGroup, 1);
        topRow->addWidget(inGroup,  1);
        root->addLayout(topRow);
    }

    // ── Chart ─────────────────────────────────────────────────────────────
    {
        auto* chartGroup = makeGroup(
            "Crossfade Preview  —  drag the red threshold line  •  Ctrl+click to add custom points",
            this);
        auto* cLay = new QVBoxLayout(chartGroup);
        cLay->setContentsMargins(4, 14, 4, 4);

        m_chart = new CrossfadeCurveView(this);
        cLay->addWidget(m_chart);

        auto* note = new QLabel(
            "ℹ  Green = fade out  •  Blue = fade in  •  Red = dB trigger threshold  "
            "•  Custom curve: set Curve to 'Custom', then Ctrl+click to add points, drag to reshape",
            chartGroup);
        note->setStyleSheet(labelQss(true));
        note->setWordWrap(true);
        cLay->addWidget(note);

        root->addWidget(chartGroup, 1);
    }

    // ── Trigger settings ─────────────────────────────────────────────────
    {
        auto* trigGroup = makeGroup("Cross-fade Trigger", this);
        auto* tLay = new QGridLayout(trigGroup);
        tLay->setContentsMargins(8, 14, 8, 6);
        tLay->setSpacing(5);
        tLay->setColumnStretch(1, 1);

        tLay->addWidget(makeLbl("Mode:", trigGroup, true), 0, 0);
        m_triggerMode = new QComboBox(trigGroup);
        m_triggerMode->setStyleSheet(comboQss());
        m_triggerMode->addItems({"Fixed time", "Auto detect (dB level)"});
        m_triggerMode->setCurrentIndex(1);
        tLay->addWidget(m_triggerMode, 0, 1, 1, 3);

        m_fixedGroup = makeGroup("Fixed cross-fade point", trigGroup);
        auto* fLay = new QHBoxLayout(m_fixedGroup);
        fLay->setContentsMargins(6, 12, 6, 4);
        fLay->addWidget(makeLbl("Time:", m_fixedGroup, true));
        m_fixedTimeSl = makeSlider(1, 300, 30, m_fixedGroup);
        fLay->addWidget(m_fixedTimeSl, 1);
        m_fixedTimeLbl = makeLbl("3.0s", m_fixedGroup);
        m_fixedTimeLbl->setFixedWidth(36);
        fLay->addWidget(m_fixedTimeLbl);
        tLay->addWidget(m_fixedGroup, 1, 0, 1, 4);

        m_detectGroup = makeGroup("Cross-fade point detection", trigGroup);
        auto* dLay = new QGridLayout(m_detectGroup);
        dLay->setContentsMargins(6, 12, 6, 4);
        dLay->setSpacing(5);

        dLay->addWidget(makeLbl("Trigger at:", m_detectGroup, true), 0, 0);
        m_triggerDb = new QDoubleSpinBox(m_detectGroup);
        m_triggerDb->setRange(-40.0, 0.0);
        m_triggerDb->setSingleStep(0.5);
        m_triggerDb->setValue(-7.0);
        m_triggerDb->setSuffix(" dB");
        m_triggerDb->setStyleSheet(spinQss());
        m_triggerDb->setFixedWidth(80);
        dLay->addWidget(m_triggerDb, 0, 1);

        dLay->addWidget(makeLbl("Min fade:", m_detectGroup, true), 0, 2);
        m_minFadeMs = new QSpinBox(m_detectGroup);
        m_minFadeMs->setRange(50, 5000);
        m_minFadeMs->setValue(100);
        m_minFadeMs->setSuffix(" ms");
        m_minFadeMs->setStyleSheet(spinQss());
        m_minFadeMs->setFixedWidth(80);
        dLay->addWidget(m_minFadeMs, 0, 3);

        dLay->addWidget(makeLbl("Max fade:", m_detectGroup, true), 1, 2);
        m_maxFadeMs = new QSpinBox(m_detectGroup);
        m_maxFadeMs->setRange(500, 30000);
        m_maxFadeMs->setValue(6000);
        m_maxFadeMs->setSuffix(" ms");
        m_maxFadeMs->setStyleSheet(spinQss());
        m_maxFadeMs->setFixedWidth(80);
        dLay->addWidget(m_maxFadeMs, 1, 3);

        tLay->addWidget(m_detectGroup, 2, 0, 1, 4);
        root->addWidget(trigGroup);
    }

    // ── Skip short tracks ─────────────────────────────────────────────────
    {
        auto* skipRow = new QHBoxLayout;
        m_skipCheck = new QCheckBox("Do not crossfade or fade tracks", this);
        m_skipCheck->setStyleSheet(checkQss());
        m_skipCheck->setChecked(true);

        m_skipSecs = new QSpinBox(this);
        m_skipSecs->setRange(5, 300);
        m_skipSecs->setValue(65);
        m_skipSecs->setSuffix(" s");
        m_skipSecs->setStyleSheet(spinQss());
        m_skipSecs->setFixedWidth(70);

        skipRow->addWidget(m_skipCheck);
        skipRow->addWidget(m_skipSecs);
        skipRow->addWidget(makeLbl("seconds or less in duration", this, true));
        skipRow->addStretch(1);
        root->addLayout(skipRow);
    }

    // ── Separator ─────────────────────────────────────────────────────────
    {
        auto* sep = new QFrame(this);
        sep->setObjectName("DlgSep");
        sep->setFrameShape(QFrame::HLine);
        root->addWidget(sep);
    }

    // ── Buttons ───────────────────────────────────────────────────────────
    {
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(6);

        auto* restoreBtn = new QPushButton("Restore Defaults", this);
        restoreBtn->setStyleSheet(btnQss());
        connect(restoreBtn, &QPushButton::clicked, this, &CrossfaderSettingsDialog::onRestoreDefaults);

        auto* okBtn     = new QPushButton("OK", this);
        auto* cancelBtn = new QPushButton("Cancel", this);
        okBtn->setStyleSheet(btnQss(dlgColors().accent));
        cancelBtn->setStyleSheet(btnQss());

        connect(okBtn,     &QPushButton::clicked, this, [this]() { saveSettings(); accept(); });
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        btnRow->addWidget(restoreBtn);
        btnRow->addStretch(1);
        btnRow->addWidget(okBtn);
        btnRow->addWidget(cancelBtn);
        root->addLayout(btnRow);
    }

    // ── Connect all controls → live chart update ──────────────────────────
    connect(m_chart, &CrossfadeCurveView::thresholdDbChanged,
            this,   &CrossfaderSettingsDialog::onThresholdDbChanged);
    connect(m_chart, &CrossfadeCurveView::customPointMoved,
            this,   &CrossfaderSettingsDialog::onCustomPointMoved);

    auto connectAll = [&](QWidget* w) {
        if (auto* cb = qobject_cast<QCheckBox*>(w))
            connect(cb,  &QCheckBox::toggled,                this, &CrossfaderSettingsDialog::onControlChanged);
        else if (auto* sl = qobject_cast<QSlider*>(w))
            connect(sl,  &QSlider::valueChanged,             this, &CrossfaderSettingsDialog::onControlChanged);
        else if (auto* co = qobject_cast<QComboBox*>(w))
            connect(co,  &QComboBox::currentIndexChanged,    this, &CrossfaderSettingsDialog::onControlChanged);
        else if (auto* sb = qobject_cast<QDoubleSpinBox*>(w))
            connect(sb,  &QDoubleSpinBox::valueChanged,      this, &CrossfaderSettingsDialog::onControlChanged);
        else if (auto* sp = qobject_cast<QSpinBox*>(w))
            connect(sp,  &QSpinBox::valueChanged,            this, &CrossfaderSettingsDialog::onControlChanged);
    };

    const QList<QWidget*> allControls = {
        m_outEnable, m_outCurve, m_outTimeSl, m_outLevelSl,
        m_inEnable,  m_inCurve,  m_inTimeSl,  m_inLevelSl,
        m_triggerMode, m_fixedTimeSl, m_triggerDb, m_minFadeMs,
        m_maxFadeMs, m_skipCheck, m_skipSecs
    };
    for (QWidget* w : allControls) connectAll(w);

    connect(m_triggerMode, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_fixedGroup->setVisible(idx == 0);
        m_detectGroup->setVisible(idx == 1);
    });
    m_fixedGroup->setVisible(m_triggerMode->currentIndex() == 0);
    m_detectGroup->setVisible(m_triggerMode->currentIndex() == 1);
}

// ─── Settings round-trip ──────────────────────────────────────────────────────
CrossfadeSettings CrossfaderSettingsDialog::collectSettings() const {
    CrossfadeSettings s;
    s.fadeOutEnabled  = m_outEnable->isChecked();
    s.fadeOutCurve    = (CrossfadeSettings::CurveType)m_outCurve->currentIndex();
    s.fadeOutTime     = m_outTimeSl->value() / 10.0f;
    s.fadeOutLevel    = m_outLevelSl->value() / 100.0f;
    s.fadeOutPoints   = m_outCustomPts;
    s.fadeInEnabled   = m_inEnable->isChecked();
    s.fadeInCurve     = (CrossfadeSettings::CurveType)m_inCurve->currentIndex();
    s.fadeInTime      = m_inTimeSl->value() / 10.0f;
    s.fadeInLevel     = m_inLevelSl->value() / 100.0f;
    s.fadeInPoints    = m_inCustomPts;
    s.triggerMode     = (CrossfadeSettings::TriggerMode)m_triggerMode->currentIndex();
    s.fixedFadeTime   = m_fixedTimeSl->value() / 10.0f;
    s.triggerDbLevel  = (float)m_triggerDb->value();
    s.minFadeTimeMs   = m_minFadeMs->value();
    s.maxFadeTimeMs   = m_maxFadeMs->value();
    s.skipShortTracks = m_skipCheck->isChecked();
    s.skipDurationSecs= m_skipSecs->value();
    return s;
}

void CrossfaderSettingsDialog::applySettings(const CrossfadeSettings& s) {
    QList<QWidget*> widgets = {m_outEnable, m_outCurve, m_outTimeSl, m_outLevelSl,
                               m_inEnable,  m_inCurve,  m_inTimeSl,  m_inLevelSl,
                               m_triggerMode, m_fixedTimeSl, m_triggerDb,
                               m_minFadeMs, m_maxFadeMs, m_skipCheck, m_skipSecs};
    for (auto* w : widgets) w->blockSignals(true);

    m_outEnable->setChecked(s.fadeOutEnabled);
    m_outCurve->setCurrentIndex((int)s.fadeOutCurve);
    m_outTimeSl->setValue((int)(s.fadeOutTime  * 10.0f));
    m_outLevelSl->setValue((int)(s.fadeOutLevel * 100.0f));
    m_inEnable->setChecked(s.fadeInEnabled);
    m_inCurve->setCurrentIndex((int)s.fadeInCurve);
    m_inTimeSl->setValue((int)(s.fadeInTime  * 10.0f));
    m_inLevelSl->setValue((int)(s.fadeInLevel * 100.0f));
    m_triggerMode->setCurrentIndex((int)s.triggerMode);
    m_fixedTimeSl->setValue((int)(s.fixedFadeTime * 10.0f));
    m_triggerDb->setValue(s.triggerDbLevel);
    m_minFadeMs->setValue(s.minFadeTimeMs);
    m_maxFadeMs->setValue(s.maxFadeTimeMs);
    m_skipCheck->setChecked(s.skipShortTracks);
    m_skipSecs->setValue(s.skipDurationSecs);
    m_outCustomPts = s.fadeOutPoints;
    m_inCustomPts  = s.fadeInPoints;

    for (auto* w : widgets) w->blockSignals(false);

    m_outTimeLbl->setText(QString("%1s").arg(s.fadeOutTime,   0, 'f', 1));
    m_outLevelLbl->setText(QString("%1%").arg((int)(s.fadeOutLevel * 100)));
    m_inTimeLbl->setText(QString("%1s").arg(s.fadeInTime,    0, 'f', 1));
    m_inLevelLbl->setText(QString("%1%").arg((int)(s.fadeInLevel  * 100)));
    m_fixedTimeLbl->setText(QString("%1s").arg(s.fixedFadeTime, 0, 'f', 1));

    m_fixedGroup->setVisible(s.triggerMode == CrossfadeSettings::Fixed);
    m_detectGroup->setVisible(s.triggerMode == CrossfadeSettings::AutoDetect);

    m_chart->updateCurves(s);
}

void CrossfaderSettingsDialog::loadSettings() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    CrossfadeSettings cfg;
    cfg.loadFromSettings(s);
    applySettings(cfg);
}

void CrossfaderSettingsDialog::saveSettings() const {
    QSettings s("Mcaster1", "Mcaster1Studio");
    collectSettings().saveToSettings(s);
}

CrossfadeSettings CrossfaderSettingsDialog::settings() const {
    return collectSettings();
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void CrossfaderSettingsDialog::onRestoreDefaults() {
    applySettings(CrossfadeSettings::defaults());
}

void CrossfaderSettingsDialog::onControlChanged() {
    m_outTimeLbl->setText(QString("%1s").arg(m_outTimeSl->value()   / 10.0f, 0, 'f', 1));
    m_outLevelLbl->setText(QString("%1%").arg(m_outLevelSl->value()));
    m_inTimeLbl->setText(QString("%1s").arg(m_inTimeSl->value()    / 10.0f, 0, 'f', 1));
    m_inLevelLbl->setText(QString("%1%").arg(m_inLevelSl->value()));
    m_fixedTimeLbl->setText(QString("%1s").arg(m_fixedTimeSl->value() / 10.0f, 0, 'f', 1));
    m_chart->updateCurves(collectSettings());
}

void CrossfaderSettingsDialog::onThresholdDbChanged(float db) {
    m_triggerDb->blockSignals(true);
    m_triggerDb->setValue(db);
    m_triggerDb->blockSignals(false);
}

void CrossfaderSettingsDialog::onCustomPointMoved(bool /*fadeOut*/, int /*idx*/, QPointF /*n*/) {
    m_chart->updateCurves(collectSettings());
}

// ─── Theme support ────────────────────────────────────────────────────────────
void CrossfadeCurveView::refreshTheme() {
    const auto col = dlgColors();
    auto* c = chart();
    c->setBackgroundBrush(QBrush(QColor(col.chartBg)));
    c->setPlotAreaBackgroundBrush(QBrush(QColor(col.chartBg)));
    if (m_axisX) {
        m_axisX->setLabelsColor(QColor(col.textMuted));
        m_axisX->setGridLineColor(QColor(col.chartGrid));
        m_axisX->setLinePen(QPen(QColor(col.chartGrid)));
        m_axisX->setTitleBrush(QBrush(QColor(col.textMuted)));
    }
    if (m_axisY) {
        m_axisY->setLabelsColor(QColor(col.textMuted));
        m_axisY->setGridLineColor(QColor(col.chartGrid));
        m_axisY->setLinePen(QPen(QColor(col.chartGrid)));
    }
    setStyleSheet(QString("background:%1; border:1px solid %2;")
                  .arg(col.chartBg, col.chartGrid));
}

void CrossfaderSettingsDialog::applyDialogColors() {
    const auto c = dlgColors();
    setStyleSheet(QString("QDialog { background:%1; }").arg(c.bg));

    for (auto* w : findChildren<QGroupBox*>())       w->setStyleSheet(groupBoxQss());
    for (auto* w : findChildren<QCheckBox*>())       w->setStyleSheet(checkQss());
    for (auto* w : findChildren<QSlider*>())         w->setStyleSheet(sliderQss());
    for (auto* w : findChildren<QComboBox*>())       w->setStyleSheet(comboQss());
    for (auto* w : findChildren<QAbstractSpinBox*>()) w->setStyleSheet(spinQss());
    for (auto* w : findChildren<QPushButton*>())     w->setStyleSheet(btnQss());
    for (auto* w : findChildren<QLabel*>())          w->setStyleSheet(labelQss(true));

    // Title label: larger, primary color, border
    if (auto* lbl = findChild<QLabel*>("DlgTitle"))
        lbl->setStyleSheet(
            QString("QLabel { color:%1; font-size:14px; font-weight:700;"
                    " border-bottom:1px solid %2; padding-bottom:6px;"
                    " background:transparent; }")
            .arg(c.textPrimary, c.border));

    // Separator
    if (auto* sep = findChild<QFrame*>("DlgSep"))
        sep->setStyleSheet(QString("QFrame { color:%1; }").arg(c.border));

    // Re-apply accent color to OK button (find by text)
    for (auto* btn : findChildren<QPushButton*>())
        if (btn->text() == "OK")
            btn->setStyleSheet(btnQss(c.accent));
}

void CrossfaderSettingsDialog::applyChartColors() {
    if (m_chart) m_chart->refreshTheme();
}

void CrossfaderSettingsDialog::onThemeChanged() {
    applyDialogColors();
    applyChartColors();
}
