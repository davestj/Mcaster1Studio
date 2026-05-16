#include "DeckWidget.h"
#include "DeckPlayer.h"
#include "CrossfaderWidget.h"
#include "PlaylistWidget.h"
#include "PlaylistModule.h"
#include "PTTModule.h"
#include "ThemePalette.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QSettings>
#include <QGridLayout>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QUrlQuery>
#include <QSvgRenderer>
#include <QPixmap>
#include <QPainter>
#include <QLinearGradient>
#include <QFileInfo>
#include <QFrame>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// ThemePalette-based color helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    // Compact shared button QSS helper — derives all colors from the palette
    QString flatBtnQss(const ThemePalette& tp,
                       const QString& checkedBg = "",
                       const QString& checkedTxt = "")
    {
        const QString chk = !checkedBg.isEmpty()
            ? QString("QPushButton:checked { background:%1; color:%2; border-color:%3; }")
                  .arg(checkedBg, checkedTxt.isEmpty() ? "#ffffff" : checkedTxt, tp.border.name())
            : QString();
        return QString(
            "QPushButton {"
            "  background:%1; color:%2; border:1px solid %3;"
            "  border-radius:2px; font-size:12px; font-weight:700;"
            "  padding:1px 4px;"
            "}"
            "QPushButton:hover { background:%4; border-color:%5; }"
            "QPushButton:pressed { background:%6; }"
            "%7"
        ).arg(tp.cardBg.name(), tp.text.name(), tp.border.name(),
              tp.cardBg.darker(105).name(), tp.accent.name(),
              tp.cardBg.darker(115).name(), chk);
    }

    // Context menu QSS from palette
    QString menuQss(const ThemePalette& tp)
    {
        return QString(
            "QMenu { background:%1; border:1px solid %2;"
            "  font-size:14px; color:%3; }"
            "QMenu::item { padding:5px 18px; }"
            "QMenu::item:selected { background:%4; color:#ffffff; }"
            "QMenu::item:disabled { color:%5; }"
            "QMenu::separator { height:1px; background:%2; margin:3px 6px; }"
        ).arg(tp.cardBg.name(), tp.border.name(), tp.text.name(),
              tp.accent.name(), tp.textDisabled.name());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DeckInlineMeter  — hi-res smooth-fill VU (dark background, any panel)
// ─────────────────────────────────────────────────────────────────────────────
DeckInlineMeter::DeckInlineMeter(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(36);
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setObjectName("DeckInlineMeter");
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_timer = new QTimer(this);
    m_timer->setInterval(33);  // ~30 fps for smoother animation
    connect(m_timer, &QTimer::timeout, this, &DeckInlineMeter::onTimer);
    m_timer->start();
}

void DeckInlineMeter::setLevels(float l, float r) {
    m_rawL.store(l, std::memory_order_relaxed);
    m_rawR.store(r, std::memory_order_relaxed);
}

void DeckInlineMeter::onTimer() {
    const float rawL = m_rawL.load(std::memory_order_relaxed);
    const float rawR = m_rawR.load(std::memory_order_relaxed);

    // Fast attack, slow release — broadcast ballistics
    auto smooth = [](float raw, float disp) -> float {
        return raw > disp ? disp + (raw - disp) * 0.6f   // ~12ms attack @30fps
                          : disp + (raw - disp) * 0.035f; // ~3s release @30fps
    };
    m_dispL = smooth(rawL, m_dispL);
    m_dispR = smooth(rawR, m_dispR);

    // Peak hold: 90 frames @30fps = 3s, then multiplicative decay
    auto updatePeak = [](float raw, float& peak, int& hold) {
        if (raw >= peak) { peak = raw; hold = 90; }
        else if (hold > 0) { --hold; }
        else { peak *= 0.97f; }
    };
    updatePeak(m_dispL, m_peakL, m_holdL);
    updatePeak(m_dispR, m_peakR, m_holdR);
    update();
}

void DeckInlineMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int W = width(), H = height();
    const float dbRange = kDbClip - kDbMin;  // 60 dB

    // ── Background ──────────────────────────────────────────────────────────
    p.fillRect(rect(), QColor(0x08, 0x0a, 0x10));

    // Subtle inset border
    p.setPen(QPen(QColor(0x1c, 0x22, 0x30), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // ── Layout ──────────────────────────────────────────────────────────────
    const int scaleW = 16;        // dB scale column width
    const int labelH = 12;        // bottom L/R/dB labels
    const int padTop = 2;
    const int padSide = 2;
    const int gap     = 2;        // gap between L and R bars

    const int barH = H - labelH - padTop - 1;
    const int barW = std::max(4, (W - scaleW - padSide * 2 - gap) / 2);
    const int xL   = padSide;
    const int xR   = xL + barW + gap;

    // Precompute zone boundaries (normalised 0..1)
    const float greenMax = (-12.0f - kDbMin) / dbRange;  // 0.80
    const float amberMax = ( -3.0f - kDbMin) / dbRange;  // 0.95

    // ── Draw one smooth-fill bar ────────────────────────────────────────────
    auto drawBar = [&](int bx, int bw, float level, float peak) {
        const float db     = std::clamp(linToDb(level), kDbMin, kDbClip);
        const float peakDb = std::clamp(linToDb(peak),  kDbMin, kDbClip);
        const float fillNorm = (db     - kDbMin) / dbRange;
        const float peakNorm = (peakDb - kDbMin) / dbRange;
        const int   fillH    = static_cast<int>(fillNorm * barH);
        const int   barBot   = padTop + barH;

        // Unlit track (dark groove)
        {
            QLinearGradient bg(bx, padTop, bx + bw, padTop);
            bg.setColorAt(0.0, QColor(0x14, 0x16, 0x1c));
            bg.setColorAt(0.5, QColor(0x1a, 0x1e, 0x26));
            bg.setColorAt(1.0, QColor(0x14, 0x16, 0x1c));
            p.fillRect(bx, padTop, bw, barH, bg);
        }

        if (fillH > 0) {
            // Green zone: bottom → greenMax
            const int greenH = static_cast<int>(std::min(fillNorm, greenMax) * barH);
            if (greenH > 0) {
                QLinearGradient g(0, barBot, 0, barBot - greenH);
                g.setColorAt(0.0, QColor(0x00, 0x88, 0x40));
                g.setColorAt(0.4, QColor(0x00, 0xbb, 0x55));
                g.setColorAt(1.0, QColor(0x00, 0xdd, 0x66));
                p.fillRect(bx, barBot - greenH, bw, greenH, g);
            }

            // Amber zone: greenMax → amberMax
            if (fillNorm > greenMax) {
                const int amberBot = barBot - static_cast<int>(greenMax * barH);
                const int amberTop = barBot - static_cast<int>(std::min(fillNorm, amberMax) * barH);
                if (amberBot > amberTop) {
                    QLinearGradient g(0, amberBot, 0, amberTop);
                    g.setColorAt(0.0, QColor(0xcc, 0x88, 0x00));
                    g.setColorAt(0.5, QColor(0xff, 0xaa, 0x00));
                    g.setColorAt(1.0, QColor(0xff, 0xcc, 0x22));
                    p.fillRect(bx, amberTop, bw, amberBot - amberTop, g);
                }
            }

            // Red zone: amberMax → fillNorm
            if (fillNorm > amberMax) {
                const int redBot = barBot - static_cast<int>(amberMax * barH);
                const int redTop = barBot - fillH;
                if (redBot > redTop) {
                    QLinearGradient g(0, redBot, 0, redTop);
                    g.setColorAt(0.0, QColor(0xcc, 0x22, 0x22));
                    g.setColorAt(0.5, QColor(0xff, 0x33, 0x33));
                    g.setColorAt(1.0, QColor(0xff, 0x66, 0x44));
                    p.fillRect(bx, redTop, bw, redBot - redTop, g);
                }
            }

            // Subtle highlight edge (left 1px brighter)
            {
                QLinearGradient shine(0, barBot, 0, barBot - fillH);
                shine.setColorAt(0.0, QColor(255, 255, 255, 10));
                shine.setColorAt(1.0, QColor(255, 255, 255, 30));
                p.fillRect(bx, barBot - fillH, 1, fillH, shine);
            }
        }

        // Peak hold tick
        if (peakDb > kDbMin + 2.0f) {
            const int peakY = barBot - static_cast<int>(peakNorm * barH);
            QColor pkCol;
            if      (peakDb >= -3.0f)  pkCol = QColor(0xff, 0x77, 0x66);
            else if (peakDb >= -12.0f) pkCol = QColor(0xff, 0xdd, 0x44);
            else                       pkCol = QColor(0x44, 0xff, 0x88);
            p.setPen(QPen(pkCol, 1.5));
            p.drawLine(bx, peakY, bx + bw - 1, peakY);
        }

        // Clip indicator (top 2px red glow)
        if (db >= -0.5f) {
            p.fillRect(bx, padTop, bw, 3, QColor(0xff, 0x22, 0x22));
        }
    };

    drawBar(xL, barW, m_dispL, m_peakL);
    drawBar(xR, barW, m_dispR, m_peakR);

    // ── dB scale (right column) ─────────────────────────────────────────────
    const int scaleX = xR + barW + 2;
    static constexpr struct { float db; const char* label; } kScale[] = {
        { 0.0f, " 0"}, {-3.0f, "-3"}, {-6.0f, "-6"},
        {-12.0f, "12"}, {-24.0f, "24"}, {-48.0f, "48"}
    };
    p.setFont(QFont(QStringLiteral("Consolas"), 6));
    const int barBot = padTop + barH;
    for (const auto& s : kScale) {
        const float norm = (s.db - kDbMin) / dbRange;
        const int y = barBot - static_cast<int>(norm * barH);
        // Tick mark
        p.setPen(QPen(QColor(0x33, 0x44, 0x55), 1));
        p.drawLine(scaleX, y, scaleX + 2, y);
        // Label
        p.setPen(QColor(0x66, 0x88, 0xaa));
        p.drawText(scaleX + 3, y - 4, scaleW - 4, 9, Qt::AlignLeft | Qt::AlignVCenter,
                   QLatin1String(s.label));
    }

    // ── Channel labels ──────────────────────────────────────────────────────
    p.setFont(QFont(QStringLiteral("Consolas"), 6, QFont::Bold));
    p.setPen(QColor(0x55, 0x88, 0xbb));
    p.drawText(xL, barBot + 1, barW, labelH, Qt::AlignCenter, QStringLiteral("L"));
    p.drawText(xR, barBot + 1, barW, labelH, Qt::AlignCenter, QStringLiteral("R"));
}

// ─────────────────────────────────────────────────────────────────────────────
// DeckPanel helpers
// ─────────────────────────────────────────────────────────────────────────────
QIcon DeckPanel::svgIcon(const QString& path, int sz) {
    QSvgRenderer r(path);
    if (!r.isValid()) return QIcon();
    QPixmap pm(sz * 2, sz * 2);
    pm.fill(Qt::transparent);
    { QPainter p(&pm); r.render(&p); }
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

// Light flat button — minimum 12px font, larger hit target
static QPushButton* makeFlatBtn(const QString& text, QWidget* parent,
                                 bool checkable = false, int minW = 32, int minH = 26)
{
    const auto tp = ThemePalette::forCurrentTheme();
    auto* b = new QPushButton(text, parent);
    b->setCheckable(checkable);
    b->setMinimumSize(minW, minH);
    b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    b->setStyleSheet(QString(
        "QPushButton {"
        "  background:%1; color:%2;"
        "  border:1px solid %3; border-radius:3px;"
        "  font-size:14px; font-weight:700; padding:2px 5px;"
        "}"
        "QPushButton:hover { background:%4; border-color:%5; }"
        "QPushButton:pressed { background:%6; }"
        "QPushButton:checked { background:%5; color:#ffffff; border-color:%5; }"
        "QPushButton:disabled { background:%7; color:%8; }"
    ).arg(tp.cardBg.name(), tp.text.name(), tp.border.name(),
          tp.cardBg.darker(105).name(), tp.accent.name(),
          tp.cardBg.darker(110).name(), tp.bg.name(), tp.textDisabled.name()));
    return b;
}

// Transport button — raised, icon-sized, SVG-ready
// Pass an empty text when using setIcon() afterwards.
static QPushButton* makeTransBtn(const QString& text, const QColor& tint, QWidget* parent) {
    const auto tp = ThemePalette::forCurrentTheme();
    auto* b = new QPushButton(text, parent);
    b->setCheckable(false);
    b->setMinimumSize(40, 34);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const QString tc = tint.name();
    b->setStyleSheet(QString(
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %4, stop:1 %5);"
        "  color:%1; border:1px solid %6; border-radius:4px;"
        "  font-size:14px; font-weight:900; padding:2px;"
        "}"
        "QPushButton:hover { background:%7; border-color:%1; }"
        "QPushButton:pressed { background:%8; }"
        "QPushButton:checked { background:%2; color:#ffffff; border-color:%3; }"
        "QPushButton:disabled { color:%9; }"
    ).arg(tc, tint.darker(120).name(), tint.name(),
          tp.cardBg.lighter(105).name(), tp.cardBg.name(),
          tp.border.name(), tp.cardBg.darker(103).name(),
          tp.cardBg.darker(108).name(), tp.textDisabled.name()));
    return b;
}

// Hot cue pad — minimum 12px font, larger hit target
static QPushButton* makeHotBtn(const QString& label, const QColor& col, QWidget* parent) {
    const auto tp = ThemePalette::forCurrentTheme();
    auto* b = new QPushButton(label, parent);
    b->setMinimumSize(36, 28);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    const QString hex = col.name();
    b->setStyleSheet(QString(
        "QPushButton {"
        "  background:%2; color:%1;"
        "  border:1px solid %1; border-radius:3px;"
        "  font-size:14px; font-weight:900; padding:2px 4px;"
        "}"
        "QPushButton:hover { background:%1; color:#fff; }"
        "QPushButton:pressed { background:%3; }"
    ).arg(hex, tp.cardBg.lighter(102).name(), tp.cardBg.darker(105).name()));
    return b;
}

// ─────────────────────────────────────────────────────────────────────────────
// DeckPanel
// ─────────────────────────────────────────────────────────────────────────────
DeckPanel::DeckPanel(M1::DeckPlayer* player, int deckIndex, QWidget* parent)
    : QWidget(parent)
    , m_player(player)
    , m_deckIndex(deckIndex)
{
    setObjectName(deckIndex == 0 ? "DeckPanelA" : "DeckPanelB");
    setAcceptDrops(true);
    setMinimumSize(300, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &DeckPanel::onArtworkSearchReply);

    setupUi();

    connect(m_player, &M1::DeckPlayer::stateChanged,   this, &DeckPanel::onStateChanged);
    connect(m_player, &M1::DeckPlayer::positionChanged, this, &DeckPanel::onPositionChanged);
    connect(m_player, &M1::DeckPlayer::bpmDetected,     this, &DeckPanel::onBpmDetected);
    connect(m_player, &M1::DeckPlayer::hotCuesChanged,  this, &DeckPanel::onHotCuesChanged);
    connect(m_player, &M1::DeckPlayer::tagsLoaded,      this, &DeckPanel::onTagsLoaded);
    connect(m_player, &M1::DeckPlayer::loadingFinished, this, [this]() {
        updateTimeDisplay();
        updateSeekSlider();
    });
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout, this, &DeckPanel::onPollTimer);
    m_pollTimer->start();

    // Repaint on theme change (paintEvent uses ThemePalette::forCurrentTheme())
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this]() { update(); });
}

