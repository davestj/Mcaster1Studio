/// @file   ServiceRunnerModule.cpp
/// @path   Modules/ServiceRunnerModule/ServiceRunnerModule.cpp

#include "ServiceRunnerModule.h"
#include "TimerClockModule.h"
#include "SwitchCasterModule.h"
#include "TranscribeRecModule.h"
#include "AudioMixModule.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QTextEdit>
#include <QSplitter>
#include <QToolButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>

namespace {

// ─── SegmentProgressBar — custom painted progress for segment timer ─────────
class SegmentProgressBar : public QWidget {
public:
    explicit SegmentProgressBar(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(24);
        setObjectName("SegmentProgressBar");
    }

    void setProgress(double fraction) { m_fraction = fraction; update(); }
    void setLabel(const QString& text) { m_label = text; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x1e, 0x29, 0x3b));
        p.drawRoundedRect(rect(), 4, 4);

        // Fill
        if (m_fraction > 0.0) {
            QColor fillColor;
            if (m_fraction < 0.75)
                fillColor = QColor(0x22, 0xc5, 0x5e); // green
            else if (m_fraction < 0.90)
                fillColor = QColor(0xf5, 0x9e, 0x0b); // amber
            else
                fillColor = QColor(0xef, 0x44, 0x44); // red

            const int w = static_cast<int>(width() * std::clamp(m_fraction, 0.0, 1.0));
            p.setBrush(fillColor);
            p.drawRoundedRect(0, 0, w, height(), 4, 4);
        }

        // Label
        p.setPen(Qt::white);
        QFont f = font();
        f.setPixelSize(12);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_label);
    }

private:
    double  m_fraction = 0.0;
    QString m_label;
};

// ─── SegmentEditorDialog — edit a single service segment ─────────────────────
class SegmentEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SegmentEditorDialog(const M1::ServiceSegment& seg, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Edit Segment");
        setMinimumWidth(380);

        auto* lay = new QVBoxLayout(this);
        lay->setSpacing(8);

        // Type combo
        auto* typeRow = new QHBoxLayout;
        typeRow->addWidget(new QLabel("Type:"));
        m_typeCombo = new QComboBox;
        const QStringList types = {
            "Welcome", "Worship", "Prayer", "Scripture", "Sermon",
            "Offering", "Announcement", "Communion", "Media Playback",
            "Closing", "Custom"
        };
        m_typeCombo->addItems(types);
        m_typeCombo->setCurrentIndex(static_cast<int>(seg.type));
        typeRow->addWidget(m_typeCombo, 1);
        lay->addLayout(typeRow);

        // Title
        auto* titleRow = new QHBoxLayout;
        titleRow->addWidget(new QLabel("Title:"));
        m_titleEdit = new QLineEdit(seg.title);
        m_titleEdit->setPlaceholderText("Segment title...");
        titleRow->addWidget(m_titleEdit, 1);
        lay->addLayout(titleRow);

        // Duration
        auto* durRow = new QHBoxLayout;
        durRow->addWidget(new QLabel("Duration (min):"));
        m_durSpin = new QSpinBox;
        m_durSpin->setRange(0, 180);
        m_durSpin->setSpecialValueText("No limit");
        m_durSpin->setValue(seg.durationSec / 60);
        m_durSpin->setToolTip("0 = no time limit");
        durRow->addWidget(m_durSpin);
        durRow->addStretch();
        lay->addLayout(durRow);

        // Auto-advance
        m_autoAdvance = new QCheckBox("Auto-advance when time expires");
        m_autoAdvance->setChecked(seg.autoAdvance);
        lay->addWidget(m_autoAdvance);

        // Notes
        lay->addWidget(new QLabel("Notes:"));
        m_notesEdit = new QTextEdit;
        m_notesEdit->setPlainText(seg.notes);
        m_notesEdit->setMaximumHeight(100);
        m_notesEdit->setPlaceholderText("Operator notes or instructions...");
        lay->addWidget(m_notesEdit);

        // Buttons
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(buttons);
    }

    M1::ServiceSegment result() const {
        M1::ServiceSegment seg;
        seg.type        = static_cast<M1::SegmentType>(m_typeCombo->currentIndex());
        seg.title       = m_titleEdit->text().trimmed();
        seg.notes       = m_notesEdit->toPlainText();
        seg.durationSec = m_durSpin->value() * 60;
        seg.autoAdvance = m_autoAdvance->isChecked();
        seg.color       = M1::ServiceRunnerModule::segmentTypeColor(seg.type);
        return seg;
    }

