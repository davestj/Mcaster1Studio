#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace M1 { class SqliteManager; }

class LibraryCategoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit LibraryCategoryPanel(QWidget* parent = nullptr);

    /// Set the database for category queries.
    void setDatabase(M1::SqliteManager* db);

    /// Refresh the tree from database.
    void reload();

signals:
    /// Emitted when user selects a category. -1 = "All Tracks".
    void categorySelected(qint64 categoryId);

    /// Emitted when categories change (add/rename/delete).
    void categoriesChanged();

    /// User wants to scan a folder into a specific category.
    void scanIntoCategoryRequested(qint64 categoryId);

    /// AI category actions — emitted so LibraryWidget can handle them
    /// (LibraryCategoryPanel has no access to AiTrackIntel).
    void aiRecommendRequested(qint64 categoryId, const QString& personaName);
    void aiGeneratePlaylistRequested(qint64 categoryId, const QString& personaName);
    void aiDaypartRequested(qint64 categoryId, const QString& personaName);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onAddCategory();
    void onAddSubCategory();
    void onRenameCategory();
    void onDeleteCategory();
    void onChangeColor();
    void onAssignPersona();

private:
    void buildTree();
    QTreeWidgetItem* createCategoryItem(qint64 id, const QString& name,
                                         const QString& type, const QString& color,
                                         int trackCount, QTreeWidgetItem* parent = nullptr);

    /// Returns the list of hardcoded AI persona preset names.
    static QStringList personaPresetNames();

    /// Returns the persona name assigned to a category (from QSettings), or empty.
    static QString categoryPersonaName(qint64 categoryId);

    QTreeWidget* m_tree = nullptr;
    M1::SqliteManager* m_db = nullptr;
    qint64 m_selectedCategory = -1;  // -1 = All Tracks
};