void DeckPanel::setupUi() {
    const auto    tp        = ThemePalette::forCurrentTheme();
    const bool    isA       = (m_deckIndex == 0);
    const QColor  badge     = isA ? tp.deckA : tp.deckB;
    const QString deckLtr   = isA ? "A" : "B";

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ───────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("DeckHeader");
    header->setFixedHeight(34);
    header->setStyleSheet(QString(
        "QWidget#DeckHeader {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border-bottom: 1px solid %3;"
        "}"
    ).arg(tp.cardBg.lighter(103).name(), tp.cardBg.darker(102).name(),
          tp.border.name()));

    auto* hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(6, 0, 6, 0);
    hdrLay->setSpacing(5);

    auto* deckBadge = new QLabel(QString::fromUtf8("● DECK ") + deckLtr, header);
    deckBadge->setObjectName("DeckBadge");
    deckBadge->setStyleSheet(QString(
        "QLabel#DeckBadge { color:%1; font-size:14px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace; }")
        .arg(tp.text.name()));
    deckBadge->setToolTip(QString("Deck %1 — audio player").arg(deckLtr));

    m_artistLabel = new QLabel("", header);
    m_artistLabel->setObjectName("DeckArtistLabel");
    m_artistLabel->setStyleSheet(QString(
        "QLabel#DeckArtistLabel { color:%1; font-size:16px; font-weight:700; }")
        .arg(tp.textMuted.darker(110).name()));
    m_artistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_artistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_titleLabel = new QLabel(QString::fromUtf8("— no track —"), header);
    m_titleLabel->setObjectName("DeckTitleLabel");
    m_titleLabel->setStyleSheet(QString(
        "QLabel#DeckTitleLabel { color:%1; font-size:16px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }")
        .arg(tp.text.name()));
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setTextFormat(Qt::PlainText);

    m_stateLabel = new QLabel("STOP", header);
    m_stateLabel->setObjectName("DeckStateLabel");
    m_stateLabel->setStyleSheet(QString(
        "QLabel#DeckStateLabel { color:%1; font-size:14px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace; padding:0 4px; }")
        .arg(tp.accent.name()));
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setToolTip("Current deck state");

    hdrLay->addWidget(deckBadge);
    hdrLay->addWidget(m_artistLabel, 2);
    hdrLay->addWidget(m_titleLabel, 3);
    hdrLay->addWidget(m_stateLabel);
    root->addWidget(header);

    // ── Middle row: art | stats | vol-fader | VU ─────────────────
    auto* midRow = new QHBoxLayout;
    midRow->setContentsMargins(5, 3, 3, 2);
    midRow->setSpacing(5);

    // Album art
    m_artLabel = new QLabel(this);
    m_artLabel->setObjectName("DeckArtwork");
    m_artLabel->setFixedSize(64, 64);
    m_artLabel->setAlignment(Qt::AlignCenter);
    m_artLabel->setToolTip("Album artwork — auto-fetched when track loads");
    m_artLabel->setStyleSheet(QString(
        "QLabel { background:%1; border:1px solid %2; border-radius:3px; }"
    ).arg(tp.bg.name(), tp.border.name()));
    {
        // Placeholder: sandy bg + vinyl record SVG + deck letter overlay
        QPixmap art(128, 128);
        art.fill(tp.bg);
        {
            QPainter pa(&art);
            pa.setRenderHint(QPainter::Antialiasing);
            QSvgRenderer vinyl(QString(":/resources/icons/vinyl-record.svg"));
            vinyl.render(&pa, QRectF(4, 4, 120, 120));
            pa.setFont(QFont("Consolas", 36, QFont::Black));
            pa.setPen(QColor(badge.red(), badge.green(), badge.blue(), 200));
            pa.drawText(QRect(0, 0, 128, 128), Qt::AlignCenter, deckLtr);
        }
        art.setDevicePixelRatio(2.0);
        m_artLabel->setPixmap(art);
    }

    // Stats grid
    auto* statsGrid = new QGridLayout;
    statsGrid->setContentsMargins(0, 0, 0, 0);
    statsGrid->setHorizontalSpacing(3);
    statsGrid->setVerticalSpacing(1);

    const QString captionQss = QString(
        "QLabel { color:%1; font-size:14px;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.textMuted.name());
    const QString valueQss = QString(
        "QLabel { color:%1; font-size:14px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.text.name());
    const QString bpmValQss = QString(
        "QLabel { color:%1; font-size:14px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.accent.name());

    auto mkCap = [&](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(captionQss);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };
    auto mkVal = [&](const QString& t, const QString& qss = {}) {
        auto* l = new QLabel(t, this);
        l->setStyleSheet(qss.isEmpty() ? valueQss : qss);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        l->setMinimumWidth(38);
        return l;
    };

    // Row 0: Cur
    statsGrid->addWidget(mkCap("Cur"),  0, 0);
    m_curLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_curLabel,    0, 1);

    // Row 1: Tot
    statsGrid->addWidget(mkCap("Tot"),  1, 0);
    m_totLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_totLabel,    1, 1);

    // Row 2: Rem
    statsGrid->addWidget(mkCap("Rem"),  2, 0);
    m_remLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_remLabel,    2, 1);

    // Row 3: BPM + nudge buttons
    statsGrid->addWidget(mkCap("BPM"),  3, 0);
    auto* bpmRow = new QHBoxLayout;
    bpmRow->setSpacing(2);
    m_bpmLabel = mkVal("---", bpmValQss);
    m_bpmMinusBtn = new QPushButton("−", this);
    m_bpmPlusBtn  = new QPushButton("+", this);
    for (auto* b : {m_bpmMinusBtn, m_bpmPlusBtn}) {
        b->setFixedSize(22, 22);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2;"
            "  border:1px solid %3; border-radius:3px;"
            "  font-size:14px; font-weight:900; }"
            "QPushButton:hover { border-color:%2; }"
            "QPushButton:pressed { background:%4; }"
        ).arg(tp.cardBg.name(), tp.accent.name(), tp.border.name(),
              tp.cardBg.darker(110).name()));
    }
    m_bpmMinusBtn->setToolTip("Decrease playback speed by 2% (pitch/tempo down)");
    m_bpmPlusBtn->setToolTip("Increase playback speed by 2% (pitch/tempo up)");
    connect(m_bpmMinusBtn, &QPushButton::clicked, this, [this]() {
        const float s = std::clamp(m_player->speed() - 0.02f, 0.5f, 1.5f);
        m_player->setSpeed(s);
        updateBpmDisplay();
    });
    connect(m_bpmPlusBtn, &QPushButton::clicked, this, [this]() {
        const float s = std::clamp(m_player->speed() + 0.02f, 0.5f, 1.5f);
        m_player->setSpeed(s);
        updateBpmDisplay();
    });
    bpmRow->addWidget(m_bpmLabel);
    bpmRow->addWidget(m_bpmMinusBtn);
    bpmRow->addWidget(m_bpmPlusBtn);
    statsGrid->addLayout(bpmRow, 3, 1, 1, 3);

    // Vertical divider
    auto* vdiv = new QFrame(this);
    vdiv->setFrameShape(QFrame::VLine);
    vdiv->setStyleSheet(QString("QFrame { color:%1; }").arg(tp.border.name()));
    statsGrid->addWidget(vdiv, 0, 2, 3, 1);

    // Right sub-columns: kbps / kHz / Stereo
    m_bitrateLabel = mkVal("---");
    m_kHzLabel     = mkVal("--.-");
    m_stereoLabel  = mkVal("---");

    statsGrid->addWidget(m_bitrateLabel,         0, 3);
    statsGrid->addWidget(mkCap("kbps"),           0, 4);
    statsGrid->addWidget(m_kHzLabel,              1, 3);
    statsGrid->addWidget(mkCap("kHz"),            1, 4);
    statsGrid->addWidget(m_stereoLabel,           2, 3, 1, 2);
    statsGrid->setColumnStretch(5, 1);

    auto* statsVbox = new QVBoxLayout;
    statsVbox->setSpacing(0);
    statsVbox->addLayout(statsGrid);
    statsVbox->addStretch(1);

    midRow->addWidget(m_artLabel, 0, Qt::AlignTop);
    midRow->addLayout(statsVbox, 1);

    // ── Volume fader column ───────────────────────────────────────
    auto* volCol = new QVBoxLayout;
    volCol->setSpacing(2);
    volCol->setContentsMargins(2, 0, 2, 0);

    auto* volCap = new QLabel("VOL", this);
    volCap->setStyleSheet(QString("QLabel { color:%1; font-size:14px; font-weight:700;"
                           "  font-family:'Consolas','Courier New',monospace; }")
                           .arg(tp.textMuted.name()));
    volCap->setAlignment(Qt::AlignCenter);
    volCap->setToolTip("Volume / Pitch fader — switch mode with V/P buttons");

    m_volModeBtn   = makeFlatBtn("V", this, true, 22, 18);
    m_pitchModeBtn = makeFlatBtn("P", this, true, 22, 18);
    m_volModeBtn->setToolTip("Volume mode — fader controls deck output level");
    m_pitchModeBtn->setToolTip("Pitch mode — fader controls playback speed (±50%)");
    m_volModeBtn->setChecked(true);
    connect(m_volModeBtn,   &QPushButton::clicked, this, [this]() { onSliderMode(0); });
    connect(m_pitchModeBtn, &QPushButton::clicked, this, [this]() { onSliderMode(1); });
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(2);
    modeRow->addWidget(m_volModeBtn);
    modeRow->addWidget(m_pitchModeBtn);

    m_fader = new QSlider(Qt::Vertical, this);
    m_fader->setRange(0, 100);
    m_fader->setValue(100);
    m_fader->setMinimumHeight(65);
    m_fader->setToolTip("Volume / Pitch fader — use V/P buttons to switch mode");
    m_fader->setStyleSheet(QString(
        "QSlider::groove:vertical {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 %1, stop:0.5 %2, stop:1 %1);"
        "  width:7px; border-radius:3px; border:1px solid %3;"
        "}"
        "QSlider::handle:vertical {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %4, stop:0.5 %2, stop:1 %1);"
        "  border:1px solid %5; width:20px; height:12px;"
        "  margin:0 -7px; border-radius:3px;"
        "}"
        "QSlider::sub-page:vertical {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 %6, stop:1 %7);"
        "  border-radius:3px;"
        "}"
    ).arg(tp.border.name(), tp.cardBg.name(), tp.border.darker(110).name(),
          tp.cardBg.lighter(105).name(), tp.border.darker(120).name(),
          tp.accent.lighter(140).name(), tp.accent.lighter(120).name()));
    connect(m_fader, &QSlider::valueChanged, this, &DeckPanel::onVolumeChanged);

    m_faderLabel = new QLabel("100%", this);
    m_faderLabel->setStyleSheet(QString("QLabel { color:%1; font-size:14px; font-weight:700;"
                                  "  font-family:'Consolas','Courier New',monospace; }")
                                  .arg(tp.accent.name()));
    m_faderLabel->setAlignment(Qt::AlignCenter);
    m_faderLabel->setToolTip("Current volume / pitch level");

    m_muteBtn   = makeFlatBtn("M",   this, true, 26, 24);
    m_airBtn    = makeFlatBtn("AIR", this, true, 34, 24);
    m_cueOutBtn = makeFlatBtn("CUE", this, true, 34, 24);
    m_muteBtn->setToolTip("Mute — silences this deck's output");
    m_airBtn->setToolTip("AIR — route this deck to the on-air (broadcast) output");
    m_cueOutBtn->setToolTip("CUE — route this deck to the headphone monitor / cue output");
    m_airBtn->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2;"
        "  border:1px solid %3; border-radius:3px; font-size:14px; font-weight:700; padding:2px 4px; }"
        "QPushButton:checked { background:%2; color:#fff; border-color:%2; }"
        "QPushButton:hover { border-color:%2; }"
    ).arg(tp.cardBg.name(), tp.success.name(), tp.border.name()));
    m_cueOutBtn->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2;"
        "  border:1px solid %3; border-radius:3px; font-size:14px; font-weight:700; padding:2px 4px; }"
        "QPushButton:checked { background:%2; color:#fff; border-color:%2; }"
        "QPushButton:hover { border-color:%2; }"
    ).arg(tp.cardBg.name(), tp.accent.name(), tp.border.name()));

    m_airBtn->setChecked(true);   // AIR on by default
    connect(m_airBtn, &QPushButton::toggled, this, [this](bool on) {
        m_player->setAirOn(on);
    });
    connect(m_cueOutBtn, &QPushButton::toggled, this, [this](bool on) {
        m_player->setCueOn(on);
    });

    auto* airCueRow = new QHBoxLayout;
    airCueRow->setSpacing(2);
    airCueRow->addWidget(m_airBtn);
    airCueRow->addWidget(m_cueOutBtn);

    volCol->addWidget(volCap, 0, Qt::AlignHCenter);
    volCol->addLayout(modeRow);
    volCol->addWidget(m_fader, 1, Qt::AlignHCenter);
    volCol->addWidget(m_faderLabel, 0, Qt::AlignHCenter);
    volCol->addWidget(m_muteBtn, 0, Qt::AlignHCenter);
    volCol->addLayout(airCueRow);

    midRow->addLayout(volCol);

    // ── VU Meter ──────────────────────────────────────────────────
    m_vuMeter = new DeckInlineMeter(this);
    midRow->addWidget(m_vuMeter);

    root->addLayout(midRow, 0);

    // ── Seek slider ───────────────────────────────────────────────
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setObjectName("DeckSeekSlider");
    m_seekSlider->setRange(0, 10000);
    m_seekSlider->setValue(0);
    m_seekSlider->setFixedHeight(14);
    m_seekSlider->setToolTip("Seek — drag to scrub forward or backward in the track");
    m_seekSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal {"
        "  background:%1; height:5px; border-radius:2px;"
        "  border:1px solid %2;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %3, stop:1 %4);"
        "  border:1px solid %5; width:10px; height:14px;"
        "  margin:-5px 0; border-radius:2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background:%6; border-radius:2px;"
        "}"
    ).arg(tp.border.name(), tp.border.darker(110).name(),
          tp.cardBg.lighter(105).name(), tp.cardBg.darker(103).name(),
          tp.border.darker(120).name(), tp.accent.name()));
    connect(m_seekSlider, &QSlider::sliderMoved, this, &DeckPanel::onSeekMoved);

    auto* seekWidget = new QWidget(this);
    seekWidget->setStyleSheet(QString("QWidget { background:%1; }")
        .arg(tp.cardBg.darker(103).name()));
    auto* seekLay = new QHBoxLayout(seekWidget);
    seekLay->setContentsMargins(5, 2, 5, 2);
    seekLay->addWidget(m_seekSlider);
    root->addWidget(seekWidget);

    // ── Transport row ────────────────────────────────────────────
    auto* transWidget = new QWidget(this);
    transWidget->setObjectName("DeckTransport");
    transWidget->setStyleSheet(QString(
        "QWidget#DeckTransport {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border-top:1px solid %3; border-bottom:1px solid %3;"
        "}"
    ).arg(tp.cardBg.lighter(103).name(), tp.cardBg.darker(102).name(),
          tp.border.name()));
    auto* transLay = new QHBoxLayout(transWidget);
    transLay->setContentsMargins(4, 3, 4, 3);
    transLay->setSpacing(3);

    m_playBtn = makeTransBtn("", tp.success, this);
    m_playBtn->setCheckable(true);
    m_playBtn->setToolTip("Play / Pause — starts or pauses deck playback (Space)");
    m_playBtn->setIcon(svgIcon(":/resources/icons/play.svg", 22));
    m_playBtn->setIconSize(QSize(26, 26));
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        m_player->togglePlayPause();
    });
    m_playBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playBtn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint&) { onPlayBtnEmptyMenu(); });

    m_stopBtn = makeTransBtn("", tp.error, this);
    m_stopBtn->setToolTip("Stop — stops playback and returns to cue point");
    m_stopBtn->setIcon(svgIcon(":/resources/icons/stop.svg", 22));
    m_stopBtn->setIconSize(QSize(26, 26));
    connect(m_stopBtn, &QPushButton::clicked, m_player, &M1::DeckPlayer::stop);

    m_nextBtn = makeTransBtn("", tp.accent, this);
    m_nextBtn->setToolTip("Eject — unloads current track from this deck");
    m_nextBtn->setIcon(svgIcon(":/resources/icons/eject.svg", 22));
    m_nextBtn->setIconSize(QSize(26, 26));
    connect(m_nextBtn, &QPushButton::clicked, m_player, &M1::DeckPlayer::unload);

    transLay->addWidget(m_playBtn);
    transLay->addWidget(m_stopBtn);
    transLay->addWidget(m_nextBtn);

    auto* div1 = new QFrame(this);
    div1->setFrameShape(QFrame::VLine);
    div1->setStyleSheet(QString("QFrame { color:%1; }").arg(tp.border.name()));
    transLay->addWidget(div1);

    m_cueBtn = makeFlatBtn("", this);
    m_cueBtn->setIcon(svgIcon(":/resources/icons/cue.svg", 18));
    m_cueBtn->setIconSize(QSize(22, 22));
    m_cueBtn->setMinimumSize(36, 30);
    m_cueBtn->setToolTip("Cue Point — set cue while playing, or jump to cue while stopped");
    connect(m_cueBtn, &QPushButton::clicked, this, [this]() {
        if (m_player->state() == M1::DeckPlayer::State::Playing)
            m_player->setCuePoint();
        else
            m_player->jumpToCue();
    });

    auto* cueJmpBtn = makeFlatBtn("▲CUE", this);
    cueJmpBtn->setMinimumSize(46, 30);
    cueJmpBtn->setToolTip("Jump to Cue Point immediately");
    connect(cueJmpBtn, &QPushButton::clicked, m_player, &M1::DeckPlayer::jumpToCue);

    m_loopBtn = makeFlatBtn("", this, true);
    m_loopBtn->setIcon(svgIcon(":/resources/icons/loop.svg", 18));
    m_loopBtn->setIconSize(QSize(22, 22));
    m_loopBtn->setMinimumSize(36, 30);
    m_loopBtn->setToolTip("Loop — toggles looping between cue point and loop-out");
    connect(m_loopBtn, &QPushButton::toggled, this, [this](bool on) {
        m_player->setLoop(on);
    });

    m_eqBtn = makeFlatBtn("EQ", this, true);  // checkable
    m_eqBtn->setMinimumSize(36, 30);
    m_eqBtn->setToolTip("Toggle 3-band EQ (Low 100Hz / Mid 1kHz / High 10kHz, ±12 dB)");

    transLay->addWidget(m_cueBtn);
    transLay->addWidget(cueJmpBtn);
    transLay->addWidget(m_loopBtn);
    transLay->addWidget(m_eqBtn);
    transLay->addStretch(1);

    // Hot cue pads — use semantic palette colors
    const QColor kHotColors[4] = {
        tp.error,      // red
        tp.success,    // green
        tp.accent,     // blue / gold
        tp.warning,    // amber
    };
    for (int i = 0; i < 4; ++i) {
        m_hotBtns[i] = makeHotBtn(QString("H%1").arg(i + 1), kHotColors[i], this);
        m_hotBtns[i]->setToolTip(
            QString("Hot Cue %1 — click to set marker, click again to jump to it").arg(i + 1));
        connect(m_hotBtns[i], &QPushButton::clicked, this, [this, i]() {
            if (m_player->hotCue(i) < 0) m_player->setHotCue(i);
            else                         m_player->jumpToHotCue(i);
        });
        transLay->addWidget(m_hotBtns[i]);
    }

    root->addWidget(transWidget);

    // ── EQ panel (hidden by default, toggled by EQ button) ────────
    m_eqPanel = new QWidget(this);
    m_eqPanel->setObjectName("DeckEqPanel");
    m_eqPanel->setStyleSheet(QString(
        "QWidget#DeckEqPanel {"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border-top:1px solid %3; border-bottom:1px solid %3;"
        "}"
        "QLabel { color:%4; font-size:14px; font-weight:700; }"
        "QSlider::groove:vertical {"
        "  background:%5; width:5px; border-radius:2px; border:none;"
        "}"
        "QSlider::handle:vertical {"
        "  background:%6; border:1px solid %7;"
        "  width:12px; height:8px; margin:-1px -4px; border-radius:2px;"
        "}"
        "QSlider::sub-page:vertical { background:%7; border-radius:2px; }"
        "QSlider::add-page:vertical { background:%8; border-radius:2px; }"
    ).arg(tp.cardBg.darker(102).name(), tp.cardBg.darker(106).name(),
          tp.border.name(), tp.text.darker(110).name(),
          tp.border.darker(105).name(), tp.cardBg.lighter(105).name(),
          tp.border.darker(115).name(), tp.accent.name()));
    m_eqPanel->setVisible(false);
    m_eqPanel->setFixedHeight(72);

    {
        auto* eqLay = new QHBoxLayout(m_eqPanel);
        eqLay->setContentsMargins(6, 4, 6, 4);
        eqLay->setSpacing(10);

        static const char* kBandLabels[3] = { "LOW",  "MID",  "HI"    };
        static const char* kBandFreqs [3] = { "100Hz","1kHz","10kHz"  };

        for (int b = 0; b < 3; ++b) {
            auto* col = new QVBoxLayout;
            col->setSpacing(1);

            auto* hdr = new QLabel(QString(kBandLabels[b]), m_eqPanel);
            hdr->setAlignment(Qt::AlignCenter);
            col->addWidget(hdr, 0, Qt::AlignHCenter);

            m_eqSlider[b] = new QSlider(Qt::Vertical, m_eqPanel);
            m_eqSlider[b]->setRange(-24, 24);      // maps to -12..+12 dB (×0.5)
            m_eqSlider[b]->setValue(0);
            m_eqSlider[b]->setTickPosition(QSlider::TicksBothSides);
            m_eqSlider[b]->setTickInterval(12);    // ticks at -6, 0, +6 dB
            m_eqSlider[b]->setFixedWidth(16);
            m_eqSlider[b]->setToolTip(
                QString("%1 (%2) EQ — ±12 dB").arg(kBandLabels[b], kBandFreqs[b]));
            col->addWidget(m_eqSlider[b], 1, Qt::AlignHCenter);

            m_eqValue[b] = new QLabel("0.0", m_eqPanel);
            m_eqValue[b]->setAlignment(Qt::AlignCenter);
            col->addWidget(m_eqValue[b], 0, Qt::AlignHCenter);

            eqLay->addLayout(col);

            connect(m_eqSlider[b], &QSlider::valueChanged, this, [this, b](int v) {
                const float dB = v * 0.5f;
                m_player->setEqGain(b, dB);
                m_eqValue[b]->setText(dB >= 0
                    ? QString("+%1").arg(dB, 0, 'f', 1)
                    : QString("%1").arg(dB, 0, 'f', 1));
            });
        }

        // RESET button
        auto* resetBtn = new QPushButton("RST", m_eqPanel);
        resetBtn->setFixedSize(36, 26);
        resetBtn->setToolTip("Reset all EQ bands to 0 dB");
        resetBtn->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2; border:1px solid %3;"
            "  border-radius:3px; font-size:14px; font-weight:700; }"
            "QPushButton:hover { background:%4; }"
            "QPushButton:pressed { background:%5; }"
        ).arg(tp.cardBg.name(), tp.text.name(), tp.border.darker(115).name(),
              tp.cardBg.darker(105).name(), tp.border.name()));
        connect(resetBtn, &QPushButton::clicked, this, [this]() {
            for (int b = 0; b < 3; ++b)
                m_eqSlider[b]->setValue(0);
        });
        eqLay->addWidget(resetBtn, 0, Qt::AlignBottom);
    }

    // Wire EQ button to toggle the panel
    connect(m_eqBtn, &QPushButton::toggled, this, [this](bool on) {
        m_eqPanel->setVisible(on);
    });

    root->addWidget(m_eqPanel);

    // ── Browser tabs (History | Library | Playlist | Queue) ────────
    setupBrowserTabs();
    root->addWidget(m_browserTabs, 1);
}

