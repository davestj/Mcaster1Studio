#include "LibraryCategoryPanel.h"
#include "SqliteManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QPixmap>
#include <QPainter>
#include <QSettings>

// ─── LibraryCategoryPanel ────────────────────────────────────────────────────

LibraryCategoryPanel::LibraryCategoryPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("LibraryCategoryPanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName("CategoryTree");
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setColumnCount(1);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(true);

    auto pal = ThemePalette::forCurrentTheme();

    m_tree->setStyleSheet(
        QString("QTreeWidget { "
                "  background: %1; "
                "  color: %2; "
                "  border: 1px solid %3; "
                "  font-size: 15px; "
                "  font-weight: bold; "
                "} "
                "QTreeWidget::item { "
                "  padding: 4px 6px; "
                "} "
                "QTreeWidget::item:selected { "
                "  background: %4; "
                "  color: #1a1814; "
                "} "
                "QTreeWidget::item:hover { "
                "  background: %5; "
                "  color: #1a1814; "
                "}")
            .arg(pal.panelBg.name(),
                 pal.text.name(),
                 pal.border.name(),
                 pal.accent.name(),
                 pal.accentHover.name()));

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked,
            this, &LibraryCategoryPanel::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &LibraryCategoryPanel::onContextMenu);
}

void LibraryCategoryPanel::setDatabase(M1::SqliteManager* db) {
    m_db = db;
    if (m_db)
        buildTree();
}

void LibraryCategoryPanel::reload() {
    buildTree();
}

// ─── Tree construction ───────────────────────────────────────────────────────

void LibraryCategoryPanel::buildTree() {
    m_tree->clear();

    // "All Tracks" root item — shows total library count
    auto* allItem = new QTreeWidgetItem(m_tree);
    int totalCount = m_db ? m_db->totalTrackCount() : 0;
    allItem->setText(0, totalCount > 0
        ? QString("All Tracks  (%1)").arg(totalCount)
        : QStringLiteral("All Tracks"));
    allItem->setData(0, Qt::UserRole, QVariant::fromValue<qint64>(-1));
    allItem->setToolTip(0, QString("Show all %1 tracks in the library").arg(totalCount));

    QFont boldFont = m_tree->font();
    boldFont.setPixelSize(15);
    boldFont.setBold(true);
    allItem->setFont(0, boldFont);

    if (!m_db) return;

    // Fetch all categories from the database (includes track_count from SQL)
    QList<QVariantMap> categories = m_db->allCategories();

    // Build lookup: parentId -> list of children
    QMap<qint64, QList<QVariantMap>> childrenOf;
    for (const auto& cat : categories) {
        qint64 parentId = cat.value("parent_id", 0).toLongLong();
        childrenOf[parentId].append(cat);
    }

    // Recursively add categories — track_count comes from allCategories() SQL,
    // no need to query tracksByCategory() per category
    std::function<void(qint64, QTreeWidgetItem*)> addChildren;
    addChildren = [&](qint64 parentId, QTreeWidgetItem* parentItem) {
        const auto& children = childrenOf.value(parentId);
        for (const auto& cat : children) {
            qint64 id    = cat.value("id").toLongLong();
            QString name = cat.value("name").toString();
            QString type = cat.value("type").toString();
            QString color = cat.value("color").toString();
            int count    = cat.value("track_count", 0).toInt();

            auto* item = createCategoryItem(id, name, type, color, count, parentItem);
            addChildren(id, item);
        }
    };

    addChildren(0, m_tree->invisibleRootItem());

    m_tree->expandAll();

    // Re-select the previously selected category if it still exists
    if (m_selectedCategory >= 0) {
        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> findItem;
        findItem = [&](QTreeWidgetItem* root) -> QTreeWidgetItem* {
            for (int i = 0; i < root->childCount(); ++i) {
                auto* child = root->child(i);
                if (child->data(0, Qt::UserRole).toLongLong() == m_selectedCategory)
                    return child;
                auto* found = findItem(child);
                if (found) return found;
            }
            return nullptr;
        };
        auto* sel = findItem(m_tree->invisibleRootItem());
        if (sel)
            m_tree->setCurrentItem(sel);
    }
}

