#pragma once
#include <QDialog>
#include <QSettings>
#include <QList>
#include <QPointF>
#include <QtCharts/QChartView>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>

class QCheckBox;
class QComboBox;
class QSlider;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QGroupBox;
class QPushButton;

// ─── CrossfadeSettings ────────────────────────────────────────────────────────
/// Persistent crossfade configuration — shared between CrossfaderWidget and
/// QueueModule AutoDJ.  Saved/loaded via QSettings under "Crossfade/" prefix.
struct CrossfadeSettings {
    enum CurveType {
        Linear      = 0,
        SCurve      = 1,   ///< Equal-power cosine (default)
        Exponential = 2,
        Custom      = 3    ///< User-drawn Bezier
    };
    enum TriggerMode {
        Fixed       = 0,   ///< Start fade at fixed time before end
        AutoDetect  = 1    ///< Start fade when dB level drops below threshold
    };

    // ── Fade Out (outgoing/current track) ────────────────────────────────────
    bool      fadeOutEnabled  = true;
    CurveType fadeOutCurve    = SCurve;
    float     fadeOutTime     = 5.0f;   // seconds
    float     fadeOutLevel    = 0.90f;  // 0–1 (max outgoing level)
    QList<QPointF> fadeOutPoints;       // custom Bezier control points (normalised 0–1)

    // ── Fade In (incoming/next track) ─────────────────────────────────────────
    bool      fadeInEnabled   = true;
    CurveType fadeInCurve     = SCurve;
    float     fadeInTime      = 3.2f;
    float     fadeInLevel     = 0.80f;
    QList<QPointF> fadeInPoints;

    // ── Trigger ───────────────────────────────────────────────────────────────
    TriggerMode triggerMode      = AutoDetect;
    float       fixedFadeTime    = 3.0f;   // seconds (Fixed mode)
    float       triggerDbLevel   = -7.0f;  // dB threshold for AutoDetect
    int         minFadeTimeMs    = 100;
    int         maxFadeTimeMs    = 6000;

    // ── Skip short tracks ─────────────────────────────────────────────────────
    bool skipShortTracks  = true;
    int  skipDurationSecs = 65;

    void saveToSettings(QSettings& s, const QString& prefix = "Crossfade") const;
    void loadFromSettings(QSettings& s, const QString& prefix = "Crossfade");
    static CrossfadeSettings defaults() { return {}; }
};

// ─── CrossfadeCurveView ───────────────────────────────────────────────────────
/// Interactive Qt Charts view showing fade-out (green) and fade-in (blue) curves.
/// Supports:
///   • Live update as settings change (updateCurves)
///   • Draggable red threshold line
///   • In Custom mode: draggable Bezier control handles
class CrossfadeCurveView : public QChartView {
    Q_OBJECT

public:
    explicit CrossfadeCurveView(QWidget* parent = nullptr);

    void updateCurves(const CrossfadeSettings& s);

signals:
    void thresholdDbChanged(float db);          ///< user dragged threshold
    void customPointMoved(bool fadeOut, int idx, QPointF normalised);

protected:
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    void buildChart();
    void rebuildSeries();

    QVector<QPointF> calcCurve(bool fadeOut) const;
    float evalCurve(CrossfadeSettings::CurveType type,
                    float t, float level, bool fadeOut) const;

    CrossfadeSettings  m_s;

    QLineSeries*    m_outLine      = nullptr;  // green curve
    QLineSeries*    m_inLine       = nullptr;  // blue curve
    QLineSeries*    m_outBaseLine  = nullptr;  // zero line for area
    QLineSeries*    m_inBaseLine   = nullptr;
    QAreaSeries*    m_outArea      = nullptr;  // green filled
    QAreaSeries*    m_inArea       = nullptr;  // blue filled
    QLineSeries*    m_threshold    = nullptr;  // red dashed
    QScatterSeries* m_outCtrlPts   = nullptr;  // custom-mode handles
    QScatterSeries* m_inCtrlPts    = nullptr;
    QValueAxis*     m_axisX        = nullptr;
    QValueAxis*     m_axisY        = nullptr;

    bool  m_draggingThreshold = false;
    int   m_draggingCtrl      = -1;   // index, -1=none
    bool  m_ctrlIsFadeOut     = true;
    QPoint m_lastMouse;
};

// ─── CrossfaderSettingsDialog ─────────────────────────────────────────────────
/// SAM Broadcaster-style Cross-Fading settings dialog.
///
/// Usage:
///   CrossfaderSettingsDialog dlg(this);
///   dlg.loadSettings();                // read from QSettings
///   if (dlg.exec() == QDialog::Accepted) {
///       dlg.saveSettings();            // write back to QSettings
///       CrossfadeSettings cfg = dlg.settings();
///   }
class CrossfaderSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit CrossfaderSettingsDialog(QWidget* parent = nullptr);

    void loadSettings();                         ///< pull from QSettings
    void saveSettings() const;                   ///< push to QSettings
    CrossfadeSettings settings() const;

private slots:
    void onRestoreDefaults();
    void onControlChanged();       ///< any control changed → refresh chart
    void onThresholdDbChanged(float db);
    void onCustomPointMoved(bool fadeOut, int idx, QPointF normalised);

private:
    void buildUi();
    void applySettings(const CrossfadeSettings& s);
    CrossfadeSettings collectSettings() const;

    // ── Fade Out controls ──────────────────────────────────────────────────
    QCheckBox*   m_outEnable   = nullptr;
    QComboBox*   m_outCurve    = nullptr;
    QSlider*     m_outTimeSl   = nullptr;
    QLabel*      m_outTimeLbl  = nullptr;
    QSlider*     m_outLevelSl  = nullptr;
    QLabel*      m_outLevelLbl = nullptr;

    // ── Fade In controls ───────────────────────────────────────────────────
    QCheckBox*   m_inEnable    = nullptr;
    QComboBox*   m_inCurve     = nullptr;
    QSlider*     m_inTimeSl    = nullptr;
    QLabel*      m_inTimeLbl   = nullptr;
    QSlider*     m_inLevelSl   = nullptr;
    QLabel*      m_inLevelLbl  = nullptr;

    // ── Trigger controls ───────────────────────────────────────────────────
    QComboBox*      m_triggerMode  = nullptr;
    QSlider*        m_fixedTimeSl  = nullptr;
    QLabel*         m_fixedTimeLbl = nullptr;
    QDoubleSpinBox* m_triggerDb    = nullptr;
    QSpinBox*       m_minFadeMs    = nullptr;
    QSpinBox*       m_maxFadeMs    = nullptr;
    QGroupBox*      m_fixedGroup   = nullptr;
    QGroupBox*      m_detectGroup  = nullptr;

    // ── Skip short tracks ──────────────────────────────────────────────────
    QCheckBox* m_skipCheck = nullptr;
    QSpinBox*  m_skipSecs  = nullptr;

    // ── Chart ─────────────────────────────────────────────────────────────
    CrossfadeCurveView* m_chart = nullptr;

    // Current custom-mode points (kept in sync with chart)
    QList<QPointF> m_outCustomPts;
    QList<QPointF> m_inCustomPts;
};
