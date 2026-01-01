#include "SubSurfaceTabBar.h"
#include "ModuleRegistry.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QMenu>
#include <QInputDialog>
#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QContextMenuEvent>

// ─── Palette of auto-assigned colors for new sub-surfaces ────────────────────
static const QList<QColor> kSubColors = {
    QColor("#0ea5e9"),  // Mcaster Blue
    QColor("#7c3aed"),  // Purple
    QColor("#22c55e"),  // Green
    QColor("#f59e0b"),  // Amber
    QColor("#ef4444"),  // Red
    QColor("#38bdf8"),  // Cyan
    QColor("#a855f7"),  // Violet
    QColor("#f97316"),  // Orange
};

// ─── OnAirButton ─────────────────────────────────────────────────────────────
OnAirButton::OnAirButton(QWidget* parent)
    : QPushButton(parent)
{
    setCheckable(true);
    setObjectName("OnAirButton");
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(20);
    setMinimumWidth(72);
    setToolTip("Status indicator — right-click for options");

    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(500);
    connect(m_flashTimer, &QTimer::timeout, this, &OnAirButton::onFlashTick);
    connect(this, &QPushButton::toggled, this, &OnAirButton::onToggled);

    applyModeStyle();
}

void OnAirButton::setStatusMode(StatusMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;

    // Start/stop flash timer based on mode
    const bool needsFlash = (mode == StatusMode::OnAir || mode == StatusMode::AutoDJ);
    if (needsFlash) {
        m_flashOn = true;
        m_flashTimer->start();
    } else {
        m_flashTimer->stop();
    }

    // Sync checked state with mode
    blockSignals(true);
    setChecked(mode == StatusMode::OnAir);
    blockSignals(false);

    applyModeStyle();
    emit statusModeChanged(mode);
    emit onAirChanged(mode == StatusMode::OnAir);
}

void OnAirButton::onToggled(bool checked) {
    // Manual click toggles between OnAir and OffAir
    setStatusMode(checked ? StatusMode::OnAir : StatusMode::OffAir);
}

void OnAirButton::onFlashTick() {
    m_flashOn = !m_flashOn;
    applyModeStyle();
}

void OnAirButton::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);

    auto addMode = [&](const QString& label, StatusMode mode) {
        auto* act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(m_mode == mode);
        connect(act, &QAction::triggered, this, [this, mode]() {
            setStatusMode(mode);
        });
    };

    addMode("OFF AIR",      StatusMode::OffAir);
    addMode("ON AIR",       StatusMode::OnAir);
    addMode("AUTO-DJ",      StatusMode::AutoDJ);
    addMode("IDLE",         StatusMode::Idle);
    menu.addSeparator();
    addMode("IN-PLANNING",  StatusMode::InPlanning);
    addMode("MONITORING",   StatusMode::Monitoring);

    menu.exec(e->globalPos());
}

