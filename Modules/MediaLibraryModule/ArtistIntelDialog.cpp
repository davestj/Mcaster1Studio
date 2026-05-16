/*
 * Mcaster1Studio — Broadcast Automation Software Suite
 * ArtistIntelDialog.cpp — AI-powered Artist Intel Report dialog
 *
 * Copyright (c) 2026 David St. John <davestj@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ArtistIntelDialog.h"
#include "AiTrackIntel.h"
#include "SqliteManager.h"
#include "ThemePalette.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextDocument>
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QMessageBox>
#include <QTabBar>
#include <QMenu>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QFrame>
#include <QRegularExpression>
#include <QApplication>

// ── Enterprise Pro HTML CSS for QTextBrowser content ─────────────────────────

static QString buildContentCss(const ThemePalette& pal)
{
    const bool light = ThemePalette::isLightTheme();
    return QStringLiteral(
        "<style>"
        "body { background: %1; color: %2; "
        "  font-family: 'Segoe UI', -apple-system, Helvetica, Arial, sans-serif; "
        "  font-size: 13px; line-height: 1.65; margin: 0; padding: 6px 10px; }"
        "h1 { color: %3; font-size: 17px; border-bottom: 2px solid %3; "
        "  padding-bottom: 6px; margin-top: 0; }"
        "h2 { color: %2; font-size: 15px; border-bottom: 1px solid %4; "
        "  padding-bottom: 4px; margin-top: 18px; }"
        "h3 { color: %5; font-size: 13px; margin-top: 14px; }"
        "a  { color: %3; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "ul { padding-left: 20px; margin: 6px 0; }"
        "li { margin: 3px 0; }"
        "p  { margin: 6px 0; }"
        "strong { font-weight: 600; }"
        "table { width: 100%%; border-collapse: collapse; margin: 10px 0; "
        "  border: 1px solid %4; }"
        "th { background: %6; color: %3; font-weight: 600; "
        "  padding: 7px 10px; text-align: left; border: 1px solid %4; font-size: 12px; }"
        "td { padding: 6px 10px; border: 1px solid %4; font-size: 12px; }"
        "tr:nth-child(even) td { background: %7; }"
        "blockquote { border-left: 3px solid %3; margin: 10px 0; "
        "  padding: 6px 14px; background: %6; }"
        "hr { border: none; border-top: 1px solid %4; margin: 14px 0; }"
        "code { background: %6; color: %3; padding: 1px 5px; border-radius: 3px; "
        "  border: 1px solid %4; font-family: 'Consolas', monospace; font-size: 12px; }"
        ".badge { display: inline-block; background: %6; color: %3; "
        "  border: 1px solid %4; border-radius: 10px; padding: 2px 8px; "
        "  font-size: 11px; margin: 2px; }"
        ".section-header { background: %6; padding: 8px 14px; border-radius: 4px; "
        "  border-left: 4px solid %3; margin-bottom: 12px; font-weight: 600; }"
        "</style>")
        .arg(pal.panelBg.name(),     // %1 body bg
             pal.text.name(),         // %2 body text
             pal.accent.name(),       // %3 accent (h1, links, th)
             pal.border.name(),       // %4 borders
             pal.textMuted.name(),    // %5 h3 muted
             pal.cardBg.name(),       // %6 card bg
             light ? QStringLiteral("#f8f6f3") : QStringLiteral("#1e1a14")); // %7 alt row
}

static QString wrapHtml(const QString& bodyHtml, const ThemePalette& pal)
{
    return QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\">")
           + buildContentCss(pal)
           + QStringLiteral("</head><body>")
           + bodyHtml
           + QStringLiteral("</body></html>");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════════

ArtistIntelDialog::ArtistIntelDialog(const QString& artistName,
                                       M1::AiTrackIntel* aiService,
                                       M1::SqliteManager* db,
                                       QWidget* parent)
    : QDialog(parent)
    , m_artistName(artistName)
    , m_ai(aiService)
    , m_db(db)
{
    setWindowFlags(Qt::Window
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint
                   | Qt::WindowCloseButtonHint);
    setWindowTitle(QStringLiteral("Artist Intel \u2014 %1").arg(m_artistName));
    resize(1100, 750);
    setSizeGripEnabled(true);

    setupUi();
    startOverviewGeneration();
}

// ═══════════════════════════════════════════════════════════════════════════════
// UI Construction
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::setupUi()
{
    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // ── Dialog-level stylesheet ─────────────────────────────────────────────
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel { color: %2; background: transparent; }"
        "QSplitter::handle { background: %3; width: 3px; }"
        "QTextBrowser { background: %4; color: %2; border: none; "
        "  selection-background-color: %5; selection-color: #FFFFFF; }"
        "QTabWidget::pane { border: 1px solid %3; background: %4; top: -1px; }"
        "QTabBar::tab { background: %1; color: %6; padding: 6px 16px; "
        "  border: 1px solid %3; border-bottom: none; "
        "  border-radius: 4px 4px 0 0; min-width: 90px; font-size: 12px; }"
        "QTabBar::tab:selected { background: %4; color: %5; font-weight: 600; "
        "  border-bottom-color: %5; }"
        "QTabBar::tab:hover { color: %2; }"
        "QPushButton { background: %4; color: %2; border: 1px solid %3; "
        "  border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
        "QPushButton:hover { border-color: %5; color: %5; }"
        "QPushButton:pressed { background: %1; }"
        "QPushButton:disabled { color: %6; border-color: %3; }"
        "QLineEdit { background: %4; color: %2; border: 1px solid %3; "
        "  border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
        "QLineEdit:focus { border-color: %5; }"
        "QFrame { border: 1px solid %3; }")
        .arg(pal.bg.name(),            // %1
             pal.text.name(),          // %2
             pal.border.name(),        // %3
             pal.panelBg.name(),       // %4
             pal.accent.name(),        // %5
             pal.textMuted.name()));   // %6

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(52);
    header->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 2px solid %2; }")
        .arg(pal.cardBg.name(), pal.accent.name()));
    auto* hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(14, 0, 14, 0);

    m_headerLabel = new QLabel(
        QStringLiteral("Generating: %1").arg(m_artistName), this);
    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 16px; font-weight: bold; }")
        .arg(pal.accent.name()));
    hdrLay->addWidget(m_headerLabel, 1);

    m_statusBadge = new QLabel(QStringLiteral("AI-STANDBY"), this);
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %2; "
        "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
        "  font-weight: bold; letter-spacing: 1px; }")
        .arg(pal.cardBg.name(), pal.textMuted.name()));
    hdrLay->addWidget(m_statusBadge);

    m_statusMsg = new QLabel(this);
    m_statusMsg->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(pal.textMuted.name()));
    hdrLay->addWidget(m_statusMsg);

    root->addWidget(header);

    // ── Main splitter: left sidebar | right tabs ────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(3);

    // ── Left sidebar ────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(splitter);
    sidebar->setFixedWidth(200);
    sidebar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-right: 1px solid %2; }")
        .arg(pal.bg.name(), pal.border.name()));
    auto* sideLay = new QVBoxLayout(sidebar);
    sideLay->setContentsMargins(8, 10, 8, 10);
    sideLay->setSpacing(6);

    // Report theme label
    auto* themeLbl = new QLabel(QStringLiteral("REPORT THEME"), sidebar);
    themeLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold; "
        "  letter-spacing: 1px; padding: 4px 0; border-top: 1px solid %2; }")
        .arg(pal.accent.name(), pal.border.name()));
    sideLay->addWidget(themeLbl);

    auto* themeNote = new QLabel(QStringLiteral("Enterprise Pro"), sidebar);
    themeNote->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-style: italic; }")
        .arg(pal.textMuted.name()));
    sideLay->addWidget(themeNote);

    // AI Research label
    auto* researchLbl = new QLabel(QStringLiteral("AI RESEARCH"), sidebar);
    researchLbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold; "
        "  letter-spacing: 1px; padding: 4px 0; margin-top: 8px; "
        "  border-top: 1px solid %2; }")
        .arg(pal.accent.name(), pal.border.name()));
    sideLay->addWidget(researchLbl);

    // Helper lambda: create a sidebar research button
    const QString btnStyle = QStringLiteral(
        "QPushButton { text-align: left; padding: 7px 10px; font-size: 12px; "
        "  background: %1; color: %2; border: 1px solid %3; border-radius: 4px; }"
        "QPushButton:hover { border-color: %4; color: %4; }"
        "QPushButton:disabled { color: %5; border-color: %3; }")
        .arg(pal.panelBg.name(), pal.text.name(), pal.border.name(),
             pal.accent.name(), pal.textMuted.name());

    auto makeBtn = [&](const QString& text, QPushButton*& member) {
        member = new QPushButton(text, sidebar);
        member->setFixedHeight(36);
        member->setMinimumWidth(170);
        member->setStyleSheet(btnStyle);
        member->setEnabled(false); // Disabled until overview completes
        sideLay->addWidget(member);
    };

    makeBtn(QStringLiteral("Touring History"),     m_btnTouring);
    makeBtn(QStringLiteral("Musical Influences"),  m_btnInfluences);
    makeBtn(QStringLiteral("Awards & Charts"),     m_btnAwards);
    makeBtn(QStringLiteral("Broadcaster Script"),  m_btnBroadcaster);
    makeBtn(QStringLiteral("Fan Base & Impact"),   m_btnFanBase);
    makeBtn(QStringLiteral("Full Timeline"),       m_btnTimeline);
    makeBtn(QStringLiteral("Gear & Equipment"),    m_btnGear);
    makeBtn(QStringLiteral("Discovery"),             m_btnDiscovery);

    connect(m_btnTouring,     &QPushButton::clicked, this, &ArtistIntelDialog::onResearchTouring);
    connect(m_btnInfluences,  &QPushButton::clicked, this, &ArtistIntelDialog::onResearchInfluences);
    connect(m_btnAwards,      &QPushButton::clicked, this, &ArtistIntelDialog::onResearchAwards);
    connect(m_btnBroadcaster, &QPushButton::clicked, this, &ArtistIntelDialog::onResearchBroadcaster);
    connect(m_btnFanBase,     &QPushButton::clicked, this, &ArtistIntelDialog::onResearchFanBase);
    connect(m_btnTimeline,    &QPushButton::clicked, this, &ArtistIntelDialog::onResearchTimeline);
    connect(m_btnGear,        &QPushButton::clicked, this, &ArtistIntelDialog::onResearchGear);
    connect(m_btnDiscovery,   &QPushButton::clicked, this, &ArtistIntelDialog::onResearchDiscovery);

    sideLay->addStretch(1);
    splitter->addWidget(sidebar);

    // ── Right: tab widget ───────────────────────────────────────────────────
    m_tabs = new QTabWidget(splitter);
    m_tabs->setDocumentMode(true);

    m_overviewBrowser  = new QTextBrowser(); m_overviewBrowser->setOpenExternalLinks(true);
    m_discoBrowser     = new QTextBrowser(); m_discoBrowser->setOpenExternalLinks(true);
    m_imagesBrowser    = new QTextBrowser(); m_imagesBrowser->setOpenExternalLinks(true);
    m_djScriptBrowser  = new QTextBrowser(); m_djScriptBrowser->setOpenExternalLinks(true);

    m_tabs->addTab(m_overviewBrowser,  QStringLiteral("Overview"));
    m_tabs->addTab(m_discoBrowser,     QStringLiteral("Discography"));
    m_tabs->addTab(m_imagesBrowser,    QStringLiteral("Images"));
    m_tabs->addTab(m_djScriptBrowser,  QStringLiteral("DJ Script"));

    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // ── Right-click context menu on tabs for Refresh ─────────────────────
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        const int tabIdx = m_tabs->tabBar()->tabAt(pos);
        if (tabIdx < 0) return;

        QMenu menu(this);
        auto* refreshAct = menu.addAction(QStringLiteral("Refresh Report"));
        auto* saveIndAct = menu.addAction(QStringLiteral("Save Individual Report as HTML..."));
        auto* chosen = menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
        if (!chosen) return;

        // ── Save individual tab as HTML ──
        if (chosen == saveIndAct) {
            auto* browser = qobject_cast<QTextBrowser*>(m_tabs->widget(tabIdx));
            if (!browser) return;
            const QString tabName = m_tabs->tabText(tabIdx).remove('*').trimmed();
            const QString safeName = QString(m_artistName + "_" + tabName)
                .replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
            const QString path = QFileDialog::getSaveFileName(this,
                "Save " + tabName + " Report",
                safeName + ".html",
                "HTML Files (*.html)");
            if (path.isEmpty()) return;
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << browser->toHtml();
                m_statusMsg->setText(QString("Saved: %1").arg(QFileInfo(path).fileName()));
            }
            return;
        }

        if (chosen != refreshAct) return;

        // Determine which tab this is and re-trigger generation
        const QString tabText = m_tabs->tabText(tabIdx);
        auto* browser = qobject_cast<QTextBrowser*>(m_tabs->widget(tabIdx));
        if (!browser) return;

        if (tabIdx == 0) {
            // Overview tab — force re-generate from AI (skip DB cache)
            m_overviewDone = false;
            m_forceRegenerate = true;
            startOverviewGeneration();
        } else {
            // Research tab or fixed tab — find and re-run the research prompt
            // Remove the tab from the research cache so sendResearchPrompt recreates it
            for (auto it = m_researchTabs.begin(); it != m_researchTabs.end(); ++it) {
                if (it.value() == browser) {
                    QString title = it.key();
                    m_researchTabs.erase(it);
                    m_researchMarkdown.remove(title);
                    m_tabs->removeTab(tabIdx);
                    delete browser;
                    // Re-click the corresponding sidebar button to retrigger
                    if (title == "Touring History")       { onResearchTouring(); return; }
                    if (title == "Musical Influences")    { onResearchInfluences(); return; }
                    if (title == "Awards & Charts")       { onResearchAwards(); return; }
                    if (title == "Broadcaster Script")    { onResearchBroadcaster(); return; }
                    if (title == "Fan Base & Impact")     { onResearchFanBase(); return; }
                    if (title == "Full Timeline")         { onResearchTimeline(); return; }
                    if (title == "Gear & Equipment")      { onResearchGear(); return; }
                    if (title == "Discovery")             { onResearchDiscovery(); return; }
                    break;
                }
            }
        }
    });

    root->addWidget(splitter, 1);

    // ── Chat bar ────────────────────────────────────────────────────────────
    auto* chatBar = new QWidget(this);
    chatBar->setFixedHeight(42);
    chatBar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top: 1px solid %2; }")
        .arg(pal.cardBg.name(), pal.border.name()));
    auto* chatLay = new QHBoxLayout(chatBar);
    chatLay->setContentsMargins(10, 4, 10, 4);
    chatLay->setSpacing(6);

    auto* chatIcon = new QLabel(QStringLiteral("Ask AI:"), chatBar);
    chatIcon->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; font-weight: bold; }")
        .arg(pal.accent.name()));
    chatLay->addWidget(chatIcon);

    m_chatInput = new QLineEdit(chatBar);
    m_chatInput->setPlaceholderText(QStringLiteral("Ask AI anything about this artist..."));
    chatLay->addWidget(m_chatInput, 1);

    m_askBtn = new QPushButton(QStringLiteral("Ask"), chatBar);
    m_askBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: none; border-radius: 4px; "
        "  padding: 4px 14px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %5; }")
        .arg(pal.accent.name(),
             pal.panelBg.name(),
             pal.accentHover.name(),
             pal.border.name(),
             pal.textMuted.name()));
    chatLay->addWidget(m_askBtn);

    connect(m_askBtn, &QPushButton::clicked, this, &ArtistIntelDialog::onAskCustom);
    connect(m_chatInput, &QLineEdit::returnPressed, m_askBtn, &QPushButton::click);

    root->addWidget(chatBar);

    // ── Bottom button bar ───────────────────────────────────────────────────
    auto* bottom = new QWidget(this);
    bottom->setFixedHeight(46);
    bottom->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top: 1px solid %2; }")
        .arg(pal.cardBg.name(), pal.border.name()));
    auto* botLay = new QHBoxLayout(bottom);
    botLay->setContentsMargins(14, 0, 14, 0);

    m_saveHtmlBtn = new QPushButton(QStringLiteral("Save as HTML"), this);
    m_saveHtmlBtn->setEnabled(false);
    connect(m_saveHtmlBtn, &QPushButton::clicked, this, &ArtistIntelDialog::onSaveAsHtml);
    botLay->addWidget(m_saveHtmlBtn);

    botLay->addStretch(1);

    m_saveReportBtn = new QPushButton(QStringLiteral("Save Report"), this);
    m_saveReportBtn->setObjectName(QStringLiteral("saveBtn"));
    m_saveReportBtn->setEnabled(false);
    m_saveReportBtn->setStyleSheet(QStringLiteral(
        "QPushButton#saveBtn { background: %1; color: %2; border: 1px solid %1; "
        "  font-weight: bold; }"
        "QPushButton#saveBtn:hover { background: %3; }"
        "QPushButton#saveBtn:disabled { background: %4; color: %5; border-color: %4; }")
        .arg(pal.accent.name(), pal.panelBg.name(), pal.accentHover.name(),
             pal.border.name(), pal.textMuted.name()));
    connect(m_saveReportBtn, &QPushButton::clicked, this, &ArtistIntelDialog::onSaveReport);
    botLay->addWidget(m_saveReportBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    botLay->addWidget(m_cancelBtn);

    root->addWidget(bottom);

    // ── Initial placeholder content ─────────────────────────────────────────
    const QString placeholder = wrapHtml(
        QStringLiteral(
            "<div style='text-align:center;padding:60px 20px;'>"
            "<p style='font-size:28px;margin:0 0 12px;'>&#9889;</p>"
            "<p style='color:%1;font-size:14px;font-weight:600;margin:0 0 6px;'>"
            "Generating Artist Intel Report</p>"
            "<p style='color:%2;font-size:12px;font-style:italic;margin:0;'>"
            "Querying AI provider for %3 ...</p>"
            "</div>")
            .arg(pal.accent.name(), pal.textMuted.name(), m_artistName.toHtmlEscaped()),
        pal);
    m_overviewBrowser->setHtml(placeholder);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DJ Script Extraction Helper
// ═══════════════════════════════════════════════════════════════════════════════

static QString extractDjScript(const QString& reportText)
{
    // Look for a DJ script section in the AI output
    static const QStringList markers = {
        QStringLiteral("## DJ Script"),
        QStringLiteral("## Broadcaster Script"),
        QStringLiteral("## On-Air Script"),
        QStringLiteral("### DJ Script"),
        QStringLiteral("### Broadcaster Script"),
    };
    for (const QString& marker : markers) {
        const int idx = reportText.indexOf(marker, 0, Qt::CaseInsensitive);
        if (idx >= 0) {
            // Extract from marker to end or next ## heading
            QString section = reportText.mid(idx);
            const int nextH2 = section.indexOf(QStringLiteral("\n## "), marker.length());
            if (nextH2 > 0) section = section.left(nextH2);
            return section;
        }
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════════════════════
// Overview Generation
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::startOverviewGeneration()
{
    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // ── Check for saved intel first — load from DB to save resources ──────
    // Skip DB if user explicitly requested a refresh (m_forceRegenerate)
    if (m_db && !m_overviewDone && !m_forceRegenerate) {
        const auto profiles = m_db->artistIntelProfiles(m_artistName);
        if (!profiles.isEmpty()) {
            // Load the most recent saved report
            const auto& latest = profiles.last();
            const QString savedText = latest.value("profile_text").toString();
            const QString savedJson = latest.value("discography_json").toString();
            const QString savedBackend = latest.value("ai_backend").toString();
            const QString savedModel = latest.value("ai_model").toString();

            if (!savedText.trimmed().isEmpty()) {
                qInfo() << "[ArtistIntelDialog] Loading saved intel for"
                        << m_artistName << "from DB";

                // Feed the overview into the overview handler
                onOverviewReady(m_artistName, savedText, savedJson,
                                savedBackend, savedModel);

                // Restore research tabs from the JSON stored in discography_json.
                // Validate that keys are actual research tab names — not raw API
                // response fields (model, created_at, done_reason, etc.) from a
                // corrupted save where the Ollama response JSON was stored instead.
                static const QSet<QString> validTabPrefixes = {
                    "Touring", "Musical", "Awards", "Broadcaster", "Fan Base",
                    "Full Timeline", "Gear", "Discovery", "Q:"
                };
                if (!savedJson.isEmpty() && savedJson != "{}") {
                    const QJsonDocument tabsDoc = QJsonDocument::fromJson(savedJson.toUtf8());
                    if (tabsDoc.isObject()) {
                        const QJsonObject tabsObj = tabsDoc.object();
                        for (auto it = tabsObj.constBegin(); it != tabsObj.constEnd(); ++it) {
                            const QString tabTitle = it.key();
                            const QString tabMd = it.value().toString();
                            if (tabMd.trimmed().isEmpty()) continue;

                            // Skip keys that look like raw API response fields
                            bool isValid = false;
                            for (const auto& prefix : validTabPrefixes) {
                                if (tabTitle.startsWith(prefix)) { isValid = true; break; }
                            }
                            if (!isValid) continue;  // skip "model", "created_at", etc.

                            auto* browser = new QTextBrowser(m_tabs);
                            browser->setOpenExternalLinks(true);
                            m_researchTabs[tabTitle] = browser;
                            m_researchMarkdown[tabTitle] = tabMd;

                            browser->setHtml(wrapHtml(markdownToHtml(tabMd), pal));
                            m_tabs->addTab(browser,
                                QStringLiteral("\u2713 %1").arg(tabTitle));
                        }
                    }
                }

                // Update header to show this is saved data
                m_statusBadge->setText(QStringLiteral("SAVED"));
                m_statusBadge->setStyleSheet(QStringLiteral(
                    "QLabel { background: %1; color: %2; border: 1px solid %2; "
                    "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
                    "  font-weight: bold; letter-spacing: 1px; }")
                    .arg(pal.cardBg.name(), pal.success.name()));

                const int tabCount = m_researchTabs.size();
                m_statusMsg->setText(tabCount > 0
                    ? QStringLiteral("Loaded saved report (%1 research tabs). "
                                     "Right-click any tab to Refresh.").arg(tabCount)
                    : QStringLiteral("Loaded saved report. Use AI Research buttons for deep-dives."));

                setWindowTitle(QStringLiteral("Artist Intel \u2014 %1").arg(m_artistName));
                return;
            }
        }
    }

    // ── No saved data (or force refresh) — generate fresh from AI ─────────
    m_forceRegenerate = false;  // reset flag
    if (!m_ai) return;

    m_statusBadge->setText(QStringLiteral("AI-GENERATING"));
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %2; "
        "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
        "  font-weight: bold; letter-spacing: 1px; }")
        .arg(pal.cardBg.name(), pal.warning.name()));
    m_statusMsg->setText(QStringLiteral("Generating AI artist intel report..."));

    // Connect signals from AiTrackIntel
    connect(m_ai, &M1::AiTrackIntel::profileReady,
            this, &ArtistIntelDialog::onOverviewReady);
    connect(m_ai, &M1::AiTrackIntel::lookupFailed,
            this, &ArtistIntelDialog::onOverviewFailed);

    m_ai->lookupArtist(m_artistName);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Overview Result Handlers
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::onOverviewReady(const QString& artistName,
                                          const QString& profileText,
                                          const QString& discographyJson,
                                          const QString& aiBackend,
                                          const QString& aiModel)
{
    // Only handle our artist
    if (artistName != m_artistName) return;

    // Disconnect so we don't re-trigger on research calls
    disconnect(m_ai, &M1::AiTrackIntel::profileReady,
               this, &ArtistIntelDialog::onOverviewReady);
    disconnect(m_ai, &M1::AiTrackIntel::lookupFailed,
               this, &ArtistIntelDialog::onOverviewFailed);

    m_overviewMarkdown = profileText;
    m_overviewBackend  = aiBackend;
    m_overviewModel    = aiModel;
    m_overviewDone     = true;

    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // Render overview
    const QString bodyHtml = markdownToHtml(profileText);
    m_overviewBrowser->setHtml(wrapHtml(bodyHtml, pal));

    // Render discography tab (raw JSON as formatted text)
    if (!discographyJson.isEmpty()) {
        m_discoBrowser->setHtml(wrapHtml(
            QStringLiteral("<div class='section-header'><strong>Discography Data</strong></div>"
                           "<p>Raw AI response data available. "
                           "Use the research buttons for detailed discography analysis.</p>"),
            pal));
    }

    // DJ Script tab — extract if present in the overview text
    const QString djSection = extractDjScript(profileText);
    if (!djSection.isEmpty()) {
        m_djScriptBrowser->setHtml(wrapHtml(markdownToHtml(djSection), pal));
    } else {
        m_djScriptBrowser->setHtml(wrapHtml(
            QStringLiteral("<p style='color:%1;font-style:italic;'>"
                           "No DJ script found in overview. "
                           "Click <strong>Broadcaster Script</strong> in the sidebar "
                           "for custom on-air scripts.</p>")
                .arg(pal.textMuted.name()),
            pal));
    }

    // Update header
    m_headerLabel->setText(QStringLiteral("Artist Intel: %1").arg(m_artistName));
    m_statusBadge->setText(QStringLiteral("AI-STANDBY"));
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %2; "
        "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
        "  font-weight: bold; letter-spacing: 1px; }")
        .arg(pal.cardBg.name(), pal.success.name()));
    m_statusMsg->setText(QStringLiteral(
        "Overview ready \u2014 Click AI Research buttons for deep-dives."));

    // Enable research buttons and save
    m_btnTouring->setEnabled(true);
    m_btnInfluences->setEnabled(true);
    m_btnAwards->setEnabled(true);
    m_btnBroadcaster->setEnabled(true);
    m_btnFanBase->setEnabled(true);
    m_btnTimeline->setEnabled(true);
    m_btnGear->setEnabled(true);
    m_saveReportBtn->setEnabled(true);
    m_saveHtmlBtn->setEnabled(true);
}

void ArtistIntelDialog::onOverviewFailed(const QString& artistName,
                                           const QString& error)
{
    if (artistName != m_artistName) return;

    disconnect(m_ai, &M1::AiTrackIntel::profileReady,
               this, &ArtistIntelDialog::onOverviewReady);
    disconnect(m_ai, &M1::AiTrackIntel::lookupFailed,
               this, &ArtistIntelDialog::onOverviewFailed);

    const ThemePalette pal = ThemePalette::forCurrentTheme();

    const bool isBusy = error.contains("busy", Qt::CaseInsensitive)
                     || error.contains("loading", Qt::CaseInsensitive)
                     || error.contains("timeout", Qt::CaseInsensitive)
                     || error.contains("503", Qt::CaseInsensitive);

    if (isBusy) {
        m_statusBadge->setText(QStringLiteral("AI-BUSY"));
        m_statusBadge->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %2; "
            "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; }")
            .arg(pal.cardBg.name(), pal.warning.name()));
        m_statusMsg->setText(QStringLiteral("AI model busy \u2014 retrying in 5 seconds..."));

        m_overviewBrowser->setHtml(wrapHtml(
            QStringLiteral("<div style='text-align:center;padding:40px 20px;'>"
                           "<p style='color:%1;font-size:16px;font-weight:600;'>"
                           "AI Model Busy</p>"
                           "<p style='color:%2;font-size:12px;'>%3</p>"
                           "<p style='color:%2;font-size:12px;font-style:italic;'>"
                           "Retrying automatically in 5 seconds...</p>"
                           "</div>")
                .arg(pal.warning.name(), pal.textMuted.name(), error.toHtmlEscaped()),
            pal));

        // Auto-retry after 5 seconds
        QTimer::singleShot(5000, this, [this]() { startOverviewGeneration(); });
    } else {
        m_statusBadge->setText(QStringLiteral("AI-ERROR"));
        m_statusBadge->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %2; "
            "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; }")
            .arg(pal.cardBg.name(), pal.error.name()));
        m_statusMsg->setText(QStringLiteral("Failed. Right-click Overview tab to retry."));

        m_overviewBrowser->setHtml(wrapHtml(
            QStringLiteral("<div style='text-align:center;padding:40px 20px;'>"
                           "<p style='color:%1;font-size:16px;font-weight:600;'>"
                           "AI Lookup Failed</p>"
                           "<p style='color:%2;font-size:12px;'>%3</p>"
                           "<p style='color:%2;font-size:12px;font-style:italic;'>"
                           "Right-click the Overview tab and select Refresh Report to try again.</p>"
                           "</div>")
                .arg(pal.error.name(), pal.textMuted.name(), error.toHtmlEscaped()),
            pal));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Research Prompt Infrastructure
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::sendResearchPrompt(const QString& systemPrompt,
                                             const QString& userPrompt,
                                             const QString& tabTitle,
                                             QPushButton* btn)
{
    if (!m_ai) return;

    // Guard: query already in flight
    if (m_researchLocked) {
        m_statusMsg->setText(QStringLiteral(
            "Please wait \u2014 \"%1\" is still generating...")
            .arg(m_activeResearchTitle));
        QTimer::singleShot(3500, this, [this]() {
            if (m_statusMsg) m_statusMsg->setText(QString());
        });
        return;
    }

    // If this tab already has completed content, just switch to it
    auto it = m_researchTabs.find(tabTitle);
    if (it != m_researchTabs.end()) {
        m_tabs->setCurrentWidget(it.value());
        return;
    }

    // Create new tab for this research category
    auto* browser = new QTextBrowser(m_tabs);
    browser->setOpenExternalLinks(true);
    m_researchTabs[tabTitle] = browser;
    m_activeResearchTitle = tabTitle;
    m_activeResearchBtn = btn;

    m_tabs->addTab(browser, QStringLiteral("* %1").arg(tabTitle));
    m_tabs->setCurrentWidget(browser);

    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // Placeholder
    browser->setHtml(wrapHtml(
        QStringLiteral(
            "<div style='text-align:center;padding:48px 20px;'>"
            "<p style='font-size:28px;margin:0 0 12px;'>&#9889;</p>"
            "<p style='color:%1;font-size:14px;font-weight:600;margin:0 0 6px;'>"
            "Researching: %2</p>"
            "<p style='color:%3;font-size:12px;font-style:italic;margin:0;'>"
            "AI response generating \u2014 please wait...</p>"
            "</div>")
            .arg(pal.accent.name(), tabTitle.toHtmlEscaped(), pal.textMuted.name()),
        pal));

    // Lock research buttons
    m_researchLocked = true;
    m_btnTouring->setEnabled(false);
    m_btnInfluences->setEnabled(false);
    m_btnAwards->setEnabled(false);
    m_btnBroadcaster->setEnabled(false);
    m_btnFanBase->setEnabled(false);
    m_btnTimeline->setEnabled(false);
    m_btnGear->setEnabled(false);

    // Update status
    m_statusBadge->setText(QStringLiteral("AI-GENERATING"));
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %2; "
        "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
        "  font-weight: bold; letter-spacing: 1px; }")
        .arg(pal.cardBg.name(), pal.warning.name()));
    m_statusMsg->setText(QStringLiteral("Researching: %1...").arg(tabTitle));

    // Use sendCustomPrompt to fire the research query
    m_ai->sendCustomPrompt(tabTitle, systemPrompt, userPrompt);

    // Connect for this research result
    connect(m_ai, &M1::AiTrackIntel::customPromptReady,
            this, &ArtistIntelDialog::onResearchReady);
    connect(m_ai, &M1::AiTrackIntel::customPromptFailed,
            this, [this, systemPrompt, userPrompt, tabTitle, btn]
            (const QString& contextName, const QString& error) {
        Q_UNUSED(contextName);
        const ThemePalette pal = ThemePalette::forCurrentTheme();

        disconnect(m_ai, &M1::AiTrackIntel::customPromptReady,
                   this, &ArtistIntelDialog::onResearchReady);
        disconnect(m_ai, &M1::AiTrackIntel::customPromptFailed, this, nullptr);

        // Check if Ollama is busy (model loading, queue full, etc.) — auto-retry
        const bool isBusy = error.contains("busy", Qt::CaseInsensitive)
                         || error.contains("loading", Qt::CaseInsensitive)
                         || error.contains("queue", Qt::CaseInsensitive)
                         || error.contains("timeout", Qt::CaseInsensitive)
                         || error.contains("503", Qt::CaseInsensitive);

        auto it = m_researchTabs.find(m_activeResearchTitle);
        if (it != m_researchTabs.end()) {
            if (isBusy) {
                it.value()->setHtml(wrapHtml(
                    QStringLiteral(
                        "<div style='text-align:center;padding:40px 20px;'>"
                        "<p style='color:%1;font-size:14px;font-weight:600;'>"
                        "AI model is busy \u2014 retrying in 5 seconds...</p>"
                        "<p style='color:%2;font-size:12px;font-style:italic;'>%3</p>"
                        "</div>")
                        .arg(pal.warning.name(), pal.textMuted.name(), error.toHtmlEscaped()),
                    pal));
            } else {
                it.value()->setHtml(wrapHtml(
                    QStringLiteral(
                        "<div style='text-align:center;padding:40px 20px;'>"
                        "<p style='color:%1;font-size:14px;font-weight:600;'>"
                        "Research failed</p>"
                        "<p style='color:%2;font-size:12px;'>%3</p>"
                        "<p style='color:%2;font-size:12px;font-style:italic;'>"
                        "Right-click this tab and select Refresh Report to retry.</p>"
                        "</div>")
                        .arg(pal.error.name(), pal.textMuted.name(), error.toHtmlEscaped()),
                    pal));
            }
        }

        // Unlock buttons
        m_researchLocked = false;
        m_statusMsg->setText(isBusy
            ? QStringLiteral("AI busy \u2014 retrying %1 in 5s...").arg(tabTitle)
            : QStringLiteral("Research failed. Right-click tab to retry."));

        // Re-enable buttons for categories not yet completed
        for (auto* b : { m_btnTouring, m_btnInfluences, m_btnAwards,
                         m_btnBroadcaster, m_btnFanBase, m_btnTimeline,
                         m_btnGear, m_btnDiscovery }) {
            if (!b) continue;
            bool completed = false;
            for (auto mIt = m_researchMarkdown.cbegin(); mIt != m_researchMarkdown.cend(); ++mIt) {
                if (b->text().contains(mIt.key())) { completed = true; break; }
            }
            if (!completed) b->setEnabled(true);
        }

        m_statusBadge->setText(QStringLiteral("AI-STANDBY"));
        m_statusBadge->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %2; "
            "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; }")
            .arg(pal.cardBg.name(), isBusy ? pal.warning.name() : pal.success.name()));

        // Auto-retry after 5 seconds if model was busy
        if (isBusy) {
            // Remove the failed tab so sendResearchPrompt can recreate it
            auto tabIt = m_researchTabs.find(tabTitle);
            if (tabIt != m_researchTabs.end()) {
                const int tabIdx = m_tabs->indexOf(tabIt.value());
                if (tabIdx >= 0) m_tabs->removeTab(tabIdx);
                delete tabIt.value();
                m_researchTabs.erase(tabIt);
            }
            QTimer::singleShot(5000, this, [this, systemPrompt, userPrompt, tabTitle, btn]() {
                sendResearchPrompt(systemPrompt, userPrompt, tabTitle, btn);
            });
        }
    });
}

void ArtistIntelDialog::onResearchReady(const QString& contextName,
                                          const QString& text,
                                          const QString& /*json*/,
                                          const QString& /*backend*/,
                                          const QString& /*model*/)
{
    // Disconnect to avoid double-handling
    disconnect(m_ai, &M1::AiTrackIntel::customPromptReady,
               this, &ArtistIntelDialog::onResearchReady);
    disconnect(m_ai, &M1::AiTrackIntel::customPromptFailed, this, nullptr);

    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // Find the tab for this research
    auto it = m_researchTabs.find(contextName);
    if (it != m_researchTabs.end()) {
        const QString bodyHtml = markdownToHtml(text);
        it.value()->setHtml(wrapHtml(bodyHtml, pal));
        // Update tab title to remove the generating indicator
        const int tabIdx = m_tabs->indexOf(it.value());
        if (tabIdx >= 0)
            m_tabs->setTabText(tabIdx, contextName);
    }

    // Store raw markdown for export
    m_researchMarkdown[contextName] = text;

    // Disable the button that triggered this (visual: it's done)
    if (m_activeResearchBtn) {
        m_activeResearchBtn->setEnabled(false);
        m_activeResearchBtn->setStyleSheet(QStringLiteral(
            "QPushButton { text-align: left; padding: 7px 10px; font-size: 12px; "
            "  background: %1; color: %2; border: 1px solid %2; border-radius: 4px; }")
            .arg(pal.cardBg.name(), pal.success.name()));
        m_activeResearchBtn = nullptr;
    }

    // Unlock research buttons (re-enable uncompleted ones)
    m_researchLocked = false;
    for (auto* btn : { m_btnTouring, m_btnInfluences, m_btnAwards,
                       m_btnBroadcaster, m_btnFanBase, m_btnTimeline, m_btnGear }) {
        if (!btn) continue;
        bool completed = false;
        for (auto mIt = m_researchMarkdown.cbegin(); mIt != m_researchMarkdown.cend(); ++mIt) {
            if (btn->text().contains(mIt.key())) { completed = true; break; }
        }
        if (!completed) btn->setEnabled(true);
    }

    // Update status
    m_statusBadge->setText(QStringLiteral("AI-STANDBY"));
    m_statusBadge->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %2; "
        "  border-radius: 8px; padding: 2px 10px; font-size: 11px; "
        "  font-weight: bold; letter-spacing: 1px; }")
        .arg(pal.cardBg.name(), pal.success.name()));
    m_statusMsg->setText(QStringLiteral("%1 research complete.").arg(contextName));
}