private:
    QComboBox*  m_typeCombo;
    QLineEdit*  m_titleEdit;
    QSpinBox*   m_durSpin;
    QCheckBox*  m_autoAdvance;
    QTextEdit*  m_notesEdit;
};

// ─── ServiceRunnerWidget ────────────────────────────────────────────────────
class ServiceRunnerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ServiceRunnerWidget(M1::ServiceRunnerModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("ServiceRunnerWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // ── Header — service title + toolbar ────────────────────────────
        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(6);

        auto* titleLabel = new QLabel("Service:");
        titleLabel->setObjectName("ServiceLabel");
        headerRow->addWidget(titleLabel);

        m_titleEdit = new QLineEdit;
        m_titleEdit->setPlaceholderText("Sunday Morning Worship");
        m_titleEdit->setObjectName("ServiceTitleEdit");
        connect(m_titleEdit, &QLineEdit::textChanged, m_mod, &M1::ServiceRunnerModule::setServiceTitle);
        headerRow->addWidget(m_titleEdit, 1);

        auto* loadBtn = new QToolButton;
        loadBtn->setText("Load");
        loadBtn->setObjectName("ServiceToolBtn");
        loadBtn->setToolTip("Load service template");
        connect(loadBtn, &QToolButton::clicked, this, &ServiceRunnerWidget::onLoad);
        headerRow->addWidget(loadBtn);

        auto* saveBtn = new QToolButton;
        saveBtn->setText("Save");
        saveBtn->setObjectName("ServiceToolBtn");
        saveBtn->setToolTip("Save service template");
        connect(saveBtn, &QToolButton::clicked, this, &ServiceRunnerWidget::onSave);
        headerRow->addWidget(saveBtn);

        auto* newBtn = new QToolButton;
        newBtn->setText("New");
        newBtn->setObjectName("ServiceToolBtn");
        newBtn->setToolTip("Clear and start new service");
        connect(newBtn, &QToolButton::clicked, this, [this]() {
            m_mod->clearService();
            m_titleEdit->clear();
        });
        headerRow->addWidget(newBtn);

        root->addLayout(headerRow);

        // ── Timing summary ──────────────────────────────────────────────
        auto* timeRow = new QHBoxLayout;
        m_elapsedLabel = new QLabel("Elapsed: --:--:--");
        m_elapsedLabel->setObjectName("ServiceTimeLabel");
        timeRow->addWidget(m_elapsedLabel);
        timeRow->addStretch();
        m_remainLabel = new QLabel("Remaining: --:--:--");
        m_remainLabel->setObjectName("ServiceTimeLabel");
        timeRow->addWidget(m_remainLabel);
        timeRow->addStretch();
        m_totalLabel = new QLabel("Total: --:--:--");
        m_totalLabel->setObjectName("ServiceTimeLabel");
        timeRow->addWidget(m_totalLabel);
        root->addLayout(timeRow);

        // ── Splitter: rundown table + notes ─────────────────────────────
        auto* splitter = new QSplitter(Qt::Vertical);
        splitter->setObjectName("ServiceSplitter");

        // Rundown table
        m_table = new QTableWidget(0, 5);
        m_table->setObjectName("ServiceRundownTable");
        m_table->setHorizontalHeaderLabels({"#", "Type", "Segment", "Duration", "Status"});
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_table->setColumnWidth(0, 30);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, &QTableWidget::customContextMenuRequested,
                this, &ServiceRunnerWidget::onContextMenu);
        splitter->addWidget(m_table);

        // Notes panel
        m_notesEdit = new QTextEdit;
        m_notesEdit->setObjectName("ServiceNotesEdit");
        m_notesEdit->setReadOnly(true);
        m_notesEdit->setPlaceholderText("Segment notes will appear here...");
        m_notesEdit->setMaximumHeight(80);
        splitter->addWidget(m_notesEdit);

        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);
        root->addWidget(splitter, 1);

        // ── Segment timer bar ───────────────────────────────────────────
        m_segProgress = new SegmentProgressBar;
        root->addWidget(m_segProgress);

        // ── Transport controls ──────────────────────────────────────────
        auto* transRow = new QHBoxLayout;
        transRow->setSpacing(6);

        m_startBtn = new QPushButton("Start Service");
        m_startBtn->setObjectName("ServiceStartBtn");
        m_startBtn->setMinimumHeight(36);
        QFont bf = m_startBtn->font();
        bf.setPixelSize(13);
        bf.setBold(true);
        m_startBtn->setFont(bf);
        connect(m_startBtn, &QPushButton::clicked, this, [this]() {
            if (!m_mod->isRunning())
                m_mod->startService();
            else
                m_mod->stopService();
        });
        transRow->addWidget(m_startBtn);

        auto* prevBtn = new QPushButton("Prev");
        prevBtn->setObjectName("ServiceNavBtn");
        prevBtn->setMinimumHeight(36);
        prevBtn->setToolTip("Previous segment");
        connect(prevBtn, &QPushButton::clicked, m_mod, &M1::ServiceRunnerModule::prevSegment);
        transRow->addWidget(prevBtn);

        m_nextBtn = new QPushButton("Next Segment");
        m_nextBtn->setObjectName("ServiceNavBtn");
        m_nextBtn->setMinimumHeight(36);
        m_nextBtn->setFont(bf);
        connect(m_nextBtn, &QPushButton::clicked, m_mod, &M1::ServiceRunnerModule::nextSegment);
        transRow->addWidget(m_nextBtn);

        m_pauseBtn = new QPushButton("Pause");
        m_pauseBtn->setObjectName("ServiceNavBtn");
        m_pauseBtn->setMinimumHeight(36);
        connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
            if (m_mod->isPaused())
                m_mod->resumeService();
            else
                m_mod->pauseService();
        });
        transRow->addWidget(m_pauseBtn);

        auto* skipBtn = new QPushButton("Skip");
        skipBtn->setObjectName("ServiceNavBtn");
        skipBtn->setMinimumHeight(36);
        skipBtn->setToolTip("Skip current segment");
        connect(skipBtn, &QPushButton::clicked, m_mod, &M1::ServiceRunnerModule::skipSegment);
        transRow->addWidget(skipBtn);

        transRow->addStretch();

        // Add segment button
        auto* addBtn = new QPushButton("+ Add Segment");
        addBtn->setObjectName("ServiceAddBtn");
        addBtn->setMinimumHeight(36);
        connect(addBtn, &QPushButton::clicked, this, &ServiceRunnerWidget::onAddSegment);
        transRow->addWidget(addBtn);

        root->addLayout(transRow);

        // ── Signal connections ───────────────────────────────────────────
        connect(m_mod, &M1::ServiceRunnerModule::serviceOrderChanged, this, &ServiceRunnerWidget::rebuildTable);
        connect(m_mod, &M1::ServiceRunnerModule::segmentChanged, this, &ServiceRunnerWidget::onSegmentChanged);
        connect(m_mod, &M1::ServiceRunnerModule::segmentTimerTick, this, &ServiceRunnerWidget::onTick);
        connect(m_mod, &M1::ServiceRunnerModule::serviceStarted, this, &ServiceRunnerWidget::onServiceStateChanged);
        connect(m_mod, &M1::ServiceRunnerModule::serviceStopped, this, &ServiceRunnerWidget::onServiceStateChanged);
        connect(m_mod, &M1::ServiceRunnerModule::servicePaused, this, &ServiceRunnerWidget::onServiceStateChanged);

        connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
            if (row >= 0 && row < m_mod->segmentCount()) {
                const auto seg = m_mod->segment(row);
                m_notesEdit->setText(seg.notes);
            }
        });

        rebuildTable();
        onServiceStateChanged();
    }

