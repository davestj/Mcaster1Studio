#pragma once
/// @file   PodShowNotesModule.h
/// @path   Modules/PodShowNotesModule/PodShowNotesModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodShowNotes — Podcast Show Notes Editor Module
/// @purpose Rich-text WYSIWYG show notes editor with timestamp links,
///          chapter markers, guest bios, resource links, templates, and
///          multi-format export (HTML, Markdown, plain text).
/// @reason  Podcast production requires structured show notes for
///          publishing alongside episodes — chapters, links, and guest info.
/// @changelog
///   2026-03-09  Initial implementation — WYSIWYG editor, chapters, templates, export

#include "IModule.h"
#include <QList>
#include <QString>

namespace M1 {

/// Timed chapter marker within an episode's show notes.
struct ChapterMarker {
    qint64  timestampMs = 0;   ///< Position in milliseconds
    QString title;             ///< Chapter display title
};

/// A hyperlink resource referenced in the show notes.
struct ResourceLink {
    QString title;             ///< Display label
    QString url;               ///< Full URL
};

/// PodShowNotesModule — rich-text show notes editor for podcast episodes.
///
/// Provides a WYSIWYG QTextEdit-based editor with:
///   - Bold / italic / underline formatting toolbar
///   - Clickable timestamp link insertion
///   - Template presets (Standard, Interview, Solo Episode)
///   - Chapter marker list with timestamp + title
///   - Guest biography section
///   - Resource link list
///   - Export to HTML, Markdown, or plain text
///   - Live word count display
class PodShowNotesModule : public IModule {
    Q_OBJECT

public:
    explicit PodShowNotesModule(QObject* parent = nullptr);
    ~PodShowNotesModule() override;

    // ── IModule identity ─────────────────────────────────────────────────
    QString moduleId()          const override { return "com.mcaster1.podcast.shownotes"; }
    QString displayName()       const override { return "Show Notes"; }
    QString version()           const override { return "1.0.0"; }
    QSize   preferredSize()     const override { return {500, 400}; }
    QSize   minimumModuleSize() const override { return {350, 300}; }

    // ── IModule lifecycle ────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;

    // ── IModule UI ───────────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;

    // ── IModule state persistence ────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Content API ──────────────────────────────────────────────────────
    void    setContent(const QString& html);
    QString content() const;

    // ── Chapter management ───────────────────────────────────────────────
    void addChapter(qint64 timestampMs, const QString& title);
    void removeChapter(int index);
    QList<ChapterMarker> chapters() const { return m_chapters; }

    // ── Resource link management ─────────────────────────────────────────
    void addResource(const QString& title, const QString& url);
    void removeResource(int index);
    QList<ResourceLink> resources() const { return m_resources; }

    // ── Guest bio ────────────────────────────────────────────────────────
    void setGuestBio(const QString& name, const QString& bio);
    QString guestName() const { return m_guestName; }
    QString guestBio()  const { return m_guestBio; }

    // ── Templates ────────────────────────────────────────────────────────
    void applyTemplate(const QString& templateName);

    // ── Export ────────────────────────────────────────────────────────────
    bool exportHtml(const QString& path);
    bool exportMarkdown(const QString& path);
    bool exportText(const QString& path);

    // ── Word count ───────────────────────────────────────────────────────
    int wordCount() const;

signals:
    void contentChanged();
    void chaptersChanged();
    void resourcesChanged();

private:
    QString              m_html;         ///< Current HTML content
    QList<ChapterMarker> m_chapters;
    QList<ResourceLink>  m_resources;
    QString              m_guestName;
    QString              m_guestBio;
};

} // namespace M1
