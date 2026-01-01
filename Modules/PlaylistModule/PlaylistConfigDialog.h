#pragma once
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QSettings>

namespace M1 { class PlaylistModule; }

// ─── AutoDJ strategy enum ───────────────────────────────────────────────────
enum class AutoDJStrategy {
    Random = 0,
    WeightedRandom,
    CategoryRotation,
    Clockwheel
};

// ─── Category definition ────────────────────────────────────────────────────
struct PlaylistCategory {
    QString name;           // e.g., "Hot", "Gold", "Recurrent"
    QString genreFilter;    // genre substring match (empty = all genres)
    int     weight = 1;     // for weighted random
};

// ─── AutoDJ config ──────────────────────────────────────────────────────────
struct AutoDJConfig {
    AutoDJStrategy strategy = AutoDJStrategy::Random;
    int  queueDepth         = 12;
    bool autoStartOnEnable  = true;
    int  artistSeparation   = 2;
    int  titleSeparation    = 4;
    bool avoidRecentlyPlayed = true;
    int  recentPlayedHours  = 4;

    bool autoRecovery       = false;  // auto-start AutoDJ on app launch

    QList<PlaylistCategory> categories;
    QStringList clockwheel;  // list of category names for hourly pattern

    void save(QSettings& s) const;
    void load(QSettings& s);
    static AutoDJConfig defaults();
};

// ─── Config dialog ──────────────────────────────────────────────────────────
class PlaylistConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit PlaylistConfigDialog(const AutoDJConfig& config, QWidget* parent = nullptr);

    AutoDJConfig config() const;

private:
    void buildGeneralTab(QWidget* tab);
    void buildCategoriesTab(QWidget* tab);
    void buildClockwheelTab(QWidget* tab);
    void populateFromConfig(const AutoDJConfig& cfg);

    // General
    QComboBox*  m_strategyCombo  = nullptr;
    QSpinBox*   m_queueDepthSpin = nullptr;
    QCheckBox*  m_autoStartCheck = nullptr;
    QSpinBox*   m_artistSepSpin  = nullptr;
    QSpinBox*   m_titleSepSpin   = nullptr;
    QCheckBox*  m_avoidRecentCheck = nullptr;
    QSpinBox*   m_recentHoursSpin  = nullptr;
    QCheckBox*  m_autoRecoveryCheck = nullptr;

    // Categories
    QTableWidget* m_catTable = nullptr;

    // Clockwheel
    QListWidget* m_cwList   = nullptr;
    QComboBox*   m_cwCatCombo = nullptr;
};