// ─── Browser tabs setup ──────────────────────────────────────────────────────
void DeckPanel::setupBrowserTabs() {
    const auto tp = ThemePalette::forCurrentTheme();
    const QString listQss = QString(
        "QListWidget, QTableWidget {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border: none; color: %3; font-size:14px;"
        "}"
        "QListWidget::item, QTableWidget::item {"
        "  padding: 2px 6px; border-bottom: 1px solid %4;"
        "}"
        "QListWidget::item:selected, QTableWidget::item:selected {"
        "  background: %5; color: #ffffff;"
        "}"
        "QListWidget::item:hover, QTableWidget::item:hover {"
        "  background: %4;"
        "}"
        "QHeaderView::section {"
        "  background: %6; color: %3; font-size:12px;"
        "  font-weight:700; border: none; border-bottom: 1px solid %7;"
        "  padding: 3px 6px;"
        "}"
        "QScrollBar:vertical { background:%8; width:6px; border:none; }"
        "QScrollBar::handle:vertical { background:%9; border-radius:3px; min-height:16px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }"
    ).arg(tp.cardBg.lighter(103).name(), tp.cardBg.name(),
          tp.text.darker(110).name(), tp.border.lighter(105).name(),
          tp.accent.name(), tp.cardBg.lighter(102).name(),
          tp.border.name(), tp.cardBg.darker(103).name(),
          tp.border.darker(110).name());

    m_browserTabs = new QTabWidget(this);
    m_browserTabs->setObjectName("DeckBrowserTabs");
    m_browserTabs->setMinimumHeight(60);
    m_browserTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_browserTabs->setStyleSheet(QString(
        "QTabWidget::pane { border: none; border-top: 1px solid %1; }"
        "QTabBar::tab {"
        "  background: %2; color: %3; font-size:12px; font-weight:700;"
        "  border: 1px solid %1; border-bottom: none;"
        "  padding: 4px 10px; margin-right: 1px;"
        "}"
        "QTabBar::tab:selected { background: %4; color: %5; }"
        "QTabBar::tab:hover { background: %6; }"
    ).arg(tp.border.name(), tp.cardBg.lighter(102).name(),
          tp.text.darker(110).name(), tp.cardBg.lighter(106).name(),
          tp.accent.name(), tp.cardBg.lighter(104).name()));

    // ── Tab 0: History ────────────────────────────────────────────────
    m_historyList = new QListWidget;
    m_historyList->setObjectName("DeckHistoryList");
    m_historyList->setSelectionMode(QAbstractItemView::NoSelection);
    m_historyList->setFocusPolicy(Qt::NoFocus);
    m_historyList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyList->setStyleSheet(listQss);
    {
        auto* hint = new QListWidgetItem(
            "  Drop audio here \xC2\xB7 click \xe2\x96\xb6 to load",
            m_historyList);
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(tp.textDisabled);
    }
    m_browserTabs->addTab(m_historyList, "History");

    // ── Tab 1: Library ────────────────────────────────────────────────
    m_libraryTable = new QTableWidget(0, 4);
    m_libraryTable->setObjectName("DeckLibraryTable");
    m_libraryTable->setHorizontalHeaderLabels({"Title", "Artist", "Duration", "Genre"});
    m_libraryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_libraryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryTable->verticalHeader()->setVisible(false);
    m_libraryTable->setAlternatingRowColors(true);
    m_libraryTable->horizontalHeader()->setStretchLastSection(true);
    m_libraryTable->setColumnWidth(0, 180);
    m_libraryTable->setColumnWidth(1, 120);
    m_libraryTable->setColumnWidth(2, 50);
    m_libraryTable->setStyleSheet(listQss);
    m_libraryTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_libraryTable, &QTableWidget::customContextMenuRequested,
            this, &DeckPanel::showLibraryContextMenu);
    connect(m_libraryTable, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) {
                auto* item = m_libraryTable->item(row, 0);
                if (!item) return;
                auto mi = item->data(Qt::UserRole).value<M1::MediaItem>();
                if (!mi.filePath.isEmpty())
                    m_player->loadFile(mi.filePath);
            });
    m_browserTabs->addTab(m_libraryTable, "Library");

    // ── Tab 2: Playlist ───────────────────────────────────────────────
    m_playlistList = new QListWidget;
    m_playlistList->setObjectName("DeckPlaylistList");
    m_playlistList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistList->setStyleSheet(listQss);
    m_playlistList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_playlistList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) { showBrowserContextMenu(pos, m_playlistList); });
    connect(m_playlistList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {
                auto mi = item->data(Qt::UserRole).value<M1::MediaItem>();
                if (!mi.filePath.isEmpty())
                    m_player->loadFile(mi.filePath);
            });
    m_browserTabs->addTab(m_playlistList, "Playlist");

    // ── Tab 3: Queue ──────────────────────────────────────────────────
    m_queueList = new QListWidget;
    m_queueList->setObjectName("DeckQueueList");
    m_queueList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_queueList->setStyleSheet(listQss);
    m_queueList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_queueList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) { showBrowserContextMenu(pos, m_queueList); });
    connect(m_queueList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {
                auto mi = item->data(Qt::UserRole).value<M1::MediaItem>();
                if (!mi.filePath.isEmpty())
                    m_player->loadFile(mi.filePath);
            });
    m_browserTabs->addTab(m_queueList, "Queue");
}