QTreeWidgetItem* LibraryCategoryPanel::createCategoryItem(
    qint64 id, const QString& name, const QString& type,
    const QString& color, int trackCount, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = parent
        ? new QTreeWidgetItem(parent)
        : new QTreeWidgetItem(m_tree);

    // Display name with track count badge
    QString display = trackCount > 0
        ? QString("%1  (%2)").arg(name).arg(trackCount)
        : name;
    item->setText(0, display);
    item->setData(0, Qt::UserRole, QVariant::fromValue<qint64>(id));

    // Build tooltip — include persona name if assigned
    QString tooltip = QString("Type: %1").arg(type.isEmpty() ? QStringLiteral("general") : type);
    const QString persona = categoryPersonaName(id);
    if (!persona.isEmpty())
        tooltip += QString("\nAI Persona: %1").arg(persona);
    item->setToolTip(0, tooltip);

    // Font sizing: 15px bold for root categories, 13px regular for subcategories
    {
        QFont f = m_tree->font();
        bool isRootCategory = (!parent || parent == m_tree->invisibleRootItem());
        if (isRootCategory) {
            f.setPixelSize(15);
            f.setBold(true);
        } else {
            f.setPixelSize(13);
            f.setBold(false);
        }
        item->setFont(0, f);
    }

    // Color swatch icon
    if (!color.isEmpty()) {
        QColor c(color);
        if (c.isValid()) {
            QPixmap pm(14, 14);
            pm.fill(Qt::transparent);
            QPainter painter(&pm);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(c);
            painter.setPen(c.darker(130));
            painter.drawEllipse(1, 1, 12, 12);
            painter.end();
            item->setIcon(0, QIcon(pm));
        }
    }

    return item;
}

// ─── Interaction ─────────────────────────────────────────────────────────────

void LibraryCategoryPanel::onItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    m_selectedCategory = item->data(0, Qt::UserRole).toLongLong();
    emit categorySelected(m_selectedCategory);
}

void LibraryCategoryPanel::onContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    qint64 catId = item ? item->data(0, Qt::UserRole).toLongLong() : -1;

    QMenu menu(this);

    // ── Category management ─────────────────────────────────────────────────
    auto* actAdd = menu.addAction(QStringLiteral("New Category..."));
    connect(actAdd, &QAction::triggered, this, &LibraryCategoryPanel::onAddCategory);

    if (item && catId > 0) {
        auto* actSubCat = menu.addAction(QStringLiteral("New Subcategory..."));
        connect(actSubCat, &QAction::triggered, this, &LibraryCategoryPanel::onAddSubCategory);
    }

    menu.addSeparator();

    if (item && catId > 0) {
        auto* actRename = menu.addAction(QStringLiteral("Rename..."));
        connect(actRename, &QAction::triggered, this, &LibraryCategoryPanel::onRenameCategory);

        auto* actColor = menu.addAction(QStringLiteral("Change Color..."));
        connect(actColor, &QAction::triggered, this, &LibraryCategoryPanel::onChangeColor);

        menu.addSeparator();

        auto* actScan = menu.addAction(QStringLiteral("Scan Folder into Category..."));
        connect(actScan, &QAction::triggered, this, [this, catId]() {
            emit scanIntoCategoryRequested(catId);
        });

        // ── AI Persona assignment ─────────────────────────────────────────
        menu.addSeparator();

        auto* actPersona = menu.addAction(QStringLiteral("Assign AI Persona..."));
        connect(actPersona, &QAction::triggered, this, &LibraryCategoryPanel::onAssignPersona);

        // Show AI actions if a persona is assigned to this category
        const QString assignedPersona = categoryPersonaName(catId);
        if (!assignedPersona.isEmpty()) {
            menu.addSeparator();

            auto* actRecommend = menu.addAction(QStringLiteral("AI: Recommend Tracks"));
            connect(actRecommend, &QAction::triggered, this, [this, catId, assignedPersona]() {
                emit aiRecommendRequested(catId, assignedPersona);
            });

            auto* actPlaylist = menu.addAction(QStringLiteral("AI: Generate Playlist"));
            connect(actPlaylist, &QAction::triggered, this, [this, catId, assignedPersona]() {
                emit aiGeneratePlaylistRequested(catId, assignedPersona);
            });

            auto* actDaypart = menu.addAction(QStringLiteral("AI: Daypart Schedule"));
            connect(actDaypart, &QAction::triggered, this, [this, catId, assignedPersona]() {
                emit aiDaypartRequested(catId, assignedPersona);
            });
        }

        menu.addSeparator();

        auto* actDelete = menu.addAction(QStringLiteral("Delete Category"));
        connect(actDelete, &QAction::triggered, this, &LibraryCategoryPanel::onDeleteCategory);
    }

    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void LibraryCategoryPanel::onAddCategory() {
    if (!m_db) return;

    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("New Category"),
                                          QStringLiteral("Category name:"),
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QColor color = QColorDialog::getColor(QColor(QStringLiteral("#5588CC")),
                                           this, QStringLiteral("Choose category color"));
    if (!color.isValid()) return;

    m_db->addCategory(name.trimmed(), QStringLiteral("Custom"),
                      color.name(), 0);
    buildTree();
    emit categoriesChanged();
}

void LibraryCategoryPanel::onAddSubCategory() {
    if (!m_db) return;

    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    qint64 parentId = item->data(0, Qt::UserRole).toLongLong();
    if (parentId <= 0) return;

    // Extract the parent name for the dialog prompt
    QString parentName = item->text(0);
    int parenIdx = parentName.lastIndexOf(QStringLiteral("  ("));
    if (parenIdx > 0)
        parentName = parentName.left(parenIdx);

    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("New Subcategory"),
                                          QString("Subcategory name (under \"%1\"):").arg(parentName),
                                          QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    QColor color = QColorDialog::getColor(QColor(QStringLiteral("#5588CC")),
                                           this, QStringLiteral("Choose category color"));
    if (!color.isValid()) return;

    m_db->addCategory(name.trimmed(), QStringLiteral("Custom"),
                      color.name(), parentId);
    buildTree();
    emit categoriesChanged();
}