void OnAirButton::applyModeStyle() {
    // Common base: border-radius 3px, 9px bold, 2px 6px padding
    static const char* kBase =
        "  border-radius: 3px; font-size: 9px; font-weight: 700; padding: 2px 6px;";

    switch (m_mode) {
    case StatusMode::OffAir:
        setText("● OFF AIR");
        setStyleSheet(QString(
            "OnAirButton { background: #166534; color: #dcfce7;"
            "  border: 1px solid #22c55e; %1 }"
            "OnAirButton:hover { background: #15803d; border-color: #4ade80; }"
        ).arg(kBase));
        break;

    case StatusMode::OnAir:
        if (m_flashOn) {
            setText("● ON AIR");
            setStyleSheet(QString(
                "OnAirButton { background: #dc2626; color: #ffffff;"
                "  border: 1px solid #ef4444; %1 }"
            ).arg(kBase));
        } else {
            setText("  ON AIR");
            setStyleSheet(QString(
                "OnAirButton { background: #7f1d1d; color: #fca5a5;"
                "  border: 1px solid #991b1b; %1 }"
            ).arg(kBase));
        }
        break;

    case StatusMode::AutoDJ:
        if (m_flashOn) {
            setText("● AUTO-DJ");
            setStyleSheet(QString(
                "OnAirButton { background: #16a34a; color: #ffffff;"
                "  border: 1px solid #22c55e; %1 }"
            ).arg(kBase));
        } else {
            setText("  AUTO-DJ");
            setStyleSheet(QString(
                "OnAirButton { background: #14532d; color: #86efac;"
                "  border: 1px solid #166534; %1 }"
            ).arg(kBase));
        }
        break;

    case StatusMode::Idle:
        setText("● IDLE");
        setStyleSheet(QString(
            "OnAirButton { background: #374151; color: #9ca3af;"
            "  border: 1px solid #4b5563; %1 }"
            "OnAirButton:hover { background: #4b5563; color: #d1d5db; }"
        ).arg(kBase));
        break;

    case StatusMode::InPlanning:
        setText("● IN-PLANNING");
        setStyleSheet(QString(
            "OnAirButton { background: #92400e; color: #fef3c7;"
            "  border: 1px solid #f59e0b; %1 }"
            "OnAirButton:hover { background: #b45309; border-color: #fbbf24; }"
        ).arg(kBase));
        break;

    case StatusMode::Monitoring:
        setText("● MONITORING");
        setStyleSheet(QString(
            "OnAirButton { background: #065f46; color: #d1fae5;"
            "  border: 1px solid #10b981; %1 }"
            "OnAirButton:hover { background: #047857; border-color: #34d399; }"
        ).arg(kBase));
        break;
    }
}

// ─── SubSurfaceTabChip ────────────────────────────────────────────────────────
SubSurfaceTabChip::SubSurfaceTabChip(const QString& name,
                                      const QColor&  color,
                                      QWidget*       parent)
    : QWidget(parent)
    , m_name(name)
    , m_color(color)
{
    setObjectName("SubSurfaceTabChip");
    setMinimumHeight(34);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 3, 8, 3);
    layout->setSpacing(6);

    m_label = new QLabel(name, this);
    m_label->setObjectName("SubTabLabel");
    m_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_onAir = new OnAirButton(this);
    connect(m_onAir, &OnAirButton::onAirChanged, this, [this](bool v) {
        emit onAirChanged(this, v);
    });
    connect(m_onAir, &OnAirButton::statusModeChanged, this, [this](OnAirButton::StatusMode mode) {
        emit statusModeChanged(this, mode);
    });

    layout->addWidget(m_label);
    layout->addWidget(m_onAir);

    setSelected(false);
    updateAppearance();
}

bool SubSurfaceTabChip::isOnAir() const {
    return m_onAir ? m_onAir->isOnAir() : false;
}

OnAirButton::StatusMode SubSurfaceTabChip::statusMode() const {
    return m_onAir ? m_onAir->mode() : OnAirButton::StatusMode::OffAir;
}

void SubSurfaceTabChip::setStatusMode(OnAirButton::StatusMode mode) {
    if (m_onAir) m_onAir->setStatusMode(mode);
}

void SubSurfaceTabChip::setSelected(bool s) {
    m_selected = s;
    updateAppearance();
    update();
}

void SubSurfaceTabChip::setName(const QString& name) {
    m_name = name;
    if (m_label) m_label->setText(name);
}

void SubSurfaceTabChip::setColor(const QColor& color) {
    m_color = color;
    updateAppearance();
    update();
}

void SubSurfaceTabChip::enterEvent(QEnterEvent*) {
    m_hovered = true;
    update();
}

void SubSurfaceTabChip::leaveEvent(QEvent*) {
    m_hovered = false;
    m_pressed = false;
    update();
}

void SubSurfaceTabChip::updateAppearance() {
    if (!m_label) return;
    const bool light = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);

    if (m_selected) {
        // Bright white text on the colored tab
        m_label->setStyleSheet(
            "QLabel { color: #ffffff; font-weight: bold; font-size: 12px;"
            "         background: transparent; }");
    } else {
        const QString tc = light ? "#504a44" : "#90aec8";
        m_label->setStyleSheet(
            QString("QLabel { color: %1; font-weight: bold; font-size: 12px;"
                    "         background: transparent; }").arg(tc));
    }
}

