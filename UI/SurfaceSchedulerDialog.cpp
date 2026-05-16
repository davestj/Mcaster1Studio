#include "SurfaceSchedulerDialog.h"
#include "ThemePalette.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileDialog>

static QString typeLabel(ScheduledEventType t) {
    switch (t) {
    case ScheduledEventType::LoadPlaylist: return "Load Playlist";
    case ScheduledEventType::InsertJingle: return "Insert Jingle";
    case ScheduledEventType::InsertVideo:  return "Insert Video";
    case ScheduledEventType::LoadMedia:    return "Load Media";
    case ScheduledEventType::RunCommand:   return "Run Command";
    }
    return "Unknown";
}

SurfaceSchedulerDialog::SurfaceSchedulerDialog(SurfaceScheduler* scheduler,
                                                const QString& surfaceName,
                                                QWidget* parent)
    : QDialog(parent, Qt::Tool)
    , m_scheduler(scheduler)
{
    buildUi(surfaceName);
    refreshList();
    setMinimumSize(680, 480);
}

void SurfaceSchedulerDialog::buildUi(const QString& surfaceName) {
    setWindowTitle("Scheduler — " + surfaceName);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    auto* title = new QLabel("Surface Automation Scheduler: " + surfaceName, this);
    QFont f = title->font(); f.setBold(true); title->setFont(f);
    root->addWidget(title);

    // ── Top: event list ─────────────────────────────────────────────────────
    auto* splitH = new QHBoxLayout();

    auto* listGroup = new QGroupBox("Scheduled Events", this);
    auto* listLay   = new QVBoxLayout(listGroup);
    m_list = new QListWidget(listGroup);
    m_list->setObjectName("SchedulerEventList");
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    listLay->addWidget(m_list);

    auto* listBtns = new QHBoxLayout();
    m_addBtn    = new QPushButton("+ Add",    listGroup);
    m_removeBtn = new QPushButton("Remove",   listGroup);
    m_toggleBtn = new QPushButton("Enable / Disable", listGroup);
    m_removeBtn->setEnabled(false);
    m_toggleBtn->setEnabled(false);
    listBtns->addWidget(m_addBtn);
    listBtns->addWidget(m_removeBtn);
    listBtns->addWidget(m_toggleBtn);
    listBtns->addStretch();
    listLay->addLayout(listBtns);
    splitH->addWidget(listGroup, 2);

    // ── Right: editor form ───────────────────────────────────────────────────
    auto* formGroup = new QGroupBox("Event Details", this);
    auto* grid      = new QGridLayout(formGroup);
    grid->setColumnStretch(1, 1);

    int row = 0;
    grid->addWidget(new QLabel("Time:", formGroup), row, 0);
    m_timeEdit = new QTimeEdit(QTime::currentTime(), formGroup);
    m_timeEdit->setDisplayFormat("HH:mm");
    grid->addWidget(m_timeEdit, row++, 1);

    grid->addWidget(new QLabel("Type:", formGroup), row, 0);
    m_typeCombo = new QComboBox(formGroup);
    m_typeCombo->addItem("Load Playlist",  (int)ScheduledEventType::LoadPlaylist);
    m_typeCombo->addItem("Insert Jingle",  (int)ScheduledEventType::InsertJingle);
    m_typeCombo->addItem("Insert Video",   (int)ScheduledEventType::InsertVideo);
    m_typeCombo->addItem("Load Media",     (int)ScheduledEventType::LoadMedia);
    m_typeCombo->addItem("Run Command",    (int)ScheduledEventType::RunCommand);
    grid->addWidget(m_typeCombo, row++, 1);

    grid->addWidget(new QLabel("Label:", formGroup), row, 0);
    m_labelEdit = new QLineEdit(formGroup);
    m_labelEdit->setPlaceholderText("Event description...");
    grid->addWidget(m_labelEdit, row++, 1);

    grid->addWidget(new QLabel("File / Data:", formGroup), row, 0);
    auto* dataRow = new QHBoxLayout();
    m_dataEdit = new QLineEdit(formGroup);
    m_dataEdit->setPlaceholderText("File path or command...");
    auto* browseBtn = new QPushButton("...", formGroup);
    browseBtn->setFixedWidth(28);
    browseBtn->setToolTip("Browse for file");
    dataRow->addWidget(m_dataEdit);
    dataRow->addWidget(browseBtn);
    grid->addLayout(dataRow, row++, 1);

    grid->addWidget(new QLabel("Deck / Extra:", formGroup), row, 0);
    m_data2Edit = new QLineEdit(formGroup);
    m_data2Edit->setPlaceholderText("Deck: 0=A, 1=B");
    grid->addWidget(m_data2Edit, row++, 1);

    grid->addWidget(new QLabel("Repeat daily:", formGroup), row, 0);
    m_repeatChk = new QCheckBox(formGroup);
    m_repeatChk->setChecked(true);
    grid->addWidget(m_repeatChk, row++, 1);

    m_applyBtn = new QPushButton("Apply Changes", formGroup);
    m_applyBtn->setEnabled(false);
    grid->addWidget(m_applyBtn, row++, 0, 1, 2);

    splitH->addWidget(formGroup, 1);
    root->addLayout(splitH, 1);

    // ── Bottom buttons ───────────────────────────────────────────────────────
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::hide);
    root->addWidget(btns);

    // Connections
    connect(m_addBtn,    &QPushButton::clicked, this, &SurfaceSchedulerDialog::onAddEvent);
    connect(m_removeBtn, &QPushButton::clicked, this, &SurfaceSchedulerDialog::onRemoveEvent);
    connect(m_toggleBtn, &QPushButton::clicked, this, &SurfaceSchedulerDialog::onToggleEnabled);
    connect(m_applyBtn,  &QPushButton::clicked, this, &SurfaceSchedulerDialog::onEditCommit);
    connect(m_list, &QListWidget::itemSelectionChanged,
            this, &SurfaceSchedulerDialog::onEventSelectionChanged);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select File", QString(),
            "Media Files (*.mp3 *.wav *.flac *.ogg *.mp4 *.m3u *.m3u8);;All Files (*)");
        if (!path.isEmpty()) m_dataEdit->setText(path);
    });
}

