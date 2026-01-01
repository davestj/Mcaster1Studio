/// @file   LyricsCasterModule.cpp
/// @path   Modules/LyricsCasterModule/LyricsCasterModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-LyricsCaster — Worship Lyrics Display Module Implementation
/// @purpose Implements the worship song library, section arrangement system,
///          live lyrics projection control, and operator/congregation dual-view
///          widget. We delegate all rendering to GraphicsEngineModule.
/// @reason  Core visual module — most-used in church context.
/// @changelog
///   2026-03-09  Initial implementation

#include "LyricsCasterModule.h"
#include "GraphicsEngineModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QDebug>

namespace M1 {

// ─── Anonymous namespace: widget implementations ─────────────────────────────
namespace {

// ─── OutputPreview — shows the congregation view (rendered frame) ────────────
class OutputPreview : public QWidget {
    Q_OBJECT
public:
    explicit OutputPreview(LyricsCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("LyricsOutputPreview");
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setToolTip("Congregation view — what the projector shows");

        connect(mod, &LyricsCasterModule::frameUpdated, this,
                [this](const QImage& frame) { m_frame = frame; update(); });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.fillRect(rect(), Qt::black);
        if (!m_frame.isNull()) {
            QSize s = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
            int x = (width() - s.width()) / 2;
            int y = (height() - s.height()) / 2;
            p.drawImage(QRect(x, y, s.width(), s.height()), m_frame);
        }
        p.setPen(QColor(60, 60, 60));
        p.drawRect(0, 0, width() - 1, height() - 1);
    }

private:
    QImage m_frame;
};

// ─── SongEditorDialog — add/edit a worship song ─────────────────────────────
class SongEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SongEditorDialog(const WorshipSong& song = {}, QWidget* parent = nullptr)
        : QDialog(parent), m_song(song)
    {
        setWindowTitle(song.id > 0 ? "Edit Song" : "New Song");
        setMinimumSize(500, 520);

        auto* lay = new QVBoxLayout(this);

        auto* grid = new QGridLayout();
        int row = 0;

        grid->addWidget(new QLabel("Title:", this), row, 0);
        m_title = new QLineEdit(song.title, this);
        grid->addWidget(m_title, row++, 1);

        grid->addWidget(new QLabel("Author:", this), row, 0);
        m_author = new QLineEdit(song.author, this);
        grid->addWidget(m_author, row++, 1);

        grid->addWidget(new QLabel("CCLI #:", this), row, 0);
        m_ccli = new QLineEdit(song.ccliNumber, this);
        grid->addWidget(m_ccli, row++, 1);

        grid->addWidget(new QLabel("Key:", this), row, 0);
        m_key = new QLineEdit(song.key, this);
        m_key->setMaximumWidth(60);
        grid->addWidget(m_key, row, 1);

        grid->addWidget(new QLabel("BPM:", this), row, 2);
        m_bpm = new QSpinBox(this);
        m_bpm->setRange(0, 300);
        m_bpm->setValue(song.bpm);
        grid->addWidget(m_bpm, row++, 3);

        lay->addLayout(grid);

        // Sections editor
        auto* secGroup = new QGroupBox("Sections", this);
        auto* secLay = new QVBoxLayout(secGroup);

        m_sectionList = new QListWidget(secGroup);
        for (const auto& sec : song.sections) {
            m_sectionList->addItem(sec.label() + ": " +
                sec.text.left(40).replace('\n', ' ') + "...");
        }
        secLay->addWidget(m_sectionList, 1);

        auto* secBtnRow = new QHBoxLayout();
        auto* addSecBtn = new QPushButton("+ Add Section", secGroup);
        auto* editSecBtn = new QPushButton("Edit", secGroup);
        auto* removeSecBtn = new QPushButton("Remove", secGroup);
        secBtnRow->addWidget(addSecBtn);
        secBtnRow->addWidget(editSecBtn);
        secBtnRow->addWidget(removeSecBtn);
        secBtnRow->addStretch();
        secLay->addLayout(secBtnRow);

        lay->addWidget(secGroup, 1);

        // Buttons
        auto* btnRow = new QHBoxLayout();
        auto* okBtn = new QPushButton("OK", this);
        auto* cancelBtn = new QPushButton("Cancel", this);
        btnRow->addStretch();
        btnRow->addWidget(okBtn);
        btnRow->addWidget(cancelBtn);
        lay->addLayout(btnRow);

        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        connect(addSecBtn, &QPushButton::clicked, this, [this]() {
            QStringList types = {"Verse", "Chorus", "Bridge", "Pre-Chorus",
                                 "Tag", "Intro", "Outro", "Interlude", "Ending"};
            bool ok;
            QString typeStr = QInputDialog::getItem(this, "Section Type",
                "Select section type:", types, 0, false, &ok);
            if (!ok) return;

            QString text = QInputDialog::getMultiLineText(this, "Section Text",
                "Enter lyrics for this section:", {}, &ok);
            if (!ok || text.isEmpty()) return;

            SongSection sec;
            sec.type = static_cast<SectionType>(types.indexOf(typeStr));
            // We count existing sections of this type to set the number
            int count = 0;
            for (const auto& s : m_song.sections) {
                if (s.type == sec.type) count++;
            }
            sec.number = count + 1;
            sec.text = text;
            m_song.sections.append(sec);
            m_sectionList->addItem(sec.label() + ": " +
                text.left(40).replace('\n', ' ') + "...");
        });

        connect(editSecBtn, &QPushButton::clicked, this, [this]() {
            int idx = m_sectionList->currentRow();
            if (idx < 0 || idx >= m_song.sections.size()) return;
            bool ok;
            QString text = QInputDialog::getMultiLineText(this, "Edit Section",
                "Edit lyrics:", m_song.sections[idx].text, &ok);
            if (!ok) return;
            m_song.sections[idx].text = text;
            m_sectionList->item(idx)->setText(
                m_song.sections[idx].label() + ": " +
                text.left(40).replace('\n', ' ') + "...");
        });

        connect(removeSecBtn, &QPushButton::clicked, this, [this]() {
            int idx = m_sectionList->currentRow();
            if (idx < 0 || idx >= m_song.sections.size()) return;
            m_song.sections.removeAt(idx);
            delete m_sectionList->takeItem(idx);
        });
    }

