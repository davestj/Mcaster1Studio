#pragma once
/// @file   AnnounceCasterModule.h
/// @path   Modules/AnnounceCasterModule/AnnounceCasterModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-AnnounceCaster — Announcements and Lower Third Display Module
/// @purpose Manages announcement slides, pre/post-service loops, lower third
///          overlays, QR code generation, countdown timer displays, and
///          transition slides (welcome, offering, closing).
/// @reason  Handles all non-worship, non-sermon visual content for the Church
///          Surface — essential for pre-service engagement and service flow.
/// @changelog
///   2026-03-09  Initial implementation — slides, loops, lower thirds, countdown

#include "IModule.h"
#include "IPlugin.h"
#include <QList>
#include <QDateTime>

class QTimer;

namespace M1 {

class GraphicsEngineModule;

// ─── Announcement slide types ───────────────────────────────────────────────
enum class SlideType {
    Announcement,   ///< General announcement (text + optional image)
    Welcome,        ///< Welcome slide
    Offering,       ///< Offering/giving slide with QR code
    Closing,        ///< Closing slide
    LowerThird,     ///< Speaker identification, social handles
    Countdown,      ///< Pre-service countdown timer
    Custom          ///< User-defined slide
};

inline QString slideTypeName(SlideType t) {
    switch (t) {
        case SlideType::Announcement: return "Announcement";
        case SlideType::Welcome:      return "Welcome";
        case SlideType::Offering:     return "Offering";
        case SlideType::Closing:      return "Closing";
        case SlideType::LowerThird:   return "Lower Third";
        case SlideType::Countdown:    return "Countdown";
        case SlideType::Custom:       return "Custom";
        default:                      return "Unknown";
    }
}

// ─── AnnouncementSlide — one slide in the announcement system ───────────────
struct AnnouncementSlide {
    int       id = 0;
    SlideType type          = SlideType::Announcement;
    QString   title;                    ///< Slide title/heading
    QString   body;                     ///< Main text content
    QString   imagePath;                ///< Optional image path
    QString   lowerThirdLine1;          ///< For lower third: name
    QString   lowerThirdLine2;          ///< For lower third: title/role
    QString   qrCodeUrl;                ///< URL for QR code generation
    int       displayDurationSec = 8;   ///< How long to show in loop mode
    QDate     startDate;                ///< Start showing on this date
    QDate     endDate;                  ///< Stop showing after this date
    bool      enabled = true;           ///< Active flag
};

// ─── AnnounceCasterModule ───────────────────────────────────────────────────
class AnnounceCasterModule : public IModule {
    Q_OBJECT

public:
    explicit AnnounceCasterModule(QObject* parent = nullptr);
    ~AnnounceCasterModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.announce"; }
    QString displayName() const override { return "Announce Caster"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {560, 440}; }
    QSize minimumModuleSize() const override { return {380, 280}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Graphics engine binding ─────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }

    // ── Slide management ────────────────────────────────────────────────────
    int addSlide(const AnnouncementSlide& slide);
    void removeSlide(int id);
    void updateSlide(const AnnouncementSlide& slide);
    const AnnouncementSlide* slide(int id) const;
    QList<AnnouncementSlide> allSlides() const { return m_slides.values(); }
    QList<AnnouncementSlide> activeSlides() const;

    // ── Live control ────────────────────────────────────────────────────────
    void showSlide(int id);
    void nextSlide();
    void prevSlide();
    void setBlank(bool blank);
    bool isBlank() const { return m_blank; }

    // ── Loop mode ───────────────────────────────────────────────────────────
    void startLoop();
    void stopLoop();
    bool isLooping() const { return m_looping; }

    // ── Lower third ─────────────────────────────────────────────────────────
    void showLowerThird(const QString& name, const QString& title = {});
    void hideLowerThird();
    bool isLowerThirdVisible() const { return m_lowerThirdVisible; }

    int currentSlideId() const { return m_currentSlideId; }
    QImage currentFrame() const { return m_currentFrame; }

signals:
    void slideChanged(int slideId);
    void blankChanged(bool blank);
    void loopStateChanged(bool looping);
    void lowerThirdStateChanged(bool visible);
    void slidesChanged();
    void frameUpdated(const QImage& frame);

private slots:
    void onLoopAdvance();

private:
    void renderCurrentSlide();

    GraphicsEngineModule*             m_graphics = nullptr;
    QMap<int, AnnouncementSlide>      m_slides;
    int                               m_nextSlideId = 1;

    // Live state
    int     m_currentSlideId     = -1;
    int     m_loopIndex          = 0;
    bool    m_blank              = false;
    bool    m_looping            = false;
    bool    m_lowerThirdVisible  = false;
    QString m_ltName, m_ltTitle;
    QImage  m_currentFrame;
    QTimer* m_loopTimer          = nullptr;

    QList<int> m_activeSlideIds;   ///< Cached list of active slide IDs for loop
};

} // namespace M1
