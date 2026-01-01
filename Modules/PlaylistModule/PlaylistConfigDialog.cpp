#include "PlaylistConfigDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTabWidget>

// ═════════════════════════════════════════════════════════════════════════════
// AutoDJConfig persistence
// ═════════════════════════════════════════════════════════════════════════════

AutoDJConfig AutoDJConfig::defaults() {
    AutoDJConfig cfg;
    cfg.strategy         = AutoDJStrategy::Random;
    cfg.queueDepth       = 12;
    cfg.autoStartOnEnable = true;
    cfg.artistSeparation  = 2;
    cfg.titleSeparation   = 4;
    cfg.avoidRecentlyPlayed = true;
    cfg.recentPlayedHours  = 4;
    cfg.autoRecovery       = false;

    // Default broadcast categories
    cfg.categories = {
        {"Hot",       "Pop",     3},
        {"Medium",    "Rock",    2},
        {"Gold",      "Classic", 2},
        {"Recurrent", "",        1},
        {"Power",     "",        3},
    };

    // Default clockwheel pattern (12 slots/hour)
    cfg.clockwheel = {
        "Hot", "Medium", "Hot", "Gold",
        "Hot", "Power",  "Hot", "Recurrent",
        "Hot", "Medium", "Hot", "Gold"
    };

    return cfg;
}

void AutoDJConfig::save(QSettings& s) const {
    s.beginGroup("AutoDJConfig");
    s.setValue("strategy",          (int)strategy);
    s.setValue("queueDepth",        queueDepth);
    s.setValue("autoStartOnEnable", autoStartOnEnable);
    s.setValue("artistSeparation",  artistSeparation);
    s.setValue("titleSeparation",   titleSeparation);
    s.setValue("avoidRecentlyPlayed", avoidRecentlyPlayed);
    s.setValue("recentPlayedHours", recentPlayedHours);
    s.setValue("autoRecovery",      autoRecovery);

    // Categories
    s.beginWriteArray("categories", categories.size());
    for (int i = 0; i < categories.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name",        categories[i].name);
        s.setValue("genreFilter", categories[i].genreFilter);
        s.setValue("weight",      categories[i].weight);
    }
    s.endArray();

    // Clockwheel
    s.setValue("clockwheel", clockwheel);

    s.endGroup();
}

void AutoDJConfig::load(QSettings& s) {
    s.beginGroup("AutoDJConfig");

    strategy           = (AutoDJStrategy)s.value("strategy", 0).toInt();
    queueDepth         = s.value("queueDepth", 12).toInt();
    autoStartOnEnable  = s.value("autoStartOnEnable", true).toBool();
    artistSeparation   = s.value("artistSeparation", 2).toInt();
    titleSeparation    = s.value("titleSeparation", 4).toInt();
    avoidRecentlyPlayed = s.value("avoidRecentlyPlayed", true).toBool();
    recentPlayedHours  = s.value("recentPlayedHours", 4).toInt();
    autoRecovery       = s.value("autoRecovery", false).toBool();

    // Categories
    categories.clear();
    int catCount = s.beginReadArray("categories");
    for (int i = 0; i < catCount; ++i) {
        s.setArrayIndex(i);
        PlaylistCategory cat;
        cat.name        = s.value("name").toString();
        cat.genreFilter = s.value("genreFilter").toString();
        cat.weight      = s.value("weight", 1).toInt();
        categories.append(cat);
    }
    s.endArray();

    // If no categories loaded, use defaults
    if (categories.isEmpty())
        categories = AutoDJConfig::defaults().categories;

    // Clockwheel
    clockwheel = s.value("clockwheel").toStringList();
    if (clockwheel.isEmpty())
        clockwheel = AutoDJConfig::defaults().clockwheel;

    s.endGroup();
}

// ═════════════════════════════════════════════════════════════════════════════
// PlaylistConfigDialog
// ═════════════════════════════════════════════════════════════════════════════

PlaylistConfigDialog::PlaylistConfigDialog(const AutoDJConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Playlist AutoDJ Configuration");
    setMinimumSize(520, 440);
    resize(560, 500);

    auto* root = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);

    auto* generalTab = new QWidget;
    buildGeneralTab(generalTab);
    tabs->addTab(generalTab, "General");

    auto* catTab = new QWidget;
    buildCategoriesTab(catTab);
    tabs->addTab(catTab, "Categories");

    auto* cwTab = new QWidget;
    buildClockwheelTab(cwTab);
    tabs->addTab(cwTab, "Clockwheel");

    root->addWidget(tabs, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    populateFromConfig(config);
}