void SubSurfaceTabChip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool light = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, 0);
    const qreal rad = 7.0;

    // ── Tab shape: rounded top corners, flat bottom ──
    QPainterPath tabShape;
    tabShape.moveTo(r.bottomLeft());
    tabShape.lineTo(r.left(), r.top() + rad);
    tabShape.quadTo(r.left(), r.top(), r.left() + rad, r.top());
    tabShape.lineTo(r.right() - rad, r.top());
    tabShape.quadTo(r.right(), r.top(), r.right(), r.top() + rad);
    tabShape.lineTo(r.bottomRight());
    tabShape.closeSubpath();

    if (m_selected) {
        // ── SELECTED: full gradient fill in the tab's color ──
        QLinearGradient grad(0, r.top(), 0, r.bottom());
        grad.setColorAt(0.0, m_color.lighter(145));
        grad.setColorAt(0.35, m_color.lighter(115));
        grad.setColorAt(0.7,  m_color);
        grad.setColorAt(1.0,  m_color.darker(115));
        p.fillPath(tabShape, grad);

        // Shine highlight (top 40%)
        QLinearGradient shine(0, r.top(), 0, r.top() + r.height() * 0.4);
        shine.setColorAt(0.0, QColor(255, 255, 255, 70));
        shine.setColorAt(1.0, QColor(255, 255, 255, 0));
        QPainterPath shineClip;
        shineClip.addRect(r.left(), r.top(), r.width(), r.height() * 0.4);
        p.fillPath(tabShape & shineClip, shine);

        // Border
        p.setPen(QPen(m_color.lighter(160), 1.2));
        p.drawPath(tabShape);

        // Inner glow along top edge
        QPen glowPen(QColor(255, 255, 255, 40), 1);
        p.setPen(glowPen);
        p.drawLine(QPointF(r.left() + rad + 1, r.top() + 1.5),
                   QPointF(r.right() - rad - 1, r.top() + 1.5));

    } else if (m_pressed) {
        // ── PRESSED: darkened color ──
        QLinearGradient grad(0, r.top(), 0, r.bottom());
        grad.setColorAt(0.0, m_color.darker(150));
        grad.setColorAt(1.0, m_color.darker(200));
        p.fillPath(tabShape, grad);

        p.setPen(QPen(m_color.darker(130), 1));
        p.drawPath(tabShape);

    } else if (m_hovered) {
        // ── HOVER: subtle color wash with glow ──
        QLinearGradient grad(0, r.top(), 0, r.bottom());
        if (light) {
            grad.setColorAt(0.0, QColor(m_color.red(), m_color.green(), m_color.blue(), 50));
            grad.setColorAt(0.5, QColor(m_color.red(), m_color.green(), m_color.blue(), 35));
            grad.setColorAt(1.0, QColor(m_color.red(), m_color.green(), m_color.blue(), 15));
        } else {
            grad.setColorAt(0.0, QColor(m_color.red(), m_color.green(), m_color.blue(), 70));
            grad.setColorAt(0.5, QColor(m_color.red(), m_color.green(), m_color.blue(), 45));
            grad.setColorAt(1.0, QColor(m_color.red(), m_color.green(), m_color.blue(), 20));
        }
        // Neutral base under the color wash
        const QColor baseBg = light ? QColor(232, 228, 224) : QColor(22, 38, 58);
        p.fillPath(tabShape, baseBg);
        p.fillPath(tabShape, grad);

        // Bottom accent line
        p.setPen(QPen(m_color, 2.5));
        p.drawLine(QPointF(r.left() + rad, r.bottom() - 0.5),
                   QPointF(r.right() - rad, r.bottom() - 0.5));

        // Subtle border
        p.setPen(QPen(m_color.darker(light ? 110 : 150), 0.8));
        p.drawPath(tabShape);

    } else {
        // ── NORMAL: flat neutral background, thin color accent ──
        const QColor bg = light ? QColor(225, 221, 217) : QColor(16, 28, 46);
        p.fillPath(tabShape, bg);

        // Thin bottom color accent
        p.setPen(QPen(QColor(m_color.red(), m_color.green(), m_color.blue(),
                             light ? 100 : 80), 1.5));
        p.drawLine(QPointF(r.left() + rad, r.bottom() - 0.5),
                   QPointF(r.right() - rad, r.bottom() - 0.5));

        // Subtle top/side border
        const QColor borderColor = light ? QColor(200, 196, 190) : QColor(30, 48, 70);
        p.setPen(QPen(borderColor, 0.6));
        p.drawPath(tabShape);
    }
}