void LibraryCategoryPanel::onRenameCategory() {
    if (!m_db) return;

    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    qint64 catId = item->data(0, Qt::UserRole).toLongLong();
    if (catId < 0) return;

    // Extract the plain name (strip track count badge)
    QString currentName = item->text(0);
    int parenIdx = currentName.lastIndexOf(QStringLiteral("  ("));
    if (parenIdx > 0)
        currentName = currentName.left(parenIdx);

    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("Rename Category"),
                                          QStringLiteral("New name:"),
                                          QLineEdit::Normal, currentName.trimmed(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_db->renameCategory(catId, name.trimmed());
    buildTree();
    emit categoriesChanged();
}

void LibraryCategoryPanel::onDeleteCategory() {
    if (!m_db) return;

    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    qint64 catId = item->data(0, Qt::UserRole).toLongLong();
    if (catId < 0) return;

    QString catName = item->text(0);
    int parenIdx = catName.lastIndexOf(QStringLiteral("  ("));
    if (parenIdx > 0)
        catName = catName.left(parenIdx);

    auto reply = QMessageBox::question(
        this, QStringLiteral("Delete Category"),
        QString("Delete category \"%1\"?\n\n"
                "Tracks will NOT be deleted, only the category assignment.")
            .arg(catName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    m_db->removeCategory(catId);

    if (m_selectedCategory == catId) {
        m_selectedCategory = -1;
        emit categorySelected(-1);
    }

    buildTree();
    emit categoriesChanged();
}

void LibraryCategoryPanel::onChangeColor() {
    if (!m_db) return;

    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    qint64 catId = item->data(0, Qt::UserRole).toLongLong();
    if (catId < 0) return;

    QColor initial(QStringLiteral("#4a90d9"));
    // Try to read current color from the icon
    QIcon icon = item->icon(0);
    if (!icon.isNull()) {
        QPixmap pm = icon.pixmap(14, 14);
        if (!pm.isNull()) {
            QImage img = pm.toImage();
            if (img.width() > 6 && img.height() > 6) {
                QColor c = img.pixelColor(7, 7);
                if (c.alpha() > 0)
                    initial = c;
            }
        }
    }

    QColor color = QColorDialog::getColor(initial, this, QStringLiteral("Category Color"));
    if (!color.isValid()) return;

    m_db->setCategoryColor(catId, color.name());
    buildTree();
    emit categoriesChanged();
}

// ─── AI Persona assignment (QSettings-backed until DB schema agent finishes) ──

QStringList LibraryCategoryPanel::personaPresetNames() {
    return {
        QStringLiteral("(None)"),
        QStringLiteral("Classic DJ"),
        QStringLiteral("Late Night Chill"),
        QStringLiteral("Morning Show Host"),
        QStringLiteral("Top 40 Hype"),
        QStringLiteral("Country Radio"),
        QStringLiteral("Hip-Hop / R&B"),
        QStringLiteral("Rock & Metal"),
        QStringLiteral("Classical / NPR"),
        QStringLiteral("Electronic / EDM"),
        QStringLiteral("Christian Radio"),
        QStringLiteral("Sports Talk"),
        QStringLiteral("News / Talk"),
        QStringLiteral("Podcast Conversational"),
        QStringLiteral("Bilingual (EN/ES)"),
        QStringLiteral("AI Assistant (Neutral)")
    };
}

QString LibraryCategoryPanel::categoryPersonaName(qint64 categoryId) {
    if (categoryId <= 0) return {};
    QSettings s("Mcaster1", "Mcaster1Studio");
    return s.value(QString("ai/categoryPersona/%1").arg(categoryId), "").toString();
}

void LibraryCategoryPanel::onAssignPersona() {
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    qint64 catId = item->data(0, Qt::UserRole).toLongLong();
    if (catId <= 0) return;

    const QStringList presets = personaPresetNames();
    const QString current = categoryPersonaName(catId);

    bool ok = false;
    QString selected = QInputDialog::getItem(
        this, QStringLiteral("Assign AI Persona"),
        QStringLiteral("Select a persona for this category:"),
        presets, presets.indexOf(current.isEmpty() ? QStringLiteral("(None)") : current),
        false, &ok);

    if (!ok) return;

    QSettings s("Mcaster1", "Mcaster1Studio");
    const QString key = QString("ai/categoryPersona/%1").arg(catId);

    if (selected == QStringLiteral("(None)")) {
        s.remove(key);
    } else {
        s.setValue(key, selected);
    }

    // Rebuild tree to update tooltips
    buildTree();
}