// ─── Browser context menu (shared for Playlist/Queue lists) ──────────────────
void DeckPanel::showBrowserContextMenu(const QPoint& pos, QListWidget* list) {
    auto* item = list->itemAt(pos);
    if (!item) return;
    auto mi = item->data(Qt::UserRole).value<M1::MediaItem>();
    if (mi.filePath.isEmpty()) return;

    const auto tp = ThemePalette::forCurrentTheme();
    QMenu menu(this);
    menu.setStyleSheet(menuQss(tp));

    menu.addAction("Load to Deck A", [this, mi]() {
        emit loadToDeckRequested(mi.filePath, 0);
    });
    menu.addAction("Load to Deck B", [this, mi]() {
        emit loadToDeckRequested(mi.filePath, 1);
    });
    menu.addSeparator();
    menu.addAction("Add to Queue", [this, mi]() {
        emit addToQueueRequested(mi);
    });
    menu.addSeparator();
    menu.addAction("Edit Tags\xe2\x80\xa6", [this, mi]() {
        emit editTagsRequested(mi);
    });

    menu.exec(list->viewport()->mapToGlobal(pos));
}

// ─── Library context menu (QTableWidget) ─────────────────────────────────────
void DeckPanel::showLibraryContextMenu(const QPoint& pos) {
    auto* item = m_libraryTable->itemAt(pos);
    if (!item) return;
    int row = item->row();
    auto* cellItem = m_libraryTable->item(row, 0);
    if (!cellItem) return;
    auto mi = cellItem->data(Qt::UserRole).value<M1::MediaItem>();
    if (mi.filePath.isEmpty()) return;

    const auto tp = ThemePalette::forCurrentTheme();
    QMenu menu(this);
    menu.setStyleSheet(menuQss(tp));

    menu.addAction("Load to Deck A", [this, mi]() {
        emit loadToDeckRequested(mi.filePath, 0);
    });
    menu.addAction("Load to Deck B", [this, mi]() {
        emit loadToDeckRequested(mi.filePath, 1);
    });
    menu.addSeparator();
    menu.addAction("Add to Queue", [this, mi]() {
        emit addToQueueRequested(mi);
    });
    menu.addSeparator();
    menu.addAction("Edit Tags\xe2\x80\xa6", [this, mi]() {
        emit editTagsRequested(mi);
    });

    menu.exec(m_libraryTable->viewport()->mapToGlobal(pos));
}

