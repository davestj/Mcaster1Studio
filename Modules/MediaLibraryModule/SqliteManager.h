#pragma once
#include "IDatabase.h"
#include <QPair>
#include <QVariantMap>

struct sqlite3;       // forward declare — keeps sqlite3.h out of header
struct sqlite3_stmt;  // forward declare for readMediaItemRow parameter

namespace M1 {

/// SqliteManager — embedded SQLite3 backend for the media library.
///
/// Zero-config: creates the database file automatically on first connect.
/// Default location: <appDir>/data/mcaster1studio.db  (portable, self-contained)
class SqliteManager : public IDatabase {
public:
    SqliteManager();
    ~SqliteManager() override;

    /// Open (or create) the database at the given file path.
    /// Pass an empty string to use the default location.
    bool    connect(const QString& dbPath = {});

    // IDatabase interface
    bool    isConnected() const override { return m_db != nullptr; }
    void    disconnect()        override;
    bool    createSchema()      override;

    bool    insertItem(MediaItem& item)        override;
    bool    updateItem(const MediaItem& item)   override;
    bool    deleteItem(qint64 id)               override;
    QList<MediaItem> loadAll()                  override;
    bool    pathExists(const QString& path)     override;

    QList<QVariantList> executeQuery(const QString& sql) override;
    QStringList         tableNames() override;

    QString lastError()    const override { return m_lastError; }
    QString backendName()  const override { return QStringLiteral("SQLite"); }
    QString databasePath() const { return m_dbPath; }

    // ─── FTS5 full-text search ──────────────────────────────────────────────
    QList<MediaItem> search(const QString& query, int limit = 200);

    /// FTS5 search restricted to a single category.  Returns tracks that match
    /// both the full-text query AND belong to the given category.
    QList<MediaItem> searchInCategory(const QString& query, qint64 categoryId,
                                      int limit = 200);

    /// Total number of media items in the library (fast COUNT(*)).
    int totalTrackCount();

    // ─── Category CRUD ──────────────────────────────────────────────────────
    qint64 addCategory(const QString& name, const QString& type,
                       const QString& color, qint64 parentId = 0);
    bool   removeCategory(qint64 id);
    bool   renameCategory(qint64 id, const QString& name);
    bool   setCategoryColor(qint64 id, const QString& color);
    QList<QVariantMap> allCategories();
    void   assignTrackToCategory(qint64 trackId, qint64 categoryId);
    void   unassignTrackFromCategory(qint64 trackId, qint64 categoryId);
    QList<qint64> categoriesForTrack(qint64 trackId);
    QList<MediaItem> tracksByCategory(qint64 categoryId);

    // ─── Playlist CRUD ──────────────────────────────────────────────────────
    qint64 createPlaylist(const QString& name, const QString& description = "");
    bool   deletePlaylist(qint64 id);
    bool   renamePlaylist(qint64 id, const QString& name);
    QList<QPair<qint64, QString>> allPlaylists();
    void   addTrackToPlaylist(qint64 playlistId, qint64 trackId, int position = -1);
    void   removeTrackFromPlaylist(qint64 playlistId, int position);
    QList<MediaItem> playlistTracks(qint64 playlistId);

    // ─── Artist Intel CRUD ──────────────────────────────────────────────────
    qint64 saveArtistIntel(const QString& artistName, const QString& profileText,
                           const QString& discographyJson, const QString& aiBackend,
                           const QString& aiModel);
    QList<QVariantMap> artistIntelProfiles(const QString& artistName);
    bool   deleteArtistIntel(qint64 id);
    QStringList artistsWithIntel();

    // ─── Stream Favorites CRUD ──────────────────────────────────────────────
    qint64 addStreamFavorite(const QString& url, const QString& name,
                             const QString& genre = "", int bitrate = 0);
    bool   removeStreamFavorite(qint64 id);
    QList<QVariantMap> allStreamFavorites();
    void   incrementStreamPlayCount(qint64 id);

    // ─── Persona CRUD ──────────────────────────────────────────────────────
    qint64 addPersona(const QString& name, const QString& description,
                       const QString& systemPrompt, const QString& color,
                       const QString& roleType, bool isPreset = false);
    bool   removePersona(qint64 id);
    bool   updatePersona(qint64 id, const QString& name, const QString& description,
                          const QString& systemPrompt, const QString& color,
                          const QString& roleType);
    QList<QVariantMap> allPersonas();
    QVariantMap personaById(qint64 id);
    QString personaPrompt(qint64 id);

    // ─── Category Persona Assignment ─────────────────────────────────────────
    void   setCategoryPersona(qint64 categoryId, qint64 personaId);
    qint64 categoryPersona(qint64 categoryId);

    // ─── Daypart Schedule ────────────────────────────────────────────────────
    qint64 addDaypartEntry(qint64 personaId, qint64 categoryId, int startHour,
                            int endHour, const QString& dow = "*");
    void   removeDaypartEntry(qint64 id);
    QList<QVariantMap> allDaypartEntries();
    qint64 activeDaypartPersona(int currentHour, const QString& currentDow);

    // ─── AutoDJ ─────────────────────────────────────────────────────────────
    void   setAutoDjWeight(qint64 trackId, int weight);
    void   incrementAutoDjSkipCount(qint64 trackId);
    MediaItem nextAutoDjTrack(qint64 excludeId = -1);

    // ─── Play count / rating ────────────────────────────────────────────────
    void   incrementPlayCount(qint64 trackId);
    void   setRating(qint64 trackId, int rating);

private:
    sqlite3* m_db = nullptr;
    QString  m_dbPath;
    QString  m_lastError;

    bool    execSql(const char* sql);

    /// Helper: populate a MediaItem from a SELECT that includes the standard column set.
    /// The statement must have columns in the same order as loadAll().
    MediaItem readMediaItemRow(struct sqlite3_stmt* stmt);
};

} // namespace M1
