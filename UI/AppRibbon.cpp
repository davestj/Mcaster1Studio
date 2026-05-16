#include "AppRibbon.h"
#include "ThemePalette.h"
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QScrollArea>
#include <QDialog>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QApplication>
#include <QDebug>

// ─── RibbonBox ───────────────────────────────────────────────────────────────

static const char* kRibbonMime = "application/x-m1-ribbon-box";

RibbonBox::RibbonBox(QWidget* content, const QString& id, QWidget* parent)
    : QWidget(parent)
    , m_id(id)
    , m_content(content)
{
    setObjectName("RibbonBox");
    setFixedHeight(92);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    // Grip
    m_grip = new QWidget(this);
    m_grip->setObjectName("RibbonBoxGrip");
    m_grip->setFixedWidth(8);
    m_grip->setCursor(Qt::SizeHorCursor);
    m_grip->setToolTip("Drag to reorder");

    auto* gripLabel = new QLabel("⠿", m_grip);
    gripLabel->setObjectName("RibbonBoxGrip");
    gripLabel->setAlignment(Qt::AlignCenter);
    auto* gl = new QVBoxLayout(m_grip);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->addWidget(gripLabel);

    // Content
    if (m_content) {
        m_content->setParent(this);
        m_content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }

    // Close button (hidden until hover)
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setObjectName("RibbonBoxClose");
    m_closeBtn->setFixedSize(16, 16);
    m_closeBtn->hide();
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested(this);
    });

    layout->addWidget(m_grip);
    if (m_content) layout->addWidget(m_content, 1);
    layout->addWidget(m_closeBtn);

    // Min width from content
    const int contentW = m_content ? qMax(80, m_content->sizeHint().width()) : 80;
    setMinimumWidth(contentW + 30);
}

void RibbonBox::enterEvent(QEnterEvent* e) {
    QWidget::enterEvent(e);
    m_closeBtn->show();
}

void RibbonBox::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    m_closeBtn->hide();
}

void RibbonBox::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_grip->geometry().contains(e->pos())) {
        m_dragStart = e->pos();
        m_dragging  = true;
    }
    QWidget::mousePressEvent(e);
}

void RibbonBox::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging) { QWidget::mouseMoveEvent(e); return; }
    if ((e->pos() - m_dragStart).manhattanLength() < QApplication::startDragDistance()) {
        QWidget::mouseMoveEvent(e); return;
    }

    m_dragging = false;
    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData(kRibbonMime,
        QByteArray::number(reinterpret_cast<quintptr>(this), 16));
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}

// ─── AppRibbon ───────────────────────────────────────────────────────────────

AppRibbon::AppRibbon(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("AppRibbon");
    setFixedHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->setSpacing(6);

    // Logo
    auto* logo = new QLabel("◉ MCASTER1", this);
    logo->setObjectName("AppRibbonLogo");
    logo->setFixedWidth(110);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setObjectName("AppRibbonSep");
    sep->setFixedWidth(2);

    // Drop zone (holds ribbon boxes)
    m_dropZone = new QWidget(this);
    m_dropZone->setAcceptDrops(true);
    m_dropZone->installEventFilter(this);
    m_dropZone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_dropLayout = new QHBoxLayout(m_dropZone);
    m_dropLayout->setContentsMargins(4, 2, 4, 2);
    m_dropLayout->setSpacing(4);
    m_dropLayout->addStretch(1);

    // Drop indicator (vertical blue line)
    m_dropIndicator = new QFrame(m_dropZone);
    m_dropIndicator->setFrameShape(QFrame::VLine);
    m_dropIndicator->setObjectName("RibbonDropIndicator");
    m_dropIndicator->setFixedWidth(3);
    m_dropIndicator->setStyleSheet(
        QString("QFrame { background: %1; border: none; }")
            .arg(ThemePalette::forCurrentTheme().accent.name()));
    m_dropIndicator->hide();

    // Right section: + Add, ON AIR
    auto* addBtn = new QPushButton("+ Add", this);
    addBtn->setObjectName("AppRibbonAddBtn");
    addBtn->setFixedHeight(36);
    connect(addBtn, &QPushButton::clicked, this, &AppRibbon::onAddClicked);

    m_onAirBtn = new QPushButton("● ON AIR", this);
    m_onAirBtn->setObjectName("AppRibbonOnAirBtn");
    m_onAirBtn->setCheckable(true);
    m_onAirBtn->setFixedHeight(36);
    connect(m_onAirBtn, &QPushButton::toggled, this, &AppRibbon::onAirToggled);

    root->addWidget(logo);
    root->addWidget(sep);
    root->addWidget(m_dropZone, 1);
    root->addWidget(addBtn);
    root->addWidget(m_onAirBtn);
}