    WorshipSong song() const {
        WorshipSong s = m_song;
        s.title      = m_title->text();
        s.author     = m_author->text();
        s.ccliNumber = m_ccli->text();
        s.key        = m_key->text();
        s.bpm        = m_bpm->value();
        // Build default arrangement if empty
        if (s.arrangement.isEmpty()) {
            for (int i = 0; i < s.sections.size(); ++i)
                s.arrangement.append(i);
        }
        return s;
    }

private:
    WorshipSong m_song;
    QLineEdit*   m_title = nullptr;
    QLineEdit*   m_author = nullptr;
    QLineEdit*   m_ccli = nullptr;
    QLineEdit*   m_key = nullptr;
    QSpinBox*    m_bpm = nullptr;
    QListWidget* m_sectionList = nullptr;
};

} // anonymous namespace

// ─── LyricsCasterModule implementation ──────────────────────────────────────

LyricsCasterModule::LyricsCasterModule(QObject* parent)
    : IModule(parent)
    , m_autoAdvanceTimer(new QTimer(this))
{
    m_autoAdvanceTimer->setSingleShot(true);
    connect(m_autoAdvanceTimer, &QTimer::timeout,
            this, &LyricsCasterModule::onAutoAdvance);
}

LyricsCasterModule::~LyricsCasterModule() {
    shutdown();
}

void LyricsCasterModule::initialize() {
    qInfo() << "[LyricsCaster] Initialized — song library:" << m_songs.size() << "songs";
}

void LyricsCasterModule::shutdown() {
    m_autoAdvanceTimer->stop();
}

