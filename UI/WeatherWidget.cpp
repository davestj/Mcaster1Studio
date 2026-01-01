#include "WeatherWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>
#include <QMenu>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QFrame>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QUrl>
#include <QDebug>
#include <cmath>

// ─── Icon helpers ─────────────────────────────────────────────────────────────

QString WeatherWidget::iconCategory(int code) {
    if (code == 113) return "sunny";
    if (code == 116) return "partcloudy";
    if (code == 119 || code == 122) return "cloudy";
    if (code == 143 || code == 248 || code == 260) return "fog";
    if (code == 200 || code == 386 || code == 389 ||
        code == 392 || code == 395) return "thunder";
    if (code == 179 || code == 227 || code == 230 ||
        (code >= 323 && code <= 338)) return "snow";
    return "rain";
}

QPixmap WeatherWidget::drawWeatherIcon(int code, int sz) {
    const QString cat = iconCategory(code);
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Helper: percent of sz
    auto P = [sz](double pct) { return (int)(sz * pct / 100.0 + 0.5); };

    if (cat == "sunny") {
        const QColor sun(252, 196, 25);
        const int cx = sz / 2, cy = sz / 2, r = P(26);
        // Rays
        QPen rayPen(sun, P(7), Qt::SolidLine, Qt::RoundCap);
        p.setPen(rayPen);
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4.0;
            const int r1 = r + P(5), r2 = P(47);
            p.drawLine(cx + (int)(r1*cos(a)), cy + (int)(r1*sin(a)),
                       cx + (int)(r2*cos(a)), cy + (int)(r2*sin(a)));
        }
        // Disc with radial gradient
        QRadialGradient rg(cx, cy - P(4), r * 1.1, cx, cy - P(6));
        rg.setColorAt(0.0, QColor(255, 240, 100));
        rg.setColorAt(1.0, QColor(240, 160, 10));
        p.setBrush(rg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(cx - r, cy - r, 2*r, 2*r);

    } else if (cat == "partcloudy") {
        // Small sun top-right
        const QColor sun(252, 196, 25);
        const int scx = P(62), scy = P(30), sr = P(14);
        QPen rayPen(sun, P(5), Qt::SolidLine, Qt::RoundCap);
        p.setPen(rayPen);
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4.0;
            const int r1 = sr + P(3), r2 = sr + P(11);
            p.drawLine(scx+(int)(r1*cos(a)), scy+(int)(r1*sin(a)),
                       scx+(int)(r2*cos(a)), scy+(int)(r2*sin(a)));
        }
        p.setBrush(sun); p.setPen(Qt::NoPen);
        p.drawEllipse(scx-sr, scy-sr, 2*sr, 2*sr);
        // Cloud
        const QColor cl(178, 196, 215);
        p.setBrush(cl);
        p.drawEllipse(P(4),  P(48), P(60), P(30));
        p.drawEllipse(P(2),  P(39), P(28), P(26));
        p.drawEllipse(P(26), P(32), P(35), P(32));

    } else if (cat == "cloudy") {
        const QColor cl(148, 164, 182);
        p.setBrush(cl); p.setPen(Qt::NoPen);
        p.drawEllipse(P(5),  P(44), P(60), P(32));
        p.drawEllipse(P(3),  P(35), P(30), P(28));
        p.drawEllipse(P(28), P(26), P(36), P(34));

    } else if (cat == "fog") {
        const QColor f(145, 158, 172);
        p.setPen(QPen(f, P(8), Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 4; ++i) {
            const int y = P(18 + i * 20);
            const int xoff = (i % 2 == 0) ? 0 : P(8);
            p.drawLine(P(8) + xoff, y, P(86) - xoff, y);
        }

    } else if (cat == "rain") {
        // Cloud
        const QColor cl(98, 112, 130);
        p.setBrush(cl); p.setPen(Qt::NoPen);
        p.drawEllipse(P(4),  P(24), P(60), P(28));
        p.drawEllipse(P(2),  P(16), P(28), P(24));
        p.drawEllipse(P(26), P(10), P(35), P(30));
        // Rain streaks
        p.setPen(QPen(QColor(96, 165, 250), P(5), Qt::SolidLine, Qt::RoundCap));
        const int rxs[] = {14, 28, 42, 56};
        for (int x : rxs)
            p.drawLine(P(x), P(56), P(x) - P(6), P(84));

    } else if (cat == "snow") {
        const QColor cl(122, 138, 158);
        p.setBrush(cl); p.setPen(Qt::NoPen);
        p.drawEllipse(P(4),  P(22), P(60), P(28));
        p.drawEllipse(P(2),  P(14), P(28), P(24));
        p.drawEllipse(P(26), P(8),  P(35), P(30));
        // Snowflake dots
        const QColor sf(205, 225, 255);
        p.setBrush(sf);
        const int sxs[] = {14, 28, 42, 20, 36};
        const int sys[] = {60, 66, 60, 80, 80};
        const int sr = P(5);
        for (int i = 0; i < 5; ++i)
            p.drawEllipse(P(sxs[i])-sr, P(sys[i])-sr, 2*sr, 2*sr);

    } else { // thunder
        const QColor cl(60, 70, 86);
        p.setBrush(cl); p.setPen(Qt::NoPen);
        p.drawEllipse(P(4),  P(16), P(66), P(32));
        p.drawEllipse(P(2),  P(8),  P(30), P(28));
        p.drawEllipse(P(28), P(2),  P(36), P(32));
        // Bolt
        QPolygon bolt;
        bolt << QPoint(P(42), P(46))
             << QPoint(P(26), P(68))
             << QPoint(P(36), P(68))
             << QPoint(P(22), P(94))
             << QPoint(P(46), P(64))
             << QPoint(P(36), P(64));
        p.setBrush(QColor(253, 224, 60));
        p.drawPolygon(bolt);
    }

    return pm;
}

