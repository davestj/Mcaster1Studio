#include "SqlDialect.h"
#include "DbServerEntry.h"

namespace M1 {

// ─── Default media_items CREATE TABLE using dialect-specific types ────────────

QString SqlDialect::createMediaItemsTableSql(const QString& tablePrefix) const {
    const QString tbl = tablePrefix.isEmpty() ? "media_items" : (tablePrefix + "media_items");
    const QString q   = quoteId(tbl);

    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7,"
        "  %8 %9,"
        "  %10 %11,"
        "  %12 %13,"
        "  %14 %15,"
        "  %16 %17,"
        "  %18 %19,"
        "  %20 %21,"
        "  %22 %23,"
        "  %24 %25,"
        "  %26 %27,"
        "  %28 %29,"
        "  %30 %31,"
        "  %32 %33 DEFAULT 0,"
        "  %34 %35,"
        "  %36 %37 DEFAULT 0,"
        "  %38 %39 DEFAULT 0,"
        "  %40 %41 DEFAULT 44100,"
        "  %42 %43 DEFAULT 2,"
        "  %44 %45,"
        "  %46 %47 DEFAULT 0,"
        "  %48 %49 DEFAULT 0,"
        "  %50 %51 DEFAULT 0,"
        "  %52 %53 DEFAULT 1,"
        "  %54 %55,"
        "  %56 %57,"
        "  %58 %59 DEFAULT 0,"
        "  %60 %61,"
        "  %62 %63,"
        "  %64 %65,"
        "  %66 %67 DEFAULT 0,"
        "  %68 %69,"
        "  %70"
        ") %71")
        .arg(q)
        .arg(quoteId("id"),            autoIncrementPK())
        .arg(quoteId("file_path"),     varcharType(2048))
        .arg(quoteId("title"),         varcharType(512))
        .arg(quoteId("artist"),        varcharType(512))
        .arg(quoteId("album_artist"),  varcharType(512))
        .arg(quoteId("album"),         varcharType(512))
        .arg(quoteId("genre"),         varcharType(256))
        .arg(quoteId("year"),          varcharType(10))
        .arg(quoteId("track_number"),  varcharType(10))
        .arg(quoteId("composer"),      varcharType(512))
        .arg(quoteId("comment"),       textType())
        .arg(quoteId("isrc"),          varcharType(32))
        .arg(quoteId("mbid"),          varcharType(64))
        .arg(quoteId("label"),         varcharType(256))
        .arg(quoteId("language"),      varcharType(64))
        .arg(quoteId("bpm"),           realType())
        .arg(quoteId("musical_key"),   varcharType(16))
        .arg(quoteId("duration_ms"),   intType())
        .arg(quoteId("bitrate"),       intType())
        .arg(quoteId("sample_rate"),   intType())
        .arg(quoteId("channels"),      intType())
        .arg(quoteId("codec"),         varcharType(32))
        .arg(quoteId("file_size"),     bigintType())
        .arg(quoteId("rating"),        intType())
        .arg(quoteId("energy"),        doubleType())
        .arg(quoteId("weight"),        doubleType())
        .arg(quoteId("mood"),          varcharType(256))
        .arg(quoteId("tags"),          textType())
        .arg(quoteId("explicit_content"), boolType())
        .arg(quoteId("date_added"),    datetimeType())
        .arg(quoteId("date_modified"), datetimeType())
        .arg(quoteId("last_played"),   datetimeType())
        .arg(quoteId("play_count"),    intType())
        .arg(quoteId("waveform_cache"), varcharType(2048))
        + QString(","
        "  %1 %2 DEFAULT 50,"
        "  %3 %4 DEFAULT 0,"
        "  %5 %6 DEFAULT 0,"
        "  %7"
        ") %8")
        .arg(quoteId("autodj_weight"),     intType())
        .arg(quoteId("autodj_skip_count"), intType())
        .arg(quoteId("has_art"),           boolType())
        .arg(uniqueFilePathConstraint())
        .arg(tableOptions());
}