// ═══════════════════════════════════════════════════════════════════════════════
// 7 Research Button Slots
// ═══════════════════════════════════════════════════════════════════════════════

static QString artistContext(const QString& artistName, const QString& overviewMd)
{
    if (overviewMd.isEmpty())
        return QStringLiteral("Artist: %1").arg(artistName);
    return QStringLiteral("Artist: %1\n\nContext from overview report:\n%2")
           .arg(artistName, overviewMd.left(800));
}

void ArtistIntelDialog::onResearchTouring()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music journalist and tour historian. "
                       "Write a detailed, broadcaster-ready touring history."),
        QStringLiteral("%1\n\nWrite a comprehensive **Touring History** covering:\n"
                       "- All major world tours with years and names\n"
                       "- Record-setting concert moments\n"
                       "- Famous live venues and festivals\n"
                       "- Notable support acts\n"
                       "- Live albums and concert films\n"
                       "Format as Markdown with tables.").arg(ctx),
        QStringLiteral("Touring History"),
        m_btnTouring);
}

void ArtistIntelDialog::onResearchInfluences()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music critic and historian. "
                       "Analyze musical influences and legacy with depth."),
        QStringLiteral("%1\n\nWrite a detailed **Musical Influences & Sound Evolution** covering:\n"
                       "- Who influenced this artist/band and how\n"
                       "- How their sound evolved across eras\n"
                       "- Bands and artists they influenced in turn\n"
                       "- Signature production techniques\n"
                       "- Genre innovations they pioneered\n"
                       "Format as Markdown.").arg(ctx),
        QStringLiteral("Musical Influences"),
        m_btnInfluences);
}

