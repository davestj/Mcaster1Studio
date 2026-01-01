/// @file   PodShowNotesModule.cpp
/// @path   Modules/PodShowNotesModule/PodShowNotesModule.cpp

#include "PodShowNotesModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace {

// ─── ChapterListWidget — chapter markers panel ─────────────────────────────
class ChapterListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChapterListWidget(M1::PodShowNotesModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodShowNotesChapterList");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        auto* header = new QLabel("Chapters");
        header->setObjectName("PodShowNotesChapterHeader");
        QFont hf = header->font();
        hf.setPixelSize(14);
        hf.setBold(true);
        header->setFont(hf);
        lay->addWidget(header);

        m_list = new QListWidget;
        m_list->setObjectName("PodShowNotesChapterListWidget");
        lay->addWidget(m_list, 1);

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(4);

        auto* addBtn = new QPushButton("+ Chapter");
        addBtn->setObjectName("PodShowNotesAddChapterBtn");
        addBtn->setToolTip("Add a new chapter marker");
        connect(addBtn, &QPushButton::clicked, this, &ChapterListWidget::onAddChapter);
        btnRow->addWidget(addBtn);

        auto* removeBtn = new QPushButton("Remove");
        removeBtn->setObjectName("PodShowNotesRemoveChapterBtn");
        removeBtn->setToolTip("Remove selected chapter");
        connect(removeBtn, &QPushButton::clicked, this, &ChapterListWidget::onRemoveChapter);
        btnRow->addWidget(removeBtn);

        lay->addLayout(btnRow);
    }

    void refresh() {
        m_list->clear();
        const auto chapters = m_mod->chapters();
        for (const auto& ch : chapters) {
            const int totalSecs = static_cast<int>(ch.timestampMs / 1000);
            const int mins = totalSecs / 60;
            const int secs = totalSecs % 60;
            m_list->addItem(QString("[%1:%2] %3")
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'))
                .arg(ch.title));
        }
    }

private slots:
    void onAddChapter() {
        bool ok = false;
        const QString title = QInputDialog::getText(this, "Add Chapter",
            "Chapter title:", QLineEdit::Normal, {}, &ok);
        if (!ok || title.isEmpty()) return;

        bool tsOk = false;
        const int mins = QInputDialog::getInt(this, "Timestamp",
            "Minutes:", 0, 0, 9999, 1, &tsOk);
        if (!tsOk) return;

        const int secs = QInputDialog::getInt(this, "Timestamp",
            "Seconds:", 0, 0, 59, 1, &tsOk);
        if (!tsOk) return;

        const qint64 ms = static_cast<qint64>((mins * 60 + secs) * 1000);
        m_mod->addChapter(ms, title);
        refresh();
    }

    void onRemoveChapter() {
        const int row = m_list->currentRow();
        if (row >= 0) {
            m_mod->removeChapter(row);
            refresh();
        }
    }

private:
    M1::PodShowNotesModule* m_mod;
    QListWidget*            m_list;
};

// ─── ResourceListWidget — resource links panel ─────────────────────────────
class ResourceListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ResourceListWidget(M1::PodShowNotesModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodShowNotesResourceList");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        auto* header = new QLabel("Resources");
        header->setObjectName("PodShowNotesResourceHeader");
        QFont hf = header->font();
        hf.setPixelSize(14);
        hf.setBold(true);
        header->setFont(hf);
        lay->addWidget(header);

        m_list = new QListWidget;
        m_list->setObjectName("PodShowNotesResourceListWidget");
        lay->addWidget(m_list, 1);

        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(4);

        auto* addBtn = new QPushButton("+ Link");
        addBtn->setObjectName("PodShowNotesAddLinkBtn");
        addBtn->setToolTip("Add a resource link");
        connect(addBtn, &QPushButton::clicked, this, &ResourceListWidget::onAddResource);
        btnRow->addWidget(addBtn);

        auto* removeBtn = new QPushButton("Remove");
        removeBtn->setObjectName("PodShowNotesRemoveLinkBtn");
        removeBtn->setToolTip("Remove selected link");
        connect(removeBtn, &QPushButton::clicked, this, &ResourceListWidget::onRemoveResource);
        btnRow->addWidget(removeBtn);

        lay->addLayout(btnRow);
    }

    void refresh() {
        m_list->clear();
        const auto resources = m_mod->resources();
        for (const auto& r : resources)
            m_list->addItem(QString("%1 — %2").arg(r.title, r.url));
    }

