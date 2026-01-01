#include "CartWallWidget.h"
#include "CartButton.h"
#include "CartPlayer.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QToolButton>
#include <QLabel>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>

namespace M1 {

CartWallWidget::CartWallWidget(QList<CartPlayer*>& players, QWidget* parent)
    : QWidget(parent)
    , m_players(players)
{
    setObjectName("CartWallWidget");
    setFocusPolicy(Qt::StrongFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    toolbar->addWidget(new QLabel("Grid:", this));

    m_rowsSpin = new QSpinBox(this);
    m_rowsSpin->setRange(1, 8);
    m_rowsSpin->setValue(m_rows);
    m_rowsSpin->setPrefix("R:");
    m_rowsSpin->setToolTip("Number of rows");
    toolbar->addWidget(m_rowsSpin);

    m_colsSpin = new QSpinBox(this);
    m_colsSpin->setRange(1, 8);
    m_colsSpin->setValue(m_cols);
    m_colsSpin->setPrefix("C:");
    m_colsSpin->setToolTip("Number of columns");
    toolbar->addWidget(m_colsSpin);

    connect(m_rowsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CartWallWidget::onGridSizeChanged);
    connect(m_colsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CartWallWidget::onGridSizeChanged);

    auto* stopAllBtn = new QToolButton(this);
    stopAllBtn->setText("Stop All");
    stopAllBtn->setToolTip("Stop all playing carts");
    connect(stopAllBtn, &QToolButton::clicked, this, &CartWallWidget::onStopAll);
    toolbar->addWidget(stopAllBtn);

    toolbar->addStretch();

    m_statusLabel = new QLabel("0 playing", this);
    m_statusLabel->setObjectName("CartStatusLabel");
    toolbar->addWidget(m_statusLabel);

    root->addLayout(toolbar);

    // ── Grid container ───────────────────────────────────────────────────────
    m_gridContainer = new QWidget(this);
    m_grid = new QGridLayout(m_gridContainer);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(4);
    root->addWidget(m_gridContainer, 1);

    rebuildGrid();
}

void CartWallWidget::setGridSize(int rows, int cols) {
    rows = std::clamp(rows, 1, 8);
    cols = std::clamp(cols, 1, 8);
    if (rows == m_rows && cols == m_cols) return;
    m_rows = rows;
    m_cols = cols;
    m_rowsSpin->blockSignals(true);
    m_colsSpin->blockSignals(true);
    m_rowsSpin->setValue(rows);
    m_colsSpin->setValue(cols);
    m_rowsSpin->blockSignals(false);
    m_colsSpin->blockSignals(false);
    rebuildGrid();
}

void CartWallWidget::rebuildGrid() {
    // Remove existing buttons from grid (don't delete — they persist across rebuilds)
    for (auto* btn : m_buttons) {
        m_grid->removeWidget(btn);
        btn->setVisible(false);
    }

    const int total = m_rows * m_cols;

    // Create any additional buttons needed
    while (m_buttons.size() < total && m_buttons.size() < kMaxCarts) {
        int idx = m_buttons.size();
        // Ensure we have a player for this index
        while (m_players.size() <= idx)
            m_players.append(new CartPlayer(this));

        auto* btn = new CartButton(idx, m_players[idx], m_gridContainer);
        connect(btn, &CartButton::triggered, this, &CartWallWidget::onCartTriggered);
        connect(btn, &CartButton::configRequested, this, &CartWallWidget::onCartConfigRequested);
        m_buttons.append(btn);
    }

    // Place buttons into grid
    for (int i = 0; i < total && i < m_buttons.size(); ++i) {
        int r = i / m_cols;
        int c = i % m_cols;
        m_grid->addWidget(m_buttons[i], r, c);
        m_buttons[i]->setVisible(true);
    }

    // Hide extra buttons
    for (int i = total; i < m_buttons.size(); ++i)
        m_buttons[i]->setVisible(false);
}

// ── Keyboard shortcuts ───────────────────────────────────────────────────────

void CartWallWidget::keyPressEvent(QKeyEvent* event) {
    // F1-F12 trigger carts 0-11
    int key = event->key();
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        int idx = key - Qt::Key_F1;
        if (idx < m_buttons.size() && m_buttons[idx]->isVisible())
            onCartTriggered(idx);
        event->accept();
        return;
    }
    // Escape = stop all
    if (key == Qt::Key_Escape) {
        onStopAll();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void CartWallWidget::onCartTriggered(int index) {
    if (index < 0 || index >= m_players.size()) return;
    auto* player = m_players[index];

    if (player->isPlaying()) {
        player->stop();
    } else if (player->state() == CartPlayer::State::Ready) {
        player->play();
    }

    // Update status
    int playing = 0;
    for (auto* p : m_players)
        if (p->isPlaying()) ++playing;
    m_statusLabel->setText(QString("%1 playing").arg(playing));

    emit cartTriggered(index);
}

void CartWallWidget::onCartConfigRequested(int index) {
    if (index < 0 || index >= m_buttons.size()) return;
    auto* btn = m_buttons[index];
    showCartContextMenu(index, QCursor::pos());
}

void CartWallWidget::showCartContextMenu(int index, const QPoint& globalPos) {
    if (index < 0 || index >= m_buttons.size()) return;
    auto* btn = m_buttons[index];
    auto* player = m_players[index];

    QMenu menu(this);

    if (player->isPlaying()) {
        menu.addAction("Stop", [player]() { player->stop(); });
        menu.addSeparator();
    } else if (player->state() == CartPlayer::State::Ready) {
        menu.addAction("Play", [player]() { player->play(); });
        menu.addSeparator();
    }

    menu.addAction("Load Audio File...", [this, btn]() {
        QString path = QFileDialog::getOpenFileName(
            this, "Load Cart Audio",
            QString(),
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.opus *.aac *.m4a *.wma);;All Files (*)");
        if (!path.isEmpty())
            btn->setFilePath(path);
    });

    menu.addAction("Set Color...", [this, btn]() {
        QColor c = QColorDialog::getColor(btn->color(), this, "Cart Color");
        if (c.isValid())
            btn->setColor(c);
    });

    if (player->state() != CartPlayer::State::Empty) {
        menu.addSeparator();
        menu.addAction("Clear", [btn]() {
            btn->setFilePath("");
        });
    }

    menu.exec(globalPos);
}

void CartWallWidget::onGridSizeChanged() {
    m_rows = m_rowsSpin->value();
    m_cols = m_colsSpin->value();
    rebuildGrid();
}

void CartWallWidget::onStopAll() {
    for (auto* p : m_players) {
        if (p->isPlaying()) p->stop();
    }
    m_statusLabel->setText("0 playing");
}

// ── Persistence ──────────────────────────────────────────────────────────────

void CartWallWidget::saveState(QSettings& s) {
    s.setValue("rows", m_rows);
    s.setValue("cols", m_cols);

    const int total = m_rows * m_cols;
    s.setValue("cartCount", total);
    for (int i = 0; i < total && i < m_buttons.size(); ++i) {
        s.beginGroup(QString("cart_%1").arg(i));
        s.setValue("title",    m_buttons[i]->title());
        s.setValue("filePath", m_buttons[i]->filePath());
        s.setValue("color",    m_buttons[i]->color().name());
        s.setValue("gain",     static_cast<double>(m_players[i]->gain()));
        s.endGroup();
    }
}

void CartWallWidget::loadState(QSettings& s) {
    int rows = s.value("rows", kDefaultRows).toInt();
    int cols = s.value("cols", kDefaultCols).toInt();
    setGridSize(rows, cols);

    int count = s.value("cartCount", 0).toInt();
    for (int i = 0; i < count && i < m_buttons.size(); ++i) {
        s.beginGroup(QString("cart_%1").arg(i));
        QString path = s.value("filePath").toString();
        if (!path.isEmpty()) {
            m_buttons[i]->setFilePath(path);
            QString title = s.value("title").toString();
            if (!title.isEmpty())
                m_buttons[i]->setTitle(title);
        }
        QString colorName = s.value("color").toString();
        if (!colorName.isEmpty())
            m_buttons[i]->setColor(QColor(colorName));
        float gain = s.value("gain", 1.0).toFloat();
        m_players[i]->setGain(gain);
        s.endGroup();
    }
}

} // namespace M1