QWidget* LyricsCasterModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("LyricsCasterWidget");
    auto* mainLay = new QVBoxLayout(container);
    mainLay->setContentsMargins(4, 4, 4, 4);
    mainLay->setSpacing(4);

    auto* splitter = new QSplitter(Qt::Horizontal, container);

    // ── Left panel: song library + arrangement ──────────────────────────────
    auto* leftPanel = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(4);

    // Search bar
    auto* searchBar = new QLineEdit(leftPanel);
    searchBar->setPlaceholderText("Search songs...");
    searchBar->setToolTip("Search by title or author");
    leftLay->addWidget(searchBar);

    // Song list
    auto* songList = new QListWidget(leftPanel);
    songList->setObjectName("LyricsSongList");
    songList->setToolTip("Double-click to load song for presentation");
    leftLay->addWidget(songList, 1);

    // Song library buttons
    auto* libBtnRow = new QHBoxLayout();
    auto* addSongBtn = new QPushButton("+ New Song", leftPanel);
    addSongBtn->setToolTip("Add a new song to the library");
    auto* editSongBtn = new QPushButton("Edit", leftPanel);
    editSongBtn->setToolTip("Edit selected song");
    auto* removeSongBtn = new QPushButton("Remove", leftPanel);
    removeSongBtn->setToolTip("Remove selected song from library");
    libBtnRow->addWidget(addSongBtn);
    libBtnRow->addWidget(editSongBtn);
    libBtnRow->addWidget(removeSongBtn);
    leftLay->addLayout(libBtnRow);

    // Arrangement list (visible when a song is loaded)
    auto* arrGroup = new QGroupBox("Arrangement", leftPanel);
    auto* arrLay = new QVBoxLayout(arrGroup);
    auto* arrList = new QListWidget(arrGroup);
    arrList->setObjectName("LyricsArrangementList");
    arrList->setToolTip("Click to jump to section — drag to reorder");
    arrList->setDragDropMode(QAbstractItemView::InternalMove);
    arrLay->addWidget(arrList, 1);
    leftLay->addWidget(arrGroup, 1);

    splitter->addWidget(leftPanel);

    // ── Right panel: live control + preview ─────────────────────────────────
    auto* rightPanel = new QWidget(splitter);
    auto* rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(4);

    // Current section display
    auto* currentGroup = new QGroupBox("Current Section", rightPanel);
    auto* currentLay = new QVBoxLayout(currentGroup);
    auto* currentLabel = new QLabel(currentGroup);
    currentLabel->setObjectName("LyricsCurrentSection");
    currentLabel->setWordWrap(true);
    currentLabel->setAlignment(Qt::AlignCenter);
    currentLabel->setMinimumHeight(80);
    QFont lyrFont("Arial", 14);
    currentLabel->setFont(lyrFont);
    currentLay->addWidget(currentLabel);
    rightLay->addWidget(currentGroup);

    // Next section preview
    auto* nextLabel = new QLabel("Next: —", rightPanel);
    nextLabel->setObjectName("LyricsNextPreview");
    nextLabel->setWordWrap(true);
    rightLay->addWidget(nextLabel);

    // Transport controls
    auto* transportRow = new QHBoxLayout();
    auto* prevBtn = new QPushButton("<< Prev", rightPanel);
    prevBtn->setToolTip("Go to previous section");
    prevBtn->setFixedHeight(40);
    auto* blankBtn = new QPushButton("BLANK", rightPanel);
    blankBtn->setToolTip("Toggle blank/black screen");
    blankBtn->setCheckable(true);
    blankBtn->setFixedHeight(40);
    auto* nextBtn = new QPushButton("Next >>", rightPanel);
    nextBtn->setToolTip("Advance to next section");
    nextBtn->setFixedHeight(40);
    transportRow->addWidget(prevBtn);
    transportRow->addWidget(blankBtn);
    transportRow->addWidget(nextBtn);
    rightLay->addLayout(transportRow);

    // Output preview
    auto* preview = new OutputPreview(this, rightPanel);
    preview->setMinimumHeight(120);
    rightLay->addWidget(preview, 1);

    // Song info bar
    auto* infoLabel = new QLabel("No song loaded", rightPanel);
    infoLabel->setObjectName("LyricsSongInfo");
    rightLay->addWidget(infoLabel);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    mainLay->addWidget(splitter);

    // ── Wire signals ────────────────────────────────────────────────────────

    // Populate song list
    auto refreshSongList = [this, songList, searchBar]() {
        songList->clear();
        QString filter = searchBar->text();
        for (const auto& s : m_songs) {
            if (!filter.isEmpty() &&
                !s.title.contains(filter, Qt::CaseInsensitive) &&
                !s.author.contains(filter, Qt::CaseInsensitive))
                continue;
            auto* item = new QListWidgetItem(
                QString("%1 — %2").arg(s.title, s.author), songList);
            item->setData(Qt::UserRole, s.id);
        }
    };
    refreshSongList();

    connect(searchBar, &QLineEdit::textChanged, container, refreshSongList);
    connect(this, &LyricsCasterModule::songLibraryChanged, container, refreshSongList);

    // Double-click song → load for presentation
    connect(songList, &QListWidget::itemDoubleClicked, container,
            [this, arrList, infoLabel](QListWidgetItem* item) {
        int songId = item->data(Qt::UserRole).toInt();
        loadSong(songId);

        // Update arrangement list
        arrList->clear();
        auto* s = song(songId);
        if (s) {
            for (int idx : s->arrangement) {
                if (idx >= 0 && idx < s->sections.size())
                    arrList->addItem(s->sections[idx].label());
            }
            infoLabel->setText(QString("%1 — %2  |  Key: %3  BPM: %4  CCLI: %5")
                .arg(s->title, s->author, s->key)
                .arg(s->bpm).arg(s->ccliNumber));
        }
    });

    // Arrangement click → jump to section
    connect(arrList, &QListWidget::currentRowChanged, container,
            [this](int row) { if (row >= 0) goToSection(row); });

    // Section changed → update UI
    connect(this, &LyricsCasterModule::sectionChanged, container,
            [this, currentLabel, nextLabel, arrList](int idx) {
        auto* sec = currentSection();
        currentLabel->setText(sec ? sec->text : "—");

        auto* nxt = nextSectionPreview();
        nextLabel->setText(nxt ? QString("Next: %1 — %2")
            .arg(nxt->label(), nxt->text.left(60).replace('\n', ' '))
            : "Next: — (end)");

        arrList->blockSignals(true);
        arrList->setCurrentRow(idx);
        arrList->blockSignals(false);
    });

    // Transport buttons
    connect(prevBtn, &QPushButton::clicked, this, &LyricsCasterModule::prevSection);
    connect(nextBtn, &QPushButton::clicked, this, &LyricsCasterModule::nextSection);
    connect(blankBtn, &QPushButton::clicked, this, [this](bool checked) {
        setBlank(checked);
    });
    connect(this, &LyricsCasterModule::blankChanged, blankBtn, &QPushButton::setChecked);

    // Add/edit/remove song buttons
    connect(addSongBtn, &QPushButton::clicked, container, [this, refreshSongList]() {
        SongEditorDialog dlg({}, nullptr);
        if (dlg.exec() == QDialog::Accepted) {
            addSong(dlg.song());
            refreshSongList();
        }
    });

    connect(editSongBtn, &QPushButton::clicked, container,
            [this, songList, refreshSongList]() {
        auto* item = songList->currentItem();
        if (!item) return;
        int id = item->data(Qt::UserRole).toInt();
        auto* s = const_cast<WorshipSong*>(song(id));
        if (!s) return;
        SongEditorDialog dlg(*s, nullptr);
        if (dlg.exec() == QDialog::Accepted) {
            WorshipSong updated = dlg.song();
            updated.id = id;
            updateSong(updated);
            refreshSongList();
        }
    });

    connect(removeSongBtn, &QPushButton::clicked, container,
            [this, songList, refreshSongList]() {
        auto* item = songList->currentItem();
        if (!item) return;
        int id = item->data(Qt::UserRole).toInt();
        removeSong(id);
        refreshSongList();
    });

    return container;
}

