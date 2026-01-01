#include "ThemeManager.h"
#include <QFile>
#include <QSettings>
#include <QApplication>
#include <QDebug>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    if (!s_instance)
        s_instance = new ThemeManager(qApp);
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {}

void ThemeManager::applyTheme(Theme t) {
    m_theme = t;

    const QString resourcePath = [t]() -> QString {
        switch (t) {
        case Theme::Classic: return ":/themes/classic.qss";
        case Theme::Light:   return ":/themes/light.qss";
        default:             return ":/themes/dark.qss";
        }
    }();

    QFile qss(resourcePath);
    if (qss.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
        qInfo() << "[ThemeManager] Applied theme:" << themeName(t);
    } else {
        qWarning() << "[ThemeManager] Could not load" << resourcePath;
    }

    emit themeChanged(t);
}

void ThemeManager::loadFromSettings(QSettings& s) {
    const QString name = s.value("UI/theme", "dark").toString();
    applyTheme(themeFromName(name));
}

void ThemeManager::saveToSettings(QSettings& s) const {
    s.setValue("UI/theme", themeName(m_theme));
}

QString ThemeManager::themeName(Theme t) {
    switch (t) {
    case Theme::Classic: return "classic";
    case Theme::Light:   return "light";
    default:             return "dark";
    }
}

ThemeManager::Theme ThemeManager::themeFromName(const QString& name) {
    const QString lower = name.toLower();
    if (lower == "classic") return Theme::Classic;
    if (lower == "light")   return Theme::Light;
    return Theme::Dark;
}