void SurfaceSchedulerDialog::refreshList() {
    const int selRow = m_list->currentRow();
    m_list->clear();
    for (const auto& ev : m_scheduler->events()) {
        const QString prefix = ev.enabled ? "[ON]  " : "[OFF] ";
        const QString line = prefix
            + ev.triggerTime.toString("HH:mm") + "  "
            + typeLabel(ev.type).leftJustified(16)
            + "  " + ev.label;
        auto* item = new QListWidgetItem(line, m_list);
        item->setData(Qt::UserRole, ev.id);
        if (!ev.enabled) item->setForeground(ThemePalette::forCurrentTheme().textDisabled);
    }
    if (selRow >= 0 && selRow < m_list->count())
        m_list->setCurrentRow(selRow);
}

void SurfaceSchedulerDialog::onEventSelectionChanged() {
    const bool sel = m_list->currentItem() != nullptr;
    m_removeBtn->setEnabled(sel);
    m_toggleBtn->setEnabled(sel);
    m_applyBtn->setEnabled(sel);
    if (!sel) return;

    const int id = m_list->currentItem()->data(Qt::UserRole).toInt();
    for (const auto& ev : m_scheduler->events()) {
        if (ev.id == id) { populateForm(ev); break; }
    }
}

void SurfaceSchedulerDialog::populateForm(const ScheduledEvent& ev) {
    m_timeEdit->setTime(ev.triggerTime);
    for (int i = 0; i < m_typeCombo->count(); ++i) {
        if (m_typeCombo->itemData(i).toInt() == (int)ev.type) {
            m_typeCombo->setCurrentIndex(i);
            break;
        }
    }
    m_labelEdit->setText(ev.label);
    m_dataEdit->setText(ev.data);
    m_data2Edit->setText(ev.data2);
    m_repeatChk->setChecked(ev.repeat);
}

ScheduledEvent SurfaceSchedulerDialog::eventFromForm() const {
    ScheduledEvent ev;
    ev.triggerTime = m_timeEdit->time();
    ev.type        = (ScheduledEventType)m_typeCombo->currentData().toInt();
    ev.label       = m_labelEdit->text().trimmed();
    ev.data        = m_dataEdit->text().trimmed();
    ev.data2       = m_data2Edit->text().trimmed();
    ev.repeat      = m_repeatChk->isChecked();
    ev.enabled     = true;
    return ev;
}

void SurfaceSchedulerDialog::onAddEvent() {
    ScheduledEvent ev = eventFromForm();
    if (ev.label.isEmpty()) ev.label = typeLabel(ev.type);
    m_scheduler->addEvent(ev);
    refreshList();
}

void SurfaceSchedulerDialog::onRemoveEvent() {
    auto* item = m_list->currentItem();
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    m_scheduler->removeEvent(id);
    refreshList();
    m_removeBtn->setEnabled(false);
    m_toggleBtn->setEnabled(false);
    m_applyBtn->setEnabled(false);
}

void SurfaceSchedulerDialog::onToggleEnabled() {
    auto* item = m_list->currentItem();
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    for (auto& ev : const_cast<QList<ScheduledEvent>&>(m_scheduler->events())) {
        if (ev.id == id) {
            ScheduledEvent updated = ev;
            updated.enabled = !updated.enabled;
            m_scheduler->updateEvent(updated);
            break;
        }
    }
    refreshList();
}

void SurfaceSchedulerDialog::onEditCommit() {
    auto* item = m_list->currentItem();
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    ScheduledEvent updated = eventFromForm();
    updated.id = id;
    // Preserve enabled state
    for (const auto& ev : m_scheduler->events()) {
        if (ev.id == id) { updated.enabled = ev.enabled; break; }
    }
    m_scheduler->updateEvent(updated);
    refreshList();
}
