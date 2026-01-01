#include "HealthWidget.h"
#include "HealthModule.h"
#include "LiveMonitorChart.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QPainter>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QDateTime>

// ═════════════════════════════════════════════════════════════════════════════
// Custom painted progress bar (CPU or Memory)
// ═════════════════════════════════════════════════════════════════════════════
class HealthBar : public QWidget {
public:
    explicit HealthBar(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(18);
        setMinimumWidth(120);
    }

    void setFill(float f) { m_fill = qBound(0.0f, f, 1.0f); update(); }
    float fill() const { return m_fill; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(0, 0, -1, -1);
        const float pct = m_fill * 100.0f;

        // Track background
        const bool isLight = ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light;
        const bool isClassic = ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Classic;

        QColor trackBg = isLight ? QColor("#e2e0dc") :
                          isClassic ? QColor("#3d2c1e") : QColor("#1e293b");
        p.setPen(Qt::NoPen);
        p.setBrush(trackBg);
        p.drawRoundedRect(r, 4, 4);

        // Fill color — green < 60%, amber 60-85%, red > 85%
        QColor fillColor;
        if (pct < 60.0f)
            fillColor = QColor("#22c55e");
        else if (pct < 85.0f)
            fillColor = QColor("#f59e0b");
        else
            fillColor = QColor("#ef4444");

        // Filled portion
        if (m_fill > 0.001f) {
            QRectF filled = r;
            filled.setWidth(r.width() * m_fill);
            p.setBrush(fillColor);
            p.drawRoundedRect(filled, 4, 4);
        }

        // Percentage text
        QColor textCol = isLight ? QColor("#1a1814") : QColor("#e2e8f0");
        p.setPen(textCol);
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QString::number(int(pct)) + "%");
    }

private:
    float m_fill = 0.0f;
};

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════
HealthWidget::HealthWidget(M1::HealthModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    setObjectName("HealthWidget");
    buildUi();
    applyStyles();

    // Connect to module snapshot signal
    connect(m_module, &M1::HealthModule::snapshotUpdated,
            this, &HealthWidget::onSnapshot);

    // Re-apply styles on theme change
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyStyles(); });
}