void LyricsCasterModule::saveState(QSettings& s) {
    s.beginGroup("LyricsCaster");
    s.setValue("songCount", m_songs.size());
    int idx = 0;
    for (auto it = m_songs.constBegin(); it != m_songs.constEnd(); ++it, ++idx) {
        const QString prefix = QString("song_%1/").arg(idx);
        const auto& song = it.value();
        s.setValue(prefix + "id",         song.id);
        s.setValue(prefix + "title",      song.title);
        s.setValue(prefix + "author",     song.author);
        s.setValue(prefix + "copyright",  song.copyright);
        s.setValue(prefix + "ccli",       song.ccliNumber);
        s.setValue(prefix + "key",        song.key);
        s.setValue(prefix + "bpm",        song.bpm);

        // We save sections
        s.setValue(prefix + "sectionCount", song.sections.size());
        for (int si = 0; si < song.sections.size(); ++si) {
            const QString sp = prefix + QString("sec_%1/").arg(si);
            s.setValue(sp + "type",   static_cast<int>(song.sections[si].type));
            s.setValue(sp + "number", song.sections[si].number);
            s.setValue(sp + "text",   song.sections[si].text);
            s.setValue(sp + "autoAdv", song.sections[si].autoAdvanceSec);
        }

        // We save arrangement
        QList<QVariant> arr;
        for (int a : song.arrangement) arr.append(a);
        s.setValue(prefix + "arrangement", arr);
    }
    s.endGroup();
}

