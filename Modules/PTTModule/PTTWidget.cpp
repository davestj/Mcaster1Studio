#include "PTTWidget.h"
#include "PTTModule.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QDial>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QEvent>
#include <QMouseEvent>
#include <QFont>
#include <QDebug>
#include <QSettings>
#include <QMediaDevices>
#include <QAudioDevice>

// ─── Status colors (fixed, not theme-dependent) ───────────────────────────────
static const char* kLiveColor         = "#ef4444";
static const char* kArmedColor        = "#f59e0b";
static const char* kOffColor          = "#334155";

// ─── Theme-aware background colors ───────────────────────────────────────────
namespace {
    struct PttColors { QString bg, panel, border, text, dark; };
    PttColors pttColors() {
        using T = ThemeManager::Theme;
        switch (ThemeManager::instance()->currentTheme()) {
        case T::Light:   return { "#f0f0f0","#fafafa","#cccccc","#1a1814","#e0e0e0" };
        case T::Classic: return { "#ede0cc","#f5ead0","#c0a878","#2a1810","#ddd0b8" };
        default:         return { "#0c1a2e","#102035","#1e3a5f","#c8d8e8","#0a1628" };
        }
    }
} // namespace

// ─── PTTButtonWidget — inner painted button ───────────────────────────────────
/// We embed a dedicated sub-widget for painting the round PTT button.
/// Mouse events are forwarded from PTTWidget via event filter instead of
/// subclassing, keeping the public API simple.
class PTTButtonWidget : public QWidget {
public:
    explicit PTTButtonWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(80, 80);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setCursor(Qt::PointingHandCursor);
    }

    void setPressed(bool p) {
        if (m_pressed == p) return;
        m_pressed = p;
        update();
    }

    bool isPressed() const { return m_pressed; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRect r = rect().adjusted(6, 6, -6, -6);

        // Outer ring
        const QColor ringColor = m_pressed ? QColor(kLiveColor) : QColor(kOffColor);
        p.setPen(QPen(ringColor, 4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);

        // Inner fill with radial gradient
        QRadialGradient grad(r.center(), r.width() / 2.0);
        if (m_pressed) {
            grad.setColorAt(0.0, QColor("#ff6b6b"));
            grad.setColorAt(0.5, QColor(kLiveColor));
            grad.setColorAt(1.0, QColor("#991b1b"));
        } else {
            grad.setColorAt(0.0, QColor("#1e3a5f"));
            grad.setColorAt(0.5, QColor("#162c48"));
            grad.setColorAt(1.0, QColor("#0c1a2e"));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawEllipse(r.adjusted(4, 4, -4, -4));

        // "PTT" label
        QFont font("Segoe UI", 14, QFont::Bold);
        p.setFont(font);
        p.setPen(m_pressed ? QColor("#ffffff") : QColor("#7090b0"));
        p.drawText(rect(), Qt::AlignCenter, "PTT");

        // Glow ring when live
        if (m_pressed) {
            QRadialGradient glow(r.center(), r.width() * 0.7);
            glow.setColorAt(0.6, QColor(0xef, 0x44, 0x44, 0));
            glow.setColorAt(0.8, QColor(0xef, 0x44, 0x44, 80));
            glow.setColorAt(1.0, QColor(0xef, 0x44, 0x44, 0));
            p.setBrush(glow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(rect());
        }
    }

private:
    bool m_pressed = false;
};

// ─── PTTWidget ────────────────────────────────────────────────────────────────

PTTWidget::PTTWidget(M1::PTTModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    buildUi();

    // Connect state changes (queued — safe across thread boundary)
    connect(module, &M1::PTTModule::stateChanged,
            this,   &PTTWidget::onStateChanged,
            Qt::QueuedConnection);

    // Theme changes
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &PTTWidget::applyTheme);

    // Level meter polling at 50ms
    connect(&m_levelTimer, &QTimer::timeout, this, &PTTWidget::pollLevelMeter);
    m_levelTimer.start(50);

    // Dial connections
    connect(m_gateDial, &QDial::valueChanged, this, [this](int v) {
        m_module->setGateThreshold(static_cast<float>(v) / 100.0f * 0.1f);
    });
    connect(m_deEssDial, &QDial::valueChanged, this, [this](int v) {
        m_module->setDeEssAmount(static_cast<float>(v) / 100.0f);
    });
    connect(m_compDial, &QDial::valueChanged, this, [this](int v) {
        m_module->setCompThreshold(0.3f + static_cast<float>(v) / 100.0f * 0.7f);
    });
}

void PTTWidget::buildUi() {
    setObjectName("PTTWidget");
    applyTheme();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ── State indicator row ───────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        m_ledLabel = new QLabel(this);
        m_ledLabel->setMinimumSize(14, 14);
        applyLedStyle(M1::PTTModule::State::Off);
        row->addWidget(m_ledLabel);

        m_stateLabel = new QLabel("OFF", this);
        QFont f = m_stateLabel->font();
        f.setBold(true);
        f.setPointSize(10);
        m_stateLabel->setFont(f);
        m_stateLabel->setStyleSheet(QString("color: %1;").arg(kOffColor));
        row->addWidget(m_stateLabel);

        row->addStretch(1);

        auto* hint = new QLabel("Hold PTT to go live", this);
        hint->setObjectName("PTTHintLabel");
        row->addWidget(hint);

        root->addLayout(row);
    }

    // ── Mic device selector ───────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);
        auto* lbl = new QLabel("Mic:", this);
        lbl->setObjectName("PTTSectionLabel");
        row->addWidget(lbl);

        m_deviceCombo = new QComboBox(this);
        m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        for (const auto& dev : QMediaDevices::audioInputs())
            m_deviceCombo->addItem(dev.description(),
                                   QVariant::fromValue(dev.id()));

        // Restore saved device
        QSettings s("Mcaster1", "Mcaster1Studio");
        const QByteArray savedId = s.value("PTT/inputDeviceId").toByteArray();
        for (int i = 0; i < m_deviceCombo->count(); ++i) {
            if (m_deviceCombo->itemData(i).toByteArray() == savedId) {
                m_deviceCombo->setCurrentIndex(i);
                break;
            }
        }
        connect(m_deviceCombo, &QComboBox::currentIndexChanged,
                this, &PTTWidget::onDeviceChanged);
        row->addWidget(m_deviceCombo);
        root->addLayout(row);
    }

    // ── PTT Button (centered) ─────────────────────────────────────────────
    {
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch(1);

        auto* btn = new PTTButtonWidget(this);
        btn->setObjectName("PTTButton");
        m_pttButton = btn;

        // Install event filter to capture mouse press/release on the button
        btn->installEventFilter(this);

        btnRow->addWidget(btn);
        btnRow->addStretch(1);
        root->addLayout(btnRow);
    }

    // ── Input VU meter ────────────────────────────────────────────────────
    {
        auto* meterRow = new QHBoxLayout;
        auto* meterLbl = new QLabel("MIC", this);
        meterLbl->setObjectName("PTTSectionLabel");
        meterLbl->setFixedWidth(28);
        meterRow->addWidget(meterLbl);

        m_vuMeter = new QProgressBar(this);
        m_vuMeter->setRange(0, 100);
        m_vuMeter->setValue(0);
        m_vuMeter->setMinimumHeight(12);
        m_vuMeter->setTextVisible(false);
        m_vuMeter->setStyleSheet(
            "QProgressBar::chunk {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "    stop:0 #22c55e, stop:0.6 #22c55e,"
            "    stop:0.75 #f59e0b, stop:0.9 #ef4444, stop:1 #ef4444);"
            "}"
        );
        meterRow->addWidget(m_vuMeter);

        root->addLayout(meterRow);
    }

    // ── Separator ─────────────────────────────────────────────────────────
    {
        m_sepLine = new QWidget(this);
        m_sepLine->setFixedHeight(1);
        root->addWidget(m_sepLine);
    }

    // ── DSP Knobs ─────────────────────────────────────────────────────────
    {
        auto* knobRow = new QHBoxLayout;
        knobRow->setSpacing(12);

        struct KnobDef { QDial** dial; const char* label; int def; };
        KnobDef defs[] = {
            { &m_gateDial,  "Gate",     10 },
            { &m_deEssDial, "De-ess",   50 },
            { &m_compDial,  "Compress", 70 },
        };

        for (auto& d : defs) {
            auto* col = new QVBoxLayout;
            col->setSpacing(2);
            col->setAlignment(Qt::AlignHCenter);

            auto* dial = new QDial(this);
            dial->setRange(0, 100);
            dial->setValue(d.def);
            dial->setMinimumSize(44, 44);
            dial->setNotchesVisible(true);
            *d.dial = dial;
            col->addWidget(dial, 0, Qt::AlignHCenter);

            auto* lbl = new QLabel(d.label, this);
            lbl->setAlignment(Qt::AlignHCenter);
            lbl->setObjectName("PTTKnobLabel");
            col->addWidget(lbl);

            knobRow->addLayout(col);
        }

        root->addLayout(knobRow);
    }

    root->addStretch(1);
}