// ═════════════════════════════════════════════════════════════════════════════
// UI Construction
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── System Group ─────────────────────────────────────────────────────────
    auto* sysGroup = new QGroupBox("System", this);
    sysGroup->setObjectName("HealthGroup");
    auto* sysLayout = new QGridLayout(sysGroup);
    sysLayout->setSpacing(4);
    sysLayout->setContentsMargins(8, 16, 8, 8);

    // CPU row
    m_cpuLabel = new QLabel("CPU:", sysGroup);
    m_cpuLabel->setObjectName("HealthLabel");
    m_cpuBar = new HealthBar(sysGroup);
    sysLayout->addWidget(m_cpuLabel, 0, 0);
    sysLayout->addWidget(m_cpuBar,   0, 1);

    // Memory row
    m_memLabel = new QLabel("Memory:", sysGroup);
    m_memLabel->setObjectName("HealthLabel");
    m_memBar = new HealthBar(sysGroup);
    sysLayout->addWidget(m_memLabel, 1, 0);
    sysLayout->addWidget(m_memBar,   1, 1);

    // Peak memory
    m_peakMemLabel = new QLabel("Peak: --", sysGroup);
    m_peakMemLabel->setObjectName("HealthDetail");
    sysLayout->addWidget(m_peakMemLabel, 2, 0, 1, 2);

    sysLayout->setColumnStretch(1, 1);
    root->addWidget(sysGroup);

    // ── Encoders Group ───────────────────────────────────────────────────────
    auto* encGroup = new QGroupBox("Encoders", this);
    encGroup->setObjectName("HealthGroup");
    auto* encLayout = new QVBoxLayout(encGroup);
    encLayout->setSpacing(4);
    encLayout->setContentsMargins(8, 16, 8, 8);

    m_encoderSummary = new QLabel("0 streaming", encGroup);
    m_encoderSummary->setObjectName("HealthLabel");
    encLayout->addWidget(m_encoderSummary);

    // Dot row: up to 8 encoder indicators
    auto* dotRow = new QHBoxLayout;
    dotRow->setSpacing(6);
    for (int i = 0; i < 8; ++i) {
        auto* dot = new QLabel(encGroup);
        dot->setObjectName("HealthEncoderDot");
        dot->setFixedSize(12, 12);
        dot->setToolTip(QString("Encoder %1").arg(i + 1));
        dot->setStyleSheet(
            "background: #6b7280; border-radius: 6px; border: none;");
        dot->setVisible(false);
        dotRow->addWidget(dot);
        m_encoderDots.append(dot);
    }
    dotRow->addStretch();
    encLayout->addLayout(dotRow);

    root->addWidget(encGroup);

    // ── Decks Group ──────────────────────────────────────────────────────────
    auto* deckGroup = new QGroupBox("Decks", this);
    deckGroup->setObjectName("HealthGroup");
    auto* deckLayout = new QVBoxLayout(deckGroup);
    deckLayout->setSpacing(4);
    deckLayout->setContentsMargins(8, 16, 8, 8);

    m_deckAStatus = new QLabel(QString::fromUtf8("\xe2\x97\x8b") + " Deck A: Idle", deckGroup);
    m_deckAStatus->setObjectName("HealthLabel");
    m_deckAStatus->setWordWrap(true);
    deckLayout->addWidget(m_deckAStatus);

    m_deckBStatus = new QLabel(QString::fromUtf8("\xe2\x97\x8b") + " Deck B: Idle", deckGroup);
    m_deckBStatus->setObjectName("HealthLabel");
    m_deckBStatus->setWordWrap(true);
    deckLayout->addWidget(m_deckBStatus);

    root->addWidget(deckGroup);

    // ── Charts ───────────────────────────────────────────────────────────────
    m_cpuChart = new LiveMonitorChart(this);
    m_cpuChart->configure("CPU %", "%",
                           QColor("#8b5cf6"),   // dark: violet
                           QColor("#9b59b6"),   // classic: purple
                           QColor("#7c3aed"),   // light: deep violet
                           60);                 // 60 samples @ 2s = 2min
    m_cpuChart->setMinimumHeight(80);
    m_cpuChart->setMaximumHeight(140);
    root->addWidget(m_cpuChart);

    m_memChart = new LiveMonitorChart(this);
    m_memChart->configure("Memory", "MB",
                           QColor("#ec4899"),   // dark: pink
                           QColor("#e74c3c"),   // classic: red-pink
                           QColor("#db2777"),   // light: deep pink
                           60);
    m_memChart->setMinimumHeight(80);
    m_memChart->setMaximumHeight(140);
    root->addWidget(m_memChart);

    // ── Export Button ────────────────────────────────────────────────────────
    auto* bottomRow = new QHBoxLayout;
    bottomRow->addStretch();

    m_exportBtn = new QPushButton("Export", this);
    m_exportBtn->setObjectName("HealthExportBtn");
    m_exportBtn->setToolTip("Export health history data");

    m_exportMenu = new QMenu(m_exportBtn);
    m_exportMenu->setObjectName("HealthExportMenu");
    auto* csvAction  = m_exportMenu->addAction("Export as CSV...");
    auto* jsonAction = m_exportMenu->addAction("Export as JSON...");
    m_exportBtn->setMenu(m_exportMenu);

    connect(csvAction,  &QAction::triggered, this, &HealthWidget::onExportCsv);
    connect(jsonAction, &QAction::triggered, this, &HealthWidget::onExportJson);

    bottomRow->addWidget(m_exportBtn);
    root->addLayout(bottomRow);

    root->addStretch();
}