// ─── library_categories ─────────────────────────────────────────────────────

QString SqlDialect::createLibraryCategoriesTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "library_categories" : prefix + "library_categories");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 DEFAULT 0,"
        "  %6 %7 NOT NULL,"
        "  %8 %9,"
        "  %10 %11,"
        "  %12 %13 DEFAULT 0,"
        "  %14 %15"
        ") %16")
        .arg(tbl)
        .arg(quoteId("id"),         autoIncrementPK())
        .arg(quoteId("parent_id"),  intType())
        .arg(quoteId("name"),       varcharType(256))
        .arg(quoteId("type"),       varcharType(64))
        .arg(quoteId("color"),      varcharType(16))
        .arg(quoteId("sort_order"), intType())
        .arg(quoteId("created_at"), datetimeType())
        .arg(tableOptions());
}

// ─── track_categories (junction) ─────────────────────────────────────────────

QString SqlDialect::createTrackCategoriesTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "track_categories" : prefix + "track_categories");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3 NOT NULL,"
        "  %4 %5 NOT NULL,"
        "  PRIMARY KEY (%6, %7)"
        ") %8")
        .arg(tbl)
        .arg(quoteId("track_id"),    intType())
        .arg(quoteId("category_id"), intType())
        .arg(quoteId("track_id"),    quoteId("category_id"))
        .arg(tableOptions());
}

// ─── playlists ───────────────────────────────────────────────────────────────

QString SqlDialect::createPlaylistsTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "playlists" : prefix + "playlists");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7,"
        "  %8 %9,"
        "  %10 %11"
        ") %12")
        .arg(tbl)
        .arg(quoteId("id"),          autoIncrementPK())
        .arg(quoteId("name"),        varcharType(256))
        .arg(quoteId("description"), textType())
        .arg(quoteId("created_at"),  datetimeType())
        .arg(quoteId("updated_at"),  datetimeType())
        .arg(tableOptions());
}

// ─── playlist_tracks ─────────────────────────────────────────────────────────

QString SqlDialect::createPlaylistTracksTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "playlist_tracks" : prefix + "playlist_tracks");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7 NOT NULL,"
        "  %8 %9 NOT NULL DEFAULT 0"
        ") %10")
        .arg(tbl)
        .arg(quoteId("id"),          autoIncrementPK())
        .arg(quoteId("playlist_id"), intType())
        .arg(quoteId("track_id"),    intType())
        .arg(quoteId("position"),    intType())
        .arg(tableOptions());
}

// ─── artist_intel ────────────────────────────────────────────────────────────

QString SqlDialect::createArtistIntelTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "artist_intel" : prefix + "artist_intel");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7,"
        "  %8 %9,"
        "  %10 %11,"
        "  %12 %13,"
        "  %14 %15,"
        "  %16 %17 DEFAULT 1"
        ") %18")
        .arg(tbl)
        .arg(quoteId("id"),               autoIncrementPK())
        .arg(quoteId("artist_name"),      varcharType(256))
        .arg(quoteId("profile_text"),     textType())
        .arg(quoteId("discography_json"), textType())
        .arg(quoteId("generated_at"),     datetimeType())
        .arg(quoteId("ai_backend"),       varcharType(64))
        .arg(quoteId("ai_model"),         varcharType(128))
        .arg(quoteId("version"),          intType())
        .arg(tableOptions());
}

// ─── stream_favorites ────────────────────────────────────────────────────────

