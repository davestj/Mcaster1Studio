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
        .arg(uniqueFilePathConstraint())
        .arg(tableOptions());
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