// ═════════════════════════════════════════════════════════════════════════════
// Theme
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::applyStyles()
{
    const auto theme = ThemeManager::instance()->currentTheme();
    const bool isLight   = (theme == ThemeManager::Theme::Light);
    const bool isClassic = (theme == ThemeManager::Theme::Classic);

    QString bg, text, accent, border, groupBg, btnBg, btnHover;
    if (isLight) {
        bg = "#f5f3f0"; text = "#1a1814"; accent = "#7c3aed";
        border = "#d8d4ce"; groupBg = "#ffffff";
        btnBg = "#e2e0dc"; btnHover = "#d0cec8";
    } else if (isClassic) {
        bg = "#2a1e14"; text = "#f0e6d8"; accent = "#9b59b6";
        border = "#5a4030"; groupBg = "#3d2c1e";
        btnBg = "#4a3628"; btnHover = "#5a4030";
    } else {
        bg = "#0f172a"; text = "#e2e8f0"; accent = "#8b5cf6";
        border = "#1e3a5f"; groupBg = "#0c1a2e";
        btnBg = "#1e293b"; btnHover = "#334155";
    }

    setStyleSheet(QString(R"(
        #HealthWidget { background: %1; }
        QGroupBox#HealthGroup {
            color: %2; font-size: 13px; font-weight: bold;
            border: 1px solid %4; border-radius: 4px;
            background: %5; margin-top: 12px; padding: 8px 6px 6px 6px;
        }
        QGroupBox#HealthGroup::title {
            subcontrol-origin: margin; left: 10px; padding: 0 4px;
        }
        QLabel#HealthLabel { color: %2; font-size: 12px; }
        QLabel#HealthDetail { color: %3; font-size: 11px; }
        QPushButton#HealthExportBtn {
            background: %6; color: %2; border: 1px solid %4;
            border-radius: 4px; padding: 4px 12px; font-size: 12px;
        }
        QPushButton#HealthExportBtn:hover { background: %7; }
        QPushButton#HealthExportBtn::menu-indicator {
            subcontrol-position: right center; subcontrol-origin: padding;
            right: 4px;
        }
    )").arg(bg, text, accent, border, groupBg, btnBg, btnHover));
}

