#include "ClockModule.h"
#include "IPlugin.h"
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QSettings>
#include <QDebug>

namespace M1 {

// ─── Timezone picker dialog ───────────────────────────────────────────────────
namespace {
class TimezonePicker : public QDialog {
    Q_OBJECT
public:
    explicit TimezonePicker(const QTimeZone& current, QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Select Timezone");
        setMinimumSize(340, 480);

        auto* layout = new QVBoxLayout(this);
        auto* search = new QLineEdit(this);
        search->setPlaceholderText("Search timezone...");
        layout->addWidget(search);

        m_list = new QListWidget(this);
        const QList<QByteArray> zones = QTimeZone::availableTimeZoneIds();
        for (const QByteArray& id : zones) {
            auto* item = new QListWidgetItem(QString::fromLatin1(id), m_list);
            if (QTimeZone(id) == current) {
                m_list->setCurrentItem(item);
                m_list->scrollToItem(item);
            }
        }
        layout->addWidget(m_list, 1);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        connect(search, &QLineEdit::textChanged, this, [this](const QString& text) {
            for (int i = 0; i < m_list->count(); ++i) {
                auto* item = m_list->item(i);
                item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
            }
        });
        connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    }

    QTimeZone selectedTimezone() const {
        if (auto* item = m_list->currentItem())
            return QTimeZone(item->text().toLatin1());
        return QTimeZone::systemTimeZone();
    }

private:
    QListWidget* m_list = nullptr;
};

// ─── ClockWidget — 3D LED-panel digital clock ─────────────────────────────────
///
/// Renders a recessed dark LCD panel with amber glowing digits.
/// Top portion: time in 12-hour format (h:mm:ss AM/PM)
/// Bottom portion (compact) or two rows (full): date + timezone
///
/// Custom paintEvent provides the 3D raised-border + dark gradient look.
/// No child widgets — all text drawn directly via QPainter for crisp rendering.
class ClockWidget : public QWidget {
    Q_OBJECT
public:
    explicit ClockWidget(ClockModule* module, bool compact, QWidget* parent = nullptr)
        : QWidget(parent), m_module(module), m_compact(compact)
    {
        setCursor(Qt::PointingHandCursor);
        setToolTip("Click to change timezone");

        if (compact) {
            setFixedHeight(88);
            setMinimumWidth(230);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        } else {
            setMinimumSize(210, 82);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }

        // Connect to module status signal: "time\ndate\ntz"
        connect(module, &ClockModule::statusChanged, this, [this](const QString& s) {
            const QStringList parts = s.split('\n');
            if (parts.size() >= 1) m_timeStr = parts[0];
            if (parts.size() >= 2) m_dateStr = parts[1];
            if (parts.size() >= 3) m_tzStr   = parts[2];
            update();
        });
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
            TimezonePicker dlg(m_module->timezone(), this);
            if (dlg.exec() == QDialog::Accepted)
                m_module->setTimezone(dlg.selectedTimezone());
        }
        QWidget::mousePressEvent(e);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const int w = width(), h = height();

        // ── 3D raised outer border ────────────────────────────────────────────
        // Top + left: lighter (highlight = raised effect)
        p.setPen(QColor(68, 72, 82));
        p.drawLine(0, 0, w - 2, 0);
        p.drawLine(0, 0, 0, h - 2);
        // Bottom + right: black (shadow = raised effect)
        p.setPen(QColor(0, 0, 0));
        p.drawLine(0, h - 1, w - 1, h - 1);
        p.drawLine(w - 1, 0, w - 1, h - 1);

        // ── Dark LCD-panel background ─────────────────────────────────────────
        {
            QLinearGradient bg(0, 1, 0, h - 1);
            bg.setColorAt(0.0, QColor(14, 18, 24));
            bg.setColorAt(1.0, QColor(7,  10, 15));
            p.fillRect(1, 1, w - 2, h - 2, bg);
        }

        // ── Inner recessed ring ───────────────────────────────────────────────
        // Top + left inner: black (recessed shadow)
        p.setPen(QColor(0, 0, 0));
        p.drawLine(1, 1, w - 3, 1);
        p.drawLine(1, 1, 1, h - 3);
        // Bottom + right inner: slightly lighter (recessed highlight)
        p.setPen(QColor(28, 34, 44));
        p.drawLine(1, h - 2, w - 2, h - 2);
        p.drawLine(w - 2, 1, w - 2, h - 2);

        // ── Amber colour palette ──────────────────────────────────────────────
        const QColor kAmber    (242, 168, 12);   // main bright amber glow
        const QColor kAmberGlow(255, 200, 60);   // brightest — colon/digits
        const QColor kAmberDim (145, 88,  4);    // date + tz row
        const QColor kAmberShdw( 30, 18,  0);    // text shadow offset

        const int pad = 7;   // horizontal text padding from border ring

