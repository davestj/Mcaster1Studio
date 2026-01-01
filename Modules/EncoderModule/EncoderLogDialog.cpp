#include "EncoderLogDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QClipboard>
#include <QApplication>

// ─────────────────────────────────────────────────────────────────────────────
EncoderLogDialog::EncoderLogDialog(EncoderEventLog* log,
                                   const QString& encoderName,
                                   QWidget* parent)
    : QDialog(parent)
    , m_log(log)
{
    setObjectName("EncoderLogDialog");
    setWindowTitle("Encoder Event Log — " + encoderName);
    setMinimumSize(700, 400);
    resize(780, 500);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    toolbar->addWidget(new QLabel("Filter:"));
    m_filter = new QComboBox;
    m_filter->addItems({"All", "DEBUG", "INFO", "WARN", "ERROR", "CONNECT", "AUTH", "ICY"});
    m_filter->setCurrentIndex(0);
    m_filter->setFixedWidth(100);
    toolbar->addWidget(m_filter);

    m_autoScroll = new QCheckBox("Auto-scroll");
    m_autoScroll->setChecked(true);
    toolbar->addWidget(m_autoScroll);

    toolbar->addStretch();

    auto* copyBtn  = new QPushButton("Copy All");
    copyBtn->setFixedHeight(22);
    toolbar->addWidget(copyBtn);

    auto* clearBtn = new QPushButton("Clear");
    clearBtn->setFixedHeight(22);
    toolbar->addWidget(clearBtn);

    root->addLayout(toolbar);

    // ── Log list ─────────────────────────────────────────────────────────────
    m_list = new QListWidget(this);
    m_list->setObjectName("EncoderLogList");
    m_list->setFont(QFont("Consolas", 9));
    m_list->setWordWrap(false);
    m_list->setAlternatingRowColors(true);
    root->addWidget(m_list);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EncoderLogDialog::onFilterChanged);
    connect(clearBtn, &QPushButton::clicked, this, &EncoderLogDialog::onClearClicked);
    connect(copyBtn,  &QPushButton::clicked, this, &EncoderLogDialog::onCopyClicked);
    connect(m_log,    &EncoderEventLog::entryAdded,
            this,     &EncoderLogDialog::onNewEntry, Qt::QueuedConnection);

    // ── Load existing entries ────────────────────────────────────────────────
    const auto entries = m_log->entries();
    for (const auto& e : entries)
        addEntryToList(e);
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderLogDialog::onNewEntry(const EncoderEventLog::Entry& entry)
{
    addEntryToList(entry);
}

void EncoderLogDialog::addEntryToList(const EncoderEventLog::Entry& entry)
{
    if (!passesFilter(entry.level)) return;

    auto* item = new QListWidgetItem(EncoderEventLog::formatEntry(entry));

    // Color-code by level
    switch (entry.level) {
        case EncoderEventLog::Level::ERR:
            item->setForeground(QColor("#dc2626")); break;
        case EncoderEventLog::Level::WARN:
            item->setForeground(QColor("#d97706")); break;
        case EncoderEventLog::Level::CONNECT:
            item->setForeground(QColor("#16a34a")); break;
        case EncoderEventLog::Level::AUTH:
            item->setForeground(QColor("#7c3aed")); break;
        case EncoderEventLog::Level::ICY_META:
            item->setForeground(QColor("#0891b2")); break;
        case EncoderEventLog::Level::DEBUG:
            item->setForeground(QColor("#6b7280")); break;
        default:
            break;
    }

    m_list->addItem(item);

    // Trim to 500 visible entries
    while (m_list->count() > 500)
        delete m_list->takeItem(0);

    if (m_autoScroll->isChecked())
        m_list->scrollToBottom();
}

bool EncoderLogDialog::passesFilter(EncoderEventLog::Level level) const
{
    const int idx = m_filter->currentIndex();
    if (idx == 0) return true;  // "All"
    // Map combo index to level: 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=CONNECT, 6=AUTH, 7=ICY
    return static_cast<int>(level) == (idx - 1);
}

void EncoderLogDialog::onFilterChanged()
{
    // Rebuild the list with current filter
    m_list->clear();
    const auto entries = m_log->entries();
    for (const auto& e : entries)
        addEntryToList(e);
}

void EncoderLogDialog::onClearClicked()
{
    m_log->clear();
    m_list->clear();
}

void EncoderLogDialog::onCopyClicked()
{
    const auto entries = m_log->entries();
    QString text;
    for (const auto& e : entries)
        text += EncoderEventLog::formatEntry(e) + "\n";
    QApplication::clipboard()->setText(text);
}
