#include "MonitorWidget.h"
#include "MonitorModule.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFontMetrics>
#include <algorithm>

// ─── ChartWidget ──────────────────────────────────────────────────────────────

ChartWidget::ChartWidget(M1::MonitorModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRect r = rect();
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);

    // Background
    p.fillRect(r, isLight ? QColor("#f5f3f0") : QColor("#0a1628"));

    // Border
    p.setPen(isLight ? QColor("#d8d4ce") : QColor("#1e3a5f"));
    p.drawRect(r.adjusted(0, 0, -1, -1));

    const auto& history = m_module->history();
    if (history.size() < 2) {
        p.setPen(isLight ? QColor("#6b6560") : QColor("#475569"));
        p.drawText(r, Qt::AlignCenter, "Waiting for data...");
        return;
    }

    // Find max listeners for Y scaling (minimum scale = 10 so empty isn't weird)
    int maxListeners = 10;
    for (const auto& snap : history)
        maxListeners = std::max(maxListeners, snap.listeners);

    const int n     = history.size();
    const int cap   = M1::MonitorModule::kHistorySize;
    const double xStep = static_cast<double>(r.width() - 2) / (cap - 1);
    const double yScale = static_cast<double>(r.height() - 16) / maxListeners;

    // Build polyline
    QVector<QPointF> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
        // x: newest point at the right edge — older points offset left
        const int ageSamples = (n - 1) - i; // 0 = newest (rightmost)
        const double x = r.right() - 1.0 - ageSamples * xStep;
        const double y = r.bottom() - 8.0 - history[i].listeners * yScale;
        pts.append(QPointF(x, y));
    }

    // Gradient fill below polyline
    if (!pts.isEmpty()) {
        QPainterPath path;
        path.moveTo(pts.first().x(), r.bottom() - 8);
        for (const QPointF& pt : pts)
            path.lineTo(pt);
        path.lineTo(pts.last().x(), r.bottom() - 8);
        path.closeSubpath();

        QLinearGradient grad(0, r.top(), 0, r.bottom());
        const QColor lineColor = isLight ? QColor(28, 92, 170) : QColor(14, 165, 233);
        grad.setColorAt(0.0, QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 120));
        grad.setColorAt(1.0, QColor(lineColor.red(), lineColor.green(), lineColor.blue(), 20));
        p.fillPath(path, grad);

        // Polyline
        p.setPen(QPen(lineColor, 1.5));
        for (int i = 1; i < pts.size(); ++i)
            p.drawLine(pts[i-1], pts[i]);
    }

    // Y-axis labels: 0 and max
    p.setPen(isLight ? QColor("#6b6560") : QColor("#64748b"));
    QFont f = p.font();
    f.setPixelSize(10);
    p.setFont(f);
    p.drawText(QPointF(3, r.bottom() - 8), "0");
    p.drawText(QPointF(3, r.top() + 12), QString::number(maxListeners));

    // X-axis label: time window
    p.drawText(QRect(0, r.bottom() - 14, r.width(), 12),
               Qt::AlignRight,
               QString("← %1 min").arg(
                   (cap * m_module->config().pollIntervalSec) / 60));
}

// ─── MonitorWidget ────────────────────────────────────────────────────────────

MonitorWidget::MonitorWidget(M1::MonitorModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
    , m_marqueeTimer(new QTimer(this))
{
    buildUi();
    applyStyles();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyStyles(); update(); });

    connect(m_module, &M1::MonitorModule::statsUpdated,
            this, &MonitorWidget::onStatsUpdated);
    connect(m_module, &M1::MonitorModule::connectionStateChanged,
            this, &MonitorWidget::onConnectionStateChanged);
    connect(m_module, &M1::MonitorModule::pollError,
            this, &MonitorWidget::onPollError);

    // Marquee scroll — advance offset every 80 ms when text is long
    connect(m_marqueeTimer, &QTimer::timeout, this, [this]() {
        if (m_nowPlayingText.length() > 40) {
            ++m_marqueeOffset;
            if (m_marqueeOffset > m_nowPlayingText.length())
                m_marqueeOffset = 0;
            const QString displayed =
                m_nowPlayingText.mid(m_marqueeOffset) + "   " +
                m_nowPlayingText.left(m_marqueeOffset);
            m_nowPlayingLabel->setText(displayed.left(60));
        }
    });
    m_marqueeTimer->start(80);

    // Populate config fields from current module config
    const M1::MonitorModule::Config& cfg = m_module->config();
    m_hostEdit->setText(cfg.host);
    m_portSpin->setValue(cfg.port);
    m_mountEdit->setText(cfg.mount);
    m_passwordEdit->setText(cfg.password);
    m_serverTypeCombo->setCurrentIndex(static_cast<int>(cfg.serverType));
    m_intervalSpin->setValue(cfg.pollIntervalSec);

    updateLed(false);
}