// ─── Forecast dialog ──────────────────────────────────────────────────────────

static void showForecastDlg(const QList<WxForecastDay>& days,
                             bool useFahrenheit,
                             const QString& city,
                             QWidget* parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QString("Forecast — %1").arg(city));
    dlg.setModal(true);
    dlg.setMinimumWidth(320);

    // Dark styled
    dlg.setStyleSheet(
        "QDialog { background:#111318; color:#e0e0e0; }"
        "QLabel  { color:#e0e0e0; }"
        "QLabel.dayName  { color:#a0b0c0; font-size:9px; font-weight:bold; }"
        "QLabel.temp     { color:#f0b429; font-size:14px; font-weight:bold; }"
        "QLabel.cond     { color:#8090a0; font-size:8px; }"
        "QPushButton { background:#1e2530; border:1px solid #2a3240;"
        "  border-radius:3px; color:#c0d0e0; padding:4px 16px; }"
        "QPushButton:hover { background:#243040; }"
    );

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // Title row
    auto* title = new QLabel(QString("%1-Day Forecast").arg(days.size()), &dlg);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:11px; font-weight:bold; color:#c0d0e0; padding-bottom:4px;");
    root->addWidget(title);

    // Horizontal card strip
    auto* strip = new QHBoxLayout;
    strip->setSpacing(8);

    for (const auto& d : days) {
        // Card frame
        auto* card = new QFrame(&dlg);
        card->setObjectName("FxCard");
        card->setStyleSheet(
            "QFrame#FxCard { background:#1a1f28; border:1px solid #2a3040;"
            "  border-radius:5px; }"
        );
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(8, 8, 8, 8);
        cl->setSpacing(4);
        cl->setAlignment(Qt::AlignHCenter);

        // Day name
        auto* dn = new QLabel(d.dayName, card);
        dn->setAlignment(Qt::AlignCenter);
        dn->setStyleSheet("font-size:10px; font-weight:bold; color:#9aacbe;");
        cl->addWidget(dn);

        // Icon
        auto* iconLbl = new QLabel(card);
        const QPixmap pm = WeatherWidget::drawWeatherIcon(d.code, 48);  // expose static
        iconLbl->setPixmap(pm);
        iconLbl->setAlignment(Qt::AlignCenter);
        cl->addWidget(iconLbl);

        // Temps
        const QChar deg(0xB0);
        const QString hi = useFahrenheit ? (d.maxF + deg + "F") : (d.maxC + deg + "C");
        const QString lo = useFahrenheit ? (d.minF + deg + "F") : (d.minC + deg + "C");
        auto* hiLbl = new QLabel(hi, card);
        hiLbl->setAlignment(Qt::AlignCenter);
        hiLbl->setStyleSheet("font-size:13px; font-weight:bold; color:#f0b429;");
        cl->addWidget(hiLbl);

        auto* loLbl = new QLabel(lo, card);
        loLbl->setAlignment(Qt::AlignCenter);
        loLbl->setStyleSheet("font-size:10px; color:#6080a0;");
        cl->addWidget(loLbl);

        // Condition
        auto* condLbl = new QLabel(d.condition, card);
        condLbl->setAlignment(Qt::AlignCenter);
        condLbl->setWordWrap(true);
        condLbl->setStyleSheet("font-size:8px; color:#7090a8;");
        cl->addWidget(condLbl);

        strip->addWidget(card, 1);
    }

    root->addLayout(strip);

    // Close button
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto* closeBtn = new QPushButton("Close", &dlg);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(closeBtn);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    dlg.exec();
}

