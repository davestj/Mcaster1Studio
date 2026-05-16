#include "EffectsRackWidget.h"
#include "EffectsRackModule.h"
#include "IEffectUnit.h"
#include "SonicMaximizer.h"
#include "Equalizer31Band.h"
#include "CompressorLimiter.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QSizePolicy>
#include <QMenu>
#include <QToolButton>

// ---------------------------------------------------------------------------
// Theme-aware colour palette
// ---------------------------------------------------------------------------
namespace {
struct FxColors {
    QString bg, headerBg, border, text, textDim, accent, slotBg, slotBorder;
    QString btnBg, btnHover, btnActive;
    QString ledOn, ledOff, danger;
};
FxColors fxColors() {
    using T = ThemeManager::Theme;
    switch (ThemeManager::instance()->currentTheme()) {
    case T::Classic:
        return { "#ede0cc", "#d8ccb0", "#bfb090", "#2a1810", "#7a6850",
                 "#8b3a0a", "#f0e8d8", "#bfb090",
                 "#ddd0b8", "#ccc0a0", "#8b3a0a",
                 "#22c55e", "#7a6850", "#dc2626" };
    default: { // EnterprisePro (via ThemePalette)
        auto p = ThemePalette::forCurrentTheme();
        return { p.bg.name(), p.cardBg.name(), p.border.name(),
                 p.text.name(), p.textMuted.name(),
                 p.accent.name(), p.panelBg.name(), p.border.name(),
                 p.cardBg.name(), p.cardBg.darker(105).name(), p.accent.name(),
                 p.success.name(), p.textMuted.name(), p.error.name() };
    }
    }
}
} // namespace

// ---------------------------------------------------------------------------
// EffectsRackWidget
// ---------------------------------------------------------------------------

EffectsRackWidget::EffectsRackWidget(M1::EffectsRackModule* module,
                                      QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    setObjectName("EffectsRackWidget");
    setMinimumWidth(360);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // --- Toolbar ---
    buildToolbar();
    outerLayout->addWidget(m_toolbar);

    // --- Scroll area wrapping the slot container ---
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFrameShape(QFrame::NoFrame);

    m_container = new QWidget();
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(6, 6, 6, 6);
    m_layout->setSpacing(6);
    m_layout->addStretch(1);

    m_scroll->setWidget(m_container);
    outerLayout->addWidget(m_scroll, 1);

    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); rebuildSlots(); });

    rebuildSlots();
}

// ---------------------------------------------------------------------------
// buildToolbar — header bar with Add Effect, Bypass All, Reset All
// ---------------------------------------------------------------------------
void EffectsRackWidget::buildToolbar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName("FxRackToolbar");
    m_toolbar->setFixedHeight(44);

    auto* tbLayout = new QHBoxLayout(m_toolbar);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(6);

    // Title
    auto* titleLbl = new QLabel("Effects Rack", m_toolbar);
    titleLbl->setObjectName("FxRackTitle");
    tbLayout->addWidget(titleLbl);
    tbLayout->addStretch(1);

    // Add Effect button with dropdown menu
    m_addBtn = new QPushButton("+ Add Effect", m_toolbar);
    m_addBtn->setObjectName("FxAddBtn");
    m_addBtn->setFixedHeight(30);
    m_addBtn->setToolTip("Add an effect unit to the rack");
    m_addBtn->setCursor(Qt::PointingHandCursor);

    m_addMenu = new QMenu(m_addBtn);
    m_addMenu->setObjectName("FxAddMenu");
    populateAddMenu();
    m_addBtn->setMenu(m_addMenu);
    tbLayout->addWidget(m_addBtn);

    // Bypass All toggle
    m_bypassAllBtn = new QPushButton("Bypass All", m_toolbar);
    m_bypassAllBtn->setObjectName("FxBypassAllBtn");
    m_bypassAllBtn->setCheckable(true);
    m_bypassAllBtn->setChecked(false);
    m_bypassAllBtn->setFixedHeight(30);
    m_bypassAllBtn->setToolTip("Bypass all effects in the rack");
    m_bypassAllBtn->setCursor(Qt::PointingHandCursor);
    connect(m_bypassAllBtn, &QPushButton::toggled, this, [this](bool checked) {
        for (int i = 0; i < m_module->unitCount(); ++i) {
            if (auto* u = m_module->unit(i))
                u->setBypassed(checked);
        }
        m_bypassAllBtn->setText(checked ? "All Bypassed" : "Bypass All");
        rebuildSlots();
    });
    tbLayout->addWidget(m_bypassAllBtn);

    // Reset All button
    auto* resetBtn = new QPushButton("Reset All", m_toolbar);
    resetBtn->setObjectName("FxResetBtn");
    resetBtn->setFixedHeight(30);
    resetBtn->setToolTip("Reset all effect parameters to defaults");
    resetBtn->setCursor(Qt::PointingHandCursor);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < m_module->unitCount(); ++i) {
            if (auto* u = m_module->unit(i))
                u->reset();
        }
        rebuildSlots();
    });
    tbLayout->addWidget(resetBtn);
}