private slots:
    void onAddResource() {
        bool ok = false;
        const QString title = QInputDialog::getText(this, "Add Resource",
            "Link title:", QLineEdit::Normal, {}, &ok);
        if (!ok || title.isEmpty()) return;

        const QString url = QInputDialog::getText(this, "Add Resource",
            "URL:", QLineEdit::Normal, "https://", &ok);
        if (!ok || url.isEmpty()) return;

        m_mod->addResource(title, url);
        refresh();
    }

    void onRemoveResource() {
        const int row = m_list->currentRow();
        if (row >= 0) {
            m_mod->removeResource(row);
            refresh();
        }
    }

private:
    M1::PodShowNotesModule* m_mod;
    QListWidget*            m_list;
};

// ─── PodShowNotesWidget — main editor widget ────────────────────────────────
class PodShowNotesWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodShowNotesWidget(M1::PodShowNotesModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodShowNotesWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        // ── Toolbar ──────────────────────────────────────────────────────
        auto* toolbar = new QToolBar;
        toolbar->setObjectName("PodShowNotesToolbar");
        toolbar->setIconSize(QSize(16, 16));

        auto* boldAct = toolbar->addAction("B");
        boldAct->setToolTip("Bold (Ctrl+B)");
        QFont bf = boldAct->font();
        bf.setBold(true);
        boldAct->setFont(bf);
        connect(boldAct, &QAction::triggered, this, &PodShowNotesWidget::toggleBold);

        auto* italicAct = toolbar->addAction("I");
        italicAct->setToolTip("Italic (Ctrl+I)");
        QFont itf = italicAct->font();
        itf.setItalic(true);
        italicAct->setFont(itf);
        connect(italicAct, &QAction::triggered, this, &PodShowNotesWidget::toggleItalic);

        auto* linkAct = toolbar->addAction("Link");
        linkAct->setToolTip("Insert hyperlink");
        connect(linkAct, &QAction::triggered, this, &PodShowNotesWidget::insertLink);

        auto* timestampAct = toolbar->addAction("TS");
        timestampAct->setToolTip("Insert timestamp link");
        connect(timestampAct, &QAction::triggered, this, &PodShowNotesWidget::insertTimestamp);

        toolbar->addSeparator();

        // Template dropdown
        m_templateCombo = new QComboBox;
        m_templateCombo->setObjectName("PodShowNotesTemplateCombo");
        m_templateCombo->addItem("-- Template --");
        m_templateCombo->addItem("Standard");
        m_templateCombo->addItem("Interview");
        m_templateCombo->addItem("Solo Episode");
        m_templateCombo->setToolTip("Apply a show notes template");
        connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &PodShowNotesWidget::onTemplateSelected);
        toolbar->addWidget(m_templateCombo);

        toolbar->addSeparator();

        // Export buttons
        auto* exportHtmlAct = toolbar->addAction("HTML");
        exportHtmlAct->setToolTip("Export as HTML");
        connect(exportHtmlAct, &QAction::triggered, this, &PodShowNotesWidget::onExportHtml);

        auto* exportMdAct = toolbar->addAction("MD");
        exportMdAct->setToolTip("Export as Markdown");
        connect(exportMdAct, &QAction::triggered, this, &PodShowNotesWidget::onExportMarkdown);

        auto* exportTxtAct = toolbar->addAction("TXT");
        exportTxtAct->setToolTip("Export as plain text");
        connect(exportTxtAct, &QAction::triggered, this, &PodShowNotesWidget::onExportText);

        root->addWidget(toolbar);

        // ── Guest Bio Section ────────────────────────────────────────────
        auto* guestGroup = new QGroupBox("Guest");
        guestGroup->setObjectName("PodShowNotesGuestGroup");
        auto* guestLay = new QHBoxLayout(guestGroup);
        guestLay->setContentsMargins(4, 4, 4, 4);
        guestLay->setSpacing(4);

        guestLay->addWidget(new QLabel("Name:"));
        m_guestNameEdit = new QLineEdit(mod->guestName());
        m_guestNameEdit->setObjectName("PodShowNotesGuestName");
        m_guestNameEdit->setPlaceholderText("Guest name");
        m_guestNameEdit->setToolTip("Guest full name");
        guestLay->addWidget(m_guestNameEdit);

        guestLay->addWidget(new QLabel("Bio:"));
        m_guestBioEdit = new QLineEdit(mod->guestBio());
        m_guestBioEdit->setObjectName("PodShowNotesGuestBio");
        m_guestBioEdit->setPlaceholderText("Short biography...");
        m_guestBioEdit->setToolTip("Guest biography / intro");
        guestLay->addWidget(m_guestBioEdit, 1);

        connect(m_guestNameEdit, &QLineEdit::textChanged, this, [this]() {
            m_mod->setGuestBio(m_guestNameEdit->text(), m_guestBioEdit->text());
        });
        connect(m_guestBioEdit, &QLineEdit::textChanged, this, [this]() {
            m_mod->setGuestBio(m_guestNameEdit->text(), m_guestBioEdit->text());
        });

        root->addWidget(guestGroup);

        // ── Main editor + side panels ────────────────────────────────────
        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->setObjectName("PodShowNotesSplitter");

        // Editor
        m_editor = new QTextEdit;
        m_editor->setObjectName("PodShowNotesEditor");
        m_editor->setAcceptRichText(true);
        m_editor->setPlaceholderText("Write your show notes here...");
        m_editor->setToolTip("WYSIWYG show notes editor");
        if (!mod->content().isEmpty())
            m_editor->setHtml(mod->content());
        connect(m_editor, &QTextEdit::textChanged, this, &PodShowNotesWidget::onEditorChanged);
        splitter->addWidget(m_editor);

        // Side panel: chapters + resources
        auto* sidePanel = new QWidget;
        sidePanel->setObjectName("PodShowNotesSidePanel");
        auto* sideLay = new QVBoxLayout(sidePanel);
        sideLay->setContentsMargins(0, 0, 0, 0);
        sideLay->setSpacing(4);

        m_chapterWidget = new ChapterListWidget(mod);
        sideLay->addWidget(m_chapterWidget, 1);

        m_resourceWidget = new ResourceListWidget(mod);
        sideLay->addWidget(m_resourceWidget, 1);

        splitter->addWidget(sidePanel);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);

        root->addWidget(splitter, 1);

        // ── Word count status bar ────────────────────────────────────────
        m_wordCountLabel = new QLabel("Words: 0");
        m_wordCountLabel->setObjectName("PodShowNotesWordCount");
        m_wordCountLabel->setAlignment(Qt::AlignRight);
        QFont wf = m_wordCountLabel->font();
        wf.setPixelSize(12);
        m_wordCountLabel->setFont(wf);
        root->addWidget(m_wordCountLabel);

        // Initial refresh
        m_chapterWidget->refresh();
        m_resourceWidget->refresh();
        updateWordCount();

        connect(mod, &M1::PodShowNotesModule::chaptersChanged, m_chapterWidget, &ChapterListWidget::refresh);
        connect(mod, &M1::PodShowNotesModule::resourcesChanged, m_resourceWidget, &ResourceListWidget::refresh);
    }

