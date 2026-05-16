#include "ThemePalette.h"

// ThemeManager is in UI/ but ThemePalette is in Core/.
// We forward-declare just the enum check we need via a minimal header-only approach:
// ThemeManager is a singleton with a currentTheme() method.
// Rather than creating a circular dependency, we use a function pointer that
// the UI layer registers at startup.

// ── Registration hook (set by MainWindow at startup) ─────────────────────────
// 0 = EnterprisePro, 1 = Classic  (matches ThemeManager::Theme enum order)
static int (*s_currentThemeFn)() = nullptr;

void themePaletteRegisterProvider(int (*fn)()) { s_currentThemeFn = fn; }

static int currentThemeIndex() {
    return s_currentThemeFn ? s_currentThemeFn() : 0; // default EnterprisePro
}

// ── Enterprise Pro palette (flat modern, white/blue/tan) ─────────────────────
static ThemePalette enterpriseProPalette() {
    ThemePalette p;
    // Backgrounds
    p.bg           = QColor("#f5f3f0");
    p.panelBg      = QColor("#ffffff");
    p.cardBg       = QColor("#f0ebe3");
    p.inputBg      = QColor("#ffffff");
    // Text
    p.text         = QColor("#1a1814");
    p.textMuted    = QColor("#6b6560");
    p.textDisabled = QColor("#a8a098");
    // Borders
    p.border       = QColor("#d8d4ce");
    p.borderAccent = QColor("#1c5caa");
    // Accent
    p.accent       = QColor("#1c5caa");
    p.accentHover  = QColor("#dbeafe");
    // Semantic
    p.success      = QColor("#16a34a");
    p.warning      = QColor("#d97706");
    p.error        = QColor("#dc2626");
    p.info         = QColor("#1c5caa");
    // Deck
    p.deckA        = QColor("#1c5caa");
    p.deckB        = QColor("#7c3aed");
    // VU
    p.vuGreen      = QColor("#16a34a");
    p.vuYellow     = QColor("#d97706");
    p.vuRed        = QColor("#dc2626");
    return p;
}

// ── Classic palette (warm mahogany / gold / nickel) ──────────────────────────
static ThemePalette classicPalette() {
    ThemePalette p;
    // Backgrounds
    p.bg           = QColor("#2a1e14");
    p.panelBg      = QColor("#3d2c1e");
    p.cardBg       = QColor("#4a3525");
    p.inputBg      = QColor("#1a100a");
    // Text
    p.text         = QColor("#f0e6d8");
    p.textMuted    = QColor("#a08870");
    p.textDisabled = QColor("#6a5840");
    // Borders
    p.border       = QColor("#5a4030");
    p.borderAccent = QColor("#d4891e");
    // Accent
    p.accent       = QColor("#d4891e");
    p.accentHover  = QColor("#f0a830");
    // Semantic
    p.success      = QColor("#22c55e");
    p.warning      = QColor("#f59e0b");
    p.error        = QColor("#ef4444");
    p.info         = QColor("#d4891e");
    // Deck
    p.deckA        = QColor("#d4891e");
    p.deckB        = QColor("#22c55e");
    // VU
    p.vuGreen      = QColor("#22c55e");
    p.vuYellow     = QColor("#f59e0b");
    p.vuRed        = QColor("#ef4444");
    return p;
}

// ── Public API ───────────────────────────────────────────────────────────────
ThemePalette ThemePalette::forCurrentTheme() {
    switch (currentThemeIndex()) {
    case 1:  return classicPalette();
    default: return enterpriseProPalette();
    }
}

bool ThemePalette::isLightTheme() {
    return currentThemeIndex() == 0; // EnterprisePro is light
}