private slots:
    void rebuildTable() {
        m_table->setRowCount(m_mod->segmentCount());
        for (int i = 0; i < m_mod->segmentCount(); ++i) {
            const auto seg = m_mod->segment(i);
            const auto status = m_mod->segmentStatus(i);

            auto* numItem = new QTableWidgetItem(QString::number(i + 1));
            numItem->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, 0, numItem);

            m_table->setItem(i, 1, new QTableWidgetItem(M1::ServiceRunnerModule::segmentTypeName(seg.type)));
            m_table->setItem(i, 2, new QTableWidgetItem(seg.title));

            QString durStr = seg.durationSec > 0
                ? QString("%1:%2").arg(seg.durationSec / 60).arg(seg.durationSec % 60, 2, 10, QChar('0'))
                : QString::fromUtf8("\u221E"); // infinity
            m_table->setItem(i, 3, new QTableWidgetItem(durStr));

            QString statusStr;
            QColor  statusColor;
            switch (status) {
            case M1::SegmentStatus::Done:    statusStr = "Done";    statusColor = QColor(0x22, 0xc5, 0x5e); break;
            case M1::SegmentStatus::Live:    statusStr = "LIVE";    statusColor = QColor(0xef, 0x44, 0x44); break;
            case M1::SegmentStatus::Skipped: statusStr = "Skipped"; statusColor = QColor(0x94, 0xa3, 0xb8); break;
            default:                         statusStr = "Pending"; statusColor = QColor(0x64, 0x74, 0x8b); break;
            }
            auto* statusItem = new QTableWidgetItem(statusStr);
            statusItem->setForeground(statusColor);
            statusItem->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(i, 4, statusItem);

            // Highlight current row
            if (status == M1::SegmentStatus::Live) {
                for (int c = 0; c < 5; ++c) {
                    if (auto* item = m_table->item(i, c))
                        item->setBackground(QColor(0x1e, 0x29, 0x3b));
                }
            }
        }

        // Update total planned time
        const qint64 totalMs = m_mod->totalPlannedMs();
        m_totalLabel->setText(QString("Total: %1").arg(formatTime(totalMs)));
    }

    void onSegmentChanged(int index) {
        rebuildTable();
        if (index >= 0 && index < m_mod->segmentCount()) {
            m_table->selectRow(index);
            const auto seg = m_mod->segment(index);
            m_notesEdit->setText(seg.notes);
        }
    }

    void onTick(qint64 segElapsed, qint64 segTotal) {
        // Segment progress
        if (segTotal > 0) {
            m_segProgress->setProgress(static_cast<double>(segElapsed) / segTotal);
            m_segProgress->setLabel(
                QString("%1 / %2").arg(formatTime(segElapsed), formatTime(segTotal)));
        } else {
            m_segProgress->setProgress(0.0);
            m_segProgress->setLabel(formatTime(segElapsed));
        }

        // Service timing
        m_elapsedLabel->setText(QString("Elapsed: %1").arg(formatTime(m_mod->serviceElapsedMs())));
        const qint64 totalMs = m_mod->totalPlannedMs();
        const qint64 serviceElapsed = m_mod->serviceElapsedMs();
        if (totalMs > 0 && serviceElapsed <= totalMs)
            m_remainLabel->setText(QString("Remaining: %1").arg(formatTime(totalMs - serviceElapsed)));
        else
            m_remainLabel->setText("Remaining: --:--:--");
    }

    void onServiceStateChanged() {
        if (m_mod->isRunning()) {
            m_startBtn->setText("Stop Service");
            m_pauseBtn->setText(m_mod->isPaused() ? "Resume" : "Pause");
        } else {
            m_startBtn->setText("Start Service");
            m_pauseBtn->setText("Pause");
            m_segProgress->setProgress(0.0);
            m_segProgress->setLabel("");
            m_elapsedLabel->setText("Elapsed: --:--:--");
            m_remainLabel->setText("Remaining: --:--:--");
        }
        rebuildTable();
    }

    void onContextMenu(const QPoint& pos) {
        const int row = m_table->rowAt(pos.y());
        QMenu menu(this);

        if (row >= 0) {
            menu.addAction("Edit Segment...", [this, row]() { onEditSegment(row); });
            menu.addAction("Remove Segment", [this, row]() { m_mod->removeSegment(row); });
            menu.addSeparator();
            if (row > 0)
                menu.addAction("Move Up", [this, row]() { m_mod->moveSegment(row, row - 1); });
            if (row < m_mod->segmentCount() - 1)
                menu.addAction("Move Down", [this, row]() { m_mod->moveSegment(row, row + 1); });
            menu.addSeparator();
            if (m_mod->isRunning())
                menu.addAction("Go To This Segment", [this, row]() { m_mod->goToSegment(row); });
        }

        menu.addSeparator();
        menu.addAction("Add Segment...", this, &ServiceRunnerWidget::onAddSegment);
        menu.exec(m_table->viewport()->mapToGlobal(pos));
    }

    void onAddSegment() {
        M1::ServiceSegment blank;
        blank.type       = M1::SegmentType::Custom;
        blank.title      = "New Segment";
        blank.durationSec = 300;
        SegmentEditorDialog dlg(blank, this);
        dlg.setWindowTitle("Add Segment");
        if (dlg.exec() == QDialog::Accepted) {
            auto seg = dlg.result();
            if (seg.title.isEmpty()) seg.title = "Untitled";
            m_mod->addSegment(seg);
        }
    }

    void onEditSegment(int row) {
        if (row < 0 || row >= m_mod->segmentCount()) return;
        auto existing = m_mod->segment(row);
        SegmentEditorDialog dlg(existing, this);
        if (dlg.exec() == QDialog::Accepted) {
            auto updated = dlg.result();
            updated.id = existing.id; // preserve original ID
            if (updated.title.isEmpty()) updated.title = "Untitled";
            m_mod->updateSegment(row, updated);
        }
    }

    void onSave() {
        const QString path = QFileDialog::getSaveFileName(
            this, "Save Service Template", {}, "Service Files (*.m1service);;JSON (*.json)");
        if (!path.isEmpty())
            m_mod->saveTemplate(path);
    }

    void onLoad() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Load Service Template", {}, "Service Files (*.m1service);;JSON (*.json)");
        if (!path.isEmpty()) {
            m_mod->loadTemplate(path);
            m_titleEdit->setText(m_mod->serviceTitle());
        }
    }

