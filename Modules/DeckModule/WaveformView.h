#pragma once
#include <QWidget>
#include <QImage>
#include <vector>
#include <cstdint>

namespace M1 { class DeckPlayer; }

/// WaveformView — waveform display widget for a DeckPlayer.
///
/// Renders a pre-computed peak waveform image (top = L channel, bottom = R),
/// colour-coded by amplitude (green/amber/red, matching the VU Meter scheme).
/// Draws a playback cursor, cue marker, loop region, and hot cue markers.
/// The display scrolls to keep the playhead centred.
class WaveformView : public QWidget {
    Q_OBJECT

public:
    explicit WaveformView(QWidget* parent = nullptr);

    void setDeckPlayer(M1::DeckPlayer* player);
    void refresh();  // rebuild waveform image from current player data

    QSize sizeHint() const override { return {600, 80}; }

public slots:
    void onPositionChanged(qint64 frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void buildImage();  // (re)render the full waveform QImage

    M1::DeckPlayer* m_player = nullptr;

    QImage   m_waveImage;        // full-width waveform (pre-rendered)
    bool     m_imageDirty = true; // rebuild needed

    qint64   m_posFrame   = 0;   // current playback frame (cached from player)
};