// ─── WeatherWidget ────────────────────────────────────────────────────────────

WeatherWidget::WeatherWidget(const QString& settingsKey, QWidget* parent)
    : QWidget(parent)
    , m_settingsKey(settingsKey)
{
    setObjectName("WeatherWidget");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
    setToolTip("Click for forecast  \xE2\x80\xA2  Right-click to configure");
    setMinimumSize(200, 64);

    connect(&m_nam, &QNetworkAccessManager::finished,
            this, &WeatherWidget::onReplyFinished);

    m_refreshTimer.setInterval(30 * 60 * 1000); // 30 minutes
    connect(&m_refreshTimer, &QTimer::timeout, this, &WeatherWidget::refresh);

    loadSettings();

    if (!m_location.isEmpty()) {
        refresh();
        m_refreshTimer.start();
    }
}

void WeatherWidget::setLocation(const QString& loc) {
    m_location = loc.trimmed();
    saveSettings();
    if (!m_location.isEmpty()) {
        refresh();
        if (!m_refreshTimer.isActive()) m_refreshTimer.start();
    }
}

void WeatherWidget::setUseFahrenheit(bool f) {
    m_useFahrenheit = f;
    saveSettings();
    if (!m_location.isEmpty()) refresh();
}

void WeatherWidget::refresh() {
    if (m_location.isEmpty()) { setErrorState(); return; }
    const QString encoded = QUrl::toPercentEncoding(m_location);
    // Request up to 5 days
    const QUrl url("https://wttr.in/" + encoded + "?format=j1&num_days=5");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mcaster1Studio/1.0");
    m_nam.get(req);
}