private slots:
    void toggleBold() {
        QTextCharFormat fmt;
        const QTextCursor cursor = m_editor->textCursor();
        fmt.setFontWeight(cursor.charFormat().fontWeight() == QFont::Bold
                              ? QFont::Normal : QFont::Bold);
        m_editor->mergeCurrentCharFormat(fmt);
    }

    void toggleItalic() {
        QTextCharFormat fmt;
        const QTextCursor cursor = m_editor->textCursor();
        fmt.setFontItalic(!cursor.charFormat().fontItalic());
        m_editor->mergeCurrentCharFormat(fmt);
    }

    void insertLink() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, "Insert Link",
            "URL:", QLineEdit::Normal, "https://", &ok);
        if (!ok || url.isEmpty()) return;

        const QString text = m_editor->textCursor().selectedText();
        const QString label = text.isEmpty() ? url : text;
        m_editor->insertHtml(QString("<a href=\"%1\">%2</a> ").arg(url, label));
    }

    void insertTimestamp() {
        bool ok = false;
        const int mins = QInputDialog::getInt(this, "Timestamp",
            "Minutes:", 0, 0, 9999, 1, &ok);
        if (!ok) return;

        const int secs = QInputDialog::getInt(this, "Timestamp",
            "Seconds:", 0, 0, 59, 1, &ok);
        if (!ok) return;

        const QString ts = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
        const qint64 ms = static_cast<qint64>((mins * 60 + secs) * 1000);

        m_editor->insertHtml(QString("<a href=\"timestamp://%1\">[%2]</a> ")
            .arg(ms).arg(ts));
    }

    void onTemplateSelected(int index) {
        if (index <= 0) return;
        const QString templateName = m_templateCombo->itemText(index);
        m_mod->applyTemplate(templateName);
        m_editor->setHtml(m_mod->content());
        m_templateCombo->setCurrentIndex(0);
        updateWordCount();
    }

    void onEditorChanged() {
        m_mod->setContent(m_editor->toHtml());
        updateWordCount();
    }

    void onExportHtml() {
        const QString path = QFileDialog::getSaveFileName(this, "Export HTML",
            {}, "HTML Files (*.html)");
        if (!path.isEmpty()) {
            if (m_mod->exportHtml(path))
                QMessageBox::information(this, "Export", "HTML exported successfully.");
            else
                QMessageBox::warning(this, "Export", "Failed to export HTML.");
        }
    }

    void onExportMarkdown() {
        const QString path = QFileDialog::getSaveFileName(this, "Export Markdown",
            {}, "Markdown Files (*.md)");
        if (!path.isEmpty()) {
            if (m_mod->exportMarkdown(path))
                QMessageBox::information(this, "Export", "Markdown exported successfully.");
            else
                QMessageBox::warning(this, "Export", "Failed to export Markdown.");
        }
    }

    void onExportText() {
        const QString path = QFileDialog::getSaveFileName(this, "Export Text",
            {}, "Text Files (*.txt)");
        if (!path.isEmpty()) {
            if (m_mod->exportText(path))
                QMessageBox::information(this, "Export", "Text exported successfully.");
            else
                QMessageBox::warning(this, "Export", "Failed to export text.");
        }
    }