private:
    static QString formatTime(qint64 ms) {
        const int secs = static_cast<int>(ms / 1000);
        const int h = secs / 3600;
        const int m = (secs % 3600) / 60;
        const int s = secs % 60;
        if (h > 0)
            return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }

    M1::ServiceRunnerModule* m_mod;
    QTableWidget*      m_table;
    QTextEdit*         m_notesEdit;
    QLineEdit*         m_titleEdit;
    QLabel*            m_elapsedLabel;
    QLabel*            m_remainLabel;
    QLabel*            m_totalLabel;
    SegmentProgressBar* m_segProgress;
    QPushButton*       m_startBtn;
    QPushButton*       m_nextBtn;
    QPushButton*       m_pauseBtn;
};

} // anonymous namespace

#include "ServiceRunnerModule.moc"

namespace M1 {

// ─── ServiceSegment JSON serialization ──────────────────────────────────────
QJsonObject ServiceSegment::toJson() const {
    QJsonObject obj;
    obj["id"]          = id;
    obj["type"]        = static_cast<int>(type);
    obj["title"]       = title;
    obj["notes"]       = notes;
    obj["durationSec"] = durationSec;
    obj["autoAdvance"] = autoAdvance;
    obj["color"]       = color;
    return obj;
}

ServiceSegment ServiceSegment::fromJson(const QJsonObject& obj) {
    ServiceSegment seg;
    seg.id          = obj["id"].toInt();
    seg.type        = static_cast<SegmentType>(obj["type"].toInt());
    seg.title       = obj["title"].toString();
    seg.notes       = obj["notes"].toString();
    seg.durationSec = obj["durationSec"].toInt();
    seg.autoAdvance = obj["autoAdvance"].toBool();
    seg.color       = obj["color"].toString();
    return seg;
}

// ─── Constructor / Destructor ───────────────────────────────────────────────
ServiceRunnerModule::ServiceRunnerModule(QObject* parent)
    : IModule(parent)
{
}

ServiceRunnerModule::~ServiceRunnerModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void ServiceRunnerModule::initialize() {
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(500); // 2Hz tick
    connect(m_tickTimer, &QTimer::timeout, this, &ServiceRunnerModule::onTick);
}

void ServiceRunnerModule::shutdown() {
    if (m_running) stopService();
    if (m_tickTimer) m_tickTimer->stop();
}

QWidget* ServiceRunnerModule::createWidget(QWidget* parent) {
    return new ServiceRunnerWidget(this, parent);
}

void ServiceRunnerModule::saveState(QSettings& s) {
    s.setValue("serviceTitle", m_serviceTitle);
    const QJsonDocument doc(toJson());
    s.setValue("serviceOrder", doc.toJson(QJsonDocument::Compact));
}

void ServiceRunnerModule::loadState(QSettings& s) {
    m_serviceTitle = s.value("serviceTitle").toString();
    const QByteArray data = s.value("serviceOrder").toByteArray();
    if (!data.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject())
            fromJson(doc.object());
    }
}

