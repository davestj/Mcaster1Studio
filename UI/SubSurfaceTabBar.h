#pragma once
#include <QWidget>
#include <QList>
#include <QColor>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QPushButton>

class QLabel;
class QHBoxLayout;
class QScrollArea;
class QTimer;

// ─── OnAirButton ──────────────────────────────────────────────────────────────
/// Multi-mode status indicator on each sub-surface tab.
///
/// Modes:
///   OffAir      — solid green "● OFF AIR" (default idle)
///   OnAir       — flashing red "● ON AIR"  (auto: encoder streaming)
///   AutoDJ      — flashing green "● AUTO-DJ" (auto: AutoDJ active)
///   Idle        — dim gray "● IDLE" (auto: AutoDJ off on playlist surface)
///   InPlanning  — amber "● IN-PLANNING" (manual: media library work)
///   Monitoring  — green "● MONITORING" (manual: health/encoder monitoring)
///
/// Right-click to manually set status; auto-modes driven by MainWindow wiring.
class OnAirButton : public QPushButton {
    Q_OBJECT
public:
    enum class StatusMode {
        OffAir,       ///< Solid green idle
        OnAir,        ///< Flashing red — encoders streaming
        AutoDJ,       ///< Flashing green — AutoDJ active
        Idle,         ///< Dim gray — deck idle
        InPlanning,   ///< Amber — media library / playlist management
        Monitoring    ///< Green — health / encoder monitoring active
    };

    explicit OnAirButton(QWidget* parent = nullptr);

    bool isOnAir()    const { return m_mode == StatusMode::OnAir; }
    StatusMode mode() const { return m_mode; }

    /// Set the status mode. Updates visuals and starts/stops flash timer.
    void setStatusMode(StatusMode mode);

signals:
    void onAirChanged(bool onAir);
    void statusModeChanged(OnAirButton::StatusMode mode);

protected:
    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
    void onToggled(bool checked);
    void onFlashTick();

private:
    void applyModeStyle();
    QTimer*    m_flashTimer = nullptr;
    bool       m_flashOn    = false;
    StatusMode m_mode       = StatusMode::OffAir;
};

// ─── SubSurfaceTabChip ────────────────────────────────────────────────────────
/// One tab in the SubSurfaceTabBar.
///
/// Visually: a 4 px colored left border, name label, and ON-AIR button.
/// Selected state = lighter background + bottom highlight.
/// Right-click opens a context menu with module management and session actions.
class SubSurfaceTabChip : public QWidget {
    Q_OBJECT
public:
    explicit SubSurfaceTabChip(const QString& name,
                                const QColor& color,
                                QWidget* parent = nullptr);

    QString name()       const { return m_name; }
    QColor  color()      const { return m_color; }
    bool    isSelected() const { return m_selected; }
    bool    isOnAir()    const;

    OnAirButton::StatusMode statusMode() const;
    void setStatusMode(OnAirButton::StatusMode mode);

    void setSelected(bool s);
    void setName(const QString& name);
    void setColor(const QColor& color);

signals:
    void clicked(SubSurfaceTabChip*);
    void onAirChanged(SubSurfaceTabChip*, bool onAir);
    void statusModeChanged(SubSurfaceTabChip*, OnAirButton::StatusMode mode);

    void addModuleRequested(SubSurfaceTabChip*, const QString& moduleId);
    void saveSessionRequested(SubSurfaceTabChip*);
    void loadSessionRequested(SubSurfaceTabChip*);
    void renameRequested(SubSurfaceTabChip*);
    void colorRequested(SubSurfaceTabChip*);
    void closeRequested(SubSurfaceTabChip*);

protected:
    void contextMenuEvent(QContextMenuEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void updateAppearance();

    QString      m_name;
    QColor       m_color;
    bool         m_selected = false;
    bool         m_hovered  = false;
    bool         m_pressed  = false;
    QLabel*      m_label    = nullptr;
    OnAirButton* m_onAir    = nullptr;
};

// ─── SubSurfaceTabBar ─────────────────────────────────────────────────────────
/// Horizontal strip of sub-surface tabs.
///
/// Layout:  [ scrollable tab chips ... ] [ + ] [ ▼ Sessions ]
///
/// Sits between the SurfaceRibbon and the module canvas in SurfaceWidget.
/// Each tab corresponds to one SubSurfacePanel in the QStackedWidget.
class SubSurfaceTabBar : public QWidget {
    Q_OBJECT
public:
    explicit SubSurfaceTabBar(const QString& defaultName,
                               const QColor&  defaultColor,
                               QWidget* parent = nullptr);

    int count()        const { return m_chips.size(); }
    int currentIndex() const { return m_currentIndex; }

    SubSurfaceTabChip* chip(int i) const;
    SubSurfaceTabChip* currentChip() const;

    int  addSubSurface(const QString& name, const QColor& color);
    void removeSubSurface(int index);
    void setCurrentIndex(int index);
    void setTabName(int index, const QString& name);
    void setTabColor(int index, const QColor& color);
    void setTabStatusMode(int index, OnAirButton::StatusMode mode);

    /// Embed a compact widget in the right-hand slot of the tab bar (e.g. clock).
    void setRightWidget(QWidget* w);

signals:
    void currentChanged(int index);
    void subSurfaceAdded(int index, const QString& name, const QColor& color);
    void subSurfaceRemoved(int index);
    void addModuleRequested(int tabIndex, const QString& moduleId);
    void saveSessionRequested(int tabIndex);
    void loadSessionRequested(int tabIndex);
    void onAirChanged(int tabIndex, bool onAir);
    void tabRenamed(int tabIndex, const QString& newName);
    void tabColorChanged(int tabIndex, const QColor& color);

private slots:
    void onChipClicked(SubSurfaceTabChip* chip);
    void onAddClicked();
    void onSessionMenuClicked();

private:
    void applyTheme();

    QList<SubSurfaceTabChip*> m_chips;
    int                       m_currentIndex = 0;
    QHBoxLayout*              m_chipLayout   = nullptr;
    QPushButton*              m_addBtn       = nullptr;
    QPushButton*              m_menuBtn      = nullptr;
    QWidget*                  m_rightSlot    = nullptr;
};
