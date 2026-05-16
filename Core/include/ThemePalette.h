#pragma once
#include <QColor>

class ThemePalette {
public:
    // ── Backgrounds ──────────────────────────────────────────
    QColor bg;              // main window background
    QColor panelBg;         // elevated panel / card surface
    QColor cardBg;          // inset card / group box
    QColor inputBg;         // text input / edit field

    // ── Text ─────────────────────────────────────────────────
    QColor text;            // primary readable text
    QColor textMuted;       // secondary / dimmed text
    QColor textDisabled;    // disabled / greyed-out text

    // ── Borders ──────────────────────────────────────────────
    QColor border;          // standard panel border
    QColor borderAccent;    // focus / active border

    // ── Accent ───────────────────────────────────────────────
    QColor accent;          // primary action / brand color
    QColor accentHover;     // hover tint of accent

    // ── Semantic status ──────────────────────────────────────
    QColor success;         // green — OK, connected, on-air OK
    QColor warning;         // amber — caution, cueing, -3 dBFS
    QColor error;           // red  — clip, error, on-air, recording
    QColor info;            // blue — informational

    // ── Deck identity ────────────────────────────────────────
    QColor deckA;           // Deck A accent
    QColor deckB;           // Deck B accent

    // ── VU / metering ────────────────────────────────────────
    QColor vuGreen;         // normal level
    QColor vuYellow;        // caution level
    QColor vuRed;           // clip / peak

    // ── Helpers ──────────────────────────────────────────────

    /// Returns the palette for the currently active ThemeManager theme.
    /// Widgets should call this on construction and on ThemeManager::themeChanged.
    static ThemePalette forCurrentTheme();

    /// Convenience: is the current theme a "light background" theme?
    /// Use this for paint code that needs fundamentally different rendering
    /// (e.g., dark text on light vs light text on dark).
    static bool isLightTheme();
};