void SubSurfaceTabChip::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        emit clicked(this);
    }
    QWidget::mousePressEvent(e);
}

void SubSurfaceTabChip::mouseReleaseEvent(QMouseEvent*) {
    m_pressed = false;
    update();
}

void SubSurfaceTabChip::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        emit renameRequested(this);
    QWidget::mouseDoubleClickEvent(e);
}

void SubSurfaceTabChip::contextMenuEvent(QContextMenuEvent* e) {
    const auto kModules = M1::availableModules();

    QMenu menu(this);

    auto* addSubMenu = new QMenu("Add Module", &menu);
    for (const auto& [id, name] : kModules) {
        addSubMenu->addAction(name, this, [this, id = id]() {
            emit addModuleRequested(this, id);
        });
    }
    menu.addMenu(addSubMenu);

    menu.addSeparator();
    menu.addAction("Save Session", this, [this]() { emit saveSessionRequested(this); });
    menu.addSeparator();
    menu.addAction("Rename...", this, [this]() { emit renameRequested(this); });
    menu.addAction("Set Color...", this, [this]() { emit colorRequested(this); });
    menu.addSeparator();

    auto* closeAct = menu.addAction("Close Sub-Surface", this, [this]() {
        emit closeRequested(this);
    });
    Q_UNUSED(closeAct);

    menu.exec(e->globalPos());
}

// ─── SubSurfaceTabBar ─────────────────────────────────────────────────────────
SubSurfaceTabBar::SubSurfaceTabBar(const QString& defaultName,
                                    const QColor&  defaultColor,
                                    QWidget*       parent)
    : QWidget(parent)
{
    setObjectName("SubSurfaceTabBar");
    setFixedHeight(38);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 2, 4, 0);
    root->setSpacing(0);

    // ── Scrollable chip area ──────────────────────────────────────
    auto* chipContainer = new QWidget(this);
    chipContainer->setObjectName("SubTabChipContainer");
    m_chipLayout = new QHBoxLayout(chipContainer);
    m_chipLayout->setContentsMargins(4, 0, 0, 0);
    m_chipLayout->setSpacing(3);
    m_chipLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("SubTabScrollArea");
    scroll->setWidget(chipContainer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFixedHeight(38);

    // ── Fixed right buttons ───────────────────────────────────────
    m_addBtn = new QPushButton("+", this);
    m_addBtn->setObjectName("SubTabAddBtn");
    m_addBtn->setToolTip("Add sub-surface");
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setFixedSize(28, 28);

    m_menuBtn = new QPushButton("▼", this);
    m_menuBtn->setObjectName("SubTabMenuBtn");
    m_menuBtn->setToolTip("Sessions menu");
    m_menuBtn->setCursor(Qt::PointingHandCursor);
    m_menuBtn->setFixedSize(28, 28);

    connect(m_addBtn,  &QPushButton::clicked, this, &SubSurfaceTabBar::onAddClicked);
    connect(m_menuBtn, &QPushButton::clicked, this, &SubSurfaceTabBar::onSessionMenuClicked);

    // ── Right slot (clock or other compact widget) ────────────────
    m_rightSlot = new QWidget(this);
    m_rightSlot->setObjectName("SubTabRightSlot");
    auto* rightLayout = new QHBoxLayout(m_rightSlot);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    root->addWidget(scroll, 1);
    root->addWidget(m_addBtn);
    root->addWidget(m_menuBtn);
    root->addWidget(m_rightSlot);

    // ── Theme-aware styling ──────────────────────────────────────
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    // Create the default tab
    addSubSurface(defaultName, defaultColor);
}