void ArtistIntelDialog::onResearchAwards()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music industry analyst. "
                       "Compile a comprehensive awards and chart history."),
        QStringLiteral("%1\n\nWrite a detailed **Awards, Charts & Critical Reception** covering:\n"
                       "- Grammy nominations and wins (with years)\n"
                       "- Other major awards (Brit Awards, MTV VMAs, etc.)\n"
                       "- Billboard chart positions for albums and singles\n"
                       "- UK and international chart performances\n"
                       "- Critical reception and Metacritic/Pitchfork scores\n"
                       "- Certification milestones (Gold, Platinum, Diamond)\n"
                       "Format as Markdown with tables.").arg(ctx),
        QStringLiteral("Awards & Charts"),
        m_btnAwards);
}

void ArtistIntelDialog::onResearchBroadcaster()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a professional radio broadcaster and DJ. "
                       "Write ready-to-read on-air scripts for a live show."),
        QStringLiteral("%1\n\nWrite a **Complete Broadcaster Script Package** containing:\n"
                       "- 3 x 30-second intro scripts (different moods: energetic, smooth, nostalgic)\n"
                       "- 5 song intro one-liners (fun, punchy, can be read live)\n"
                       "- 3 x fun listener trivia facts to drop on air\n"
                       "- 2 x outro/wrap-up lines after playing a song\n"
                       "- 1 x 60-second artist spotlight feature script\n"
                       "Write them ready to read, not as instructions.").arg(ctx),
        QStringLiteral("Broadcaster Script"),
        m_btnBroadcaster);
}