// ─── Data population methods ─────────────────────────────────────────────────
void DeckPanel::populateListFromItems(QListWidget* list, const QList<M1::MediaItem>& items) {
    list->clear();
    for (const auto& mi : items) {
        QString label = mi.displayTitle();
        if (!mi.artist.isEmpty())
            label = mi.artist + " \xe2\x80\x93 " + label;
        if (mi.durationMs > 0)
            label += "  [" + mi.durationString() + "]";

        auto* item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, QVariant::fromValue(mi));
        item->setToolTip(mi.filePath);
    }
}

void DeckPanel::setLibraryItems(const QList<M1::MediaItem>& items) {
    m_libraryTable->setRowCount(0);
    m_libraryTable->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const auto& mi = items[i];
        auto* titleItem = new QTableWidgetItem(mi.displayTitle());
        titleItem->setData(Qt::UserRole, QVariant::fromValue(mi));
        titleItem->setToolTip(mi.filePath);
        m_libraryTable->setItem(i, 0, titleItem);
        m_libraryTable->setItem(i, 1, new QTableWidgetItem(mi.artist));
        m_libraryTable->setItem(i, 2, new QTableWidgetItem(mi.durationString()));
        m_libraryTable->setItem(i, 3, new QTableWidgetItem(mi.genre));
    }
}