bool PTTWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_pttButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                static_cast<PTTButtonWidget*>(m_pttButton)->setPressed(true);
                m_module->setState(M1::PTTModule::State::Live);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                static_cast<PTTButtonWidget*>(m_pttButton)->setPressed(false);
                m_module->setState(M1::PTTModule::State::Off);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void PTTWidget::mousePressEvent(QMouseEvent* event) {
    QWidget::mousePressEvent(event);
}

void PTTWidget::mouseReleaseEvent(QMouseEvent* event) {
    QWidget::mouseReleaseEvent(event);
}

void PTTWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
}

void PTTWidget::onStateChanged(M1::PTTModule::State newState) {
    applyLedStyle(newState);
    applyButtonStyle(newState);

    const QString stateColor = (newState == M1::PTTModule::State::Live)   ? kLiveColor
                             : (newState == M1::PTTModule::State::Armed) ? kArmedColor
                                                                         : kOffColor;
    switch (newState) {
    case M1::PTTModule::State::Off:   m_stateLabel->setText("OFF");   break;
    case M1::PTTModule::State::Armed: m_stateLabel->setText("ARMED"); break;
    case M1::PTTModule::State::Live:  m_stateLabel->setText("LIVE");  break;
    }
    m_stateLabel->setStyleSheet(
        QString("color:%1; font-weight:bold;").arg(stateColor));
}

