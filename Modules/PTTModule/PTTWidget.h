#pragma once
#include <QWidget>
#include <QTimer>
#include <QEvent>
#include "PTTModule.h"

class QProgressBar;
class QDial;
class QLabel;
class QPushButton;
class QComboBox;

namespace M1 { class PTTModule; }

/// PTTWidget — UI for PTTModule.
///
/// Layout (top to bottom):
///   - State indicator LED (gray / amber / red)
///   - Large round PTT button (120x120 minimum) — hold for live
///   - Input VU meter (QProgressBar, green/amber/red segments)
///   - Three knobs: Gate Threshold, De-ess Amount, Compress Threshold
///   - Knob labels row
///
/// Dark broadcast hardware style: #0c1a2e background.
class PTTWidget : public QWidget {
    Q_OBJECT

public:
    explicit PTTWidget(M1::PTTModule* module, QWidget* parent = nullptr);
    ~PTTWidget() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onStateChanged(M1::PTTModule::State newState);
    void pollLevelMeter();
    void onDeviceChanged(int index);

private:
    void buildUi();
    void applyTheme();
    void applyButtonStyle(M1::PTTModule::State s);
    void applyLedStyle(M1::PTTModule::State s);

    M1::PTTModule* m_module = nullptr;

    // ── Widgets ───────────────────────────────────────────────────────────
    QLabel*      m_ledLabel     = nullptr;  ///< State indicator LED
    QWidget*     m_pttButton    = nullptr;  ///< Large round PTT button (custom painted)
    QProgressBar* m_vuMeter     = nullptr;  ///< Input VU meter
    QDial*       m_gateDial     = nullptr;  ///< Gate threshold knob (0..100)
    QDial*       m_deEssDial    = nullptr;  ///< De-ess amount knob (0..100)
    QDial*       m_compDial     = nullptr;  ///< Compressor threshold knob (0..100)
    QLabel*      m_stateLabel   = nullptr;  ///< Text status: "OFF / ARMED / LIVE"
    QComboBox*   m_deviceCombo  = nullptr;  ///< Mic input device selector
    QWidget*     m_sepLine      = nullptr;  ///< Thin separator line (theme-colored)

    // ── PTT button state (for paint) ─────────────────────────────────────
    bool  m_pttPressed = false;

    // ── Level polling timer ───────────────────────────────────────────────
    QTimer m_levelTimer;
};