void DeckPanel::setPlaylistItems(const QList<M1::MediaItem>& items) {
    populateListFromItems(m_playlistList, items);
}

void DeckPanel::setQueueItems(const QList<M1::MediaItem>& items) {
    populateListFromItems(m_queueList, items);
}

// ─── paintEvent — themed background ──────────────────────────────────────────
void DeckPanel::paintEvent(QPaintEvent* e) {
    const auto tp = ThemePalette::forCurrentTheme();
    QPainter p(this);
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, tp.panelBg);
    bg.setColorAt(1.0, tp.cardBg);
    p.fillRect(rect(), bg);
    // Subtle border
    p.setPen(QPen(tp.border, 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    QWidget::paintEvent(e);
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void DeckPanel::onStateChanged(M1::DeckPlayer::State state) {
    const auto tp = ThemePalette::forCurrentTheme();
    const QString baseLblQss = "QLabel { color:%1; font-size:14px; font-weight:900;"
                               "  font-family:'Consolas','Courier New',monospace; }";
    updateTransportButtons(state);
    switch (state) {
    case M1::DeckPlayer::State::Empty:
        m_stateLabel->setText("EMPTY");
        m_stateLabel->setStyleSheet(baseLblQss.arg(tp.textDisabled.name()));
        break;
    case M1::DeckPlayer::State::Loading:
        m_stateLabel->setText("LOADING");
        m_stateLabel->setStyleSheet(baseLblQss.arg(tp.warning.name()));
        break;
    case M1::DeckPlayer::State::Ready:
        m_stateLabel->setText("CUE");
        m_stateLabel->setStyleSheet(baseLblQss.arg(tp.info.name()));
        break;
    case M1::DeckPlayer::State::Playing:
        m_stateLabel->setText(QString::fromUtf8("▶ PLAY"));
        m_stateLabel->setStyleSheet(baseLblQss.arg(tp.success.name()));
        break;
    case M1::DeckPlayer::State::Paused:
        m_stateLabel->setText(QString::fromUtf8("⏸ PAUSE"));
        m_stateLabel->setStyleSheet(baseLblQss.arg(tp.textMuted.name()));
        break;
    }
}

void DeckPanel::onPositionChanged(qint64 /*frame*/) {
    updateTimeDisplay();
    updateSeekSlider();
}

void DeckPanel::onPollTimer() {
    if (m_player->state() == M1::DeckPlayer::State::Playing) {
        updateTimeDisplay();
        updateSeekSlider();
    }
    m_vuMeter->setLevels(m_player->levelL(), m_player->levelR());
}

void DeckPanel::onBpmDetected(float bpm) {
    m_baseBpm = bpm;
    updateBpmDisplay();
}

void DeckPanel::updateBpmDisplay() {
    const float speed = m_player->speed();
    if (m_baseBpm > 0) {
        const float adjusted = m_baseBpm * speed;
        if (std::abs(speed - 1.0f) < 0.001f) {
            m_bpmLabel->setText(QString("%1").arg(adjusted, 0, 'f', 1));
        } else {
            const float pct = (speed - 1.0f) * 100.0f;
            m_bpmLabel->setText(QString("%1 (%2%3%)")
                .arg(adjusted, 0, 'f', 1)
                .arg(pct >= 0 ? "+" : "")
                .arg(pct, 0, 'f', 1));
        }
    } else if (std::abs(speed - 1.0f) >= 0.001f) {
        const float pct = (speed - 1.0f) * 100.0f;
        m_bpmLabel->setText(QString("%1%2%")
            .arg(pct >= 0 ? "+" : "")
            .arg(pct, 0, 'f', 1));
    }
}

void DeckPanel::onHotCuesChanged() {
    for (int i = 0; i < 4; ++i) {
        bool set = m_player->hotCue(i) >= 0;
        m_hotBtns[i]->setText(set ? QString("H%1●").arg(i + 1)
                                   : QString("H%1").arg(i + 1));
    }
}

void DeckPanel::onTagsLoaded() {
    const QString artist = m_player->tagArtist();
    const QString title  = m_player->tagTitle();
    const int     br     = m_player->bitrate();
    const int     sr     = m_player->sampleRate();
    const int     ch     = m_player->channels();

    m_artistLabel->setText(artist);
    m_titleLabel->setText(!title.isEmpty() ? title
                          : QFileInfo(m_player->loadedPath()).baseName());

    m_bitrateLabel->setText(br > 0 ? QString::number(br) : "PCM");
    m_kHzLabel->setText(sr > 0 ? QString("%1").arg(sr / 1000.0, 0, 'f', 1) : "--.-");
    m_stereoLabel->setText(ch == 1 ? "Mono" : ch == 2 ? "Stereo" : "---");

    // Kick off album art fetch if we have enough info
    if (!artist.isEmpty() && !title.isEmpty())
        fetchAlbumArt(artist, title);

    // ── Track history ────────────────────────────────────────────────────
    const QString entry = (!artist.isEmpty() && !title.isEmpty())
        ? QString("\xe2\x99\xaa  %1 \xe2\x80\x93 %2").arg(artist.trimmed(), title.trimmed())
        : (!title.trimmed().isEmpty()
            ? QString("\xe2\x99\xaa  %1").arg(title.trimmed())
            : QString("\xe2\x99\xaa  %1").arg(QFileInfo(m_player->loadedPath()).completeBaseName()));

    // Remove empty-state hint if it's the only item
    if (m_historyList->count() == 1
            && m_historyList->item(0)->flags() == Qt::NoItemFlags)
        m_historyList->clear();

    {
        const auto tp = ThemePalette::forCurrentTheme();
        m_historyList->insertItem(0, entry);
        m_historyList->item(0)->setForeground(tp.accent);   // highlight newest
        if (m_historyList->count() > 1)
            m_historyList->item(1)->setForeground(tp.text); // de-emphasise previous
    }

    // Cap at 50 entries
    while (m_historyList->count() > 50)
        delete m_historyList->takeItem(m_historyList->count() - 1);
}

void DeckPanel::onVolumeChanged(int value) {
    if (m_sliderMode == 0) {
        m_player->setGain(value / 100.0f);
        m_faderLabel->setText(QString("%1%").arg(value));
    } else {
        onPitchChanged(value);
    }
}

void DeckPanel::onPitchChanged(int value) {
    const float pct  = (value - 50) / 50.0f * 50.0f;
    const float rate = 1.0f + pct / 100.0f;
    m_player->setSpeed(std::clamp(rate, 0.5f, 1.5f));
    m_faderLabel->setText(QString("%1%2%").arg(pct >= 0 ? "+" : "").arg(pct, 0, 'f', 1));
    updateBpmDisplay();
}

void DeckPanel::onSliderMode(int mode) {
    m_sliderMode = mode;
    m_volModeBtn->setChecked(mode == 0);
    m_pitchModeBtn->setChecked(mode == 1);
    if (mode == 0) {
        m_fader->setRange(0, 100);
        const int v = qRound(m_player->gain() * 100.0f);
        m_fader->setValue(v);
        m_faderLabel->setText(QString("%1%").arg(v));
    } else {
        m_fader->setRange(0, 100);
        const int v = qRound((m_player->speed() - 1.0f) / 0.5f * 50.0f + 50.0f);
        m_fader->setValue(std::clamp(v, 0, 100));
    }
}

void DeckPanel::onSeekMoved(int value) {
    const qint64 total = m_player->totalSamples();
    if (total > 0)
        m_player->seek(total * (qint64)value / 10000LL);
}

// ─── Album art via MusicBrainz ────────────────────────────────────────────────
void DeckPanel::fetchAlbumArt(const QString& artist, const QString& title) {
    // Stage 1: MusicBrainz recording search
    const QString query = QString("artist:\"%1\" AND recording:\"%2\"")
                          .arg(artist.left(80), title.left(80));
    QUrl url("https://musicbrainz.org/ws/2/recording");
    QUrlQuery q;
    q.addQueryItem("query", query);
    q.addQueryItem("limit", "1");
    q.addQueryItem("fmt",   "json");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "Mcaster1Studio/1.0 (mcaster1.com)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    // Tag this request as MusicBrainz search
    req.setAttribute(QNetworkRequest::User, QString("mb_search"));
    m_nam->get(req);
}

void DeckPanel::onArtworkSearchReply(QNetworkReply* reply) {
    reply->deleteLater();
    const QByteArray attr = reply->request()
        .attribute(QNetworkRequest::User).toByteArray();

    if (attr == "mb_search") {
        // Parse MusicBrainz response to get a release MBID
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray recs   = doc["recordings"].toArray();
        if (recs.isEmpty()) return;
        const QJsonArray rels = recs[0]["releases"].toArray();
        if (rels.isEmpty()) return;
        const QString mbid = rels[0]["id"].toString();
        if (mbid.isEmpty()) return;

        // Stage 2: Cover Art Archive
        QNetworkRequest req(QUrl("https://coverartarchive.org/release/" + mbid + "/front-250"));
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mcaster1Studio/1.0 (mcaster1.com)");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setAttribute(QNetworkRequest::User, QString("caa_image"));
        m_nam->get(req);

    } else if (attr == "caa_image") {
        onArtworkImageReply(reply);
    }
}

void DeckPanel::onArtworkImageReply(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) return;
    QPixmap pix;
    if (!pix.loadFromData(reply->readAll())) return;
    pix = pix.scaled(112, 112, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    // Center-crop to 112×112
    QPixmap cropped(112, 112);
    cropped.fill(Qt::black);
    {
        QPainter p(&cropped);
        const int xOff = (pix.width()  - 112) / 2;
        const int yOff = (pix.height() - 112) / 2;
        p.drawPixmap(0, 0, pix, xOff, yOff, 112, 112);
    }
    cropped.setDevicePixelRatio(2.0);
    m_artLabel->setPixmap(cropped);
    m_artLabel->setStyleSheet(QString("QLabel { border:1px solid %1; }")
        .arg(ThemePalette::forCurrentTheme().border.darker(110).name()));
}

// ─── Smart play menu ─────────────────────────────────────────────────────────
void DeckPanel::onPlayBtnEmptyMenu()
{
    const auto tp = ThemePalette::forCurrentTheme();
    QMenu menu(this);
    menu.setStyleSheet(menuQss(tp));

    const QString deckLtr = (m_deckIndex == 0) ? "A" : "B";
    auto* hdr = menu.addAction("Load track onto Deck " + deckLtr);
    hdr->setEnabled(false);
    menu.addSeparator();

    auto* queueAct = menu.addAction("\xe2\x96\xb6   Play Next from Queue");
    queueAct->setToolTip("Load and play the next track from the Queue / AutoDJ module");

    auto* fileAct = menu.addAction("Open Audio File\xe2\x80\xa6");
    fileAct->setToolTip("Browse for an audio file");

    auto* libAct = menu.addAction("Browse Media Library");
    libAct->setToolTip("Drag a track from the Media Library onto this deck");

    menu.addSeparator();
    auto* dropHint = menu.addAction("  Drag & drop an audio file onto this deck");
    dropHint->setEnabled(false);

    auto* chosen = menu.exec(m_playBtn->mapToGlobal(QPoint(0, m_playBtn->height())));

    if (chosen == queueAct) {
        emit loadNextFromQueueRequested(m_deckIndex);

    } else if (chosen == fileAct) {
        const QString path = QFileDialog::getOpenFileName(
            this,
            QString("Load Track \xe2\x80\x94 Deck %1").arg(deckLtr),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.aac *.m4a *.opus *.wma *.aiff);;"
            "Playlist Files (*.m3u *.m3u8 *.pls *.xspf);;"
            "All Files (*)");

        if (!path.isEmpty()) {
            static const QStringList kPlaylistExts{"m3u", "m3u8", "pls", "xspf"};
            if (kPlaylistExts.contains(QFileInfo(path).suffix().toLower()))
                emit addPlaylistToQueueRequested(path);
            else
                m_player->loadFile(path);
        }

    } else if (chosen == libAct) {
        emit loadFromLibraryRequested(m_deckIndex);
    }
}

// ─── Time display ─────────────────────────────────────────────────────────────
void DeckPanel::updateTimeDisplay() {
    const double elapsed   = m_player->positionSeconds();
    const double total     = m_player->durationSeconds();
    const double remaining = total - elapsed;
    m_curLabel->setText(formatTime(elapsed));
    m_totLabel->setText(formatTime(total));
    m_remLabel->setText("-" + formatTime(remaining));
}

void DeckPanel::updateSeekSlider() {
    const qint64 total = m_player->totalSamples();
    const qint64 pos   = m_player->positionSamples();
    if (total > 0) {
        const int v = (int)(pos * 10000LL / total);
        m_seekSlider->blockSignals(true);
        m_seekSlider->setValue(v);
        m_seekSlider->blockSignals(false);
    }
}

void DeckPanel::updateTransportButtons(M1::DeckPlayer::State state) {
    const bool hasTrack = (state != M1::DeckPlayer::State::Empty &&
                           state != M1::DeckPlayer::State::Loading);
    m_playBtn->setEnabled(true);        // always enabled — shows menu when empty
    m_stopBtn->setEnabled(hasTrack);
    m_cueBtn->setEnabled(hasTrack);
    m_loopBtn->setEnabled(hasTrack);
    m_seekSlider->setEnabled(hasTrack);
    m_bpmMinusBtn->setEnabled(hasTrack);
    m_bpmPlusBtn->setEnabled(hasTrack);

    const bool playing = (state == M1::DeckPlayer::State::Playing);
    m_playBtn->setChecked(playing);
    m_playBtn->setText(playing ? "⏸" : "▶");
}

QString DeckPanel::formatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int m = (int)(seconds / 60);
    double s = seconds - m * 60;
    return QString("%1:%2").arg(m).arg(s, 4, 'f', 1, '0');
}

