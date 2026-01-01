/// @file   PodRSSModule.cpp
/// @path   Modules/PodRSSModule/PodRSSModule.cpp

#include "PodRSSModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDateTimeEdit>
#include <QGroupBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QUuid>

namespace {

// ─── EpisodeDialog — modal dialog for adding/editing episodes ───────────────
class EpisodeDialog : public QDialog {
    Q_OBJECT
public:
    explicit EpisodeDialog(QWidget* parent = nullptr,
                           const M1::PodcastEpisode& ep = {})
        : QDialog(parent)
    {
        setWindowTitle(ep.title.isEmpty() ? "Add Episode" : "Edit Episode");
        setObjectName("PodRSSEpisodeDialog");
        setMinimumSize(450, 500);

        auto* lay = new QFormLayout(this);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(6);

        m_titleEdit = new QLineEdit(ep.title);
        m_titleEdit->setObjectName("PodRSSEpTitle");
        m_titleEdit->setPlaceholderText("Episode title");
        m_titleEdit->setToolTip("Episode title");
        lay->addRow("Title:", m_titleEdit);

        m_descEdit = new QTextEdit;
        m_descEdit->setObjectName("PodRSSEpDesc");
        m_descEdit->setPlainText(ep.description);
        m_descEdit->setMaximumHeight(100);
        m_descEdit->setToolTip("Episode description / summary");
        lay->addRow("Description:", m_descEdit);

        m_numberSpin = new QSpinBox;
        m_numberSpin->setRange(0, 99999);
        m_numberSpin->setValue(ep.number);
        m_numberSpin->setToolTip("Episode number");
        lay->addRow("Episode #:", m_numberSpin);

        m_seasonSpin = new QSpinBox;
        m_seasonSpin->setRange(0, 999);
        m_seasonSpin->setValue(ep.season);
        m_seasonSpin->setToolTip("Season number");
        lay->addRow("Season:", m_seasonSpin);

        m_typeCombo = new QComboBox;
        m_typeCombo->setObjectName("PodRSSEpType");
        m_typeCombo->addItems({"full", "trailer", "bonus"});
        m_typeCombo->setCurrentText(ep.type);
        m_typeCombo->setToolTip("Episode type");
        lay->addRow("Type:", m_typeCombo);

        m_pubDateEdit = new QDateTimeEdit(
            ep.pubDate.isValid() ? ep.pubDate : QDateTime::currentDateTime());
        m_pubDateEdit->setObjectName("PodRSSEpPubDate");
        m_pubDateEdit->setCalendarPopup(true);
        m_pubDateEdit->setToolTip("Publication date");
        lay->addRow("Pub Date:", m_pubDateEdit);

        m_enclosureUrlEdit = new QLineEdit(ep.enclosureUrl);
        m_enclosureUrlEdit->setObjectName("PodRSSEpEncUrl");
        m_enclosureUrlEdit->setPlaceholderText("https://example.com/ep001.mp3");
        m_enclosureUrlEdit->setToolTip("Audio file URL (enclosure)");
        lay->addRow("Enclosure URL:", m_enclosureUrlEdit);

        m_enclosureSizeSpin = new QSpinBox;
        m_enclosureSizeSpin->setObjectName("PodRSSEpEncSize");
        m_enclosureSizeSpin->setRange(0, 2147483647);
        m_enclosureSizeSpin->setValue(static_cast<int>(ep.enclosureSize));
        m_enclosureSizeSpin->setSuffix(" bytes");
        m_enclosureSizeSpin->setToolTip("Audio file size in bytes");
        lay->addRow("File Size:", m_enclosureSizeSpin);

        m_durationSpin = new QSpinBox;
        m_durationSpin->setObjectName("PodRSSEpDuration");
        m_durationSpin->setRange(0, 999999);
        m_durationSpin->setValue(ep.duration);
        m_durationSpin->setSuffix(" sec");
        m_durationSpin->setToolTip("Episode duration in seconds");
        lay->addRow("Duration:", m_durationSpin);

        m_explicitCheck = new QCheckBox;
        m_explicitCheck->setChecked(ep.isExplicit);
        m_explicitCheck->setToolTip("Mark episode as explicit content");
        lay->addRow("Explicit:", m_explicitCheck);

        m_transcriptUrlEdit = new QLineEdit(ep.transcriptUrl);
        m_transcriptUrlEdit->setObjectName("PodRSSEpTranscript");
        m_transcriptUrlEdit->setPlaceholderText("https://example.com/ep001.srt");
        m_transcriptUrlEdit->setToolTip("Transcript file URL (optional)");
        lay->addRow("Transcript URL:", m_transcriptUrlEdit);

        auto* btnBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addRow(btnBox);
    }