void PTTWidget::applyLedStyle(M1::PTTModule::State s) {
    if (!m_ledLabel) return;
    QString color;
    switch (s) {
    case M1::PTTModule::State::Off:    color = kOffColor;   break;
    case M1::PTTModule::State::Armed:  color = kArmedColor; break;
    case M1::PTTModule::State::Live:   color = kLiveColor;  break;
    }
    m_ledLabel->setStyleSheet(QString(
        "background: %1; border-radius: 8px; border: 1px solid #334155;"
    ).arg(color));
}

void PTTWidget::applyButtonStyle(M1::PTTModule::State /*s*/) {
    // PTTButtonWidget handles its own paint based on pressed state
    if (m_pttButton) m_pttButton->update();
}

void PTTWidget::pollLevelMeter() {
    if (!m_module || !m_vuMeter) return;
    // Only display mic level when mic is active (Armed or Live); show zero when Off.
    if (m_module->state() == M1::PTTModule::State::Off) {
        m_vuMeter->setValue(0);
        return;
    }
    const float level = m_module->inputLevel();
    const int pct = static_cast<int>(std::min(1.0f, level) * 100.0f);
    m_vuMeter->setValue(pct);
}

void PTTWidget::applyTheme() {
    const auto c = pttColors();
    setStyleSheet(QString(
        "PTTWidget { background-color:%1; border:1px solid %2; border-radius:6px; }"
        "QLabel { color:%3; font-family:'Segoe UI'; }"
        "QProgressBar { border:1px solid %2; border-radius:3px;"
        "  background:%4; text-align:center; color:transparent; }"
        "QProgressBar::chunk { border-radius:2px; }"
        "QDial { background:%1; }"
        "QComboBox { background:%5; color:%3; border:1px solid %2; border-radius:3px;"
        "  font-size:9px; padding:2px 4px; }"
        "QComboBox::drop-down { border:none; width:16px; }"
        "QComboBox QAbstractItemView { background:%5; color:%3;"
        "  border:1px solid %2; selection-background-color:%2; }"
    ).arg(c.bg, c.border, c.text, c.dark, c.panel));

    if (m_sepLine)
        m_sepLine->setStyleSheet(QString("background:%1;").arg(c.border));
}

void PTTWidget::onDeviceChanged(int index) {
    if (!m_deviceCombo || !m_module) return;
    const QByteArray id = m_deviceCombo->itemData(index).toByteArray();
    QSettings s("Mcaster1", "Mcaster1Studio");
    s.setValue("PTT/inputDeviceId", id);
    m_module->setInputDeviceId(QString::fromUtf8(id));
}