void ArtistIntelDialog::onResearchFanBase()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music marketing analyst. "
                       "Analyze fan culture and cultural impact."),
        QStringLiteral("%1\n\nWrite a **Fan Base & Cultural Impact** report covering:\n"
                       "- Fan community names and culture\n"
                       "- Demographics and geographic strongholds\n"
                       "- Cultural movements and social impact\n"
                       "- Merchandise, branding and commercial legacy\n"
                       "- Notable celebrity fans\n"
                       "- Social media presence and streaming numbers\n"
                       "- Tribute bands, cover artists, sampling in other music\n"
                       "Format as Markdown.").arg(ctx),
        QStringLiteral("Fan Base & Impact"),
        m_btnFanBase);
}

void ArtistIntelDialog::onResearchTimeline()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music historian. "
                       "Create an authoritative chronological timeline."),
        QStringLiteral("%1\n\nCreate a **Complete Chronological Timeline** as a Markdown table:\n"
                       "| Year | Event |\n|------|-------|\n"
                       "Include: formation, member changes, album releases, tours, "
                       "breakups/reunions, deaths, awards, controversies, and current status. "
                       "Be specific with dates and details. "
                       "Cover every significant year from formation to present.").arg(ctx),
        QStringLiteral("Full Timeline"),
        m_btnTimeline);
}

