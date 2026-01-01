#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include "MediaItem.h"
#include <QStringList>

namespace M1 {

class IDatabase;
class ScanWorker;
class LibraryModel;
class LibraryWidget;
class MusicBrainzLookup;

/// MediaLibraryModule — Phase 3 media library and audio file scanner.
///
/// Owns:
///   - IDatabase        — SQLite (default) or MySQL/MariaDB backend
///   - ScanWorker       — background TagLib directory scanner
///   - LibraryModel     — in-memory QAbstractTableModel
///   - MusicBrainzLookup — async MB enrichment
///
/// The widget (LibraryWidget) is created on first call to createWidget().
/// Drag-to-deck works by emitting requestLoadMedia(item, deckIndex).
class MediaLibraryModule : public IModule {
    Q_OBJECT

public:
    explicit MediaLibraryModule(QObject* parent = nullptr);
    ~MediaLibraryModule() override;

    // IModule identity
    QString moduleId()    const override { return "com.mcaster1.library"; }
    QString displayName() const override { return "Media Library"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {900, 500}; }
    QSize minimumModuleSize() const override { return {640, 300}; }

    // Lifecycle
    void initialize() override;
    void shutdown()   override;

    // UI
    QWidget* createWidget(QWidget* parent) override;

    // State persistence
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // No RT audio processing
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}

    IDatabase*    db()    const { return m_db; }
    LibraryModel* model() const { return m_model; }

public slots:
    void startScan(const QStringList& dirs);

private slots:
    void onItemScanned(const M1::MediaItem& item);
    void onMbLookupComplete(const M1::MediaItem& enriched);

private:
    void createDatabase();   ///< Factory: reads QSettings, creates SQLite or MySQL backend

    IDatabase*         m_db      = nullptr;
    ScanWorker*        m_scanner = nullptr;
    LibraryModel*      m_model   = nullptr;
    LibraryWidget*     m_widget  = nullptr;
    MusicBrainzLookup* m_mb      = nullptr;
    QStringList        m_scanDirs;
};

} // namespace M1