// ─── Service order management ───────────────────────────────────────────────
int ServiceRunnerModule::addSegment(const ServiceSegment& seg) {
    ServiceSegment s = seg;
    s.id = m_nextId++;
    m_segments.append(s);
    m_statuses.append(SegmentStatus::Pending);
    emit segmentAdded(m_segments.size() - 1);
    emit serviceOrderChanged();
    return s.id;
}

void ServiceRunnerModule::insertSegment(int index, const ServiceSegment& seg) {
    if (index < 0 || index > m_segments.size()) return;
    ServiceSegment s = seg;
    s.id = m_nextId++;
    m_segments.insert(index, s);
    m_statuses.insert(index, SegmentStatus::Pending);
    emit segmentAdded(index);
    emit serviceOrderChanged();
}

void ServiceRunnerModule::removeSegment(int index) {
    if (index < 0 || index >= m_segments.size()) return;
    m_segments.removeAt(index);
    m_statuses.removeAt(index);

    // Adjust current index if needed
    if (m_running && index <= m_currentIndex) {
        m_currentIndex = std::max(m_currentIndex - 1, 0);
    }

    emit segmentRemoved(index);
    emit serviceOrderChanged();
}

void ServiceRunnerModule::moveSegment(int from, int to) {
    if (from < 0 || from >= m_segments.size()) return;
    if (to < 0   || to >= m_segments.size())   return;
    if (from == to) return;

    m_segments.move(from, to);
    m_statuses.move(from, to);

    emit segmentMoved(from, to);
    emit serviceOrderChanged();
}