void ArtistIntelDialog::onResearchGear()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a gear expert, music equipment historian, and tech journalist. "
                       "Write a broadcaster-ready technical equipment intelligence report "
                       "for gear-head audiences \u2014 guitarists, audiophiles, and music tech fans."),
        QStringLiteral("%1\n\n"
                       "Write a **Gear & Equipment Intelligence Report** covering every known "
                       "instrument, amplifier, effects, and studio gear for this artist. Include:\n\n"
                       "**Guitars & String Instruments**\n"
                       "- Signature guitars, models, custom specs, custom colors, years used\n"
                       "- Vintage and rare pieces; notable auction/collection items\n"
                       "- Acoustic instruments and other string gear\n\n"
                       "**Amplifiers & Cabinets**\n"
                       "- Known amp heads, combos, cabinet configurations\n"
                       "- Vintage vs modern rigs; live vs studio differences\n\n"
                       "**Effects & Signal Chain**\n"
                       "- Signature pedals, effect units, rack gear\n"
                       "- Pedalboard evolution across eras/tours\n\n"
                       "**Bass & Low-End Gear**\n"
                       "- Bass guitars, strings, bass amps and cabs\n\n"
                       "**Drums & Percussion**\n"
                       "- Drum kits, cymbals, hardware, electronic pads\n"
                       "- Signature drumhead designs\n\n"
                       "**Keyboards, Synths & Electronics**\n"
                       "- Keyboards, synthesizers, samplers, MIDI controllers\n"
                       "- Studio production gear and DAW usage\n\n"
                       "**Microphones & Vocal Gear**\n"
                       "- Preferred microphone models (live and studio)\n"
                       "- In-ear monitors, vocal processing\n\n"
                       "**Brand Endorsements & Sponsorships**\n"
                       "- Official gear endorsements and sponsor relationships\n"
                       "- Signature product lines and collaborations\n\n"
                       "Format as Markdown with clear sections. "
                       "Include model numbers, years, and broadcaster talking points.").arg(ctx),
        QStringLiteral("Gear & Equipment"),
        m_btnGear);
}

