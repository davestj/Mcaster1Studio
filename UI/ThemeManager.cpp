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
        case Theme::Classic:       return ":/themes/classic.qss";
        default:                   return ":/themes/enterprise-pro.qss";
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
    const QString name = s.value("UI/theme", "enterprise-pro").toString();
    applyTheme(themeFromName(name));
}

void ThemeManager::saveToSettings(QSettings& s) const {
    s.setValue("UI/theme", themeName(m_theme));
}

QString ThemeManager::themeName(Theme t) {
    switch (t) {
    case Theme::Classic:       return "classic";
    default:                   return "enterprise-pro";
    }
}

ThemeManager::Theme ThemeManager::themeFromName(const QString& name) {
    const QString lower = name.toLower();
    if (lower == "classic")        return Theme::Classic;
    // Migration: old "dark" and "light" both map to EnterprisePro
    return Theme::EnterprisePro;
}