void MonitorWidget::buildUi() {
    setObjectName("MonitorWidget");

    // ── LED + listener count + stats grid ────────────────────────────────
    m_ledLabel = new QLabel(this);
    m_ledLabel->setObjectName("MonitorLED");
    m_ledLabel->setFixedSize(14, 14);
    m_ledLabel->setToolTip("gray = disconnected, green = polling, red = error");

    m_listenersLabel = new QLabel("0", this);
    m_listenersLabel->setObjectName("ListenersLabel");
    m_listenersLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_peakLabel    = new QLabel("Peak: 0",     this);
    m_uptimeLabel  = new QLabel("Up: --",      this);
    m_bitrateLabel = new QLabel("Bitrate: --", this);
    m_peakLabel->setObjectName("StatLabel");
    m_uptimeLabel->setObjectName("StatLabel");
    m_bitrateLabel->setObjectName("StatLabel");

    // Stats row: LED + big listener count + smaller stats
    auto* statsRow = new QHBoxLayout;
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(16);
    statsRow->addWidget(m_ledLabel);
    statsRow->addWidget(m_listenersLabel);
    statsRow->addStretch(0);

    auto* smallStats = new QVBoxLayout;
    smallStats->setContentsMargins(0, 0, 0, 0);
    smallStats->setSpacing(2);
    smallStats->addWidget(m_peakLabel);
    smallStats->addWidget(m_uptimeLabel);
    smallStats->addWidget(m_bitrateLabel);
    statsRow->addLayout(smallStats);
    statsRow->addStretch(1);

    // ── Now Playing ───────────────────────────────────────────────────────
    auto* npTitle = new QLabel("Now Playing:", this);
    npTitle->setObjectName("SectionTitle");

    m_nowPlayingLabel = new QLabel("--", this);
    m_nowPlayingLabel->setObjectName("NowPlayingLabel");
    m_nowPlayingLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* npRow = new QHBoxLayout;
    npRow->setContentsMargins(0, 0, 0, 0);
    npRow->setSpacing(8);
    npRow->addWidget(npTitle);
    npRow->addWidget(m_nowPlayingLabel, 1);

    // ── History chart ─────────────────────────────────────────────────────
    m_chartWidget = new ChartWidget(m_module, this);
    m_chartWidget->setObjectName("HistoryChart");

    // ── Config group (collapsible) ────────────────────────────────────────
    m_toggleCfgBtn = new QPushButton("▾  Connection Config", this);
    m_toggleCfgBtn->setObjectName("ToggleCfgBtn");
    m_toggleCfgBtn->setCheckable(true);
    m_toggleCfgBtn->setChecked(true);
    connect(m_toggleCfgBtn, &QPushButton::clicked, this, &MonitorWidget::onToggleConfig);

    m_configGroup = new QGroupBox(this);
    m_configGroup->setObjectName("ConfigGroup");
    m_configGroup->setVisible(true);

    m_hostEdit    = new QLineEdit(m_configGroup);
    m_hostEdit->setPlaceholderText("hostname or IP");
    m_portSpin    = new QSpinBox(m_configGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8000);
    m_mountEdit   = new QLineEdit(m_configGroup);
    m_mountEdit->setPlaceholderText("/stream");
    m_passwordEdit = new QLineEdit(m_configGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("(optional)");

    m_serverTypeCombo = new QComboBox(m_configGroup);
    m_serverTypeCombo->addItem("Icecast2");
    m_serverTypeCombo->addItem("Shoutcast");
    m_serverTypeCombo->addItem("Mcaster1DNAS");

    m_intervalSpin = new QSpinBox(m_configGroup);
    m_intervalSpin->setRange(1, 300);
    m_intervalSpin->setValue(10);
    m_intervalSpin->setSuffix(" s");

    m_connectBtn    = new QPushButton("Connect",    m_configGroup);
    m_disconnectBtn = new QPushButton("Disconnect", m_configGroup);
    m_connectBtn->setObjectName("ConnectBtn");
    m_disconnectBtn->setObjectName("DisconnectBtn");
    m_disconnectBtn->setEnabled(false);

    connect(m_connectBtn,    &QPushButton::clicked, this, &MonitorWidget::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MonitorWidget::onDisconnectClicked);

    // Config grid layout
    auto* cfgGrid = new QGridLayout(m_configGroup);
    cfgGrid->setContentsMargins(8, 8, 8, 8);
    cfgGrid->setSpacing(6);

    cfgGrid->addWidget(new QLabel("Host:"),         0, 0);
    cfgGrid->addWidget(m_hostEdit,                  0, 1);
    cfgGrid->addWidget(new QLabel("Port:"),         0, 2);
    cfgGrid->addWidget(m_portSpin,                  0, 3);
    cfgGrid->addWidget(new QLabel("Mount:"),        1, 0);
    cfgGrid->addWidget(m_mountEdit,                 1, 1);
    cfgGrid->addWidget(new QLabel("Password:"),     1, 2);
    cfgGrid->addWidget(m_passwordEdit,              1, 3);
    cfgGrid->addWidget(new QLabel("Server Type:"),  2, 0);
    cfgGrid->addWidget(m_serverTypeCombo,           2, 1);
    cfgGrid->addWidget(new QLabel("Interval:"),     2, 2);
    cfgGrid->addWidget(m_intervalSpin,              2, 3);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(8);
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addStretch(1);

    cfgGrid->addLayout(btnRow, 3, 0, 1, 4);

    // ── Main layout ───────────────────────────────────────────────────────
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    mainLayout->addLayout(statsRow);

    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("HSep");
    mainLayout->addWidget(sep1);

    mainLayout->addLayout(npRow);
    mainLayout->addWidget(m_chartWidget, 0);

    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("HSep");
    mainLayout->addWidget(sep2);

    mainLayout->addWidget(m_toggleCfgBtn, 0);
    mainLayout->addWidget(m_configGroup, 0);
    mainLayout->addStretch(1);
}

void MonitorWidget::applyStyles() {
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    const QString bg      = isLight ? "#f5f3f0" : "#0c1a2e";
    const QString text    = isLight ? "#1a1814" : "#e2e8f0";
    const QString accent  = isLight ? "#1c5caa" : "#0ea5e9";
    const QString muted   = isLight ? "#6b6560" : "#94a3b8";
    const QString section = isLight ? "#6b6560" : "#64748b";
    const QString chartBg = isLight ? "#f5f3f0" : "#0a1628";
    const QString border  = isLight ? "#d8d4ce" : "#1e3a5f";
    const QString inBg    = isLight ? "#ffffff"  : "#0c1a2e";
    const QString cfgBg   = isLight ? "#ede8e0" : "#0f2040";

    setStyleSheet(QString(R"(
        #MonitorWidget {
            background: %1;
            color: %2;
        }
        #ListenersLabel {
            color: %3;
            font-size: 32px;
            font-weight: bold;
        }
        #StatLabel {
            color: %4;
            font-size: 12px;
        }
        #SectionTitle {
            color: %5;
            font-size: 11px;
        }
        #NowPlayingLabel {
            color: %2;
            font-size: 13px;
            font-style: italic;
        }
        #HistoryChart {
            background: %6;
            border: 1px solid %7;
            border-radius: 3px;
        }
        #HSep { color: %7; }
        #ToggleCfgBtn {
            background: %9;
            color: %4;
            border: 1px solid %7;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
            text-align: left;
        }
        #ToggleCfgBtn:hover { background: %7; color: %2; }
        #ConfigGroup {
            background: %6;
            border: 1px solid %7;
            border-radius: 4px;
        }
        QLabel { color: %4; font-size: 12px; }
        QLineEdit, QSpinBox, QComboBox {
            background: %8;
            color: %2;
            border: 1px solid %7;
            border-radius: 3px;
            padding: 3px 6px;
            font-size: 12px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid %3; }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background: %8;
            color: %2;
            selection-background-color: %3;
        }
        #ConnectBtn {
            background: %3;
            color: #ffffff;
            border: 1px solid %7;
            border-radius: 4px;
            padding: 4px 16px;
            font-size: 12px;
            font-weight: bold;
        }
        #ConnectBtn:hover { background: %3; opacity: 0.85; }
        #ConnectBtn:disabled { background: %7; color: %5; }
        #DisconnectBtn {
            background: %7;
            color: %2;
            border: 1px solid %7;
            border-radius: 4px;
            padding: 4px 16px;
            font-size: 12px;
        }
        #DisconnectBtn:hover { background: #dc2626; color: #ffffff; }
        #DisconnectBtn:disabled { color: %5; }
    )").arg(bg, text, accent, muted, section, chartBg, border, inBg, cfgBg));
}