// ─── Drag & drop ──────────────────────────────────────────────────────────────
void DeckPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const auto& url = event->mimeData()->urls().first();
        static const QStringList audio =
            {"mp3","flac","wav","aif","aiff","ogg","opus","m4a","aac","wv","ape"};
        if (audio.contains(QFileInfo(url.toLocalFile()).suffix().toLower()))
            event->acceptProposedAction();
    }
}

void DeckPanel::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) { event->ignore(); return; }
    const QString path = urls.first().toLocalFile();
    if (path.isEmpty()) { event->ignore(); return; }
    m_player->loadFile(path);
    emit fileDropped(path, m_deckIndex);
    event->acceptProposedAction();
}

void DeckPanel::dropFile(const QString& path) { m_player->loadFile(path); }

// ─── DeckWidget ───────────────────────────────────────────────────────────────
DeckWidget::DeckWidget(M1::DeckPlayer* deckA, M1::DeckPlayer* deckB, QWidget* parent)
    : QWidget(parent)
{
    setObjectName("DeckWidget");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(3);

    m_panelA     = new DeckPanel(deckA, 0, this);
    m_crossfader = new CrossfaderWidget(this);
    m_crossfader->setMinimumWidth(180);
    m_crossfader->setDecks(deckA, deckB);   // wire for AUTOFADE remaining-time detection
    m_panelB     = new DeckPanel(deckB, 1, this);

    // Forward DeckPanel signals to DeckWidget level for MainWindow wiring
    for (auto* panel : {m_panelA, m_panelB}) {
        connect(panel, &DeckPanel::loadNextFromQueueRequested,
                this,  &DeckWidget::loadNextFromQueueRequested);
        connect(panel, &DeckPanel::loadFromLibraryRequested,
                this,  &DeckWidget::loadFromLibraryRequested);
        connect(panel, &DeckPanel::addPlaylistToQueueRequested,
                this,  &DeckWidget::addPlaylistToQueueRequested);
        connect(panel, &DeckPanel::loadToDeckRequested,
                this,  &DeckWidget::loadToDeckRequested);
        connect(panel, &DeckPanel::addToQueueRequested,
                this,  &DeckWidget::addToQueueRequested);
        connect(panel, &DeckPanel::editTagsRequested,
                this,  &DeckWidget::editTagsRequested);
    }

    // ── PTT panel — always visible below crossfader, themed ─────────
    const auto tp = ThemePalette::forCurrentTheme();
    m_pttPanel = new QWidget(this);
    m_pttPanel->setObjectName("PttPanel");
    m_pttPanel->setFixedHeight(44);
    m_pttPanel->setStyleSheet(QString(
        "QWidget#PttPanel { background:%1; border-top:1px solid %2; }"
        "QPushButton { background:%3; color:%4; border:1px solid %5;"
        "  border-radius:2px; font-size:12px; font-weight:700; padding:2px 6px; }"
        "QPushButton:checked { background:%6; border-color:%7; color:#fff; }"
        "QPushButton:hover { background:%8; }"
        "QPushButton:disabled { background:%1; color:%9; border-color:%2; }"
        "QLabel { color:%4; font-size:12px; }"
        "QLabel:disabled { color:%9; }"
    ).arg(tp.bg.name(), tp.border.name(),
          tp.cardBg.lighter(102).name(), tp.text.name(),
          tp.border.darker(110).name(), tp.error.name(),
          tp.error.darker(130).name(), tp.cardBg.darker(105).name(),
          tp.textDisabled.name())
    + QString(
        "QComboBox { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:2px; font-size:12px; padding:1px 4px; }"
        "QComboBox:disabled { background:%4; color:%5; border-color:%6; }"
        "QComboBox::drop-down { border:none; width:14px; }"
        "QProgressBar { border:1px solid %3; border-radius:2px;"
        "  background:%7; max-height:7px; }"
        "QProgressBar::chunk { background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 %8,stop:0.7 %9,stop:0.9 " + tp.error.name() + "); border-radius:2px; }"
    ).arg(tp.cardBg.name(), tp.text.name(), tp.border.darker(110).name(),
          tp.bg.name(), tp.textDisabled.name(), tp.border.name(),
          tp.cardBg.darker(105).name(),
          tp.success.name(), tp.warning.name()));

    auto* panelLay = new QHBoxLayout(m_pttPanel);
    panelLay->setContentsMargins(6, 4, 6, 4);
    panelLay->setSpacing(6);

    m_pttLed = new QLabel(m_pttPanel);
    m_pttLed->setFixedSize(10, 10);
    m_pttLed->setStyleSheet(QString("background:%1; border-radius:5px;")
        .arg(tp.textDisabled.name()));
    panelLay->addWidget(m_pttLed);

    m_pttBtn = new QPushButton("PTT", m_pttPanel);
    m_pttBtn->setCheckable(true);
    m_pttBtn->setFixedSize(48, 26);
    m_pttBtn->setEnabled(false);
    panelLay->addWidget(m_pttBtn);

    panelLay->addWidget(new QLabel("Mic:", m_pttPanel));
    m_micCombo = new QComboBox(m_pttPanel);
    m_micCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_micCombo->setEnabled(false);
    m_micCombo->addItem("(No PTT module)");
    panelLay->addWidget(m_micCombo, 1);

    m_pttMeter = new QProgressBar(m_pttPanel);
    m_pttMeter->setRange(0, 100);
    m_pttMeter->setValue(0);
    m_pttMeter->setTextVisible(false);
    m_pttMeter->setFixedWidth(80);
    panelLay->addWidget(m_pttMeter);

    // ── Center column: crossfader top-aligned, PTT below, playlist fills rest ──
    m_centerCol = new QVBoxLayout;
    m_centerCol->setContentsMargins(0, 0, 0, 0);
    m_centerCol->setSpacing(2);
    m_centerCol->addWidget(m_crossfader, 0);   // top-aligned with deck tops
    m_centerCol->addWidget(m_pttPanel, 0);     // flush below crossfader
    // Playlist widget will be inserted here by setPlaylistModule()
    m_centerCol->addStretch(1);                // fallback stretch if no playlist

    root->addWidget(m_panelA, 5);
    root->addLayout(m_centerCol, 2);
    root->addWidget(m_panelB, 5);
}