// ---------------------------------------------------------------------------
// populateAddMenu
// ---------------------------------------------------------------------------
void EffectsRackWidget::populateAddMenu()
{
    m_addMenu->clear();

    auto addAction = [this](const QString& name, auto factory) {
        m_addMenu->addAction(name, this, [this, factory]() {
            if (m_module->unitCount() >= M1::EffectsRackModule::kMaxSlots)
                return;
            auto* unit = factory();
            unit->initialize(48000.0, 512);
            addUnit(unit);
        });
    };

    addAction("BBE Sonic Maximizer", []() -> M1::IEffectUnit* { return new M1::SonicMaximizer(); });
    addAction("31-Band Graphic EQ",  []() -> M1::IEffectUnit* { return new M1::Equalizer31Band(); });
    addAction("Compressor / Limiter", []() -> M1::IEffectUnit* { return new M1::CompressorLimiter(); });
}

// ---------------------------------------------------------------------------
// addUnit
// ---------------------------------------------------------------------------
void EffectsRackWidget::addUnit(M1::IEffectUnit* unit)
{
    if (!unit)
        return;
    m_module->addUnit(unit);
    rebuildSlots();
}

// ---------------------------------------------------------------------------
// refresh / removeUnit
// ---------------------------------------------------------------------------
void EffectsRackWidget::refresh()
{
    rebuildSlots();
}

void EffectsRackWidget::removeUnit(int slotIndex)
{
    m_module->removeUnit(slotIndex);
    rebuildSlots();
}

