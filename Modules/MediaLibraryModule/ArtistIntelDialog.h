/*
 * Mcaster1Studio — Broadcast Automation Software Suite
 * ArtistIntelDialog.h — AI-powered Artist Intel Report dialog
 *
 * Full-featured broadcaster-grade artist research window with:
 *   - Overview, Discography, Images, DJ Script tabs
 *   - 7 AI research deep-dive buttons (sidebar)
 *   - Custom "Ask AI" chat bar
 *   - Save to database + Export as HTML
 *   - Theme-aware rendering via ThemePalette
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>
#include <QTextBrowser>
#include <QTabWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMap>

namespace M1 {
class AiTrackIntel;
class SqliteManager;
} // namespace M1

class ArtistIntelDialog : public QDialog {
    Q_OBJECT
public:
    explicit ArtistIntelDialog(const QString& artistName,
                                M1::AiTrackIntel* aiService,
                                M1::SqliteManager* db,
                                QWidget* parent = nullptr);
    ~ArtistIntelDialog() override = default;

private slots:
    void onOverviewReady(const QString& artistName, const QString& profileText,
                          const QString& discographyJson, const QString& aiBackend,
                          const QString& aiModel);
    void onOverviewFailed(const QString& artistName, const QString& error);

    // 8 Research buttons
    void onResearchTouring();
    void onResearchInfluences();
    void onResearchAwards();
    void onResearchBroadcaster();
    void onResearchFanBase();
    void onResearchTimeline();
    void onResearchGear();
    void onResearchDiscovery();

    void onAskCustom();
    void onSaveReport();
    void onSaveAsHtml();

private:
    void setupUi();
    void startOverviewGeneration();
    void sendResearchPrompt(const QString& systemPrompt, const QString& userPrompt,
                             const QString& tabTitle, QPushButton* btn);
    void onResearchReady(const QString& artistName, const QString& text,
                          const QString& json, const QString& backend,
                          const QString& model);
    QString markdownToHtml(const QString& md) const;

    QString m_artistName;
    M1::AiTrackIntel* m_ai = nullptr;
    M1::SqliteManager* m_db = nullptr;

    // Header
    QLabel* m_headerLabel = nullptr;
    QLabel* m_statusBadge = nullptr;
    QLabel* m_statusMsg = nullptr;

    // Tabs
    QTabWidget* m_tabs = nullptr;
    QTextBrowser* m_overviewBrowser = nullptr;
    QTextBrowser* m_discoBrowser = nullptr;
    QTextBrowser* m_imagesBrowser = nullptr;
    QTextBrowser* m_djScriptBrowser = nullptr;

    // Research tabs (dynamic)
    QMap<QString, QTextBrowser*> m_researchTabs;
    QMap<QString, QString> m_researchMarkdown;
    bool m_researchLocked = false;
    QString m_activeResearchTitle;
    QPushButton* m_activeResearchBtn = nullptr;

    // Sidebar research buttons
    QPushButton* m_btnTouring = nullptr;
    QPushButton* m_btnInfluences = nullptr;
    QPushButton* m_btnAwards = nullptr;
    QPushButton* m_btnBroadcaster = nullptr;
    QPushButton* m_btnFanBase = nullptr;
    QPushButton* m_btnTimeline = nullptr;
    QPushButton* m_btnGear = nullptr;
    QPushButton* m_btnDiscovery = nullptr;

    // Chat
    QLineEdit* m_chatInput = nullptr;
    QPushButton* m_askBtn = nullptr;

    // Bottom bar
    QPushButton* m_saveReportBtn = nullptr;
    QPushButton* m_saveHtmlBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    // State
    QString m_overviewMarkdown;
    QString m_overviewBackend;
    QString m_overviewModel;
    bool m_overviewDone = false;
    bool m_forceRegenerate = false;  ///< Skip DB cache on next startOverviewGeneration()
};