void SubSurfaceTabBar::applyTheme() {
    const bool light = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);

    // Tab bar strip background — contrasts with the tab chips
    const QString barBg   = light ? "#d8d4ce" : "#0a1628";
    const QString btnBg   = light ? "#ccc8c2" : "#12263d";
    const QString btnText = light ? "#504a44" : "#8ab4d8";
    const QString btnBord = light ? "#b8b4ae" : "#1e3a5f";
    const QString btnHov  = light ? "#bfbbb5" : "#1e3a5f";
    const QString btnHovT = light ? "#2a2420" : "#c8d8f0";

    setStyleSheet(QString(
        "#SubSurfaceTabBar { background-color: %1; }"
        "#SubTabChipContainer { background: transparent; }"
        "#SubTabScrollArea { background: transparent; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ).arg(barBg));

    const QString btnStyle = QString(
        "QPushButton {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: %4; color: %5; border-color: %5;"
        "}"
        "QPushButton:pressed {"
        "  background: %3;"
        "}"
    ).arg(btnBg, btnText, btnBord, btnHov, btnHovT);

    m_addBtn->setStyleSheet(btnStyle);
    m_menuBtn->setStyleSheet(btnStyle);

    // Refresh all chip appearances for new theme
    for (auto* chip : m_chips)
        chip->update();
}

int SubSurfaceTabBar::addSubSurface(const QString& name, const QColor& color) {
    auto* chip = new SubSurfaceTabChip(name, color, this);

    connect(chip, &SubSurfaceTabChip::clicked,
            this, &SubSurfaceTabBar::onChipClicked);

    connect(chip, &SubSurfaceTabChip::onAirChanged,
            this, [this](SubSurfaceTabChip* c, bool v) {
        const int idx = m_chips.indexOf(c);
        if (idx >= 0) emit onAirChanged(idx, v);
    });

    connect(chip, &SubSurfaceTabChip::addModuleRequested,
            this, [this](SubSurfaceTabChip* c, const QString& id) {
        const int idx = m_chips.indexOf(c);
        if (idx >= 0) emit addModuleRequested(idx, id);
    });

    connect(chip, &SubSurfaceTabChip::saveSessionRequested,
            this, [this](SubSurfaceTabChip* c) {
        const int idx = m_chips.indexOf(c);
        if (idx >= 0) emit saveSessionRequested(idx);
    });

    connect(chip, &SubSurfaceTabChip::loadSessionRequested,
            this, [this](SubSurfaceTabChip* c) {
        const int idx = m_chips.indexOf(c);
        if (idx >= 0) emit loadSessionRequested(idx);
    });

    connect(chip, &SubSurfaceTabChip::renameRequested,
            this, [this](SubSurfaceTabChip* c) {
        const int idx = m_chips.indexOf(c);
        if (idx < 0) return;
        bool ok = false;
        const QString newName = QInputDialog::getText(this, "Rename Sub-Surface",
            "Name:", QLineEdit::Normal, c->name(), &ok);
        if (ok && !newName.isEmpty()) {
            c->setName(newName);
            emit tabRenamed(idx, newName);
        }
    });

    connect(chip, &SubSurfaceTabChip::colorRequested,
            this, [this](SubSurfaceTabChip* c) {
        const int idx = m_chips.indexOf(c);
        if (idx < 0) return;
        const QColor col = QColorDialog::getColor(c->color(), this, "Sub-Surface Color");
        if (col.isValid()) {
            c->setColor(col);
            emit tabColorChanged(idx, col);
        }
    });

    connect(chip, &SubSurfaceTabChip::closeRequested,
            this, [this](SubSurfaceTabChip* c) {
        const int idx = m_chips.indexOf(c);
        if (idx >= 0 && m_chips.size() > 1)
            removeSubSurface(idx);
    });

    // Insert before the stretch at the end
    const int insertPos = qMax(0, m_chipLayout->count() - 1);
    m_chipLayout->insertWidget(insertPos, chip);
    m_chips.append(chip);

    const int newIdx = m_chips.size() - 1;
    setCurrentIndex(newIdx);
    emit subSurfaceAdded(newIdx, name, color);
    return newIdx;
}

void SubSurfaceTabBar::removeSubSurface(int index) {
    if (index < 0 || index >= m_chips.size() || m_chips.size() <= 1) return;

    auto* chip = m_chips.takeAt(index);
    chip->setParent(nullptr);
    chip->deleteLater();

    // Adjust current index
    const int newCurrent = qBound(0, m_currentIndex, m_chips.size() - 1);
    m_currentIndex = -1; // force re-selection
    setCurrentIndex(newCurrent);

    emit subSurfaceRemoved(index);
}

void SubSurfaceTabBar::setCurrentIndex(int index) {
    if (index < 0 || index >= m_chips.size()) return;
    if (index == m_currentIndex) {
        m_chips[index]->setSelected(true);
        return;
    }
    if (m_currentIndex >= 0 && m_currentIndex < m_chips.size())
        m_chips[m_currentIndex]->setSelected(false);
    m_currentIndex = index;
    m_chips[index]->setSelected(true);
    emit currentChanged(index);
}

SubSurfaceTabChip* SubSurfaceTabBar::chip(int i) const {
    return (i >= 0 && i < m_chips.size()) ? m_chips[i] : nullptr;
}

SubSurfaceTabChip* SubSurfaceTabBar::currentChip() const {
    return chip(m_currentIndex);
}

void SubSurfaceTabBar::setTabName(int index, const QString& name) {
    if (auto* c = chip(index)) c->setName(name);
}

void SubSurfaceTabBar::setTabColor(int index, const QColor& color) {
    if (auto* c = chip(index)) c->setColor(color);
}

void SubSurfaceTabBar::setTabStatusMode(int index, OnAirButton::StatusMode mode) {
    if (auto* c = chip(index)) c->setStatusMode(mode);
}

void SubSurfaceTabBar::onChipClicked(SubSurfaceTabChip* chip) {
    const int idx = m_chips.indexOf(chip);
    if (idx >= 0) setCurrentIndex(idx);
}

void SubSurfaceTabBar::onAddClicked() {
    // Pick next color from palette, cycling
    const QColor col = kSubColors.value(m_chips.size() % kSubColors.size(),
                                         QColor("#0ea5e9"));
    const QString name = QString("Sub-Surface %1").arg(m_chips.size() + 1);
    addSubSurface(name, col);
}

void SubSurfaceTabBar::onSessionMenuClicked() {
    if (!m_menuBtn) return;
    QMenu menu(this);

    menu.addAction("New Sub-Surface...", this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, "New Sub-Surface",
            "Name:", QLineEdit::Normal,
            QString("Sub-Surface %1").arg(m_chips.size() + 1), &ok);
        if (!ok || name.isEmpty()) return;
        const QColor col = kSubColors.value(m_chips.size() % kSubColors.size(),
                                             QColor("#0ea5e9"));
        addSubSurface(name, col);
    });

    menu.addSeparator();

    // Quick-switch to any tab by name
    menu.addSection("Switch to");
    for (int i = 0; i < m_chips.size(); ++i) {
        auto* act = menu.addAction(m_chips[i]->name(), this, [this, i]() {
            setCurrentIndex(i);
        });
        act->setCheckable(true);
        act->setChecked(i == m_currentIndex);
    }

    menu.exec(m_menuBtn->mapToGlobal(QPoint(0, m_menuBtn->height())));
}

void SubSurfaceTabBar::setRightWidget(QWidget* w) {
    if (!m_rightSlot || !w) return;
    auto* layout = qobject_cast<QHBoxLayout*>(m_rightSlot->layout());
    if (!layout) return;
    // Clear any existing right widget
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->setParent(nullptr);
        delete item;
    }
    w->setParent(m_rightSlot);
    layout->addWidget(w);
}