    M1::PodcastEpisode episode() const {
        M1::PodcastEpisode ep;
        ep.title         = m_titleEdit->text();
        ep.description   = m_descEdit->toPlainText();
        ep.number        = m_numberSpin->value();
        ep.season        = m_seasonSpin->value();
        ep.type          = m_typeCombo->currentText();
        ep.pubDate       = m_pubDateEdit->dateTime();
        ep.enclosureUrl  = m_enclosureUrlEdit->text();
        ep.enclosureSize = m_enclosureSizeSpin->value();
        ep.duration      = m_durationSpin->value();
        ep.isExplicit    = m_explicitCheck->isChecked();
        ep.transcriptUrl = m_transcriptUrlEdit->text();
        ep.guid          = ep.enclosureUrl.isEmpty()
                               ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                               : ep.enclosureUrl;
        return ep;
    }

private:
    QLineEdit*      m_titleEdit;
    QTextEdit*      m_descEdit;
    QSpinBox*       m_numberSpin;
    QSpinBox*       m_seasonSpin;
    QComboBox*      m_typeCombo;
    QDateTimeEdit*  m_pubDateEdit;
    QLineEdit*      m_enclosureUrlEdit;
    QSpinBox*       m_enclosureSizeSpin;
    QSpinBox*       m_durationSpin;
    QCheckBox*      m_explicitCheck;
    QLineEdit*      m_transcriptUrlEdit;
};

// ─── PodRSSWidget — main RSS feed editor widget ────────────────────────────
class PodRSSWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodRSSWidget(M1::PodRSSModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodRSSWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(4, 4, 4, 4);
        root->setSpacing(4);

        m_tabs = new QTabWidget;
        m_tabs->setObjectName("PodRSSTabs");

        // ── Show Settings tab ────────────────────────────────────────────
        auto* showTab = new QWidget;
        showTab->setObjectName("PodRSSShowTab");
        auto* showLay = new QFormLayout(showTab);
        showLay->setContentsMargins(8, 8, 8, 8);
        showLay->setSpacing(6);

        const auto show = mod->show();

        m_showTitle = new QLineEdit(show.title);
        m_showTitle->setObjectName("PodRSSShowTitle");
        m_showTitle->setPlaceholderText("My Podcast");
        m_showTitle->setToolTip("Podcast title");
        showLay->addRow("Title:", m_showTitle);

        m_showDesc = new QTextEdit;
        m_showDesc->setObjectName("PodRSSShowDesc");
        m_showDesc->setPlainText(show.description);
        m_showDesc->setMaximumHeight(80);
        m_showDesc->setToolTip("Podcast description");
        showLay->addRow("Description:", m_showDesc);

        m_showAuthor = new QLineEdit(show.author);
        m_showAuthor->setObjectName("PodRSSShowAuthor");
        m_showAuthor->setToolTip("Podcast author name");
        showLay->addRow("Author:", m_showAuthor);

        m_showEmail = new QLineEdit(show.ownerEmail);
        m_showEmail->setObjectName("PodRSSShowEmail");
        m_showEmail->setPlaceholderText("owner@example.com");
        m_showEmail->setToolTip("Owner email for iTunes");
        showLay->addRow("Owner Email:", m_showEmail);

        m_showCategory = new QComboBox;
        m_showCategory->setObjectName("PodRSSShowCategory");
        m_showCategory->setEditable(true);
        m_showCategory->addItems({
            "Arts", "Business", "Comedy", "Education", "Fiction",
            "Government", "Health & Fitness", "History", "Kids & Family",
            "Leisure", "Music", "News", "Religion & Spirituality",
            "Science", "Society & Culture", "Sports", "Technology",
            "True Crime", "TV & Film"
        });
        m_showCategory->setCurrentText(show.category);
        m_showCategory->setToolTip("iTunes category");
        showLay->addRow("Category:", m_showCategory);

        m_showWebsite = new QLineEdit(show.website);
        m_showWebsite->setObjectName("PodRSSShowWebsite");
        m_showWebsite->setPlaceholderText("https://mypodcast.com");
        m_showWebsite->setToolTip("Podcast website URL");
        showLay->addRow("Website:", m_showWebsite);

        m_showLanguage = new QLineEdit(show.language);
        m_showLanguage->setObjectName("PodRSSShowLanguage");
        m_showLanguage->setToolTip("ISO 639 language code (e.g., en, es, fr)");
        showLay->addRow("Language:", m_showLanguage);

        m_showCoverArt = new QLineEdit(show.coverArtPath);
        m_showCoverArt->setObjectName("PodRSSShowCoverArt");
        m_showCoverArt->setToolTip("Path to podcast cover art image");
        auto* coverBrowse = new QPushButton("Browse...");
        coverBrowse->setToolTip("Choose cover art image file");
        connect(coverBrowse, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(this, "Cover Art",
                {}, "Images (*.png *.jpg *.jpeg)");
            if (!path.isEmpty()) m_showCoverArt->setText(path);
        });
        auto* coverRow = new QHBoxLayout;
        coverRow->addWidget(m_showCoverArt, 1);
        coverRow->addWidget(coverBrowse);
        showLay->addRow("Cover Art:", coverRow);