// ─── Paint ───────────────────────────────────────────────────────────────────

void MonitorWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    // Additional background fill to ensure theme color reaches edges
    QPainter p(this);
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    p.fillRect(rect(), isLight ? QColor("#f5f3f0") : QColor("#0c1a2e"));
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void MonitorWidget::onStatsUpdated(int listeners, int peak, const QString& title) {
    m_listenersLabel->setText(QString::number(listeners));
    m_peakLabel->setText(QString("Peak: %1").arg(peak));

    const qint64 upSecs = m_module->uptimeSeconds();
    m_uptimeLabel->setText("Up: " + formatUptime(upSecs));

    const int br = m_module->bitrate();
    m_bitrateLabel->setText(br > 0 ? QString("Bitrate: %1 kbps").arg(br)
                                   : "Bitrate: --");

    m_nowPlayingText = title.isEmpty() ? "--" : title;
    if (m_nowPlayingText.length() <= 40) {
        m_nowPlayingLabel->setText(m_nowPlayingText);
        m_marqueeOffset = 0;
    }

    m_hasError = false;
    updateLed(true, false);

    // Repaint the chart
    if (m_chartWidget)
        m_chartWidget->update();
}

void MonitorWidget::onConnectionStateChanged(bool connected) {
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_hasError = false;
    updateLed(connected, false);
}