void LyricsCasterModule::loadState(QSettings& s) {
    s.beginGroup("LyricsCaster");
    int count = s.value("songCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("song_%1/").arg(i);
        WorshipSong song;
        song.id         = s.value(prefix + "id", 0).toInt();
        song.title      = s.value(prefix + "title").toString();
        song.author     = s.value(prefix + "author").toString();
        song.copyright  = s.value(prefix + "copyright").toString();
        song.ccliNumber = s.value(prefix + "ccli").toString();
        song.key        = s.value(prefix + "key").toString();
        song.bpm        = s.value(prefix + "bpm", 0).toInt();

        int secCount = s.value(prefix + "sectionCount", 0).toInt();
        for (int si = 0; si < secCount; ++si) {
            const QString sp = prefix + QString("sec_%1/").arg(si);
            SongSection sec;
            sec.type      = static_cast<SectionType>(s.value(sp + "type", 0).toInt());
            sec.number    = s.value(sp + "number", 1).toInt();
            sec.text      = s.value(sp + "text").toString();
            sec.autoAdvanceSec = s.value(sp + "autoAdv", 0).toInt();
            song.sections.append(sec);
        }

        QList<QVariant> arr = s.value(prefix + "arrangement").toList();
        for (const auto& v : arr)
            song.arrangement.append(v.toInt());

        if (song.id >= m_nextSongId) m_nextSongId = song.id + 1;
        m_songs.insert(song.id, song);
    }
    s.endGroup();
}

// ─── Song library management ─────────────────────────────────────────────────

int LyricsCasterModule::addSong(const WorshipSong& song) {
    WorshipSong s = song;
    s.id = m_nextSongId++;
    if (s.arrangement.isEmpty()) {
        for (int i = 0; i < s.sections.size(); ++i)
            s.arrangement.append(i);
    }
    m_songs.insert(s.id, s);
    emit songLibraryChanged();
    qInfo() << "[LyricsCaster] Added song:" << s.title << "id:" << s.id;
    return s.id;
}

void LyricsCasterModule::removeSong(int id) {
    m_songs.remove(id);
    if (m_currentSongId == id) {
        m_currentSongId = -1;
        m_currentIndex = -1;
    }
    emit songLibraryChanged();
}

void LyricsCasterModule::updateSong(const WorshipSong& song) {
    if (m_songs.contains(song.id)) {
        m_songs[song.id] = song;
        emit songLibraryChanged();
    }
}

const WorshipSong* LyricsCasterModule::song(int id) const {
    auto it = m_songs.constFind(id);
    return (it != m_songs.constEnd()) ? &it.value() : nullptr;
}

QList<WorshipSong> LyricsCasterModule::searchSongs(const QString& query) const {
    QList<WorshipSong> results;
    for (const auto& s : m_songs) {
        if (s.title.contains(query, Qt::CaseInsensitive) ||
            s.author.contains(query, Qt::CaseInsensitive))
            results.append(s);
    }
    return results;
}

// ─── Live presentation ───────────────────────────────────────────────────────

void LyricsCasterModule::loadSong(int songId) {
    if (!m_songs.contains(songId)) return;
    m_currentSongId = songId;
    m_currentIndex = -1;
    m_blank = false;
    emit songLoaded(songId);
    emit blankChanged(false);
    // We start at first section
    if (!m_songs[songId].arrangement.isEmpty())
        goToSection(0);
    qInfo() << "[LyricsCaster] Loaded song:" << m_songs[songId].title;
}

void LyricsCasterModule::nextSection() {
    if (m_currentSongId < 0) return;
    auto* s = const_cast<WorshipSong*>(song(m_currentSongId));
    if (!s || s->arrangement.isEmpty()) return;
    int next = m_currentIndex + 1;
    if (next < s->arrangement.size())
        goToSection(next);
}