void ArtistIntelDialog::onResearchDiscovery()
{
    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);

    // Read the user's AI persona from settings (falls back to a broadcast DJ default)
    QSettings settings("Mcaster1", "Mcaster1Studio");
    QString persona = settings.value("ai/persona/global",
        QStringLiteral("You are a professional radio DJ and music curator with deep knowledge "
                       "of music history, genres, and audience engagement. You recommend music "
                       "that keeps listeners engaged and fits seamlessly into broadcast rotations."))
        .toString();

    const QString systemPrompt = persona + QStringLiteral(
        "\n\nYou are also a music discovery engine. Your job is to recommend artists and tracks "
        "that a broadcaster, DJ, podcast host, or music curator would want to add to their "
        "playlist, queue, or rotation. Be specific with track names and albums. "
        "Think like a program director building a station playlist.");

    sendResearchPrompt(
        systemPrompt,
        QStringLiteral("%1\n\n"
            "Based on this artist, generate a **Discovery Report** \u2014 a curated list of "
            "similar artists and tracks that a broadcaster or DJ would want to spin, queue, or "
            "research next. Organize into these sections:\n\n"
            "## Artists You Should Be Playing\n"
            "List 10-15 artists similar in style, energy, and audience appeal. For each:\n"
            "- Artist name\n"
            "- Why they fit (1 sentence \u2014 genre, vibe, crossover appeal)\n"
            "- Top 3 tracks to start with (title + album + year)\n"
            "- DJ tip: when/how to mix them into a set or rotation\n\n"
            "## Deep Cuts & Hidden Gems\n"
            "5-8 lesser-known tracks from similar artists that hardcore fans and music-savvy "
            "listeners will appreciate. Include track name, artist, album, year, and why it's special.\n\n"
            "## Genre Bridges\n"
            "3-5 artists from adjacent genres that would surprise listeners in a good way. "
            "Explain the crossover appeal and which tracks to use as bridges.\n\n"
            "## Next Up: Research These Artists\n"
            "5 artists the user should generate full AI Intel reports on next. "
            "Explain why each one deserves a deep-dive.\n\n"
            "## Playlist Starter Pack\n"
            "A ready-to-use 20-track playlist mixing this artist with the discoveries above. "
            "Format as a numbered list: Artist \u2014 Track (Album, Year)\n"
            "Order them for smooth transitions and energy flow.\n\n"
            "Format as Markdown. Be specific with real track names, albums, and years. "
            "Think like a program director, not a search engine.").arg(ctx),
        QStringLiteral("Discovery"),
        m_btnDiscovery);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Custom "Ask AI" Chat
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::onAskCustom()
{
    const QString question = m_chatInput->text().trimmed();
    if (question.isEmpty() || !m_ai) return;

    const QString ctx = artistContext(m_artistName, m_overviewMarkdown);
    sendResearchPrompt(
        QStringLiteral("You are a music expert and broadcaster research assistant. "
                       "Answer the user's question about this artist thoroughly and accurately. "
                       "Use Markdown formatting with headings and bullet points."),
        QStringLiteral("%1\n\n**User Question:** %2").arg(ctx, question),
        QStringLiteral("Q: %1").arg(question.left(30)),
        nullptr);  // No specific button to disable for custom queries
    m_chatInput->clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Save Report to Database
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::onSaveReport()
{
    if (!m_db) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
            QStringLiteral("No database connection available.\n\n"
                           "Check your database settings in Preferences."));
        return;
    }
    if (m_overviewMarkdown.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
            QStringLiteral("No report content to save.\n\n"
                           "Generate an overview first, then try saving."));
        return;
    }

    // Save overview as profile_text, all research tabs as JSON in discography_json
    QJsonObject tabsJson;
    for (auto it = m_researchMarkdown.cbegin(); it != m_researchMarkdown.cend(); ++it)
        tabsJson[it.key()] = it.value();

    const QString tabsJsonStr = QString::fromUtf8(
        QJsonDocument(tabsJson).toJson(QJsonDocument::Compact));

    const qint64 savedId = m_db->saveArtistIntel(
        m_artistName, m_overviewMarkdown, tabsJsonStr, m_overviewBackend, m_overviewModel);

    if (savedId > 0) {
        const int tabCount = m_researchMarkdown.size();
        const QString dbPath = m_db->databasePath();
        QMessageBox::information(this, QStringLiteral("Report Saved"),
            QStringLiteral("Artist Intel report saved successfully.\n\n"
                           "Artist: %1\n"
                           "Database: %2\n"
                           "Table: artist_intel\n"
                           "Record ID: %3\n"
                           "Overview: %4 chars\n"
                           "Research tabs: %5")
                .arg(m_artistName,
                     dbPath.isEmpty() ? QStringLiteral("Default SQLite") : dbPath)
                .arg(savedId)
                .arg(m_overviewMarkdown.size())
                .arg(tabCount));

        m_statusMsg->setText(QStringLiteral("Report saved (ID %1).").arg(savedId));
        const ThemePalette pal = ThemePalette::forCurrentTheme();
        m_statusMsg->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; font-weight: 600; }")
            .arg(pal.success.name()));

        setWindowTitle(QStringLiteral("Artist Intel \u2014 %1").arg(m_artistName));
    } else {
        const QString lastErr = m_db->lastError();
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
            QStringLiteral("Failed to save Artist Intel report.\n\n"
                           "Artist: %1\n"
                           "Database: %2\n"
                           "Table: artist_intel\n"
                           "Error: %3")
                .arg(m_artistName,
                     m_db->databasePath().isEmpty() ? QStringLiteral("Default SQLite") : m_db->databasePath(),
                     lastErr.isEmpty() ? QStringLiteral("Unknown error") : lastErr));

        m_statusMsg->setText(QStringLiteral("Save failed: %1").arg(lastErr));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Save as HTML Export
