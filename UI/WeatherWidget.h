#pragma once
#include <QWidget>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QPixmap>
#include <QList>

class QNetworkReply;

/// Per-day forecast record from wttr.in
struct WxForecastDay {
    QString date;       ///< "2026-03-05"
    QString dayName;    ///< "Thu"
    QString condition;
    int     code  = 113;
    QString maxF, minF;
    QString maxC, minC;
};

/// WeatherWidget — 3D-styled weather panel for the AppRibbon.
///
/// Renders a dark beveled panel (matching ClockWidget aesthetic) with:
///   - Left side: weather icon drawn with QPainter
///   - Right: city name, temperature (large amber), condition + feels-like
///
/// Left-click  → opens ForecastDialog (up to 5 days from wttr.in)
/// Right-click → config/refresh context menu
///
/// Data source: wttr.in JSON API (no API key required, 30-min refresh).
class WeatherWidget : public QWidget {
    Q_OBJECT

public:
    explicit WeatherWidget(const QString& settingsKey = "0", QWidget* parent = nullptr);

    QString location()      const { return m_location; }
    bool    useFahrenheit() const { return m_useFahrenheit; }

    void setLocation(const QString& loc);
    void setUseFahrenheit(bool f);

    QSize sizeHint()        const override { return {280, 88}; }
    QSize minimumSizeHint() const override { return {220, 88}; }

    /// Draw a weather icon into a QPixmap of the given size (also used by ForecastDialog).
    static QPixmap drawWeatherIcon(int code, int size);
    /// Map a wttr.in weather code to an icon category string.
    static QString iconCategory(int code);

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
    void onReplyFinished(QNetworkReply* reply);
    void showConfigDialog();
    void showForecastDialog();

private:
    void loadSettings();
    void saveSettings();
    void setErrorState();

    QString  m_settingsKey;
    QString  m_location;
    bool     m_useFahrenheit = true;

    QNetworkAccessManager m_nam;
    QTimer                m_refreshTimer;

    // Current conditions
    bool    m_hasData     = false;
    QString m_city;
    QString m_condition;
    int     m_weatherCode = 113;
    QString m_tempF, m_tempC;
    QString m_feelsF, m_feelsC;
    int     m_humidity    = 0;

    // Forecast (up to 5 days)
    QList<WxForecastDay> m_forecast;

    // Cached icon pixmap
    QPixmap m_iconPm;
    int     m_iconCodeCached = -999;
};
