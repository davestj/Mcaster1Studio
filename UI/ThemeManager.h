#pragma once
#include <QObject>

class QSettings;

/// ThemeManager — singleton that loads and applies application QSS themes.
///
/// Themes are QSS files embedded as Qt resources:
///   :/themes/enterprise-pro.qss  — Clean white/blue professional (DEFAULT)
///   :/themes/classic.qss         — Warm brown SAM Broadcaster 4 style
///
/// Usage:
///   ThemeManager::instance()->applyTheme(ThemeManager::Theme::Classic);
///   ThemeManager::instance()->saveToSettings(s);
class ThemeManager : public QObject {
    Q_OBJECT

public:
    enum class Theme { EnterprisePro, Classic };

    static ThemeManager* instance();

    void  applyTheme(Theme t);
    Theme currentTheme() const { return m_theme; }

    void loadFromSettings(QSettings& s);
    void saveToSettings(QSettings& s) const;

    static QString themeName(Theme t);
    static Theme   themeFromName(const QString& name);

signals:
    void themeChanged(ThemeManager::Theme newTheme);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    static ThemeManager* s_instance;

    Theme m_theme = Theme::EnterprisePro;
};

Q_DECLARE_METATYPE(ThemeManager::Theme)