void MonitorWidget::onPollError(const QString& /*err*/) {
    m_hasError = true;
    updateLed(m_module->isConnected(), true);
}

void MonitorWidget::onConnectClicked() {
    // Read config fields into module config
    M1::MonitorModule::Config cfg;
    cfg.host             = m_hostEdit->text().trimmed();
    cfg.port             = m_portSpin->value();
    cfg.mount            = m_mountEdit->text().trimmed();
    cfg.password         = m_passwordEdit->text();
    cfg.serverType       = static_cast<M1::ServerType>(m_serverTypeCombo->currentIndex());
    cfg.pollIntervalSec  = m_intervalSpin->value();

    m_module->setConfig(cfg);
    m_module->connectToServer();
}

void MonitorWidget::onDisconnectClicked() {
    m_module->disconnectFromServer();
    m_listenersLabel->setText("0");
    m_peakLabel->setText("Peak: 0");
    m_uptimeLabel->setText("Up: --");
    m_bitrateLabel->setText("Bitrate: --");
    m_nowPlayingLabel->setText("--");
    if (m_chartWidget) m_chartWidget->update();
}

void MonitorWidget::onToggleConfig() {
    const bool show = m_toggleCfgBtn->isChecked();
    m_configGroup->setVisible(show);
    m_toggleCfgBtn->setText(show ? "▾  Connection Config"
                                 : "▸  Connection Config");
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void MonitorWidget::updateLed(bool connected, bool error) {
    if (!connected) {
        // Gray
        m_ledLabel->setStyleSheet(
            "background:#475569; border-radius:7px; border:1px solid #334155;");
        m_ledLabel->setToolTip("Disconnected");
    } else if (error) {
        // Red
        m_ledLabel->setStyleSheet(
            "background:#ef4444; border-radius:7px; border:1px solid #dc2626;");
        m_ledLabel->setToolTip("Poll error");
    } else {
        // Green
        m_ledLabel->setStyleSheet(
            "background:#22c55e; border-radius:7px; border:1px solid #16a34a;");
        m_ledLabel->setToolTip("Polling OK");
    }
}

QString MonitorWidget::formatUptime(qint64 secs) {
    if (secs <= 0) return "--";
    const qint64 h = secs / 3600;
    const qint64 m = (secs % 3600) / 60;
    const qint64 s = secs % 60;
    if (h > 0)
        return QString("%1h %2m").arg(h).arg(m);
    if (m > 0)
        return QString("%1m %2s").arg(m).arg(s);
    return QString("%1s").arg(s);
}