        m_showTypeCombo = new QComboBox;
        m_showTypeCombo->setObjectName("PodRSSShowType");
        m_showTypeCombo->addItems({"episodic", "serial"});
        m_showTypeCombo->setCurrentText(show.type);
        m_showTypeCombo->setToolTip("Show type: episodic (newest first) or serial (oldest first)");
        showLay->addRow("Show Type:", m_showTypeCombo);

        m_showExplicit = new QCheckBox;
        m_showExplicit->setChecked(show.isExplicit);
        m_showExplicit->setToolTip("Mark entire podcast as explicit content");
        showLay->addRow("Explicit:", m_showExplicit);

        auto* applyShowBtn = new QPushButton("Apply Show Settings");
        applyShowBtn->setObjectName("PodRSSApplyShowBtn");
        applyShowBtn->setToolTip("Save show settings to module");
        connect(applyShowBtn, &QPushButton::clicked, this, &PodRSSWidget::applyShowSettings);
        showLay->addRow(applyShowBtn);

        m_tabs->addTab(showTab, "Show Settings");

        // ── Episodes tab ─────────────────────────────────────────────────
        auto* epTab = new QWidget;
        epTab->setObjectName("PodRSSEpisodeTab");
        auto* epLay = new QVBoxLayout(epTab);
        epLay->setContentsMargins(4, 4, 4, 4);
        epLay->setSpacing(4);

        m_epTable = new QTableWidget(0, 5);
        m_epTable->setObjectName("PodRSSEpisodeTable");
        m_epTable->setHorizontalHeaderLabels({"#", "Title", "Date", "Duration", "Type"});
        m_epTable->horizontalHeader()->setStretchLastSection(true);
        m_epTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_epTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_epTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_epTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_epTable->setToolTip("Episode list");
        epLay->addWidget(m_epTable, 1);

        auto* epBtnRow = new QHBoxLayout;
        epBtnRow->setSpacing(4);

        auto* addEpBtn = new QPushButton("Add Episode");
        addEpBtn->setObjectName("PodRSSAddEpBtn");
        addEpBtn->setToolTip("Add a new episode");
        connect(addEpBtn, &QPushButton::clicked, this, &PodRSSWidget::onAddEpisode);
        epBtnRow->addWidget(addEpBtn);

        auto* editEpBtn = new QPushButton("Edit");
        editEpBtn->setObjectName("PodRSSEditEpBtn");
        editEpBtn->setToolTip("Edit selected episode");
        connect(editEpBtn, &QPushButton::clicked, this, &PodRSSWidget::onEditEpisode);
        epBtnRow->addWidget(editEpBtn);

