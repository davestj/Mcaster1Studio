#pragma once
/// @file   ScriptureCasterModule.h
/// @path   Modules/ScriptureCasterModule/ScriptureCasterModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-ScriptureCaster — Bible Verse and Sermon Notes Display Module
/// @purpose Manages Bible verse lookup, sermon outline points, and live display
///          for the congregation during the sermon segment. We support multiple
///          translations, passage ranges, dual-translation display, and a sermon
///          queue for pre-loaded content.
/// @reason  Second most-used Church Surface module — scripture display is critical
///          during every sermon segment.
/// @changelog
///   2026-03-09  Initial implementation — verse lookup, sermon queue, outline points

#include "IModule.h"
#include "IPlugin.h"
#include <QList>
#include <QMap>

namespace M1 {

class GraphicsEngineModule;

// ─── ScriptureReference — a parsed Bible reference ──────────────────────────
struct ScriptureReference {
    QString book;
    int     chapter     = 0;
    int     verseStart  = 0;
    int     verseEnd    = 0;   ///< 0 = single verse, >0 = range
    QString translation = "KJV";

    QString formatted() const {
        QString ref = QString("%1 %2:%3").arg(book).arg(chapter).arg(verseStart);
        if (verseEnd > verseStart) ref += "-" + QString::number(verseEnd);
        if (!translation.isEmpty()) ref += " " + translation;
        return ref;
    }
};

// ─── SermonQueueItem — one item in the sermon presentation queue ────────────
struct SermonQueueItem {
    enum class Type { Scripture, SermonPoint, Blank };

    Type    type = Type::Scripture;
    QString text;                    ///< Verse text or outline point text
    QString reference;               ///< "John 3:16 NIV" or outline number "I."
    QString secondaryText;           ///< Second translation text (for dual view)
    QString secondaryReference;      ///< Second translation reference
};

// ─── ScriptureCasterModule ──────────────────────────────────────────────────
class ScriptureCasterModule : public IModule {
    Q_OBJECT

public:
    explicit ScriptureCasterModule(QObject* parent = nullptr);
    ~ScriptureCasterModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.scripture"; }
    QString displayName() const override { return "Scripture Caster"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {580, 460}; }
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

    // ── Sermon queue API ────────────────────────────────────────────────────
    void addScripture(const QString& text, const QString& reference,
                      const QString& translation = "KJV");
    void addScriptureDual(const QString& text1, const QString& ref1,
                          const QString& trans1,
                          const QString& text2, const QString& ref2,
                          const QString& trans2);
    void addSermonPoint(const QString& point, const QString& number = {});
    void addBlank();
    void removeQueueItem(int index);
    void clearQueue();
    void moveQueueItem(int from, int to);

    int queueSize() const { return m_queue.size(); }
    const SermonQueueItem* queueItem(int index) const;

    // ── Live presentation API ───────────────────────────────────────────────
    void nextItem();
    void prevItem();
    void goToItem(int index);
    void setBlank(bool blank);
    bool isBlank() const { return m_blank; }

    int currentIndex() const { return m_currentIndex; }
    const SermonQueueItem* currentItem() const;
    QImage currentFrame() const { return m_currentFrame; }

    // ── Quick Bible lookup ──────────────────────────────────────────────────
    /// We parse a reference string like "John 3:16" or "Gen 1:1-3" and return
    /// the verse text. For v1, we use a built-in map of commonly used verses.
    /// A full Bible database can be loaded from a file in a future version.
    QString lookupVerse(const QString& book, int chapter, int verse,
                        const QString& translation = "KJV") const;

    /// We parse a reference string and return formatted verse text
    QString lookupReference(const QString& refStr,
                            const QString& translation = "KJV") const;

    /// Get list of supported translations
    QStringList availableTranslations() const;

    /// Get list of Bible books
    QStringList bibleBooks() const;

signals:
    void queueChanged();
    void currentItemChanged(int index);
    void blankChanged(bool blank);
    void frameUpdated(const QImage& frame);

private:
    void renderCurrentItem();
    void initBibleData();

    GraphicsEngineModule*    m_graphics = nullptr;
    QList<SermonQueueItem>   m_queue;
    int                      m_currentIndex = -1;
    bool                     m_blank = false;
    QImage                   m_currentFrame;

    /// We store a sample set of commonly used Bible verses for quick access.
    /// In production, this would be a full SQLite Bible database.
    QMap<QString, QString>   m_verseCache;   ///< "KJV:John:3:16" → text
    QStringList              m_bibleBooks;
    QStringList              m_translations;
};

} // namespace M1