// ---------------------------------------------------------------------------
// applyTheme
// ---------------------------------------------------------------------------
void EffectsRackWidget::applyTheme()
{
    const auto c = fxColors();

    // Main widget background
    setStyleSheet(QString("QWidget#EffectsRackWidget { background: %1; }").arg(c.bg));

    // Toolbar
    if (m_toolbar) {
        m_toolbar->setStyleSheet(QString(
            "QWidget#FxRackToolbar { background: %1; border-bottom: 1px solid %2; }"
        ).arg(c.headerBg).arg(c.border));
    }

    // Title label (find by object name)
    if (auto* title = m_toolbar ? m_toolbar->findChild<QLabel*>("FxRackTitle") : nullptr) {
        title->setStyleSheet(QString(
            "QLabel#FxRackTitle { color: %1; font: bold 11pt; background: transparent; }"
        ).arg(c.accent));
    }

    // Button styles
    const QString btnStyle = QString(
        "QPushButton { background: %1; color: %2; font: bold 9pt; "
        "border: 1px solid %3; border-radius: 4px; padding: 4px 12px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton::menu-indicator { subcontrol-position: right center; width: 12px; }"
    ).arg(c.btnBg).arg(c.text).arg(c.border).arg(c.btnHover);

    if (m_addBtn) m_addBtn->setStyleSheet(btnStyle);
    if (m_bypassAllBtn) {
        auto tp = ThemePalette::forCurrentTheme();
        m_bypassAllBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; font: bold 9pt; "
            "border: 1px solid %3; border-radius: 4px; padding: 4px 12px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:checked { background: %5; color: %6; border-color: %7; }"
        ).arg(c.btnBg, c.text, c.border, c.btnHover,
              tp.warning.name(), tp.text.name(), tp.warning.darker(130).name()));
    }

    // Reset button
    if (auto* resetBtn = m_toolbar ? m_toolbar->findChild<QPushButton*>("FxResetBtn") : nullptr) {
        resetBtn->setStyleSheet(btnStyle);
    }

    // Menu style
    if (m_addMenu) {
        m_addMenu->setStyleSheet(QString(
            "QMenu { background: %1; border: 1px solid %2; padding: 4px; }"
            "QMenu::item { color: %3; font: 10pt; padding: 6px 16px; }"
            "QMenu::item:selected { background: %4; color: white; border-radius: 3px; }"
        ).arg(c.slotBg).arg(c.border).arg(c.text).arg(c.accent));
    }

    // Scroll area
    if (m_scroll) {
        m_scroll->setStyleSheet(QString(
            "QScrollArea { background: %1; border: none; }"
            "QScrollBar:vertical { background: %2; width: 10px; border-radius: 5px; margin: 2px; }"
            "QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 30px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        ).arg(c.bg).arg(c.bg).arg(c.border));
    }
    if (m_container) {
        m_container->setStyleSheet(QString("background: %1;").arg(c.bg));
    }
}

// ---------------------------------------------------------------------------
// rebuildSlots — tear down and reconstruct every slot card.
// ---------------------------------------------------------------------------
void EffectsRackWidget::rebuildSlots()
{
    // Remove all items from layout (except the trailing stretch).
    while (m_layout->count() > 1) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const auto c = fxColors();
    const int loaded   = m_module->unitCount();
    const int maxSlots = M1::EffectsRackModule::kMaxSlots;

    // ── Build one card per loaded unit ──────────────────────────────────────
    for (int i = 0; i < loaded; ++i) {
        M1::IEffectUnit* u = m_module->unit(i);

        // Outer card frame
        auto* card = new QFrame();
        card->setFrameShape(QFrame::NoFrame);
        card->setStyleSheet(QString(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }"
        ).arg(c.slotBg).arg(c.slotBorder));

        auto* cardV = new QVBoxLayout(card);
        cardV->setContentsMargins(0, 0, 0, 0);
        cardV->setSpacing(0);

        // ── Header strip ──────────────────────────────────────────────────
        auto* header = new QWidget(card);
        header->setStyleSheet(QString(
            "QWidget { background: %1; border-top-left-radius: 6px; "
            "border-top-right-radius: 6px; border-bottom: 1px solid %2; }"
        ).arg(c.headerBg).arg(c.slotBorder));
        header->setFixedHeight(36);

        auto* headerH = new QHBoxLayout(header);
        headerH->setContentsMargins(10, 0, 6, 0);
        headerH->setSpacing(8);

        // Slot number badge
        auto* slotBadge = new QLabel(QString::number(i + 1), header);
        slotBadge->setFixedSize(24, 24);
        slotBadge->setAlignment(Qt::AlignCenter);
        slotBadge->setStyleSheet(QString(
            "QLabel { color: %1; font: bold 10pt; background: %2; "
            "border: 1px solid %3; border-radius: 12px; }"
        ).arg(c.text).arg(c.btnBg).arg(c.border));
        headerH->addWidget(slotBadge);

        // Effect name
        auto* nameLbl = new QLabel(u->displayName(), header);
        nameLbl->setStyleSheet(QString(
            "QLabel { color: %1; font: bold 10pt; background: transparent; border: none; }"
        ).arg(c.text));
        headerH->addWidget(nameLbl);
        headerH->addStretch(1);

        // Active/Bypass LED indicator
        const bool bypassed = u->isBypassed();
        auto* ledLbl = new QLabel(header);
        ledLbl->setFixedSize(14, 14);
        auto tp = ThemePalette::forCurrentTheme();
        ledLbl->setStyleSheet(QString(
            "QLabel { background: %1; border: 1px solid %2; border-radius: 7px; }"
        ).arg(bypassed ? c.ledOff : c.ledOn)
         .arg(bypassed ? c.border : tp.success.name()));
        ledLbl->setToolTip(bypassed ? "Bypassed" : "Active");
        headerH->addWidget(ledLbl);

        // Bypass toggle button
        auto* bypassBtn = new QPushButton(bypassed ? "BYPASSED" : "ACTIVE", header);
        bypassBtn->setCheckable(true);
        bypassBtn->setChecked(bypassed);
        bypassBtn->setFixedHeight(26);
        bypassBtn->setCursor(Qt::PointingHandCursor);
        bypassBtn->setToolTip("Toggle bypass for this effect");
        bypassBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; font: bold 9pt; "
            "border: 1px solid %3; border-radius: 4px; padding: 2px 10px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:checked { background: %5; color: %6; border-color: %7; }"
        ).arg(c.btnBg, c.text, c.border, c.btnHover,
              tp.warning.name(), tp.text.name(), tp.warning.darker(130).name()));

        const QString ledActiveColor = tp.success.name();
        connect(bypassBtn, &QPushButton::toggled, this,
            [u, bypassBtn, ledLbl, c, ledActiveColor](bool checked) {
                u->setBypassed(checked);
                bypassBtn->setText(checked ? "BYPASSED" : "ACTIVE");
                ledLbl->setStyleSheet(QString(
                    "QLabel { background: %1; border: 1px solid %2; border-radius: 7px; }"
                ).arg(checked ? c.ledOff : c.ledOn)
                 .arg(checked ? c.border : ledActiveColor));
                ledLbl->setToolTip(checked ? "Bypassed" : "Active");
            });
        headerH->addWidget(bypassBtn);

        // Remove button
        auto* removeBtn = new QPushButton("X", header);
        removeBtn->setFixedSize(26, 26);
        removeBtn->setToolTip("Remove this effect from the rack");
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setStyleSheet(QString(
            "QPushButton { background: %1; color: white; font: bold 10pt; "
            "border: 1px solid %2; border-radius: 4px; }"
            "QPushButton:hover { background: %3; border-color: %3; }"
        ).arg(c.danger, c.danger, tp.error.lighter(140).name()));

        const int capturedIndex = i;
        connect(removeBtn, &QPushButton::clicked, this,
            [this, capturedIndex]() { removeUnit(capturedIndex); });
        headerH->addWidget(removeBtn);

        cardV->addWidget(header);

        // ── Effect panel content ──────────────────────────────────────────
        QWidget* panel = u->createPanel(card);
        if (panel) {
            panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            // Override background to match the card bottom
            panel->setStyleSheet(panel->styleSheet() + QString(
                " QWidget { border-bottom-left-radius: 6px; border-bottom-right-radius: 6px; }"
            ));
            cardV->addWidget(panel);
        }

        m_layout->insertWidget(m_layout->count() - 1, card);
    }

    // ── Fill remaining empty slots ──────────────────────────────────────────
    for (int i = loaded; i < maxSlots; ++i) {
        auto* emptyFrame = new QFrame();
        emptyFrame->setFrameShape(QFrame::NoFrame);
        emptyFrame->setFixedHeight(40);
        emptyFrame->setStyleSheet(QString(
            "QFrame { background: transparent; border: 1px dashed %1; border-radius: 6px; }"
        ).arg(c.border));

        auto* emptyLayout = new QHBoxLayout(emptyFrame);
        emptyLayout->setContentsMargins(12, 0, 12, 0);

        auto* idxLabel = new QLabel(QString::number(i + 1));
        idxLabel->setFixedWidth(24);
        idxLabel->setAlignment(Qt::AlignCenter);
        idxLabel->setStyleSheet(QString(
            "QLabel { color: %1; font: bold 10pt; background: transparent; border: none; }"
        ).arg(c.textDim));
        emptyLayout->addWidget(idxLabel);

        auto* emptyLabel = new QLabel("Empty Slot");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QString(
            "QLabel { color: %1; font: italic 9pt; background: transparent; border: none; }"
        ).arg(c.textDim));
        emptyLayout->addWidget(emptyLabel, 1);

        m_layout->insertWidget(m_layout->count() - 1, emptyFrame);
    }
}