// ═══════════════════════════════════════════════════════════════════════════════

void ArtistIntelDialog::onSaveAsHtml()
{
    if (m_overviewMarkdown.isEmpty()) return;

    const QString safeName = QString(m_artistName)
        .replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_\\- ]")), QString())
        .simplified().replace(' ', '_');

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Artist Intel as HTML"),
        QStringLiteral("%1_Artist_Intel.html").arg(safeName),
        QStringLiteral("HTML Files (*.html)"));

    if (filePath.isEmpty()) return;

    const ThemePalette pal = ThemePalette::forCurrentTheme();

    // Build full HTML with all tabs
    QString fullBody;
    fullBody += QStringLiteral("<h1>Artist Intel Report: %1</h1>\n")
                    .arg(m_artistName.toHtmlEscaped());
    fullBody += QStringLiteral("<p><em>Generated via %1 / %2</em></p>\n")
                    .arg(m_overviewBackend.toHtmlEscaped(), m_overviewModel.toHtmlEscaped());
    fullBody += QStringLiteral("<hr>\n");

    // Overview
    fullBody += QStringLiteral("<h2>Overview</h2>\n");
    fullBody += markdownToHtml(m_overviewMarkdown);

    // All research sections
    for (auto it = m_researchMarkdown.cbegin(); it != m_researchMarkdown.cend(); ++it) {
        fullBody += QStringLiteral("\n<hr>\n<h2>%1</h2>\n").arg(it.key().toHtmlEscaped());
        fullBody += markdownToHtml(it.value());
    }

    fullBody += QStringLiteral("\n<hr>\n<p><em>Generated by Mcaster1Studio Artist Intel</em></p>\n");

    const QString html = wrapHtml(fullBody, pal);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << html;
        file.close();
        m_statusMsg->setText(QStringLiteral("HTML saved: %1").arg(filePath));
    } else {
        QMessageBox::warning(this, QStringLiteral("Save Failed"),
            QStringLiteral("Could not write to: %1").arg(filePath));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Markdown to HTML conversion
// ═══════════════════════════════════════════════════════════════════════════════

QString ArtistIntelDialog::markdownToHtml(const QString& md) const
{
    if (md.isEmpty()) return {};
    QTextDocument doc;
    doc.setMarkdown(md);
    QString full = doc.toHtml();
    // Extract body content from Qt's generated HTML
    const int bodyStart = full.indexOf(QStringLiteral("<body"));
    const int bodyEnd   = full.lastIndexOf(QStringLiteral("</body>"));
    if (bodyStart != -1 && bodyEnd != -1) {
        const int contentStart = full.indexOf('>', bodyStart) + 1;
        return full.mid(contentStart, bodyEnd - contentStart);
    }
    return full;
}