        auto* removeEpBtn = new QPushButton("Remove");
        removeEpBtn->setObjectName("PodRSSRemoveEpBtn");
        removeEpBtn->setToolTip("Remove selected episode");
        connect(removeEpBtn, &QPushButton::clicked, this, &PodRSSWidget::onRemoveEpisode);
        epBtnRow->addWidget(removeEpBtn);

        epBtnRow->addStretch();
        epLay->addLayout(epBtnRow);

        m_tabs->addTab(epTab, "Episodes");
        root->addWidget(m_tabs, 1);

        // ── Bottom buttons ───────────────────────────────────────────────
        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(6);

        auto* validateBtn = new QPushButton("Validate Feed");
        validateBtn->setObjectName("PodRSSValidateBtn");
        validateBtn->setToolTip("Check feed for potential issues");
        connect(validateBtn, &QPushButton::clicked, this, &PodRSSWidget::onValidate);
        bottomRow->addWidget(validateBtn);

        auto* previewBtn = new QPushButton("Preview XML");
        previewBtn->setObjectName("PodRSSPreviewBtn");
        previewBtn->setToolTip("Preview generated RSS XML");
        connect(previewBtn, &QPushButton::clicked, this, &PodRSSWidget::onPreview);
        bottomRow->addWidget(previewBtn);

        auto* exportBtn = new QPushButton("Export Feed...");
        exportBtn->setObjectName("PodRSSExportBtn");
        exportBtn->setToolTip("Export RSS feed to XML file");
        connect(exportBtn, &QPushButton::clicked, this, &PodRSSWidget::onExport);
        bottomRow->addWidget(exportBtn);

        root->addLayout(bottomRow);

        refreshEpisodeTable();
        connect(mod, &M1::PodRSSModule::episodesChanged, this, &PodRSSWidget::refreshEpisodeTable);
    }

private slots:
    void applyShowSettings() {
        M1::PodcastShow show;
        show.title        = m_showTitle->text();
        show.description  = m_showDesc->toPlainText();
        show.author       = m_showAuthor->text();
        show.ownerEmail   = m_showEmail->text();
        show.category     = m_showCategory->currentText();
        show.website      = m_showWebsite->text();
        show.language     = m_showLanguage->text();
        show.coverArtPath = m_showCoverArt->text();
        show.type         = m_showTypeCombo->currentText();
        show.isExplicit   = m_showExplicit->isChecked();
        m_mod->setShow(show);
    }

    void refreshEpisodeTable() {
        const auto episodes = m_mod->episodes();
        m_epTable->setRowCount(episodes.size());
        for (int i = 0; i < episodes.size(); ++i) {
            const auto& ep = episodes[i];
            m_epTable->setItem(i, 0, new QTableWidgetItem(QString::number(ep.number)));
            m_epTable->setItem(i, 1, new QTableWidgetItem(ep.title));
            m_epTable->setItem(i, 2, new QTableWidgetItem(
                ep.pubDate.isValid() ? ep.pubDate.toString("yyyy-MM-dd") : ""));
            m_epTable->setItem(i, 3, new QTableWidgetItem(
                M1::PodRSSModule::formatDuration(ep.duration)));
            m_epTable->setItem(i, 4, new QTableWidgetItem(ep.type));
        }
    }

    void onAddEpisode() {
        EpisodeDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            m_mod->addEpisode(dlg.episode());
        }
    }

    void onEditEpisode() {
        const int row = m_epTable->currentRow();
        if (row < 0) return;
        const auto episodes = m_mod->episodes();
        if (row >= episodes.size()) return;

        EpisodeDialog dlg(this, episodes[row]);
        if (dlg.exec() == QDialog::Accepted) {
            m_mod->updateEpisode(row, dlg.episode());
        }
    }

    void onRemoveEpisode() {
        const int row = m_epTable->currentRow();
        if (row >= 0) {
            m_mod->removeEpisode(row);
        }
    }

    void onValidate() {
        applyShowSettings();
        const QStringList warnings = m_mod->validateFeed();
        if (warnings.isEmpty()) {
            QMessageBox::information(this, "Feed Validation",
                "Feed is valid. No issues found.");
        } else {
            QMessageBox::warning(this, "Feed Validation",
                "Issues found:\n\n" + warnings.join("\n"));
        }
    }

    void onPreview() {
        applyShowSettings();
        const QString xml = m_mod->generateFeed();

        QDialog dlg(this);
        dlg.setWindowTitle("RSS Feed Preview");
        dlg.setMinimumSize(600, 500);
        auto* lay = new QVBoxLayout(&dlg);

        auto* preview = new QTextEdit;
        preview->setObjectName("PodRSSPreviewEdit");
        preview->setReadOnly(true);
        preview->setPlainText(xml);
        QFont f = preview->font();
        f.setFamily("Consolas");
        f.setPixelSize(12);
        preview->setFont(f);
        lay->addWidget(preview);

        auto* closeBtn = new QPushButton("Close");
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        lay->addWidget(closeBtn);

        dlg.exec();
    }

    void onExport() {
        applyShowSettings();
        const QString path = QFileDialog::getSaveFileName(this, "Export RSS Feed",
            {}, "XML Files (*.xml)");
        if (path.isEmpty()) return;

        if (m_mod->exportFeed(path))
            QMessageBox::information(this, "Export", "RSS feed exported successfully.");
        else
            QMessageBox::warning(this, "Export", "Failed to export RSS feed.");
    }