void ServiceRunnerModule::clearService() {
    if (m_running) stopService();
    m_segments.clear();
    m_statuses.clear();
    m_nextId = 1;
    emit serviceOrderChanged();
}

ServiceSegment ServiceRunnerModule::segment(int index) const {
    if (index < 0 || index >= m_segments.size())
        return ServiceSegment{};
    return m_segments[index];
}

void ServiceRunnerModule::updateSegment(int index, const ServiceSegment& seg) {
    if (index < 0 || index >= m_segments.size()) return;
    m_segments[index] = seg;
    emit serviceOrderChanged();
}

// ─── Live control ──────────────────────────────────────────────────────────
void ServiceRunnerModule::startService() {
    if (m_running || m_segments.isEmpty()) return;

    m_running = true;
    m_paused  = false;
    m_servicePauseAccum = 0;
    m_serviceTimer.start();

    // Reset all statuses
    for (auto& st : m_statuses)
        st = SegmentStatus::Pending;

    m_tickTimer->start();
    emit serviceStarted();

    goToSegment(0);
}

void ServiceRunnerModule::nextSegment() {
    if (!m_running) return;
    if (m_currentIndex + 1 < m_segments.size()) {
        goToSegment(m_currentIndex + 1);
    } else {
        // End of service
        stopService();
    }
}