// ═════════════════════════════════════════════════════════════════════════════
// Snapshot Update
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::onSnapshot(const M1::HealthSnapshot& snap)
{
    // CPU bar — cpuPercent is 0-100
    m_cpuFill = static_cast<float>(snap.cpuPercent) / 100.0f;
    static_cast<HealthBar*>(m_cpuBar)->setFill(m_cpuFill);
    m_cpuLabel->setText(QString("CPU: %1%").arg(snap.cpuPercent, 0, 'f', 1));

    // Memory bar — show working set, use 2 GB as reference max
    const float memMb = static_cast<float>(snap.memoryBytes) / (1024.0f * 1024.0f);
    const float refMax = 2048.0f; // 2 GB reference for bar fill
    m_memFill = qBound(0.0f, memMb / refMax, 1.0f);
    static_cast<HealthBar*>(m_memBar)->setFill(m_memFill);
    m_memLabel->setText(QString("Memory: %1").arg(formatMemory(snap.memoryBytes)));
    m_peakMemLabel->setText("Peak: " + formatMemory(snap.peakMemory));

    // Encoder indicators
    updateEncoderIndicators(snap);

    // Deck A status
    if (snap.deckAPlaying) {
        const int mins = static_cast<int>(snap.deckAPosition) / 60;
        const int secs = static_cast<int>(snap.deckAPosition) % 60;
        m_deckAStatus->setText(QString::fromUtf8("\xe2\x96\xb6") +
            QString(" Deck A: %1  [%2:%3]")
                .arg(snap.deckATrack)
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0')));
    } else {
        m_deckAStatus->setText(QString::fromUtf8("\xe2\x97\x8b") + " Deck A: Idle");
    }

    // Deck B status
    if (snap.deckBPlaying) {
        const int mins = static_cast<int>(snap.deckBPosition) / 60;
        const int secs = static_cast<int>(snap.deckBPosition) % 60;
        m_deckBStatus->setText(QString::fromUtf8("\xe2\x96\xb6") +
            QString(" Deck B: %1  [%2:%3]")
                .arg(snap.deckBTrack)
                .arg(mins, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0')));
    } else {
        m_deckBStatus->setText(QString::fromUtf8("\xe2\x97\x8b") + " Deck B: Idle");
    }

    // Push chart samples
    m_cpuChart->pushSample(static_cast<float>(snap.cpuPercent));
    m_memChart->pushSample(memMb);
}

// ═════════════════════════════════════════════════════════════════════════════
// Encoder Indicators
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::updateEncoderIndicators(const M1::HealthSnapshot& snap)
{
    const int total = qMin(snap.encoderTotal, 8);
    const int live  = snap.encoderLive;

    for (int i = 0; i < 8; ++i) {
        if (i < total) {
            m_encoderDots[i]->setVisible(true);
            // Color: first N dots green (live), rest gray (idle)
            if (i < live) {
                m_encoderDots[i]->setStyleSheet(
                    "background: #22c55e; border-radius: 6px; border: none;");
                m_encoderDots[i]->setToolTip(QString("Encoder %1: Streaming").arg(i + 1));
            } else {
                m_encoderDots[i]->setStyleSheet(
                    "background: #6b7280; border-radius: 6px; border: none;");
                m_encoderDots[i]->setToolTip(QString("Encoder %1: Idle").arg(i + 1));
            }
        } else {
            m_encoderDots[i]->setVisible(false);
        }
    }

    m_encoderSummary->setText(QString("%1 streaming / %2 total")
                               .arg(live).arg(total));
}

// ═════════════════════════════════════════════════════════════════════════════
// Export CSV
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Health Data as CSV", QString(), "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    const auto& history = m_module->history();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << "timestamp,cpu_percent,memory_mb,encoders_live,deck_a_track,deck_b_track\n";

    auto csvEscape = [](const QString& s) -> QString {
        if (s.contains(',') || s.contains('"') || s.contains('\n')) {
            QString escaped = s;
            escaped.replace('"', "\"\"");
            return '"' + escaped + '"';
        }
        return s;
    };

    for (const auto& snap : history) {
        const float memMb = static_cast<float>(snap.memoryBytes) / (1024.0f * 1024.0f);
        const QString ts = QDateTime::fromMSecsSinceEpoch(snap.timestampMs)
                               .toString(Qt::ISODate);

        out << ts << ','
            << QString::number(snap.cpuPercent, 'f', 1) << ','
            << QString::number(memMb, 'f', 1) << ','
            << snap.encoderLive << ','
            << csvEscape(snap.deckATrack) << ','
            << csvEscape(snap.deckBTrack) << '\n';
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Export JSON
// ═════════════════════════════════════════════════════════════════════════════
void HealthWidget::onExportJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Health Data as JSON", QString(), "JSON Files (*.json)");
    if (path.isEmpty()) return;

    const auto& history = m_module->history();

    QJsonArray arr;
    for (const auto& snap : history) {
        const float memMb = static_cast<float>(snap.memoryBytes) / (1024.0f * 1024.0f);
        const QString ts = QDateTime::fromMSecsSinceEpoch(snap.timestampMs)
                               .toString(Qt::ISODate);

        QJsonObject obj;
        obj["timestamp"]     = ts;
        obj["cpu_percent"]   = snap.cpuPercent;
        obj["memory_mb"]     = double(memMb);
        obj["encoders_live"] = snap.encoderLive;
        obj["deck_a_track"]  = snap.deckATrack;
        obj["deck_b_track"]  = snap.deckBTrack;
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
}

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════
QString HealthWidget::formatMemory(qint64 bytes)
{
    if (bytes < 1024LL)
        return QString::number(bytes) + " B";
    if (bytes < 1024LL * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}