private:
    void updateWordCount() {
        m_wordCountLabel->setText(QString("Words: %1").arg(m_mod->wordCount()));
    }

    M1::PodShowNotesModule* m_mod;
    QTextEdit*              m_editor;
    QComboBox*              m_templateCombo;
    QLineEdit*              m_guestNameEdit;
    QLineEdit*              m_guestBioEdit;
    QLabel*                 m_wordCountLabel;
    ChapterListWidget*      m_chapterWidget;
    ResourceListWidget*     m_resourceWidget;
};

} // anonymous namespace

#include "PodShowNotesModule.moc"

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PodShowNotesModule::PodShowNotesModule(QObject* parent) : IModule(parent) {}
PodShowNotesModule::~PodShowNotesModule() = default;

// ─── IModule lifecycle ────────────────────────────────────────────────────────

void PodShowNotesModule::initialize() {
    qInfo() << "[PodShowNotesModule] Initialized.";
}

void PodShowNotesModule::shutdown() {
    qInfo() << "[PodShowNotesModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────

QWidget* PodShowNotesModule::createWidget(QWidget* parent) {
    return new PodShowNotesWidget(this, parent);
}

// ─── Content API ──────────────────────────────────────────────────────────────

void PodShowNotesModule::setContent(const QString& html) {
    m_html = html;
    emit contentChanged();
}

QString PodShowNotesModule::content() const {
    return m_html;
}

// ─── Chapter management ──────────────────────────────────────────────────────

void PodShowNotesModule::addChapter(qint64 timestampMs, const QString& title) {
    ChapterMarker ch;
    ch.timestampMs = timestampMs;
    ch.title       = title.isEmpty()
        ? QString("Chapter %1").arg(m_chapters.size() + 1)
        : title;
    m_chapters.append(ch);
    emit chaptersChanged();
}

void PodShowNotesModule::removeChapter(int index) {
    if (index >= 0 && index < m_chapters.size()) {
        m_chapters.removeAt(index);
        emit chaptersChanged();
    }
}

// ─── Resource link management ────────────────────────────────────────────────

void PodShowNotesModule::addResource(const QString& title, const QString& url) {
    ResourceLink link;
    link.title = title;
    link.url   = url;
    m_resources.append(link);
    emit resourcesChanged();
}

void PodShowNotesModule::removeResource(int index) {
    if (index >= 0 && index < m_resources.size()) {
        m_resources.removeAt(index);
        emit resourcesChanged();
    }
}

// ─── Guest bio ───────────────────────────────────────────────────────────────

void PodShowNotesModule::setGuestBio(const QString& name, const QString& bio) {
    m_guestName = name;
    m_guestBio  = bio;
}

// ─── Templates ───────────────────────────────────────────────────────────────

void PodShowNotesModule::applyTemplate(const QString& templateName) {
    if (templateName == "Standard") {
        m_html =
            "<h2>Episode Title</h2>"
            "<p>Episode summary goes here.</p>"
            "<h3>Topics Covered</h3>"
            "<ul>"
            "<li>Topic 1</li>"
            "<li>Topic 2</li>"
            "<li>Topic 3</li>"
            "</ul>"
            "<h3>Links &amp; Resources</h3>"
            "<ul>"
            "<li><a href=\"\">Resource 1</a></li>"
            "</ul>"
            "<h3>Contact</h3>"
            "<p>Email: | Twitter: | Website:</p>";
    } else if (templateName == "Interview") {
        m_html =
            "<h2>Interview: [Guest Name]</h2>"
            "<h3>About the Guest</h3>"
            "<p>[Guest bio and background]</p>"
            "<h3>Key Takeaways</h3>"
            "<ol>"
            "<li>Takeaway 1</li>"
            "<li>Takeaway 2</li>"
            "<li>Takeaway 3</li>"
            "</ol>"
            "<h3>Timestamps</h3>"
            "<ul>"
            "<li>[00:00] Introduction</li>"
            "<li>[00:00] Topic discussion</li>"
            "<li>[00:00] Closing thoughts</li>"
            "</ul>"
            "<h3>Guest Links</h3>"
            "<ul>"
            "<li>Website: </li>"
            "<li>Social: </li>"
            "</ul>"
            "<h3>Sponsor</h3>"
            "<p>[Sponsor message]</p>";
    } else if (templateName == "Solo Episode") {
        m_html =
            "<h2>Episode Title</h2>"
            "<p><em>A solo episode about [topic].</em></p>"
            "<h3>Overview</h3>"
            "<p>[Brief overview of what this episode covers]</p>"
            "<h3>Main Points</h3>"
            "<ol>"
            "<li>Point 1</li>"
            "<li>Point 2</li>"
            "<li>Point 3</li>"
            "</ol>"
            "<h3>Action Items</h3>"
            "<ul>"
            "<li>Action 1</li>"
            "<li>Action 2</li>"
            "</ul>"
            "<h3>Resources Mentioned</h3>"
            "<ul>"
            "<li><a href=\"\">Resource</a></li>"
            "</ul>";
    }
    emit contentChanged();
}

// ─── Export ──────────────────────────────────────────────────────────────────

bool PodShowNotesModule::exportHtml(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out << "<!DOCTYPE html>\n<html>\n<head>\n"
        << "<meta charset=\"UTF-8\">\n"
        << "<title>Show Notes</title>\n"
        << "</head>\n<body>\n";

    out << m_html << "\n";

    // Append chapters
    if (!m_chapters.isEmpty()) {
        out << "<h3>Chapters</h3>\n<ul>\n";
        for (const auto& ch : m_chapters) {
            const int totalSecs = static_cast<int>(ch.timestampMs / 1000);
            const int mins = totalSecs / 60;
            const int secs = totalSecs % 60;
            out << QString("<li>[%1:%2] %3</li>\n")
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'))
                .arg(ch.title);
        }
        out << "</ul>\n";
    }

    // Append guest bio
    if (!m_guestName.isEmpty()) {
        out << "<h3>Guest: " << m_guestName << "</h3>\n";
        if (!m_guestBio.isEmpty())
            out << "<p>" << m_guestBio << "</p>\n";
    }

    // Append resources
    if (!m_resources.isEmpty()) {
        out << "<h3>Resources</h3>\n<ul>\n";
        for (const auto& r : m_resources)
            out << QString("<li><a href=\"%1\">%2</a></li>\n").arg(r.url, r.title);
        out << "</ul>\n";
    }

    out << "</body>\n</html>\n";
    return true;
}

bool PodShowNotesModule::exportMarkdown(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);

    // Convert HTML to plain text for markdown base
    QTextDocument doc;
    doc.setHtml(m_html);
    const QString plainContent = doc.toPlainText();

    out << plainContent << "\n\n";

    // Append chapters
    if (!m_chapters.isEmpty()) {
        out << "## Chapters\n\n";
        for (const auto& ch : m_chapters) {
            const int totalSecs = static_cast<int>(ch.timestampMs / 1000);
            const int mins = totalSecs / 60;
            const int secs = totalSecs % 60;
            out << QString("- [%1:%2] %3\n")
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'))
                .arg(ch.title);
        }
        out << "\n";
    }

    // Append guest bio
    if (!m_guestName.isEmpty()) {
        out << "## Guest: " << m_guestName << "\n\n";
        if (!m_guestBio.isEmpty())
            out << m_guestBio << "\n\n";
    }

    // Append resources
    if (!m_resources.isEmpty()) {
        out << "## Resources\n\n";
        for (const auto& r : m_resources)
            out << QString("- [%1](%2)\n").arg(r.title, r.url);
        out << "\n";
    }

    return true;
}