void ServiceRunnerModule::prevSegment() {
    if (!m_running || m_currentIndex <= 0) return;
    goToSegment(m_currentIndex - 1);
}

void ServiceRunnerModule::goToSegment(int index) {
    if (index < 0 || index >= m_segments.size()) return;

    // Deactivate current
    if (m_currentIndex >= 0 && m_currentIndex < m_segments.size())
        deactivateSegment(m_currentIndex);

    m_currentIndex = index;
    activateSegment(index);
    emit segmentChanged(index);
}

void ServiceRunnerModule::skipSegment() {
    if (!m_running || m_currentIndex < 0) return;

    if (m_currentIndex < m_statuses.size())
        m_statuses[m_currentIndex] = SegmentStatus::Skipped;

    nextSegment();
}

void ServiceRunnerModule::pauseService() {
    if (!m_running || m_paused) return;
    m_paused = true;
    m_pauseStartMs = m_serviceTimer.elapsed();
    emit servicePaused(true);
}

void ServiceRunnerModule::resumeService() {
    if (!m_running || !m_paused) return;
    m_paused = false;
    const qint64 pauseDuration = m_serviceTimer.elapsed() - m_pauseStartMs;
    m_servicePauseAccum += pauseDuration;
    m_segmentPauseAccum += pauseDuration;
    emit servicePaused(false);
}

void ServiceRunnerModule::stopService() {
    if (!m_running) return;

    // Mark current as done
    if (m_currentIndex >= 0 && m_currentIndex < m_statuses.size()) {
        deactivateSegment(m_currentIndex);
    }

    m_running      = false;
    m_paused       = false;
    m_currentIndex = -1;
    m_tickTimer->stop();
    emit serviceStopped();
}

// ─── State query ───────────────────────────────────────────────────────────
qint64 ServiceRunnerModule::segmentElapsedMs() const {
    if (!m_running || m_currentIndex < 0) return 0;
    return m_segmentTimer.elapsed() - m_segmentPauseAccum;
}

qint64 ServiceRunnerModule::serviceElapsedMs() const {
    if (!m_running) return 0;
    return m_serviceTimer.elapsed() - m_servicePauseAccum;
}

qint64 ServiceRunnerModule::totalPlannedMs() const {
    qint64 total = 0;
    for (const auto& seg : m_segments)
        total += static_cast<qint64>(seg.durationSec) * 1000;
    return total;
}

SegmentStatus ServiceRunnerModule::segmentStatus(int index) const {
    if (index < 0 || index >= m_statuses.size())
        return SegmentStatus::Pending;
    return m_statuses[index];
}