bool AppRibbon::isOnAir() const {
    return m_onAirBtn->isChecked();
}

void AppRibbon::setOnAir(bool onAir) {
    if (m_onAirBtn->isChecked() != onAir)
        m_onAirBtn->setChecked(onAir);
}

RibbonBox* AppRibbon::addBox(QWidget* content, const QString& id) {
    auto* box = new RibbonBox(content, id, m_dropZone);
    connect(box, &RibbonBox::closeRequested, this, &AppRibbon::onBoxCloseRequested);
    m_boxes.append(box);
    rebuildOrder();
    return box;
}

void AppRibbon::removeBox(RibbonBox* box) {
    m_boxes.removeOne(box);
    box->setParent(nullptr);
    box->deleteLater();
    rebuildOrder();
}

void AppRibbon::rebuildOrder() {
    // Remove all items from layout except stretch at end
    while (m_dropLayout->count() > 0) {
        auto* item = m_dropLayout->takeAt(0);
        if (item->widget()) item->widget()->setParent(nullptr);
        delete item;
    }

    for (auto* box : m_boxes)
        m_dropLayout->addWidget(box);

    m_dropLayout->addStretch(1);
}

void AppRibbon::onBoxCloseRequested(RibbonBox* box) {
    removeBox(box);
}

void AppRibbon::onAddClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle("Add to Ribbon");
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(8);

    auto* clockBtn   = new QRadioButton("Clock  (system timezone)", &dlg);
    auto* weatherBtn = new QRadioButton("Weather  (configure location)", &dlg);
    auto* vuBtn      = new QRadioButton("VU Meter  (compact horizontal bars)", &dlg);
    clockBtn->setChecked(true);

    layout->addWidget(clockBtn);
    layout->addWidget(weatherBtn);
    layout->addWidget(vuBtn);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    if (clockBtn->isChecked())        emit addClockRequested();
    else if (weatherBtn->isChecked()) emit addWeatherRequested();
    else if (vuBtn->isChecked())      emit addVUMeterRequested();
}

// ─── Event filter (drop zone) ─────────────────────────────────────────────────

bool AppRibbon::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_dropZone) return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::DragEnter:
        return handleDragEnter(static_cast<QDragEnterEvent*>(event));
    case QEvent::DragMove:
        return handleDragMove(static_cast<QDragMoveEvent*>(event));
    case QEvent::Drop:
        return handleDrop(static_cast<QDropEvent*>(event));
    case QEvent::DragLeave:
        handleDragLeave();
        return false;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

bool AppRibbon::handleDragEnter(QDragEnterEvent* e) {
    if (e->mimeData()->hasFormat(kRibbonMime)) {
        e->acceptProposedAction();
        return true;
    }
    return false;
}

bool AppRibbon::handleDragMove(QDragMoveEvent* e) {
    if (!e->mimeData()->hasFormat(kRibbonMime)) return false;
    e->acceptProposedAction();
    const int idx = dropIndexAt(e->position().toPoint());
    positionDropIndicator(idx);
    return true;
}

bool AppRibbon::handleDrop(QDropEvent* e) {
    if (!e->mimeData()->hasFormat(kRibbonMime)) return false;

    handleDragLeave();

    const quintptr ptr = e->mimeData()->data(kRibbonMime).toLongLong(nullptr, 16);
    auto* box = reinterpret_cast<RibbonBox*>(ptr);
    if (!box || !m_boxes.contains(box)) return false;

    const int newIdx = dropIndexAt(e->position().toPoint());
    m_boxes.removeOne(box);
    const int insertIdx = qMin(newIdx, m_boxes.size());
    m_boxes.insert(insertIdx, box);
    rebuildOrder();

    e->acceptProposedAction();
    return true;
}

void AppRibbon::handleDragLeave() {
    m_dropIndicator->hide();
    m_dropIndex = -1;
}

int AppRibbon::dropIndexAt(const QPoint& pos) const {
    for (int i = 0; i < m_boxes.size(); ++i) {
        const QRect r = m_boxes[i]->geometry();
        if (pos.x() < r.center().x()) return i;
    }
    return m_boxes.size();
}

void AppRibbon::positionDropIndicator(int index) {
    if (index == m_dropIndex) return;
    m_dropIndex = index;

    int x = 4; // default: before first
    if (index > 0 && index <= m_boxes.size()) {
        const QRect r = m_boxes[index - 1]->geometry();
        x = r.right() + 2;
    } else if (!m_boxes.isEmpty() && index == 0) {
        x = m_boxes[0]->geometry().left() - 4;
    }

    m_dropIndicator->setGeometry(x, 2, 3, m_dropZone->height() - 4);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}