void DeckWidget::setPTTModule(M1::PTTModule* ptt) {
    if (m_pttMod == ptt) return;  // already wired — avoid duplicate connections
    m_pttMod = ptt;
    if (!ptt || !m_pttPanel) return;

    // Enable controls now that a PTT module is attached
    m_pttBtn->setEnabled(true);
    m_micCombo->setEnabled(true);

    // Populate mic device combo
    m_micCombo->clear();
    for (const auto& dev : QMediaDevices::audioInputs())
        m_micCombo->addItem(dev.description(), QVariant::fromValue(dev.id()));

    // Restore saved device
    QSettings s("Mcaster1", "Mcaster1Studio");
    const QByteArray savedId = s.value("PTT/inputDeviceId").toByteArray();
    for (int i = 0; i < m_micCombo->count(); ++i) {
        if (m_micCombo->itemData(i).toByteArray() == savedId) {
            m_micCombo->setCurrentIndex(i);
            break;
        }
    }

    // Connect mic combo
    connect(m_micCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QByteArray id = m_micCombo->itemData(idx).toByteArray();
        QSettings s2("Mcaster1", "Mcaster1Studio");
        s2.setValue("PTT/inputDeviceId", id);
        if (m_pttMod) m_pttMod->setInputDeviceId(QString::fromUtf8(id));
    });

    // Connect PTT toggle button
    connect(m_pttBtn, &QPushButton::toggled, this, [this](bool live) {
        if (m_pttMod)
            m_pttMod->setState(live ? M1::PTTModule::State::Live
                                    : M1::PTTModule::State::Armed);
    });

    // Sync button state with module state
    connect(ptt, &M1::PTTModule::stateChanged, this,
            [this](M1::PTTModule::State st) {
                const auto pal = ThemePalette::forCurrentTheme();
                const QString ledColor =
                    (st == M1::PTTModule::State::Live)  ? pal.error.name() :
                    (st == M1::PTTModule::State::Armed) ? pal.warning.name() : pal.textDisabled.name();
                m_pttLed->setStyleSheet(
                    QString("background:%1; border-radius:5px;").arg(ledColor));
                m_pttBtn->blockSignals(true);
                m_pttBtn->setChecked(st == M1::PTTModule::State::Live);
                m_pttBtn->blockSignals(false);
            }, Qt::QueuedConnection);

    // Poll timer for level meter
    m_pttPoll = new QTimer(this);
    m_pttPoll->start(50);
    connect(m_pttPoll, &QTimer::timeout, this, [this]() {
        if (!m_pttMod) return;
        const int pct = qBound(0, (int)(m_pttMod->inputLevel() * 100.0f), 100);
        m_pttMeter->setValue(pct);
    });

    // Arm the module
    ptt->setState(M1::PTTModule::State::Armed);
}

// ── DO NOT REMOVE — Playlist/AutoDJ is permanently embedded in DeckPlayer ──
// The AutoDJ module lives in the center column between the decks, below the
// crossfader and PTT. It is auto-created by MainWindow whenever a deck exists.
// NEVER remove, disable, or refactor this code without explicit permission.
void DeckWidget::setPlaylistModule(M1::PlaylistModule* playlist) {
    if (!playlist || m_playlistWidget) return;  // already set

    m_playlistWidget = new PlaylistWidget(playlist, this);

    // Insert into center column: after PTT (index 2), before the stretch
    // Remove the fallback stretch first, add playlist expanding, then re-add stretch
    auto* stretchItem = m_centerCol->itemAt(m_centerCol->count() - 1);
    if (stretchItem && stretchItem->spacerItem()) {
        m_centerCol->removeItem(stretchItem);
        delete stretchItem;
    }
    m_centerCol->addWidget(m_playlistWidget, 1);  // stretch factor 1 — fills remaining space
}

float DeckWidget::crossfaderValue() const { return m_crossfader->value(); }

void DeckWidget::setLibraryItems(const QList<M1::MediaItem>& items) {
    if (m_panelA) m_panelA->setLibraryItems(items);
    if (m_panelB) m_panelB->setLibraryItems(items);
}

void DeckWidget::setPlaylistItems(const QList<M1::MediaItem>& items) {
    if (m_panelA) m_panelA->setPlaylistItems(items);
    if (m_panelB) m_panelB->setPlaylistItems(items);
}

void DeckWidget::setQueueItems(const QList<M1::MediaItem>& items) {
    if (m_panelA) m_panelA->setQueueItems(items);
    if (m_panelB) m_panelB->setQueueItems(items);
}