void PlaylistConfigDialog::buildGeneralTab(QWidget* tab) {
    auto* layout = new QFormLayout(tab);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    // Strategy
    m_strategyCombo = new QComboBox(tab);
    m_strategyCombo->addItem("Random",             (int)AutoDJStrategy::Random);
    m_strategyCombo->addItem("Weighted Random",     (int)AutoDJStrategy::WeightedRandom);
    m_strategyCombo->addItem("Category Rotation",   (int)AutoDJStrategy::CategoryRotation);
    m_strategyCombo->addItem("Clockwheel",          (int)AutoDJStrategy::Clockwheel);
    m_strategyCombo->setToolTip(
        "Random: Pick tracks randomly from the library\n"
        "Weighted Random: Favor tracks with higher weight\n"
        "Category Rotation: Cycle through genre categories\n"
        "Clockwheel: Follow an hourly broadcast clock pattern");
    layout->addRow("Strategy:", m_strategyCombo);

    // Queue depth
    m_queueDepthSpin = new QSpinBox(tab);
    m_queueDepthSpin->setRange(3, 100);
    m_queueDepthSpin->setToolTip("Number of tracks to keep loaded in the queue");
    layout->addRow("Queue Depth:", m_queueDepthSpin);

    // Auto-start
    m_autoStartCheck = new QCheckBox("Auto-play first track when AutoDJ is enabled", tab);
    layout->addRow("", m_autoStartCheck);

    // Auto-recovery
    m_autoRecoveryCheck = new QCheckBox("Auto-Recovery: start AutoDJ on app launch", tab);
    m_autoRecoveryCheck->setToolTip(
        "When enabled, AutoDJ will automatically start on application launch.\n"
        "It will queue tracks based on the configured playlist rules and\n"
        "begin playing music without user intervention.");
    layout->addRow("", m_autoRecoveryCheck);

    // Separator
    auto* sep = new QFrame(tab);
    sep->setFrameShape(QFrame::HLine);
    layout->addRow(sep);

    // Separation rules
    auto* sepGroup = new QGroupBox("Rotation Rules", tab);
    auto* sepLay = new QFormLayout(sepGroup);

    m_artistSepSpin = new QSpinBox(sepGroup);
    m_artistSepSpin->setRange(0, 20);
    m_artistSepSpin->setToolTip("Minimum tracks between the same artist");
    sepLay->addRow("Artist Separation:", m_artistSepSpin);

    m_titleSepSpin = new QSpinBox(sepGroup);
    m_titleSepSpin->setRange(0, 20);
    m_titleSepSpin->setToolTip("Minimum tracks between the same title");
    sepLay->addRow("Title Separation:", m_titleSepSpin);

    m_avoidRecentCheck = new QCheckBox("Avoid recently played tracks", sepGroup);
    sepLay->addRow("", m_avoidRecentCheck);

    m_recentHoursSpin = new QSpinBox(sepGroup);
    m_recentHoursSpin->setRange(1, 48);
    m_recentHoursSpin->setSuffix(" hours");
    m_recentHoursSpin->setToolTip("Don't replay tracks played within this window");
    sepLay->addRow("Cooldown:", m_recentHoursSpin);

    layout->addRow(sepGroup);
}

void PlaylistConfigDialog::buildCategoriesTab(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* desc = new QLabel(
        "Define genre categories for rotation scheduling.\n"
        "Genre Filter matches the track's genre field (substring, case-insensitive).\n"
        "Leave empty to match all genres. Weight controls selection probability.", tab);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_catTable = new QTableWidget(0, 3, tab);
    m_catTable->setHorizontalHeaderLabels({"Category Name", "Genre Filter", "Weight"});
    m_catTable->horizontalHeader()->setStretchLastSection(true);
    m_catTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_catTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_catTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_catTable->horizontalHeader()->resizeSection(2, 60);
    m_catTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_catTable, 1);

    auto* btnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton("+ Add Category", tab);
    addBtn->setToolTip("Add a new genre category");
    auto* removeBtn = new QPushButton("- Remove", tab);
    removeBtn->setToolTip("Remove selected category");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(removeBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_catTable->rowCount();
        m_catTable->insertRow(row);
        m_catTable->setItem(row, 0, new QTableWidgetItem("New Category"));
        m_catTable->setItem(row, 1, new QTableWidgetItem(""));
        auto* wSpin = new QSpinBox(m_catTable);
        wSpin->setRange(1, 10);
        wSpin->setValue(1);
        m_catTable->setCellWidget(row, 2, wSpin);
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_catTable->currentRow();
        if (row >= 0) m_catTable->removeRow(row);
    });
}

