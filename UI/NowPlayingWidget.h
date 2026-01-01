#pragma once
#include <QWidget>
#include <QTimer>
#include <QString>

class QLabel;

/// NowPlayingWidget — compact "streaming" indicator for the AppRibbon.
///
/// When a deck is actively playing it shows:
///   [● STREAMING]  [DK-A]  Artist — Title
/// with the ● indicator blinking at 1Hz.
/// When idle it shows a dim "─ NO DECK ─" message.
///
/// Add to the ribbon via:
///   m_appRibbon->addBox(m_nowPlaying, "nowplaying");
class NowPlayingWidget : public QWidget {
    Q_OBJECT

public:
    explicit NowPlayingWidget(QWidget* parent = nullptr);

    /// Update with the currently playing track info.
    void setPlaying(const QString& artist, const QString& title,
                    const QString& deckId);

    /// Mark as stopped / idle.
    void clearPlaying();

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void onBlink();
    void applyTheme();

private:
    QString m_artist;
    QString m_title;
    QString m_deckId;
    bool    m_isPlaying  = false;
    bool    m_blinkState = false;

    QLabel* m_dotLabel   = nullptr;
    QLabel* m_deckLabel  = nullptr;
    QLabel* m_trackLabel = nullptr;
    QTimer* m_blinkTimer = nullptr;
};