void LyricsCasterModule::prevSection() {
    if (m_currentSongId < 0 || m_currentIndex <= 0) return;
    goToSection(m_currentIndex - 1);
}

void LyricsCasterModule::goToSection(int arrangementIndex) {
    auto* s = const_cast<WorshipSong*>(song(m_currentSongId));
    if (!s || arrangementIndex < 0 || arrangementIndex >= s->arrangement.size())
        return;

    m_currentIndex = arrangementIndex;
    m_autoAdvanceTimer->stop();

    renderCurrentSection();
    emit sectionChanged(m_currentIndex);

    // We set up auto-advance if configured
    int secIdx = s->arrangement[m_currentIndex];
    if (secIdx >= 0 && secIdx < s->sections.size()) {
        int autoSec = s->sections[secIdx].autoAdvanceSec;
        if (autoSec > 0) {
            m_autoAdvanceTimer->start(autoSec * 1000);
        }
    }
}

void LyricsCasterModule::setBlank(bool blank) {
    if (m_blank == blank) return;
    m_blank = blank;
    renderCurrentSection();
    emit blankChanged(blank);
}

const SongSection* LyricsCasterModule::currentSection() const {
    auto* s = song(m_currentSongId);
    if (!s || m_currentIndex < 0 || m_currentIndex >= s->arrangement.size())
        return nullptr;
    int secIdx = s->arrangement[m_currentIndex];
    if (secIdx < 0 || secIdx >= s->sections.size()) return nullptr;
    return &s->sections[secIdx];
}

const SongSection* LyricsCasterModule::nextSectionPreview() const {
    auto* s = song(m_currentSongId);
    if (!s) return nullptr;
    int nextIdx = m_currentIndex + 1;
    if (nextIdx < 0 || nextIdx >= s->arrangement.size()) return nullptr;
    int secIdx = s->arrangement[nextIdx];
    if (secIdx < 0 || secIdx >= s->sections.size()) return nullptr;
    return &s->sections[secIdx];
}

// ─── Arrangement editing ─────────────────────────────────────────────────────

void LyricsCasterModule::setArrangement(int songId, const QList<int>& sectionIndices) {
    if (m_songs.contains(songId))
        m_songs[songId].arrangement = sectionIndices;
}

void LyricsCasterModule::insertArrangementItem(int songId, int position, int sectionIndex) {
    if (!m_songs.contains(songId)) return;
    auto& arr = m_songs[songId].arrangement;
    if (position < 0 || position > arr.size()) position = arr.size();
    arr.insert(position, sectionIndex);
}

void LyricsCasterModule::removeArrangementItem(int songId, int position) {
    if (!m_songs.contains(songId)) return;
    auto& arr = m_songs[songId].arrangement;
    if (position >= 0 && position < arr.size())
        arr.removeAt(position);
}

// ─── Private ─────────────────────────────────────────────────────────────────

void LyricsCasterModule::renderCurrentSection() {
    if (m_blank || !m_graphics) {
        m_currentFrame = m_graphics ? m_graphics->renderBlank() : QImage();
        emit frameUpdated(m_currentFrame);
        return;
    }

    auto* sec = currentSection();
    auto* s = song(m_currentSongId);
    if (!sec || !s) {
        m_currentFrame = m_graphics->renderBlank();
        emit frameUpdated(m_currentFrame);
        return;
    }

    m_currentFrame = m_graphics->renderLyrics(
        sec->text,
        s->title + " — " + s->author,
        s->ccliNumber.isEmpty() ? QString() : "CCLI #" + s->ccliNumber
    );
    emit frameUpdated(m_currentFrame);
}

void LyricsCasterModule::onAutoAdvance() {
    nextSection();
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_lyricsInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.lyrics",
    "Lyrics Caster",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Worship lyrics display — song library, arrangement, live projection, CCLI"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_lyrics_plugin_info() { return &s_lyricsInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_lyrics_create_module(IModuleHost*) {
    return new M1::LyricsCasterModule();
}
MCASTER1_PLUGIN_API void mcaster1_lyrics_destroy_module(IModule* m) { delete m; }
}

#include "LyricsCasterModule.moc"