void WeatherWidget::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[WeatherWidget] Network error:" << reply->errorString();
        setErrorState();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isNull() || !doc.isObject()) { setErrorState(); return; }

    const QJsonObject root = doc.object();

    // City name
    m_city = m_location;
    const QJsonArray nearestArea = root["nearest_area"].toArray();
    if (!nearestArea.isEmpty()) {
        const QJsonArray areaName = nearestArea[0].toObject()["areaName"].toArray();
        if (!areaName.isEmpty())
            m_city = areaName[0].toObject()["value"].toString();
    }

    // Current condition
    const QJsonArray conds = root["current_condition"].toArray();
    if (conds.isEmpty()) { setErrorState(); return; }

    const QJsonObject cur = conds[0].toObject();
    m_tempF    = cur["temp_F"].toString();
    m_tempC    = cur["temp_C"].toString();
    m_feelsF   = cur["FeelsLikeF"].toString();
    m_feelsC   = cur["FeelsLikeC"].toString();
    m_humidity = cur["humidity"].toString().toInt();

    const QJsonArray descArr = cur["weatherDesc"].toArray();
    m_condition = descArr.isEmpty() ? QString()
                : descArr[0].toObject()["value"].toString();

    m_weatherCode = cur["weatherCode"].toString().toInt();
    m_hasData = true;

    // Forecast days
    m_forecast.clear();
    const QJsonArray wxDays = root["weather"].toArray();
    const QStringList dayNames{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    for (const QJsonValue& v : wxDays) {
        const QJsonObject d = v.toObject();
        WxForecastDay fd;
        fd.date = d["date"].toString();           // "2026-03-05"
        fd.maxF = d["maxtempF"].toString();
        fd.minF = d["mintempF"].toString();
        fd.maxC = d["maxtempC"].toString();
        fd.minC = d["mintempC"].toString();

        // Day name from date
        const QDate qd = QDate::fromString(fd.date, "yyyy-MM-dd");
        fd.dayName = qd.isValid() ? dayNames[qd.dayOfWeek() % 7] : fd.date;

        // Condition + code from first hourly entry
        const QJsonArray hourly = d["hourly"].toArray();
        if (!hourly.isEmpty()) {
            const QJsonObject h0 = hourly[0].toObject();
            fd.code = h0["weatherCode"].toString().toInt();
            const QJsonArray hdesc = h0["weatherDesc"].toArray();
            fd.condition = hdesc.isEmpty() ? QString()
                         : hdesc[0].toObject()["value"].toString();
        }

        m_forecast.append(fd);
    }

    // Invalidate icon cache
    m_iconCodeCached = -999;
    update();
}

void WeatherWidget::setErrorState() {
    m_hasData   = false;
    m_city      = m_location.isEmpty() ? "Weather" : m_location;
    m_condition = "Unavailable";
    m_tempF = m_tempC = m_feelsF = m_feelsC = "--";
    update();
}

// ─── paintEvent — 3D dark panel ───────────────────────────────────────────────

void WeatherWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const int w = width(), h = height();

    // ── 3D raised outer border ────────────────────────────────────────────
    p.setPen(QColor(68, 72, 84));
    p.drawLine(0, 0, w-2, 0);
    p.drawLine(0, 0, 0, h-2);
    p.setPen(QColor(0, 0, 0));
    p.drawLine(0, h-1, w-1, h-1);
    p.drawLine(w-1, 0, w-1, h-1);

    // ── Dark gradient background ──────────────────────────────────────────
    {
        QLinearGradient bg(0, 1, 0, h-1);
        bg.setColorAt(0.0, QColor(14, 18, 24));
        bg.setColorAt(1.0, QColor(7,  10, 15));
        p.fillRect(1, 1, w-2, h-2, bg);
    }

    // ── Inner recessed ring ───────────────────────────────────────────────
    p.setPen(QColor(0, 0, 0));
    p.drawLine(1, 1, w-3, 1);
    p.drawLine(1, 1, 1, h-3);
    p.setPen(QColor(28, 34, 44));
    p.drawLine(1, h-2, w-2, h-2);
    p.drawLine(w-2, 1, w-2, h-2);

    // ── Icon ─────────────────────────────────────────────────────────────
    const int iconSz  = h - 8;   // leave 4px top+bottom padding
    const int iconX   = 6;
    const int iconY   = 4;

    if (m_weatherCode != m_iconCodeCached) {
        m_iconPm         = drawWeatherIcon(m_weatherCode, iconSz);
        m_iconCodeCached = m_weatherCode;
    }
    if (!m_iconPm.isNull())
        p.drawPixmap(iconX, iconY, m_iconPm);

    // ── Text area ─────────────────────────────────────────────────────────
    const int textX = iconX + iconSz + 8;
    const int textW = w - textX - 6;

    if (!m_hasData) {
        // No data — guidance
        QFont f("Segoe UI", 9);
        p.setFont(f);
        p.setPen(QColor(80, 95, 110));
        p.drawText(QRect(textX, 0, textW, h),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   "Right-click to configure location");
        return;
    }

    const QChar deg(0xB0);
    const QString temp = m_useFahrenheit
        ? (m_tempF + deg + "F")
        : (m_tempC + deg + "C");
    const QString feels = m_useFahrenheit
        ? (QString("Feels ") + m_feelsF + deg)
        : (QString("Feels ") + m_feelsC + deg);

    const int halfH = h / 2;

    // Row 1: City (left) + Temperature (right, large amber)
    {
        QFont cityFont("Segoe UI", 9, QFont::Bold);
        p.setFont(cityFont);
        p.setPen(QColor(165, 185, 205));
        p.drawText(QRect(textX, 4, textW - 2, halfH - 4),
                   Qt::AlignLeft | Qt::AlignVCenter, m_city);

        QFont tempFont("Consolas", 15, QFont::Bold);
        tempFont.setStyleHint(QFont::Monospace);
        p.setFont(tempFont);
        // Shadow
        p.setPen(QColor(30, 18, 0));
        p.drawText(QRect(textX+1, 5, textW-1, halfH-4),
                   Qt::AlignRight | Qt::AlignVCenter, temp);
        // Main
        p.setPen(QColor(242, 168, 12));
        p.drawText(QRect(textX, 4, textW, halfH-4),
                   Qt::AlignRight | Qt::AlignVCenter, temp);
    }

    // Row 2: Condition (left) + Feels like (right, dim amber)
    {
        QFont condFont("Segoe UI", 8);
        p.setFont(condFont);
        p.setPen(QColor(100, 125, 150));
        p.drawText(QRect(textX, halfH, textW, halfH - 4),
                   Qt::AlignLeft | Qt::AlignVCenter, m_condition);

        QFont feelsFont("Segoe UI", 8);
        p.setFont(feelsFont);
        p.setPen(QColor(140, 90, 5));
        p.drawText(QRect(textX, halfH, textW, halfH - 4),
                   Qt::AlignRight | Qt::AlignVCenter, feels);
    }
}

void WeatherWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (m_hasData && !m_forecast.isEmpty())
            showForecastDialog();
        else
            showConfigDialog();
    }
    QWidget::mousePressEvent(e);
}

void WeatherWidget::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);
    if (m_hasData && !m_forecast.isEmpty())
        menu.addAction("View Forecast...", this, &WeatherWidget::showForecastDialog);
    menu.addAction("Configure Weather...", this, &WeatherWidget::showConfigDialog);
    menu.addAction("Refresh Now",          this, &WeatherWidget::refresh);
    menu.exec(e->globalPos());
}

void WeatherWidget::showForecastDialog() {
    if (m_forecast.isEmpty()) { showConfigDialog(); return; }
    showForecastDlg(m_forecast, m_useFahrenheit, m_city, this);
}

void WeatherWidget::showConfigDialog() {
    QDialog dlg(this);
    dlg.setWindowTitle("Weather Settings");
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* locEdit = new QLineEdit(m_location, &dlg);
    locEdit->setPlaceholderText("City name, US zip code, or lat,lon");
    form->addRow("Location:", locEdit);

    auto* unitCombo = new QComboBox(&dlg);
    unitCombo->addItem(QString("Fahrenheit (") + QChar(0xB0) + "F)");
    unitCombo->addItem(QString("Celsius (")    + QChar(0xB0) + "C)");
    unitCombo->setCurrentIndex(m_useFahrenheit ? 0 : 1);
    form->addRow("Units:", unitCombo);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        setUseFahrenheit(unitCombo->currentIndex() == 0);
        setLocation(locEdit->text().trimmed());
    }
}

void WeatherWidget::loadSettings() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    s.beginGroup("AppRibbon/Weather/" + m_settingsKey);
    m_location      = s.value("location", QString()).toString();
    m_useFahrenheit = s.value("fahrenheit", true).toBool();
    s.endGroup();
}

void WeatherWidget::saveSettings() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    s.beginGroup("AppRibbon/Weather/" + m_settingsKey);
    s.setValue("location",   m_location);
    s.setValue("fahrenheit", m_useFahrenheit);
    s.endGroup();
}