private:
    M1::PodRSSModule* m_mod;
    QTabWidget*       m_tabs;
    QTableWidget*     m_epTable;

    // Show settings controls
    QLineEdit*  m_showTitle;
    QTextEdit*  m_showDesc;
    QLineEdit*  m_showAuthor;
    QLineEdit*  m_showEmail;
    QComboBox*  m_showCategory;
    QLineEdit*  m_showWebsite;
    QLineEdit*  m_showLanguage;
    QLineEdit*  m_showCoverArt;
    QComboBox*  m_showTypeCombo;
    QCheckBox*  m_showExplicit;
};

} // anonymous namespace

#include "PodRSSModule.moc"

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PodRSSModule::PodRSSModule(QObject* parent) : IModule(parent) {}
PodRSSModule::~PodRSSModule() = default;

// ─── IModule lifecycle ────────────────────────────────────────────────────────

void PodRSSModule::initialize() {
    qInfo() << "[PodRSSModule] Initialized.";
}

void PodRSSModule::shutdown() {
    qInfo() << "[PodRSSModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────

QWidget* PodRSSModule::createWidget(QWidget* parent) {
    return new PodRSSWidget(this, parent);
}

// ─── Show API ─────────────────────────────────────────────────────────────────

void PodRSSModule::setShow(const PodcastShow& show) {
    m_show = show;
    emit showChanged();
}

// ─── Episode API ──────────────────────────────────────────────────────────────

int PodRSSModule::addEpisode(const PodcastEpisode& ep) {
    m_episodes.append(ep);
    emit episodesChanged();
    return m_episodes.size() - 1;
}

void PodRSSModule::removeEpisode(int index) {
    if (index >= 0 && index < m_episodes.size()) {
        m_episodes.removeAt(index);
        emit episodesChanged();
    }
}

void PodRSSModule::updateEpisode(int index, const PodcastEpisode& ep) {
    if (index >= 0 && index < m_episodes.size()) {
        m_episodes[index] = ep;
        emit episodesChanged();
    }
}

// ─── Feed generation ──────────────────────────────────────────────────────────

QString PodRSSModule::xmlEscape(const QString& text) {
    QString escaped = text;
    escaped.replace('&',  "&amp;");
    escaped.replace('<',  "&lt;");
    escaped.replace('>',  "&gt;");
    escaped.replace('"',  "&quot;");
    escaped.replace('\'', "&apos;");
    return escaped;
}

QString PodRSSModule::formatDuration(int seconds) {
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

QString PodRSSModule::generateFeed() const {
    QString xml;
    QTextStream out(&xml);

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<rss version=\"2.0\"\n";
    out << "     xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\"\n";
    out << "     xmlns:content=\"http://purl.org/rss/1.0/modules/content/\"\n";
    out << "     xmlns:podcast=\"https://podcastindex.org/namespace/1.0\">\n";
    out << "<channel>\n";

    // Show-level elements
    out << "  <title>" << xmlEscape(m_show.title) << "</title>\n";
    out << "  <description>" << xmlEscape(m_show.description) << "</description>\n";
    out << "  <language>" << xmlEscape(m_show.language) << "</language>\n";

    if (!m_show.website.isEmpty())
        out << "  <link>" << xmlEscape(m_show.website) << "</link>\n";

    out << "  <lastBuildDate>" << QDateTime::currentDateTimeUtc()
            .toString(Qt::RFC2822Date) << "</lastBuildDate>\n";

    // iTunes elements
    out << "  <itunes:author>" << xmlEscape(m_show.author) << "</itunes:author>\n";
    out << "  <itunes:summary>" << xmlEscape(m_show.description) << "</itunes:summary>\n";

    if (!m_show.ownerEmail.isEmpty()) {
        out << "  <itunes:owner>\n";
        out << "    <itunes:name>" << xmlEscape(m_show.author) << "</itunes:name>\n";
        out << "    <itunes:email>" << xmlEscape(m_show.ownerEmail) << "</itunes:email>\n";
        out << "  </itunes:owner>\n";
    }

    if (!m_show.coverArtPath.isEmpty())
        out << "  <itunes:image href=\"" << xmlEscape(m_show.coverArtPath) << "\" />\n";

    if (!m_show.category.isEmpty())
        out << "  <itunes:category text=\"" << xmlEscape(m_show.category) << "\" />\n";

    out << "  <itunes:explicit>" << (m_show.isExplicit ? "true" : "false") << "</itunes:explicit>\n";
    out << "  <itunes:type>" << xmlEscape(m_show.type) << "</itunes:type>\n";

    out << "  <generator>Mcaster1Studio</generator>\n";

    // Episodes
    for (const auto& ep : m_episodes) {
        out << "  <item>\n";
        out << "    <title>" << xmlEscape(ep.title) << "</title>\n";
        out << "    <description>" << xmlEscape(ep.description) << "</description>\n";

        if (!ep.enclosureUrl.isEmpty()) {
            out << "    <enclosure url=\"" << xmlEscape(ep.enclosureUrl)
                << "\" length=\"" << ep.enclosureSize
                << "\" type=\"audio/mpeg\" />\n";
        }

        const QString guid = ep.guid.isEmpty() ? ep.enclosureUrl : ep.guid;
        if (!guid.isEmpty())
            out << "    <guid isPermaLink=\"false\">" << xmlEscape(guid) << "</guid>\n";

        if (ep.pubDate.isValid())
            out << "    <pubDate>" << ep.pubDate.toUTC().toString(Qt::RFC2822Date) << "</pubDate>\n";

        out << "    <itunes:duration>" << formatDuration(ep.duration) << "</itunes:duration>\n";
        out << "    <itunes:explicit>" << (ep.isExplicit ? "true" : "false") << "</itunes:explicit>\n";
        out << "    <itunes:episodeType>" << xmlEscape(ep.type) << "</itunes:episodeType>\n";

        if (ep.number > 0)
            out << "    <itunes:episode>" << ep.number << "</itunes:episode>\n";
        if (ep.season > 0)
            out << "    <itunes:season>" << ep.season << "</itunes:season>\n";

        if (!ep.coverArt.isEmpty())
            out << "    <itunes:image href=\"" << xmlEscape(ep.coverArt) << "\" />\n";

        if (!ep.transcriptUrl.isEmpty())
            out << "    <podcast:transcript url=\"" << xmlEscape(ep.transcriptUrl)
                << "\" type=\"application/srt\" />\n";

        out << "  </item>\n";
    }

    out << "</channel>\n";
    out << "</rss>\n";

    return xml;
}

bool PodRSSModule::exportFeed(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out << generateFeed();
    return true;
}

QStringList PodRSSModule::validateFeed() const {
    QStringList warnings;

    if (m_show.title.isEmpty())
        warnings << "Show title is empty.";
    if (m_show.description.isEmpty())
        warnings << "Show description is empty.";
    if (m_show.author.isEmpty())
        warnings << "Author is not set.";
    if (m_show.ownerEmail.isEmpty())
        warnings << "Owner email is not set (required by Apple Podcasts).";
    if (m_show.category.isEmpty())
        warnings << "iTunes category is not set.";
    if (m_show.coverArtPath.isEmpty())
        warnings << "Cover art is not set (required by most directories).";
    if (m_show.language.isEmpty())
        warnings << "Language is not set.";
    if (m_show.website.isEmpty())
        warnings << "Website URL is not set.";

    if (m_episodes.isEmpty()) {
        warnings << "Feed has no episodes.";
    } else {
        for (int i = 0; i < m_episodes.size(); ++i) {
            const auto& ep = m_episodes[i];
            const QString prefix = QString("Episode %1: ").arg(i + 1);
            if (ep.title.isEmpty())
                warnings << prefix + "Title is empty.";
            if (ep.enclosureUrl.isEmpty())
                warnings << prefix + "Enclosure URL is empty.";
            if (ep.duration <= 0)
                warnings << prefix + "Duration is not set.";
            if (!ep.pubDate.isValid())
                warnings << prefix + "Publication date is not set.";
        }
    }

    return warnings;
}

// ─── State persistence ───────────────────────────────────────────────────────

void PodRSSModule::saveState(QSettings& s) {
    // Show settings
    s.setValue("show/title", m_show.title);
    s.setValue("show/description", m_show.description);
    s.setValue("show/author", m_show.author);
    s.setValue("show/ownerEmail", m_show.ownerEmail);
    s.setValue("show/category", m_show.category);
    s.setValue("show/coverArtPath", m_show.coverArtPath);
    s.setValue("show/website", m_show.website);
    s.setValue("show/language", m_show.language);
    s.setValue("show/isExplicit", m_show.isExplicit);
    s.setValue("show/type", m_show.type);

    // Episodes
    s.beginWriteArray("episodes", m_episodes.size());
    for (int i = 0; i < m_episodes.size(); ++i) {
        s.setArrayIndex(i);
        const auto& ep = m_episodes[i];
        s.setValue("title", ep.title);
        s.setValue("description", ep.description);
        s.setValue("number", ep.number);
        s.setValue("season", ep.season);
        s.setValue("type", ep.type);
        s.setValue("pubDate", ep.pubDate);
        s.setValue("enclosureUrl", ep.enclosureUrl);
        s.setValue("enclosureSize", ep.enclosureSize);
        s.setValue("duration", ep.duration);
        s.setValue("guid", ep.guid);
        s.setValue("isExplicit", ep.isExplicit);
        s.setValue("coverArt", ep.coverArt);
        s.setValue("transcriptUrl", ep.transcriptUrl);
    }
    s.endArray();
}

void PodRSSModule::loadState(QSettings& s) {
    // Show settings
    m_show.title        = s.value("show/title").toString();
    m_show.description  = s.value("show/description").toString();
    m_show.author       = s.value("show/author").toString();
    m_show.ownerEmail   = s.value("show/ownerEmail").toString();
    m_show.category     = s.value("show/category").toString();
    m_show.coverArtPath = s.value("show/coverArtPath").toString();
    m_show.website      = s.value("show/website").toString();
    m_show.language     = s.value("show/language", "en").toString();
    m_show.isExplicit   = s.value("show/isExplicit", false).toBool();
    m_show.type         = s.value("show/type", "episodic").toString();

    // Episodes
    m_episodes.clear();
    const int epCount = s.beginReadArray("episodes");
    for (int i = 0; i < epCount; ++i) {
        s.setArrayIndex(i);
        PodcastEpisode ep;
        ep.title         = s.value("title").toString();
        ep.description   = s.value("description").toString();
        ep.number        = s.value("number", 1).toInt();
        ep.season        = s.value("season", 1).toInt();
        ep.type          = s.value("type", "full").toString();
        ep.pubDate       = s.value("pubDate").toDateTime();
        ep.enclosureUrl  = s.value("enclosureUrl").toString();
        ep.enclosureSize = s.value("enclosureSize", 0).toLongLong();
        ep.duration      = s.value("duration", 0).toInt();
        ep.guid          = s.value("guid").toString();
        ep.isExplicit    = s.value("isExplicit", false).toBool();
        ep.coverArt      = s.value("coverArt").toString();
        ep.transcriptUrl = s.value("transcriptUrl").toString();
        m_episodes.append(ep);
    }
    s.endArray();
}

} // namespace M1