QString SqlDialect::createStreamFavoritesTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "stream_favorites" : prefix + "stream_favorites");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7,"
        "  %8 %9,"
        "  %10 %11 DEFAULT 0,"
        "  %12 %13,"
        "  %14 %15,"
        "  %16 %17,"
        "  %18 %19 DEFAULT 0,"
        "  %20 %21,"
        "  %22 %23 DEFAULT 0"
        ") %24")
        .arg(tbl)
        .arg(quoteId("id"),           autoIncrementPK())
        .arg(quoteId("url"),          varcharType(2048))
        .arg(quoteId("name"),         varcharType(256))
        .arg(quoteId("genre"),        varcharType(128))
        .arg(quoteId("bitrate_kbps"), intType())
        .arg(quoteId("description"),  textType())
        .arg(quoteId("codec"),        varcharType(32))
        .arg(quoteId("logo_url"),     varcharType(2048))
        .arg(quoteId("play_count"),   intType())
        .arg(quoteId("last_played"),  datetimeType())
        .arg(quoteId("sort_order"),   intType())
        .arg(tableOptions());
}

// ─── ai_personas ────────────────────────────────────────────────────────────

QString SqlDialect::createAiPersonasTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "ai_personas" : prefix + "ai_personas");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7,"
        "  %8 %9 NOT NULL,"
        "  %10 %11 DEFAULT '#1c5caa',"
        "  %12 %13 DEFAULT 'Custom',"
        "  %14 %15 DEFAULT 0,"
        "  %16 %17"
        ") %18")
        .arg(tbl)
        .arg(quoteId("id"),            autoIncrementPK())
        .arg(quoteId("name"),          varcharType(256))
        .arg(quoteId("description"),   textType())
        .arg(quoteId("system_prompt"), textType())
        .arg(quoteId("color"),         varcharType(16))
        .arg(quoteId("role_type"),     varcharType(64))
        .arg(quoteId("is_preset"),     boolType())
        .arg(quoteId("created_at"),    datetimeType())
        .arg(tableOptions());
}

// ─── daypart_schedule ───────────────────────────────────────────────────────

QString SqlDialect::createDaypartScheduleTableSql(const QString& prefix) const {
    const QString tbl = quoteId(prefix.isEmpty() ? "daypart_schedule" : prefix + "daypart_schedule");
    return QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  %2 %3,"
        "  %4 %5 NOT NULL,"
        "  %6 %7 DEFAULT 0,"
        "  %8 %9 NOT NULL,"
        "  %10 %11 NOT NULL,"
        "  %12 %13 DEFAULT '*',"
        "  %14 %15 DEFAULT 0"
        ") %16")
        .arg(tbl)
        .arg(quoteId("id"),          autoIncrementPK())
        .arg(quoteId("persona_id"),  intType())
        .arg(quoteId("category_id"), intType())
        .arg(quoteId("start_hour"),  intType())
        .arg(quoteId("end_hour"),    intType())
        .arg(quoteId("day_of_week"), varcharType(16))
        .arg(quoteId("priority"),    intType())
        .arg(tableOptions());
}

// ─── Full schema — all tables ────────────────────────────────────────────────

QStringList SqlDialect::createFullSchemaSql(const QString& prefix) const {
    return {
        createMediaItemsTableSql(prefix),
        createLibraryCategoriesTableSql(prefix),
        createTrackCategoriesTableSql(prefix),
        createPlaylistsTableSql(prefix),
        createPlaylistTracksTableSql(prefix),
        createArtistIntelTableSql(prefix),
        createStreamFavoritesTableSql(prefix),
        createAiPersonasTableSql(prefix),
        createDaypartScheduleTableSql(prefix)
    };
}

// ─── Dialect factory — returns static singletons ─────────────────────────────

SqlDialect* dialectFor(int backendEnumValue) {
    using B = DbServerEntry::Backend;
    switch (static_cast<B>(backendEnumValue)) {
    case B::SQLite: {
        static SqliteDialect s;
        return &s;
    }
    case B::MySQL: {
        static MySqlDialect s;
        return &s;
    }
    case B::PostgreSQL: {
        static PostgresDialect s;
        return &s;
    }
    case B::Firebird: {
        static FirebirdDialect s;
        return &s;
    }
    case B::MSSQL: {
        static MssqlDialect s;
        return &s;
    }
    }
    // Fallback — SQLite
    static SqliteDialect fallback;
    return &fallback;
}

} // namespace M1
