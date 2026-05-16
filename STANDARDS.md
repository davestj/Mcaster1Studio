# STANDARDS.md — Mcaster1Studio Mandatory Development Standards

These standards are MANDATORY and override any default behavior.
Referenced from CLAUDE.md. Every developer and AI assistant MUST follow these.

---

## 1. PORTABLE APPLICATION — NOTHING OUTSIDE THE APP FOLDER

**ALL data, config, databases, logs, cache, and settings MUST live inside the application directory.**

| Data Type | Location | Example Path |
|-----------|----------|-------------|
| Settings (INI) | `<appDir>/config/Mcaster1/Mcaster1Studio.ini` | `config/Mcaster1/Mcaster1Studio.ini` |
| Surface configs | `<appDir>/config/surfaces/*.yaml` | `config/surfaces/surface_DJ.yaml` |
| SQLite databases | `<appDir>/data/*.db` | `data/mcaster1studio.db` |
| Debug logs | `<appDir>/logs/` | `logs/mcaster1studio_debug.log` |
| Crash dumps | `<appDir>/logs/` | `logs/mcaster1studio_crash.dmp` |
| Album art cache | `<appDir>/cache/artwork/` | `cache/artwork/<sha256>.jpg` |
| Recordings | `<appDir>/recordings/` | `recordings/AuxDeck_name_date.wav` |
| Themes | `<appDir>/themes/*.qss` | `themes/enterprise-pro.qss` |
| Plugins | `<appDir>/plugins/{modules,effects}/` | `plugins/modules/SampleModule.dll` |
| Layout YAML | `<appDir>/mcaster1_studio_layout.yaml` | Root of app dir |
| Certs | `<appDir>/certs/` | `certs/Mcaster1CodeSigning.cer` |

### NEVER use these for storage:
- `%APPDATA%` (Roaming profile)
- `%LOCALAPPDATA%` (Local profile)
- `%PROGRAMDATA%`
- `%TEMP%`
- `C:\Program Files\` or `C:\Program Files (x86)\`
- Windows Registry (except Add/Remove Programs entry from installer)
- `QStandardPaths::AppDataLocation`
- `QStandardPaths::AppConfigLocation`
- `QStandardPaths::AppLocalDataLocation`

### Acceptable QStandardPaths usage:
- `QStandardPaths::MusicLocation` — for file browser starting directory ONLY
- `QStandardPaths::DocumentsLocation` — for save/export file dialog ONLY
- These are NOT storage locations — they are just default browse paths for file dialogs

### How portability works in code:
```cpp
// main.cpp sets this BEFORE any QSettings are constructed:
QSettings::setDefaultFormat(QSettings::IniFormat);
QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                   QCoreApplication::applicationDirPath() + "/config");
```

All `QSettings("Mcaster1", "Mcaster1Studio")` calls then write to `<appDir>/config/Mcaster1/Mcaster1Studio.ini`.

### Database default path:
```cpp
// SqliteManager::connect() default:
const QString dir = QCoreApplication::applicationDirPath() + "/data";
QDir().mkpath(dir);
m_dbPath = dir + "/mcaster1studio.db";
```

---

## 2. SQL DIALECT SYNC

Every new database table or column MUST be added to ALL 5 SQL dialects simultaneously:

| Backend | Dialect Class | Key Differences |
|---------|--------------|-----------------|
| SQLite | `SqliteDialect` | AUTOINCREMENT, TEXT dates, INTEGER booleans |
| MySQL | `MySqlDialect` | AUTO_INCREMENT, TINYINT(1) booleans, ENGINE=InnoDB |
| PostgreSQL | `PostgresDialect` | BIGSERIAL, BOOLEAN, TIMESTAMP, DOUBLE PRECISION |
| Firebird | `FirebirdDialect` | IDENTITY, SMALLINT booleans, FIRST/SKIP pagination |
| SQL Server | `MssqlDialect` | IDENTITY(1,1), BIT booleans, DATETIME2, OFFSET/FETCH |

When adding a new table:
1. Add `createXxxTableSql()` method to `SqlDialect` base class (Core/include/SqlDialect.h)
2. Implement using dialect-specific types (`autoIncrementPK()`, `varcharType()`, etc.)
3. Add to `createFullSchemaSql()` return list
4. Add migration `ALTER TABLE` in `SqliteManager::createSchema()` for existing databases
5. Test: verify the DDL generates correctly for each dialect

---

## 3. PLAYLIST/AUTODJ IN DECKPLAYER — DO NOT REMOVE

The Playlist/AutoDJ module is PERMANENTLY EMBEDDED in the DeckPlayer center column.

```
┌──────────┬──────────────┬──────────┐
│          │  CROSSFADER  │          │
│          │     PTT      │          │
│  Deck A  │──────────────│  Deck B  │
│          │  PLAYLIST    │          │
│          │  AUTO DJ     │          │
└──────────┴──────────────┴──────────┘
```

**Code locations — NEVER modify without explicit permission:**
- `DeckWidget.h` — `m_playlistWidget`, `m_centerCol`, `setPlaylistModule()`
- `DeckWidget.cpp` — `setPlaylistModule()` embeds PlaylistWidget
- `MainWindow.cpp` — auto-creates PlaylistModule when deck exists

---

## 4. THEME SYSTEM

- **Default theme:** Enterprise Pro (`enterprise-pro.qss`)
- **Available:** Enterprise Pro, Classic
- **NEVER** hardcode colors in `setStyleSheet()` — use `ThemePalette::forCurrentTheme()` or QSS objectName rules
- **NEVER** use font sizes below 12px
- **ALWAYS** add QSS rules to ALL theme files when adding new objectNames

---

## 5. RUNTIME DLLS

Every external DLL the app needs MUST have a CMake post-build copy step.
The DLL source file MUST be committed to the repo or come from vcpkg.
NEVER rely on Debug build output DLLs when running Release.

---

## 6. CODE SIGNING

- CMake post-build auto-signs Mcaster1Studio.exe
- NSIS installer imports cert before binaries land
- Installer .exe itself is signed
- Certificate: `SIGNING-KEYS/Mcaster1CodeSigning.pfx`

---

## 7. VERSION BUMP CADENCE

When bumping version, update ALL of these simultaneously:
- `CMakeLists.txt` (project VERSION)
- `VERSION.txt`
- `vcpkg.json`
- `CLAUDE.md`
- `CHANGELOG.md`
- `installer/installer.nsi` (PRODUCT_VERSION)
- `installer/build-installer.bat`
- All `docs/*.html` headers and footers