bool PodShowNotesModule::exportText(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);

    // Convert HTML to plain text
    QTextDocument doc;
    doc.setHtml(m_html);
    out << doc.toPlainText() << "\n\n";

    // Append chapters
    if (!m_chapters.isEmpty()) {
        out << "CHAPTERS\n";
        out << QString("-").repeated(40) << "\n";
        for (const auto& ch : m_chapters) {
            const int totalSecs = static_cast<int>(ch.timestampMs / 1000);
            const int mins = totalSecs / 60;
            const int secs = totalSecs % 60;
            out << QString("[%1:%2] %3\n")
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'))
                .arg(ch.title);
        }
        out << "\n";
    }

    // Append guest bio
    if (!m_guestName.isEmpty()) {
        out << "GUEST: " << m_guestName << "\n";
        if (!m_guestBio.isEmpty())
            out << m_guestBio << "\n";
        out << "\n";
    }

    // Append resources
    if (!m_resources.isEmpty()) {
        out << "RESOURCES\n";
        out << QString("-").repeated(40) << "\n";
        for (const auto& r : m_resources)
            out << QString("%1 - %2\n").arg(r.title, r.url);
        out << "\n";
    }

    return true;
}

// ─── Word count ──────────────────────────────────────────────────────────────

int PodShowNotesModule::wordCount() const {
    QTextDocument doc;
    doc.setHtml(m_html);
    const QString plain = doc.toPlainText().trimmed();
    if (plain.isEmpty()) return 0;

    static const QRegularExpression ws("\\s+");
    return static_cast<int>(plain.split(ws, Qt::SkipEmptyParts).size());
}