// ─── Templates ─────────────────────────────────────────────────────────────
bool ServiceRunnerModule::saveTemplate(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool ServiceRunnerModule::loadTemplate(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;

    return fromJson(doc.object());
}

QJsonObject ServiceRunnerModule::toJson() const {
    QJsonObject obj;
    obj["serviceTitle"] = m_serviceTitle;

    QJsonArray arr;
    for (const auto& seg : m_segments)
        arr.append(seg.toJson());
    obj["segments"] = arr;

    return obj;
}

bool ServiceRunnerModule::fromJson(const QJsonObject& obj) {
    if (m_running) stopService();

    m_serviceTitle = obj["serviceTitle"].toString();
    m_segments.clear();
    m_statuses.clear();

    const QJsonArray arr = obj["segments"].toArray();
    for (const auto& v : arr) {
        m_segments.append(ServiceSegment::fromJson(v.toObject()));
        m_statuses.append(SegmentStatus::Pending);
    }

    // Ensure IDs are unique
    m_nextId = 1;
    for (const auto& seg : m_segments)
        m_nextId = std::max(m_nextId, seg.id + 1);

    emit serviceOrderChanged();
    return true;
}

// ─── Segment type utilities ────────────────────────────────────────────────
QString ServiceRunnerModule::segmentTypeName(SegmentType type) {
    switch (type) {
    case SegmentType::Welcome:       return "Welcome";
    case SegmentType::Worship:       return "Worship";
    case SegmentType::Prayer:        return "Prayer";
    case SegmentType::Scripture:     return "Scripture";
    case SegmentType::Sermon:        return "Sermon";
    case SegmentType::Offering:      return "Offering";
    case SegmentType::Announcement:  return "Announce";
    case SegmentType::Communion:     return "Communion";
    case SegmentType::MediaPlayback: return "Media";
    case SegmentType::Closing:       return "Closing";
    case SegmentType::Custom:        return "Custom";
    }
    return "Custom";
}

QString ServiceRunnerModule::segmentTypeColor(SegmentType type) {
    switch (type) {
    case SegmentType::Welcome:       return "#22c55e"; // green
    case SegmentType::Worship:       return "#8b5cf6"; // violet
    case SegmentType::Prayer:        return "#0ea5e9"; // sky
    case SegmentType::Scripture:     return "#f59e0b"; // amber
    case SegmentType::Sermon:        return "#ec4899"; // pink
    case SegmentType::Offering:      return "#14b8a6"; // teal
    case SegmentType::Announcement:  return "#f97316"; // orange
    case SegmentType::Communion:     return "#a78bfa"; // purple
    case SegmentType::MediaPlayback: return "#06b6d4"; // cyan
    case SegmentType::Closing:       return "#64748b"; // slate
    case SegmentType::Custom:        return "#94a3b8"; // gray
    }
    return "#94a3b8";
}

// ─── Private slots ─────────────────────────────────────────────────────────
void ServiceRunnerModule::onTick() {
    if (!m_running || m_paused) return;

    const qint64 segElapsed = segmentElapsedMs();
    const auto& seg = m_segments[m_currentIndex];
    const qint64 segTotalMs = static_cast<qint64>(seg.durationSec) * 1000;

    emit segmentTimerTick(segElapsed, segTotalMs);

    // Auto-advance check
    if (seg.autoAdvance && seg.durationSec > 0 && segElapsed >= segTotalMs) {
        nextSegment();
    }
}

// ─── Helpers ────────────────────────────────────────────────────────────────
void ServiceRunnerModule::activateSegment(int index) {
    if (index < 0 || index >= m_segments.size()) return;

    m_statuses[index] = SegmentStatus::Live;
    m_segmentPauseAccum = 0;
    m_segmentTimer.start();

    // Create a timer in TimerClockModule if available
    if (m_timerClock) {
        const auto& seg = m_segments[index];
        if (seg.durationSec > 0) {
            m_segmentTimerId = m_timerClock->createTimer(
                seg.title,
                TimerInstance::Mode::CountDown,
                static_cast<qint64>(seg.durationSec) * 1000);
            m_timerClock->startTimer(m_segmentTimerId);
        }
    }
}

void ServiceRunnerModule::deactivateSegment(int index) {
    if (index < 0 || index >= m_statuses.size()) return;

    if (m_statuses[index] == SegmentStatus::Live)
        m_statuses[index] = SegmentStatus::Done;

    // Stop and remove the timer
    if (m_timerClock && m_segmentTimerId >= 0) {
        m_timerClock->stopTimer(m_segmentTimerId);
        m_timerClock->removeTimer(m_segmentTimerId);
        m_segmentTimerId = -1;
    }
}

} // namespace M1
