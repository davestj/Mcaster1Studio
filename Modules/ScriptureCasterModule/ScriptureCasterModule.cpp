/// @file   ScriptureCasterModule.cpp
/// @path   Modules/ScriptureCasterModule/ScriptureCasterModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-ScriptureCaster — Bible Verse and Sermon Notes Display Implementation
/// @purpose Implements scripture lookup, sermon queue management, and live verse/
///          outline point projection. We delegate rendering to GraphicsEngineModule.
/// @reason  Core visual module — scripture display during every sermon segment.
/// @changelog
///   2026-03-09  Initial implementation

#include "ScriptureCasterModule.h"
#include "GraphicsEngineModule.h"
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
#include <QCompleter>
#include <QStringListModel>
#include <QInputDialog>
#include <QPainter>
#include <QSettings>
#include <QDebug>

namespace M1 {

namespace {

// ─── OutputPreview — congregation view ──────────────────────────────────────
class ScripturePreview : public QWidget {
    Q_OBJECT
public:
    explicit ScripturePreview(ScriptureCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("ScriptureOutputPreview");
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setToolTip("Congregation view — what the projector shows");
        connect(mod, &ScriptureCasterModule::frameUpdated, this,
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

} // anonymous namespace

// ─── ScriptureCasterModule implementation ────────────────────────────────────

ScriptureCasterModule::ScriptureCasterModule(QObject* parent)
    : IModule(parent)
{
    initBibleData();
}

ScriptureCasterModule::~ScriptureCasterModule() = default;

void ScriptureCasterModule::initialize() {
    qInfo() << "[ScriptureCaster] Initialized — translations:" << m_translations.size()
            << " cached verses:" << m_verseCache.size();
}

void ScriptureCasterModule::shutdown() {}

void ScriptureCasterModule::initBibleData() {
    // We initialize the list of Bible books (66 books, canonical order)
    m_bibleBooks = {
        "Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy",
        "Joshua", "Judges", "Ruth", "1 Samuel", "2 Samuel",
        "1 Kings", "2 Kings", "1 Chronicles", "2 Chronicles",
        "Ezra", "Nehemiah", "Esther", "Job", "Psalms", "Proverbs",
        "Ecclesiastes", "Song of Solomon", "Isaiah", "Jeremiah",
        "Lamentations", "Ezekiel", "Daniel", "Hosea", "Joel", "Amos",
        "Obadiah", "Jonah", "Micah", "Nahum", "Habakkuk", "Zephaniah",
        "Haggai", "Zechariah", "Malachi",
        "Matthew", "Mark", "Luke", "John", "Acts",
        "Romans", "1 Corinthians", "2 Corinthians", "Galatians",
        "Ephesians", "Philippians", "Colossians",
        "1 Thessalonians", "2 Thessalonians",
        "1 Timothy", "2 Timothy", "Titus", "Philemon",
        "Hebrews", "James", "1 Peter", "2 Peter",
        "1 John", "2 John", "3 John", "Jude", "Revelation"
    };

    m_translations = {"KJV", "NIV", "ESV", "NASB", "NLT", "NKJV"};

    // We seed a small cache of commonly referenced verses for demo/testing.
    // In production, we will load a full Bible database from SQLite.
    m_verseCache["KJV:John:3:16"] =
        "For God so loved the world, that he gave his only begotten Son, "
        "that whosoever believeth in him should not perish, but have everlasting life.";
    m_verseCache["KJV:John:3:17"] =
        "For God sent not his Son into the world to condemn the world; "
        "but that the world through him might be saved.";
    m_verseCache["KJV:Psalms:23:1"] =
        "The LORD is my shepherd; I shall not want.";
    m_verseCache["KJV:Psalms:23:2"] =
        "He maketh me to lie down in green pastures: he leadeth me beside the still waters.";
    m_verseCache["KJV:Psalms:23:3"] =
        "He restoreth my soul: he leadeth me in the paths of righteousness for his name's sake.";
    m_verseCache["KJV:Psalms:23:4"] =
        "Yea, though I walk through the valley of the shadow of death, I will fear no evil: "
        "for thou art with me; thy rod and thy staff they comfort me.";
    m_verseCache["KJV:Romans:8:28"] =
        "And we know that all things work together for good to them that love God, "
        "to them who are the called according to his purpose.";
    m_verseCache["KJV:Philippians:4:13"] =
        "I can do all things through Christ which strengtheneth me.";
    m_verseCache["KJV:Jeremiah:29:11"] =
        "For I know the thoughts that I think toward you, saith the LORD, "
        "thoughts of peace, and not of evil, to give you an expected end.";
    m_verseCache["KJV:Proverbs:3:5"] =
        "Trust in the LORD with all thine heart; and lean not unto thine own understanding.";
    m_verseCache["KJV:Proverbs:3:6"] =
        "In all thy ways acknowledge him, and he shall direct thy paths.";
    m_verseCache["KJV:Isaiah:40:31"] =
        "But they that wait upon the LORD shall renew their strength; "
        "they shall mount up with wings as eagles; they shall run, and not be weary; "
        "and they shall walk, and not faint.";
    m_verseCache["KJV:Matthew:28:19"] =
        "Go ye therefore, and teach all nations, baptizing them in the name of the Father, "
        "and of the Son, and of the Holy Ghost.";
    m_verseCache["KJV:Matthew:28:20"] =
        "Teaching them to observe all things whatsoever I have commanded you: and, lo, "
        "I am with you always, even unto the end of the world. Amen.";
    m_verseCache["KJV:Genesis:1:1"] =
        "In the beginning God created the heaven and the earth.";
    m_verseCache["KJV:Revelation:21:4"] =
        "And God shall wipe away all tears from their eyes; and there shall be no more death, "
        "neither sorrow, nor crying, neither shall there be any more pain: "
        "for the former things are passed away.";

    // NIV versions of key verses
    m_verseCache["NIV:John:3:16"] =
        "For God so loved the world that he gave his one and only Son, "
        "that whoever believes in him shall not perish but have eternal life.";
    m_verseCache["NIV:Psalms:23:1"] =
        "The LORD is my shepherd, I lack nothing.";
    m_verseCache["NIV:Romans:8:28"] =
        "And we know that in all things God works for the good of those who love him, "
        "who have been called according to his purpose.";
    m_verseCache["NIV:Philippians:4:13"] =
        "I can do all this through him who gives me strength.";
    m_verseCache["NIV:Jeremiah:29:11"] =
        "For I know the plans I have for you, declares the LORD, "
        "plans to prosper you and not to harm you, plans to give you hope and a future.";
}

QWidget* ScriptureCasterModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("ScriptureCasterWidget");
    auto* mainLay = new QVBoxLayout(container);
    mainLay->setContentsMargins(4, 4, 4, 4);
    mainLay->setSpacing(4);

    auto* splitter = new QSplitter(Qt::Horizontal, container);

    // ── Left panel: quick lookup + sermon queue ─────────────────────────────
    auto* leftPanel = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(4);

    // Quick verse lookup
    auto* lookupGroup = new QGroupBox("Quick Lookup", leftPanel);
    auto* lookupLay = new QGridLayout(lookupGroup);
    int row = 0;

    lookupLay->addWidget(new QLabel("Reference:", lookupGroup), row, 0);
    auto* refInput = new QLineEdit(lookupGroup);
    refInput->setPlaceholderText("e.g., John 3:16 or Psalms 23:1-4");
    refInput->setToolTip("Type a Bible reference — we auto-complete book names");
    // We set up auto-completion on Bible book names
    auto* completer = new QCompleter(m_bibleBooks, refInput);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    refInput->setCompleter(completer);
    lookupLay->addWidget(refInput, row++, 1, 1, 2);

    lookupLay->addWidget(new QLabel("Translation:", lookupGroup), row, 0);
    auto* transCombo = new QComboBox(lookupGroup);
    transCombo->addItems(m_translations);
    transCombo->setToolTip("Select Bible translation");
    lookupLay->addWidget(transCombo, row, 1);

    auto* lookupBtn = new QPushButton("Lookup", lookupGroup);
    lookupBtn->setToolTip("Look up verse and add to sermon queue");
    lookupLay->addWidget(lookupBtn, row++, 2);

    auto* versePreview = new QTextEdit(lookupGroup);
    versePreview->setObjectName("ScriptureVersePreview");
    versePreview->setReadOnly(true);
    versePreview->setMaximumHeight(80);
    versePreview->setPlaceholderText("Verse text will appear here...");
    lookupLay->addWidget(versePreview, row++, 0, 1, 3);

    auto* addToQueueBtn = new QPushButton("+ Add to Queue", lookupGroup);
    addToQueueBtn->setToolTip("Add the current verse to the sermon queue");
    lookupLay->addWidget(addToQueueBtn, row++, 0, 1, 3);

    leftLay->addWidget(lookupGroup);

    // Sermon queue
    auto* queueGroup = new QGroupBox("Sermon Queue", leftPanel);
    auto* queueLay = new QVBoxLayout(queueGroup);
    auto* queueList = new QListWidget(queueGroup);
    queueList->setObjectName("ScriptureQueueList");
    queueList->setToolTip("Double-click to jump to item — drag to reorder");
    queueList->setDragDropMode(QAbstractItemView::InternalMove);
    queueLay->addWidget(queueList, 1);

    auto* queueBtnRow = new QHBoxLayout();
    auto* addPointBtn = new QPushButton("+ Sermon Point", queueGroup);
    addPointBtn->setToolTip("Add a sermon outline point to the queue");
    auto* addBlankBtn = new QPushButton("+ Blank", queueGroup);
    addBlankBtn->setToolTip("Add a blank screen to the queue");
    auto* removeBtn = new QPushButton("Remove", queueGroup);
    removeBtn->setToolTip("Remove selected item from queue");
    auto* clearBtn = new QPushButton("Clear All", queueGroup);
    clearBtn->setToolTip("Clear the entire sermon queue");
    queueBtnRow->addWidget(addPointBtn);
    queueBtnRow->addWidget(addBlankBtn);
    queueBtnRow->addWidget(removeBtn);
    queueBtnRow->addWidget(clearBtn);
    queueLay->addLayout(queueBtnRow);

    leftLay->addWidget(queueGroup, 1);
    splitter->addWidget(leftPanel);

    // ── Right panel: live control + preview ─────────────────────────────────
    auto* rightPanel = new QWidget(splitter);
    auto* rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(4);

    // Current display
    auto* currentGroup = new QGroupBox("Current Display", rightPanel);
    auto* currentLay = new QVBoxLayout(currentGroup);
    auto* currentText = new QLabel(currentGroup);
    currentText->setObjectName("ScriptureCurrentText");
    currentText->setWordWrap(true);
    currentText->setAlignment(Qt::AlignCenter);
    currentText->setMinimumHeight(60);
    QFont textFont("Arial", 12);
    currentText->setFont(textFont);
    currentLay->addWidget(currentText);
    auto* currentRef = new QLabel(currentGroup);
    currentRef->setObjectName("ScriptureCurrentRef");
    currentRef->setAlignment(Qt::AlignCenter);
    QFont refFont("Arial", 10, QFont::Bold);
    currentRef->setFont(refFont);
    currentLay->addWidget(currentRef);
    rightLay->addWidget(currentGroup);

    // Transport
    auto* transportRow = new QHBoxLayout();
    auto* prevBtn = new QPushButton("<< Prev", rightPanel);
    prevBtn->setToolTip("Previous item in sermon queue");
    prevBtn->setFixedHeight(40);
    auto* blankBtn = new QPushButton("BLANK", rightPanel);
    blankBtn->setToolTip("Toggle blank/black screen");
    blankBtn->setCheckable(true);
    blankBtn->setFixedHeight(40);
    auto* nextBtn = new QPushButton("Next >>", rightPanel);
    nextBtn->setToolTip("Next item in sermon queue");
    nextBtn->setFixedHeight(40);
    transportRow->addWidget(prevBtn);
    transportRow->addWidget(blankBtn);
    transportRow->addWidget(nextBtn);
    rightLay->addLayout(transportRow);

    // Output preview
    auto* preview = new ScripturePreview(this, rightPanel);
    preview->setMinimumHeight(120);
    rightLay->addWidget(preview, 1);

    // Position indicator
    auto* posLabel = new QLabel("0 / 0", rightPanel);
    posLabel->setObjectName("ScripturePosition");
    posLabel->setAlignment(Qt::AlignCenter);
    rightLay->addWidget(posLabel);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    mainLay->addWidget(splitter);

    // ── Wire signals ────────────────────────────────────────────────────────

    auto refreshQueue = [this, queueList]() {
        queueList->clear();
        for (int i = 0; i < m_queue.size(); ++i) {
            const auto& item = m_queue[i];
            QString label;
            switch (item.type) {
                case SermonQueueItem::Type::Scripture:
                    label = QString("[Scripture] %1").arg(item.reference);
                    break;
                case SermonQueueItem::Type::SermonPoint:
                    label = QString("[Point] %1 %2").arg(item.reference,
                        item.text.left(40).replace('\n', ' '));
                    break;
                case SermonQueueItem::Type::Blank:
                    label = "[Blank]";
                    break;
            }
            queueList->addItem(label);
        }
    };

    connect(this, &ScriptureCasterModule::queueChanged, container, refreshQueue);

    // Lookup button
    connect(lookupBtn, &QPushButton::clicked, container,
            [this, refInput, transCombo, versePreview]() {
        QString ref = refInput->text().trimmed();
        QString trans = transCombo->currentText();
        QString text = lookupReference(ref, trans);
        if (text.isEmpty())
            text = "(Verse not found in cache — enter text manually or load Bible database)";
        versePreview->setText(text);
    });

    // Add verse to queue
    connect(addToQueueBtn, &QPushButton::clicked, container,
            [this, refInput, transCombo, versePreview]() {
        QString text = versePreview->toPlainText();
        if (text.isEmpty() || text.startsWith("(Verse not")) return;
        addScripture(text, refInput->text().trimmed(), transCombo->currentText());
    });

    // Queue double-click → jump
    connect(queueList, &QListWidget::currentRowChanged, container,
            [this](int row) { if (row >= 0) goToItem(row); });

    // Add sermon point
    connect(addPointBtn, &QPushButton::clicked, container, [this]() {
        bool ok;
        QString number = QInputDialog::getText(nullptr, "Outline Number",
            "Enter outline number (e.g., I., A., 1.):", QLineEdit::Normal, "", &ok);
        if (!ok) return;
        QString text = QInputDialog::getMultiLineText(nullptr, "Sermon Point",
            "Enter sermon point text:", {}, &ok);
        if (!ok || text.isEmpty()) return;
        addSermonPoint(text, number);
    });

    connect(addBlankBtn, &QPushButton::clicked, this, &ScriptureCasterModule::addBlank);
    connect(removeBtn, &QPushButton::clicked, container, [this, queueList]() {
        int row = queueList->currentRow();
        if (row >= 0) removeQueueItem(row);
    });
    connect(clearBtn, &QPushButton::clicked, this, &ScriptureCasterModule::clearQueue);

    // Transport
    connect(prevBtn, &QPushButton::clicked, this, &ScriptureCasterModule::prevItem);
    connect(nextBtn, &QPushButton::clicked, this, &ScriptureCasterModule::nextItem);
    connect(blankBtn, &QPushButton::clicked, this, [this](bool checked) {
        setBlank(checked);
    });
    connect(this, &ScriptureCasterModule::blankChanged, blankBtn, &QPushButton::setChecked);

    // Current item changed → update UI
    connect(this, &ScriptureCasterModule::currentItemChanged, container,
            [this, currentText, currentRef, posLabel, queueList](int idx) {
        auto* item = currentItem();
        if (item) {
            currentText->setText(item->text);
            currentRef->setText(item->reference);
        } else {
            currentText->setText("—");
            currentRef->setText("");
        }
        posLabel->setText(QString("%1 / %2").arg(idx + 1).arg(m_queue.size()));
        queueList->blockSignals(true);
        queueList->setCurrentRow(idx);
        queueList->blockSignals(false);
    });

    return container;
}

void ScriptureCasterModule::saveState(QSettings& s) {
    s.beginGroup("ScriptureCaster");
    s.setValue("queueCount", m_queue.size());
    for (int i = 0; i < m_queue.size(); ++i) {
        const QString prefix = QString("queue_%1/").arg(i);
        s.setValue(prefix + "type",      static_cast<int>(m_queue[i].type));
        s.setValue(prefix + "text",      m_queue[i].text);
        s.setValue(prefix + "reference", m_queue[i].reference);
        s.setValue(prefix + "secText",   m_queue[i].secondaryText);
        s.setValue(prefix + "secRef",    m_queue[i].secondaryReference);
    }
    s.endGroup();
}

void ScriptureCasterModule::loadState(QSettings& s) {
    s.beginGroup("ScriptureCaster");
    int count = s.value("queueCount", 0).toInt();
    m_queue.clear();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("queue_%1/").arg(i);
        SermonQueueItem item;
        item.type               = static_cast<SermonQueueItem::Type>(
                                      s.value(prefix + "type", 0).toInt());
        item.text               = s.value(prefix + "text").toString();
        item.reference          = s.value(prefix + "reference").toString();
        item.secondaryText      = s.value(prefix + "secText").toString();
        item.secondaryReference = s.value(prefix + "secRef").toString();
        m_queue.append(item);
    }
    s.endGroup();
}

// ─── Sermon queue management ─────────────────────────────────────────────────

void ScriptureCasterModule::addScripture(const QString& text, const QString& reference,
                                          const QString& translation) {
    SermonQueueItem item;
    item.type      = SermonQueueItem::Type::Scripture;
    item.text      = text;
    item.reference = reference + " " + translation;
    m_queue.append(item);
    emit queueChanged();
}

void ScriptureCasterModule::addScriptureDual(const QString& text1, const QString& ref1,
                                              const QString& trans1,
                                              const QString& text2, const QString& ref2,
                                              const QString& trans2) {
    SermonQueueItem item;
    item.type               = SermonQueueItem::Type::Scripture;
    item.text               = text1;
    item.reference          = ref1 + " " + trans1;
    item.secondaryText      = text2;
    item.secondaryReference = ref2 + " " + trans2;
    m_queue.append(item);
    emit queueChanged();
}

void ScriptureCasterModule::addSermonPoint(const QString& point, const QString& number) {
    SermonQueueItem item;
    item.type      = SermonQueueItem::Type::SermonPoint;
    item.text      = point;
    item.reference = number;
    m_queue.append(item);
    emit queueChanged();
}

void ScriptureCasterModule::addBlank() {
    SermonQueueItem item;
    item.type = SermonQueueItem::Type::Blank;
    m_queue.append(item);
    emit queueChanged();
}

void ScriptureCasterModule::removeQueueItem(int index) {
    if (index >= 0 && index < m_queue.size()) {
        m_queue.removeAt(index);
        if (m_currentIndex >= m_queue.size())
            m_currentIndex = m_queue.size() - 1;
        emit queueChanged();
    }
}

void ScriptureCasterModule::clearQueue() {
    m_queue.clear();
    m_currentIndex = -1;
    emit queueChanged();
}

void ScriptureCasterModule::moveQueueItem(int from, int to) {
    if (from >= 0 && from < m_queue.size() &&
        to >= 0 && to < m_queue.size() && from != to) {
        m_queue.move(from, to);
        emit queueChanged();
    }
}

const SermonQueueItem* ScriptureCasterModule::queueItem(int index) const {
    if (index >= 0 && index < m_queue.size())
        return &m_queue[index];
    return nullptr;
}

// ─── Live presentation ───────────────────────────────────────────────────────

void ScriptureCasterModule::nextItem() {
    if (m_currentIndex + 1 < m_queue.size())
        goToItem(m_currentIndex + 1);
}

void ScriptureCasterModule::prevItem() {
    if (m_currentIndex > 0)
        goToItem(m_currentIndex - 1);
}

void ScriptureCasterModule::goToItem(int index) {
    if (index < 0 || index >= m_queue.size()) return;
    m_currentIndex = index;
    renderCurrentItem();
    emit currentItemChanged(m_currentIndex);
}

void ScriptureCasterModule::setBlank(bool blank) {
    if (m_blank == blank) return;
    m_blank = blank;
    renderCurrentItem();
    emit blankChanged(blank);
}

const SermonQueueItem* ScriptureCasterModule::currentItem() const {
    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size())
        return &m_queue[m_currentIndex];
    return nullptr;
}

void ScriptureCasterModule::renderCurrentItem() {
    if (m_blank || !m_graphics) {
        m_currentFrame = m_graphics ? m_graphics->renderBlank() : QImage();
        emit frameUpdated(m_currentFrame);
        return;
    }

    auto* item = currentItem();
    if (!item) {
        m_currentFrame = m_graphics->renderBlank();
        emit frameUpdated(m_currentFrame);
        return;
    }

    switch (item->type) {
        case SermonQueueItem::Type::Scripture:
            if (!item->secondaryText.isEmpty()) {
                m_currentFrame = m_graphics->renderScriptureDual(
                    item->text, item->reference, {},
                    item->secondaryText, item->secondaryReference, {}
                );
            } else {
                m_currentFrame = m_graphics->renderScripture(
                    item->text, item->reference
                );
            }
            break;
        case SermonQueueItem::Type::SermonPoint:
            m_currentFrame = m_graphics->renderSermonPoint(
                item->text, item->reference
            );
            break;
        case SermonQueueItem::Type::Blank:
            m_currentFrame = m_graphics->renderBlank();
            break;
    }

    emit frameUpdated(m_currentFrame);
}

// ─── Bible lookup ────────────────────────────────────────────────────────────

QString ScriptureCasterModule::lookupVerse(const QString& book, int chapter, int verse,
                                            const QString& translation) const {
    QString key = QString("%1:%2:%3:%4").arg(translation, book)
                      .arg(chapter).arg(verse);
    return m_verseCache.value(key);
}

QString ScriptureCasterModule::lookupReference(const QString& refStr,
                                                const QString& translation) const {
    // We attempt to parse "Book Chapter:Verse" or "Book Chapter:Start-End"
    // Simple parser — handles "John 3:16", "1 John 1:9", "Psalms 23:1-4"
    QString ref = refStr.trimmed();
    // We find the last space before a digit group containing ":"
    int colonPos = ref.lastIndexOf(':');
    if (colonPos < 0) return {};

    // Everything before the chapter:verse is the book name
    int spaceBeforeChapter = ref.lastIndexOf(' ', colonPos);
    if (spaceBeforeChapter < 0) return {};

    QString book = ref.left(spaceBeforeChapter).trimmed();
    QString chapVerse = ref.mid(spaceBeforeChapter + 1);
    QStringList parts = chapVerse.split(':');
    if (parts.size() < 2) return {};

    bool ok;
    int chapter = parts[0].toInt(&ok);
    if (!ok) return {};

    // We check for verse range (e.g., "1-4")
    QString versePart = parts[1];
    int dashPos = versePart.indexOf('-');
    int verseStart, verseEnd;
    if (dashPos > 0) {
        verseStart = versePart.left(dashPos).toInt(&ok);
        if (!ok) return {};
        verseEnd = versePart.mid(dashPos + 1).toInt(&ok);
        if (!ok) verseEnd = verseStart;
    } else {
        verseStart = versePart.toInt(&ok);
        if (!ok) return {};
        verseEnd = verseStart;
    }

    // We build the full text from individual verse lookups
    QString result;
    for (int v = verseStart; v <= verseEnd; ++v) {
        QString text = lookupVerse(book, chapter, v, translation);
        if (!text.isEmpty()) {
            if (!result.isEmpty()) result += " ";
            if (verseEnd > verseStart)
                result += QString("[%1] ").arg(v);
            result += text;
        }
    }
    return result;
}

QStringList ScriptureCasterModule::availableTranslations() const { return m_translations; }
QStringList ScriptureCasterModule::bibleBooks() const { return m_bibleBooks; }

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_scriptureInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.scripture",
    "Scripture Caster",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Bible verse and sermon notes display — quick lookup, sermon queue, dual translation"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_scripture_plugin_info() { return &s_scriptureInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_scripture_create_module(IModuleHost*) {
    return new M1::ScriptureCasterModule();
}
MCASTER1_PLUGIN_API void mcaster1_scripture_destroy_module(IModule* m) { delete m; }
}

#include "ScriptureCasterModule.moc"