// ─── State persistence ───────────────────────────────────────────────────────

void PodShowNotesModule::saveState(QSettings& s) {
    s.setValue("html", m_html);
    s.setValue("guestName", m_guestName);
    s.setValue("guestBio", m_guestBio);

    s.beginWriteArray("chapters", m_chapters.size());
    for (int i = 0; i < m_chapters.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("timestampMs", m_chapters[i].timestampMs);
        s.setValue("title", m_chapters[i].title);
    }
    s.endArray();

    s.beginWriteArray("resources", m_resources.size());
    for (int i = 0; i < m_resources.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("title", m_resources[i].title);
        s.setValue("url", m_resources[i].url);
    }
    s.endArray();
}

void PodShowNotesModule::loadState(QSettings& s) {
    m_html      = s.value("html").toString();
    m_guestName = s.value("guestName").toString();
    m_guestBio  = s.value("guestBio").toString();

    m_chapters.clear();
    const int chCount = s.beginReadArray("chapters");
    for (int i = 0; i < chCount; ++i) {
        s.setArrayIndex(i);
        ChapterMarker ch;
        ch.timestampMs = s.value("timestampMs", 0).toLongLong();
        ch.title       = s.value("title").toString();
        m_chapters.append(ch);
    }
    s.endArray();

    m_resources.clear();
    const int resCount = s.beginReadArray("resources");
    for (int i = 0; i < resCount; ++i) {
        s.setArrayIndex(i);
        ResourceLink link;
        link.title = s.value("title").toString();
        link.url   = s.value("url").toString();
        m_resources.append(link);
    }
    s.endArray();
}

} // namespace M1
