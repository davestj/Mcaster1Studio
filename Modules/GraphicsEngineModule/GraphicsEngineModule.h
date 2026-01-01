#pragma once
/// @file   GraphicsEngineModule.h
/// @path   Modules/GraphicsEngineModule/GraphicsEngineModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-GraphicsEngine — Template and Overlay Rendering Backend
/// @purpose Provides visual rendering services for LyricsCaster, ScriptureCaster,
///          AnnounceCaster, and TelePrompt. We handle template management, theme
///          switching, text layout, lower third rendering, and output resolution
///          targeting for projectors, confidence monitors, and stream overlays.
/// @reason  Foundation layer — all visual Church Surface modules depend on this
///          engine for consistent branded output.
/// @changelog
///   2026-03-09  Initial implementation — template system, theme manager, text layout

#include "IModule.h"
#include "IPlugin.h"
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPixmap>
#include <QMap>
#include <QSize>

class QPainter;
class QTimer;

namespace M1 {

// ─── Visual Theme — defines the look of all rendered output ─────────────────
/// We store complete theme definitions so the church can switch between
/// Christmas, Easter, standard Sunday, special events, etc.
struct VisualTheme {
    QString  name;                              ///< "Standard Sunday"
    QColor   backgroundColor = QColor(0, 0, 30);  ///< Dark blue default
    QColor   textColor       = QColor(255, 255, 255);
    QColor   accentColor     = QColor(59, 130, 246);  ///< Blue accent
    QColor   subtitleColor   = QColor(200, 200, 200);
    QString  fontFamily      = "Arial";
    int      titleFontSize   = 48;
    int      bodyFontSize    = 36;
    int      subtitleFontSize = 24;
    int      lowerThirdFontSize = 20;
    QString  logoPath;                          ///< Path to church logo image
    QString  backgroundImagePath;               ///< Path to background image
    QString  backgroundVideoPath;               ///< Path to background video loop
    float    backgroundOpacity = 0.3f;          ///< Overlay opacity for bg image
    int      lowerThirdHeight  = 120;           ///< Pixels from bottom
    QColor   lowerThirdBgColor = QColor(0, 0, 0, 180);
    QColor   lowerThirdTextColor = QColor(255, 255, 255);
    int      transitionMs      = 500;           ///< Default transition duration
    int      textPaddingPx     = 40;            ///< Horizontal text padding
    int      lineSpacingPx     = 8;             ///< Extra line spacing
};

// ─── RenderTarget — output resolution + destination type ────────────────────
/// We support rendering for different output destinations, each potentially
/// at a different resolution.
struct RenderTarget {
    enum class Type { Projector, ConfidenceMonitor, StreamOverlay, Recording };
    Type   type   = Type::Projector;
    QSize  size   = QSize(1920, 1080);
    QString name  = "Main Projector";
};

// ─── TemplateType — what kind of content we are rendering ───────────────────
enum class TemplateType {
    Lyrics,         ///< Worship lyrics (large centered text)
    Scripture,      ///< Bible verse with reference
    ScriptureDual,  ///< Two translations side by side
    SermonPoint,    ///< Outline point with hierarchical number
    Announcement,   ///< Announcement slide
    LowerThird,     ///< Lower third overlay (name, info)
    TitleCard,      ///< Full screen title (service title, etc.)
    Countdown,      ///< Pre-service countdown timer
    Blank,          ///< Black/blank screen
    Custom          ///< User-defined template
};

// ─── RenderRequest — what a module wants rendered ───────────────────────────
/// Client modules (LyricsCaster, ScriptureCaster, etc.) build a RenderRequest
/// and submit it to the GraphicsEngine, which returns a rendered QImage.
struct RenderRequest {
    TemplateType type       = TemplateType::Blank;
    QString      primaryText;                   ///< Main text content
    QString      secondaryText;                 ///< Secondary text (subtitle, translation)
    QString      reference;                     ///< Scripture reference, song title, etc.
    QString      attribution;                   ///< CCLI, author, copyright
    QString      lowerThirdLine1;               ///< Name line for lower third
    QString      lowerThirdLine2;               ///< Title/role line for lower third
    QImage       overlayImage;                  ///< Optional image overlay
    bool         showLogo        = false;       ///< Show church logo
    bool         showLowerThird  = false;       ///< Show lower third bar
    float        opacity         = 1.0f;        ///< Overall opacity (for transitions)
};

// ─── GraphicsEngineModule ────────────────────────────────────────────────────
/// Rendering backend for all visual output in the Church Surface.
///
/// We do NOT display anything directly — we provide rendering services.
/// Our widget is a template management and preview UI. Client modules call
/// renderFrame() to get a rendered QImage they can display or output.
class GraphicsEngineModule : public IModule {
    Q_OBJECT

public:
    explicit GraphicsEngineModule(QObject* parent = nullptr);
    ~GraphicsEngineModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.graphics"; }
    QString displayName() const override { return "Graphics Engine"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {480, 360}; }
    QSize minimumModuleSize() const override { return {340, 260}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Rendering API ───────────────────────────────────────────────────────

    /// Render a frame for the given request and target resolution.
    /// This is the main entry point that client modules call.
    QImage renderFrame(const RenderRequest& req, const QSize& targetSize = QSize(1920, 1080));

    /// Render lyrics text (large centered, multi-line with proper wrapping)
    QImage renderLyrics(const QString& text, const QString& songTitle = {},
                        const QString& ccli = {}, const QSize& size = QSize(1920, 1080));

    /// Render a scripture verse with reference
    QImage renderScripture(const QString& text, const QString& reference,
                           const QString& translation = {},
                           const QSize& size = QSize(1920, 1080));

    /// Render dual scripture (two translations side by side)
    QImage renderScriptureDual(const QString& text1, const QString& ref1,
                               const QString& trans1,
                               const QString& text2, const QString& ref2,
                               const QString& trans2,
                               const QSize& size = QSize(1920, 1080));

    /// Render a sermon outline point
    QImage renderSermonPoint(const QString& point, const QString& number = {},
                             const QSize& size = QSize(1920, 1080));

    /// Render a lower third overlay (returns image with alpha)
    QImage renderLowerThird(const QString& name, const QString& title = {},
                            const QSize& size = QSize(1920, 1080));

    /// Render a title card (full screen with optional logo)
    QImage renderTitleCard(const QString& title, const QString& subtitle = {},
                           const QSize& size = QSize(1920, 1080));

    /// Render a blank/black frame
    QImage renderBlank(const QSize& size = QSize(1920, 1080));

    /// Render a countdown timer display
    QImage renderCountdown(const QString& timeStr, const QString& label = {},
                           const QSize& size = QSize(1920, 1080));

    // ── Theme management ────────────────────────────────────────────────────

    /// Get the current active theme
    const VisualTheme& activeTheme() const { return m_activeTheme; }

    /// Set the active theme by name
    void setActiveTheme(const QString& name);

    /// Add or update a theme
    void addTheme(const VisualTheme& theme);

    /// Remove a theme by name
    void removeTheme(const QString& name);

    /// Get all available theme names
    QStringList themeNames() const;

    // ── Render targets ──────────────────────────────────────────────────────

    /// Add a render target
    void addRenderTarget(const RenderTarget& target);

    /// Get all render targets
    QList<RenderTarget> renderTargets() const { return m_targets; }

signals:
    /// Emitted when the active theme changes
    void themeChanged(const QString& themeName);

    /// Emitted when a new frame is rendered (for preview widgets to update)
    void frameRendered(const QImage& frame);

private:
    /// We paint the background (solid color + optional image overlay)
    void paintBackground(QPainter& p, const QSize& size);

    /// We paint the church logo if enabled
    void paintLogo(QPainter& p, const QSize& size);

    /// We paint the lower third bar overlay
    void paintLowerThird(QPainter& p, const QSize& size,
                         const QString& line1, const QString& line2);

    /// We perform smart text wrapping that respects line length limits
    QStringList wrapText(const QString& text, const QFont& font,
                         int maxWidth) const;

    VisualTheme              m_activeTheme;
    QMap<QString, VisualTheme> m_themes;
    QList<RenderTarget>      m_targets;
    QImage                   m_backgroundImage;   ///< Cached background image
    QImage                   m_logoImage;          ///< Cached logo image
};

} // namespace M1
