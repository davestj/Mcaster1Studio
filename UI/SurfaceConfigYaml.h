#pragma once
#include <QString>
#include <QList>
#include <QMap>

/// Snapshot of a single surface's visual layout — column sizes and floating positions.
/// Kept in this header (not SurfaceWidget.h) so both SurfaceWidget and MainWindow
/// can use it without circular includes.
struct SurfaceLayoutSnapshot {
    QList<int>           columnWidths;    ///< m_splitter->sizes()
    QList<QList<int>>    columnHeights;   ///< per-column QSplitter sizes
    struct FloatingItem {
        QString moduleId;
        int x = 0, y = 0, w = 200, h = 150;
    };
    QList<FloatingItem>  floatingItems;   ///< canvas-floating docks
};

/// Static helpers for reading/writing the surface layout YAML file.
///
/// File format (valid YAML, flat key-value under each surface name):
///
///   version: 1
///   surfaces:
///     Alpha:
///       column_widths: 400,300
///       column_0_heights: 200,300
///       column_1_heights: 500
///       floating_count: 1
///       floating_0_module_id: com.mcaster1.encoder
///       floating_0_x: 100
///       floating_0_y: 50
///       floating_0_width: 400
///       floating_0_height: 300
///
namespace SurfaceConfigYaml {

/// Write layout data for all surfaces to `filePath`.
bool save(const QString& filePath,
          const QMap<QString, SurfaceLayoutSnapshot>& data);

/// Read layout data from `filePath`. Returns empty map on error.
QMap<QString, SurfaceLayoutSnapshot> load(const QString& filePath);

} // namespace SurfaceConfigYaml