void PlaylistConfigDialog::buildClockwheelTab(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* desc = new QLabel(
        "Define the hourly broadcast clock pattern.\n"
        "Each slot selects a track from the named category.\n"
        "The pattern repeats every hour.", tab);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_cwList = new QListWidget(tab);
    layout->addWidget(m_cwList, 1);

    auto* addRow = new QHBoxLayout;

    m_cwCatCombo = new QComboBox(tab);
    m_cwCatCombo->setEditable(true);
    m_cwCatCombo->setToolTip("Category name for this clock slot");
    addRow->addWidget(m_cwCatCombo, 1);

    auto* addSlotBtn = new QPushButton("+ Add Slot", tab);
    addSlotBtn->setToolTip("Add a slot to the clockwheel pattern");
    addRow->addWidget(addSlotBtn);

    layout->addLayout(addRow);

    auto* ctrlRow = new QHBoxLayout;
    auto* moveUpBtn   = new QPushButton("Move Up", tab);
    auto* moveDownBtn = new QPushButton("Move Down", tab);
    auto* removeBtn   = new QPushButton("Remove", tab);
    ctrlRow->addWidget(moveUpBtn);
    ctrlRow->addWidget(moveDownBtn);
    ctrlRow->addWidget(removeBtn);
    ctrlRow->addStretch();
    layout->addLayout(ctrlRow);

    connect(addSlotBtn, &QPushButton::clicked, this, [this]() {
        const QString cat = m_cwCatCombo->currentText().trimmed();
        if (!cat.isEmpty())
            m_cwList->addItem(cat);
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        delete m_cwList->takeItem(m_cwList->currentRow());
    });

    connect(moveUpBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_cwList->currentRow();
        if (row > 0) {
            auto* item = m_cwList->takeItem(row);
            m_cwList->insertItem(row - 1, item);
            m_cwList->setCurrentRow(row - 1);
        }
    });

    connect(moveDownBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_cwList->currentRow();
        if (row >= 0 && row < m_cwList->count() - 1) {
            auto* item = m_cwList->takeItem(row);
            m_cwList->insertItem(row + 1, item);
            m_cwList->setCurrentRow(row + 1);
        }
    });
}

void PlaylistConfigDialog::populateFromConfig(const AutoDJConfig& cfg) {
    // General
    for (int i = 0; i < m_strategyCombo->count(); ++i) {
        if (m_strategyCombo->itemData(i).toInt() == (int)cfg.strategy) {
            m_strategyCombo->setCurrentIndex(i);
            break;
        }
    }
    m_queueDepthSpin->setValue(cfg.queueDepth);
    m_autoStartCheck->setChecked(cfg.autoStartOnEnable);
    m_autoRecoveryCheck->setChecked(cfg.autoRecovery);
    m_artistSepSpin->setValue(cfg.artistSeparation);
    m_titleSepSpin->setValue(cfg.titleSeparation);
    m_avoidRecentCheck->setChecked(cfg.avoidRecentlyPlayed);
    m_recentHoursSpin->setValue(cfg.recentPlayedHours);

    // Categories
    m_catTable->setRowCount(0);
    for (const auto& cat : cfg.categories) {
        const int row = m_catTable->rowCount();
        m_catTable->insertRow(row);
        m_catTable->setItem(row, 0, new QTableWidgetItem(cat.name));
        m_catTable->setItem(row, 1, new QTableWidgetItem(cat.genreFilter));
        auto* wSpin = new QSpinBox(m_catTable);
        wSpin->setRange(1, 10);
        wSpin->setValue(cat.weight);
        m_catTable->setCellWidget(row, 2, wSpin);
    }

    // Clockwheel
    m_cwList->clear();
    for (const QString& slot : cfg.clockwheel)
        m_cwList->addItem(slot);

    // Populate clockwheel category combo from categories
    m_cwCatCombo->clear();
    for (const auto& cat : cfg.categories)
        m_cwCatCombo->addItem(cat.name);
}

AutoDJConfig PlaylistConfigDialog::config() const {
    AutoDJConfig cfg;

    cfg.strategy          = (AutoDJStrategy)m_strategyCombo->currentData().toInt();
    cfg.queueDepth        = m_queueDepthSpin->value();
    cfg.autoStartOnEnable = m_autoStartCheck->isChecked();
    cfg.autoRecovery      = m_autoRecoveryCheck->isChecked();
    cfg.artistSeparation  = m_artistSepSpin->value();
    cfg.titleSeparation   = m_titleSepSpin->value();
    cfg.avoidRecentlyPlayed = m_avoidRecentCheck->isChecked();
    cfg.recentPlayedHours = m_recentHoursSpin->value();

    // Categories
    cfg.categories.clear();
    for (int row = 0; row < m_catTable->rowCount(); ++row) {
        PlaylistCategory cat;
        cat.name        = m_catTable->item(row, 0)->text().trimmed();
        cat.genreFilter = m_catTable->item(row, 1)->text().trimmed();
        auto* wSpin = qobject_cast<QSpinBox*>(m_catTable->cellWidget(row, 2));
        cat.weight = wSpin ? wSpin->value() : 1;
        if (!cat.name.isEmpty())
            cfg.categories.append(cat);
    }

    // Clockwheel
    cfg.clockwheel.clear();
    for (int i = 0; i < m_cwList->count(); ++i)
        cfg.clockwheel.append(m_cwList->item(i)->text());

    return cfg;
}