        if (m_compact) {
            // Compact (88px tall) — three rows inside ~84px usable space
            // Row 1: time   y: 2..56 (54px) — large bold
            // Row 2: date   y: 56..76 (20px)
            // Row 3: TZ     y: 76..86 (10px) — right-aligned dim

            // ── Time row ────────────────────────────────────────
            QFont tf("Consolas", 22, QFont::Bold);
            tf.setStyleHint(QFont::Monospace);
            p.setFont(tf);

            const QRect tRect(pad, 2, w - 2 * pad, 54);
            p.setPen(kAmberShdw);
            p.drawText(tRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, m_timeStr);
            p.setPen(kAmberGlow);
            p.drawText(tRect, Qt::AlignLeft | Qt::AlignVCenter, m_timeStr);

            // ── Date row ────────────────────────────────────────
            QFont df("Consolas", 9);
            df.setStyleHint(QFont::Monospace);
            p.setFont(df);

            const QRect dRect(pad, 56, w - 2 * pad, 20);
            p.setPen(kAmberShdw);
            p.drawText(dRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, m_dateStr);
            p.setPen(kAmber);
            p.drawText(dRect, Qt::AlignLeft | Qt::AlignVCenter, m_dateStr);

            // ── Timezone row ─────────────────────────────────────
            QFont zf("Consolas", 8);
            zf.setStyleHint(QFont::Monospace);
            p.setFont(zf);

            const QRect zRect(pad, 76, w - 2 * pad, 10);
            p.setPen(kAmberShdw);
            p.drawText(zRect.translated(1, 1), Qt::AlignRight | Qt::AlignVCenter, m_tzStr);
            p.setPen(kAmberDim);
            p.drawText(zRect, Qt::AlignRight | Qt::AlignVCenter, m_tzStr);

        } else {
            // Full dock mode — three rows: big time / date / timezone
            const int timeH = (h * 54) / 100;
            const int dateH = (h * 27) / 100;
            // remaining for TZ

            // ── Time row ────────────────────────────────────────
            QFont tf("Consolas", 20, QFont::Bold);
            tf.setStyleHint(QFont::Monospace);
            p.setFont(tf);

            const QRect tRect(pad, 2, w - 2 * pad, timeH - 2);
            p.setPen(kAmberShdw);
            p.drawText(tRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);
            p.setPen(kAmberGlow);
            p.drawText(tRect, Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);

            // ── Date row ─────────────────────────────────────────
            QFont df("Consolas", 10, QFont::Bold);
            df.setStyleHint(QFont::Monospace);
            p.setFont(df);

            const QRect dRect(pad, timeH, w - 2 * pad, dateH);
            p.setPen(kAmberShdw);
            p.drawText(dRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);
            p.setPen(kAmber);
            p.drawText(dRect, Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);

            // ── Timezone row ──────────────────────────────────────
            QFont zf("Consolas", 9);
            zf.setStyleHint(QFont::Monospace);
            p.setFont(zf);

            const QRect zRect(pad, timeH + dateH, w - 2 * pad, h - timeH - dateH - 2);
            p.setPen(kAmberShdw);
            p.drawText(zRect.translated(1, 1), Qt::AlignRight | Qt::AlignVCenter, m_tzStr);
            p.setPen(kAmberDim);
            p.drawText(zRect, Qt::AlignRight | Qt::AlignVCenter, m_tzStr);
        }
    }

private:
    ClockModule* m_module;
    bool         m_compact;
    QString      m_timeStr = "12:00:00 AM";
    QString      m_dateStr = "Thu, Jan  1, 2026";
    QString      m_tzStr   = "UTC";
};

} // anonymous namespace

// ─── ClockModule ──────────────────────────────────────────────────────────────
ClockModule::ClockModule(QObject* parent)
    : IModule(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(500); // 2 Hz — more than enough for seconds display
    connect(m_timer, &QTimer::timeout, this, &ClockModule::onTick);
}

ClockModule::~ClockModule() {
    shutdown();
}

void ClockModule::initialize() {
    m_timer->start();
    onTick(); // immediate first display
    qInfo() << "[ClockModule] Started, timezone:" << m_timezone.id();
}

void ClockModule::shutdown() {
    m_timer->stop();
}

QWidget* ClockModule::createWidget(QWidget* parent) {
    return new ClockWidget(this, false, parent);
}

QWidget* ClockModule::createCompactWidget(QWidget* parent) {
    return new ClockWidget(this, true, parent);
}

void ClockModule::onTick() {
    const QDateTime now = QDateTime::currentDateTime().toTimeZone(m_timezone);
    // 12-hour time with AM/PM, no leading zero on hour
    const QString timeStr = now.toString("h:mm:ss AP");
    // Date: short day-of-week, short month, day (no leading zero), year
    const QString dateStr = now.toString("ddd  MMM d  yyyy");
    const QString tzStr   = m_timezone.abbreviation(now);

    emit statusChanged(timeStr + "\n" + dateStr + "\n" + tzStr);
}

void ClockModule::setTimezone(const QTimeZone& tz) {
    m_timezone = tz;
    emit timezoneChanged(tz);
    onTick();
    qInfo() << "[ClockModule] Timezone changed to:" << tz.id();
}

void ClockModule::saveState(QSettings& s) {
    s.beginGroup("ClockModule");
    s.setValue("timezone", QString::fromLatin1(m_timezone.id()));
    s.endGroup();
}

void ClockModule::loadState(QSettings& s) {
    s.beginGroup("ClockModule");
    const QString tzId = s.value("timezone").toString();
    s.endGroup();
    if (!tzId.isEmpty()) {
        QTimeZone tz(tzId.toLatin1());
        if (tz.isValid()) m_timezone = tz;
    }
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_clockInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.clock",
    "Clock",
    "1.0.0",
    "*",       // all surface types
    "module",
    "Mcaster1",
    "Real-time clock with configurable timezone — for relay, simulcast, and multi-region broadcast"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_clock_plugin_info() { return &s_clockInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_clock_create_module(IModuleHost*) {
    return new M1::ClockModule();
}
MCASTER1_PLUGIN_API void mcaster1_clock_destroy_module(IModule* m) { delete m; }
}

#include "ClockModule.moc"
