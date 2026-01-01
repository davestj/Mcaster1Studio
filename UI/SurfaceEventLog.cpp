#include "SurfaceEventLog.h"
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QDateTime>

SurfaceEventLog::SurfaceEventLog(const QString& surfaceName, QObject* parent)
    : QObject(parent)
    , m_surfaceName(surfaceName)
{}

void SurfaceEventLog::appendEvent(const QString& category, const QString& message) {
    SurfaceEventEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.category  = category;
    entry.message   = message;
    m_entries.append(entry);

    // Cap at 2000 entries to avoid unbounded growth
    if (m_entries.size() > 2000)
        m_entries.removeFirst();

    ++m_unreadCount;

    // Update live dialog if open
    if (m_listWidget) {
        const QString timeStr = entry.timestamp.toString("HH:mm:ss");
        const QString text    = QString("[%1] %-8 %2").arg(timeStr, entry.category.leftJustified(8), entry.message);
        auto* item = new QListWidgetItem(
            QString("[%1] %2  %3").arg(timeStr, entry.category.leftJustified(8), entry.message),
            m_listWidget);

        // Colour-code by category
        if (category == "ERROR")
            item->setForeground(QColor("#ef4444"));
        else if (category == "SCHED")
            item->setForeground(QColor("#f59e0b"));
        else if (category == "DECK")
            item->setForeground(QColor("#38bdf8"));
        else if (category == "ENCODER")
            item->setForeground(QColor("#22c55e"));

        m_listWidget->scrollToBottom();
    }

    emit eventAppended(entry);
}

void SurfaceEventLog::showDialog(QWidget* parentWidget) {
    if (!m_dialog)
        buildDialog(parentWidget);

    m_unreadCount = 0;

    if (m_dialog->isVisible()) {
        m_dialog->raise();
        m_dialog->activateWindow();
    } else {
        m_dialog->show();
    }
}

void SurfaceEventLog::buildDialog(QWidget* parentWidget) {
    m_dialog = new QDialog(parentWidget, Qt::Tool | Qt::WindowStaysOnTopHint);
    m_dialog->setWindowTitle("Event Log — " + m_surfaceName);
    m_dialog->resize(620, 420);

    auto* layout = new QVBoxLayout(m_dialog);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Header
    auto* header = new QHBoxLayout();
    auto* titleLabel = new QLabel("Surface Event Log: " + m_surfaceName, m_dialog);
    titleLabel->setObjectName("EventLogTitle");
    QFont f = titleLabel->font();
    f.setBold(true);
    titleLabel->setFont(f);
    header->addWidget(titleLabel);
    header->addStretch();

    auto* clearBtn = new QPushButton("Clear", m_dialog);
    clearBtn->setFixedHeight(22);
    connect(clearBtn, &QPushButton::clicked, m_dialog, [this]() {
        m_entries.clear();
        m_unreadCount = 0;
        if (m_listWidget) m_listWidget->clear();
    });
    header->addWidget(clearBtn);

    auto* closeBtn = new QPushButton("Close", m_dialog);
    closeBtn->setFixedHeight(22);
    connect(closeBtn, &QPushButton::clicked, m_dialog, &QDialog::hide);
    header->addWidget(closeBtn);

    layout->addLayout(header);

    // Event list
    m_listWidget = new QListWidget(m_dialog);
    m_listWidget->setObjectName("EventLogList");
    m_listWidget->setFont(QFont("Consolas", 9));
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listWidget, 1);

    // Populate from existing entries
    for (const auto& e : m_entries) {
        const QString text = QString("[%1] %2  %3")
            .arg(e.timestamp.toString("HH:mm:ss"),
                 e.category.leftJustified(8),
                 e.message);
        auto* item = new QListWidgetItem(text, m_listWidget);
        if (e.category == "ERROR")
            item->setForeground(QColor("#ef4444"));
        else if (e.category == "SCHED")
            item->setForeground(QColor("#f59e0b"));
        else if (e.category == "DECK")
            item->setForeground(QColor("#38bdf8"));
        else if (e.category == "ENCODER")
            item->setForeground(QColor("#22c55e"));
    }
    m_listWidget->scrollToBottom();
}
