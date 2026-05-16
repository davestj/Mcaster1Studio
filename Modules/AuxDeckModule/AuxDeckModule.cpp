#include <portaudio.h>      // MUST be before namespace M1 — C types
#include "AuxDeckModule.h"
#include "DeckPlayer.h"
#include "DeckWidget.h"    // for DeckInlineMeter
#include "WaveformView.h"
#include "AudioDevice.h"
#include "ThemePalette.h"
#include "ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QStyle>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QPainter>
#include <QLinearGradient>
#include <QTabWidget>
#include <QListWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QMenu>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace M1 {

// --- Local helpers ---------------------------------------------------------------

namespace {

// Flat button -- minimum 14px font, themed
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

// Transport button -- raised, tinted
static QPushButton* makeTransBtn(const QString& text, const QColor& tint, QWidget* parent)
{
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

// Hot cue pad button
static QPushButton* makeHotBtn(const QString& label, const QColor& col, QWidget* parent)
{
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

// Write a WAV file from interleaved float32 data
static bool writeWavFile(const QString& path, const float* data, int totalSamples,
                          int sampleRate, int channels)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    const int bitsPerSample = 16;
    const int byteRate = sampleRate * channels * (bitsPerSample / 8);
    const int blockAlign = channels * (bitsPerSample / 8);
    const int dataSize = totalSamples * (bitsPerSample / 8);
    const int fileSize = 36 + dataSize;

    // RIFF header
    f.write("RIFF", 4);
    quint32 v32 = static_cast<quint32>(fileSize);
    f.write(reinterpret_cast<const char*>(&v32), 4);
    f.write("WAVE", 4);

    // fmt chunk
    f.write("fmt ", 4);
    v32 = 16; f.write(reinterpret_cast<const char*>(&v32), 4);
    quint16 v16 = 1; // PCM
    f.write(reinterpret_cast<const char*>(&v16), 2);
    v16 = static_cast<quint16>(channels);
    f.write(reinterpret_cast<const char*>(&v16), 2);
    v32 = static_cast<quint32>(sampleRate);
    f.write(reinterpret_cast<const char*>(&v32), 4);
    v32 = static_cast<quint32>(byteRate);
    f.write(reinterpret_cast<const char*>(&v32), 4);
    v16 = static_cast<quint16>(blockAlign);
    f.write(reinterpret_cast<const char*>(&v16), 2);
    v16 = static_cast<quint16>(bitsPerSample);
    f.write(reinterpret_cast<const char*>(&v16), 2);

    // data chunk
    f.write("data", 4);
    v32 = static_cast<quint32>(dataSize);
    f.write(reinterpret_cast<const char*>(&v32), 4);

    // Convert float32 [-1,1] to int16 and write in chunks
    constexpr int kChunkSamples = 4096;
    std::vector<qint16> buf(kChunkSamples);
    int remaining = totalSamples;
    int offset = 0;
    while (remaining > 0) {
        const int n = std::min(remaining, kChunkSamples);
        for (int i = 0; i < n; ++i) {
            float s = std::clamp(data[offset + i], -1.0f, 1.0f);
            buf[i] = static_cast<qint16>(s * 32767.0f);
        }
        f.write(reinterpret_cast<const char*>(buf.data()), n * 2);
        offset += n;
        remaining -= n;
    }

    f.close();
    return true;
}

} // anonymous namespace

// --- Construction ----------------------------------------------------------------

AuxDeckModule::AuxDeckModule(QObject* parent)
    : IModule(parent)
{
    // Pre-allocate CUE and temp buffers (same as DeckAModule)
    const int maxSamples = 8192 * 2;
    m_tmpBuf.resize(maxSamples, 0.0f);
    m_cueBuf.resize(maxSamples, 0.0f);

    // Set default recording path
    m_recOutputPath = QCoreApplication::applicationDirPath() + "/recordings";
}

AuxDeckModule::~AuxDeckModule()
{
    shutdown();
}

QString AuxDeckModule::displayName() const
{
    return m_deckName.isEmpty() ? QStringLiteral("AUX Deck") : m_deckName;
}

// --- Lifecycle -------------------------------------------------------------------

void AuxDeckModule::initialize()
{
    // DeckPlayer requires a deck index: use 99 to indicate auxiliary
    m_player = new DeckPlayer(99, this);

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &AuxDeckModule::onArtworkReply);

    // Connect DeckPlayer signals
    connect(m_player, &DeckPlayer::stateChanged,   this, &AuxDeckModule::onStateChanged);
    connect(m_player, &DeckPlayer::bpmDetected,     this, &AuxDeckModule::onBpmDetected);
    connect(m_player, &DeckPlayer::hotCuesChanged,  this, &AuxDeckModule::onHotCuesChanged);
    connect(m_player, &DeckPlayer::tagsLoaded,      this, &AuxDeckModule::onTagsLoaded);
    connect(m_player, &DeckPlayer::loadingFinished, this, [this]() {
        updateTimeDisplay();
        updateSeekSlider();
    });

    // When a track finishes, add it to history
    connect(m_player, &DeckPlayer::finished, this, [this]() {
        if (!m_player) return;
        addTrackHistoryEntry(m_player->tagArtist(), m_player->tagTitle(),
                             m_player->durationSeconds());
    });

    // Default: AIR on so audio plays through global output immediately
    m_player->setAirOn(true);
    m_player->setCueOn(false);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout, this, &AuxDeckModule::onPollTimer);
    m_pollTimer->start();
}

void AuxDeckModule::shutdown()
{
    // Stop recording if active
    if (m_recording.load(std::memory_order_relaxed))
        stopRecording();

    m_airBus.close();
    m_cueBus.close();
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    if (m_player) {
        m_player->stop();
    }
}

// --- UI --------------------------------------------------------------------------

QWidget* AuxDeckModule::createWidget(QWidget* parent)
{
    m_widget = new QWidget(parent);
    m_widget->setObjectName("AuxDeckWidget");
    buildWidget(m_widget);
    return m_widget;
}

void AuxDeckModule::buildWidget(QWidget* container)
{
    const auto tp = ThemePalette::forCurrentTheme();

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ======================================================================
    // ROW 1: Header bar -- [AUX DECK: CustomName] [State] [AIR combo] [CUE combo]
    // ======================================================================
    auto* header = new QWidget(container);
    header->setObjectName("AuxDeckHeader");
    header->setFixedHeight(36);
    header->setStyleSheet(QString(
        "QWidget#AuxDeckHeader {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border-bottom: 1px solid %3;"
        "}"
    ).arg(tp.cardBg.lighter(103).name(), tp.cardBg.darker(102).name(),
          tp.border.name()));

    auto* hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(6, 0, 6, 0);
    hdrLay->setSpacing(5);

    auto* auxBadge = new QLabel(QString::fromUtf8("\xe2\x97\x8f AUX:"), header);
    auxBadge->setStyleSheet(QString(
        "QLabel { color:%1; font-size:14px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace; }")
        .arg(tp.accent.name()));

    m_nameEdit = new QLineEdit(m_deckName, header);
    m_nameEdit->setPlaceholderText("Deck name...");
    m_nameEdit->setMaximumWidth(140);
    m_nameEdit->setStyleSheet(QString(
        "QLineEdit { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:2px; font-size:14px; font-weight:700; padding:1px 4px; }"
        "QLineEdit:focus { border-color:%4; }"
    ).arg(tp.inputBg.name(), tp.text.name(), tp.border.name(), tp.accent.name()));
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString& t) {
        setDeckName(t);
    });

    m_stateLabel = new QLabel("STOP", header);
    m_stateLabel->setObjectName("AuxDeckStateLabel");
    m_stateLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:14px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace; padding:0 6px; }")
        .arg(tp.textDisabled.name()));
    m_stateLabel->setAlignment(Qt::AlignCenter);

    // Device combos — NO custom QSS on the combo itself (breaks dropdown on some themes)
    auto* airLbl = new QLabel("AIR:", header);
    airLbl->setStyleSheet(QString("QLabel { color:%1; font-size:12px; font-weight:700; }")
        .arg(tp.success.name()));
    m_airOutCombo = new QComboBox(header);
    m_airOutCombo->setMinimumWidth(140);
    m_airOutCombo->setToolTip("AIR OUT: Main on-air output device (applies immediately)");
    // CRITICAL: Apply device IMMEDIATELY on selection change
    connect(m_airOutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx < 0) return;
        const int devIdx = m_airOutCombo->itemData(idx).toInt();
        // Collision check: prevent AIR and CUE from using the same device
        if (devIdx >= 0 && devIdx == m_cueOutDevice) {
            qWarning() << "[AuxDeck] AIR device collision with CUE — blocked";
            // Revert to previous selection
            m_airOutCombo->blockSignals(true);
            for (int i = 0; i < m_airOutCombo->count(); ++i) {
                if (m_airOutCombo->itemData(i).toInt() == m_airOutDevice) {
                    m_airOutCombo->setCurrentIndex(i);
                    break;
                }
            }
            m_airOutCombo->blockSignals(false);
            return;
        }
        setAirOutDevice(devIdx);
        if (m_airToggle && m_airToggle->isChecked() && devIdx >= 0) {
            if (m_player) m_player->setAirOn(true);
            m_airBus.open(devIdx, this);
        }
    });

    auto* cueLbl = new QLabel("CUE:", header);
    cueLbl->setStyleSheet(QString("QLabel { color:%1; font-size:12px; font-weight:700; }")
        .arg(tp.accent.name()));
    m_cueOutCombo = new QComboBox(header);
    m_cueOutCombo->setMinimumWidth(140);
    m_cueOutCombo->setToolTip("CUE OUT: Headphone/preview output device (applies immediately)");
    connect(m_cueOutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx < 0) return;
        const int devIdx = m_cueOutCombo->itemData(idx).toInt();
        // Collision check: prevent CUE and AIR from using the same device
        if (devIdx >= 0 && devIdx == m_airOutDevice) {
            qWarning() << "[AuxDeck] CUE device collision with AIR — blocked";
            m_cueOutCombo->blockSignals(true);
            for (int i = 0; i < m_cueOutCombo->count(); ++i) {
                if (m_cueOutCombo->itemData(i).toInt() == m_cueOutDevice) {
                    m_cueOutCombo->setCurrentIndex(i);
                    break;
                }
            }
            m_cueOutCombo->blockSignals(false);
            return;
        }
        setCueOutDevice(devIdx);
        if (m_cueToggle && m_cueToggle->isChecked() && devIdx >= 0) {
            if (m_player) m_player->setCueOn(true);
            m_cueBus.open(devIdx, this);
        }
    });

    hdrLay->addWidget(auxBadge);
    hdrLay->addWidget(m_nameEdit);
    hdrLay->addWidget(m_stateLabel);
    hdrLay->addStretch(1);
    hdrLay->addWidget(airLbl);
    hdrLay->addWidget(m_airOutCombo);
    hdrLay->addWidget(cueLbl);
    hdrLay->addWidget(m_cueOutCombo);
    root->addWidget(header);

    // Populate device lists
    refreshDeviceList();

    // ======================================================================
    // ROW 2: Art | Metadata + Stats | Fader | VU
    // ======================================================================
    auto* midRow = new QHBoxLayout;
    midRow->setContentsMargins(5, 3, 3, 2);
    midRow->setSpacing(5);

    // -- Album art --------------------------------------------------------
    m_artLabel = new QLabel(container);
    m_artLabel->setObjectName("AuxDeckArtwork");
    m_artLabel->setFixedSize(56, 56);
    m_artLabel->setAlignment(Qt::AlignCenter);
    m_artLabel->setToolTip("Album artwork");
    m_artLabel->setStyleSheet(QString(
        "QLabel { background:%1; border:1px solid %2; border-radius:3px; }"
    ).arg(tp.bg.name(), tp.border.name()));
    // Placeholder
    {
        QPixmap art(112, 112);
        art.fill(tp.bg);
        {
            QPainter pa(&art);
            pa.setRenderHint(QPainter::Antialiasing);
            pa.setFont(QFont("Consolas", 28, QFont::Black));
            pa.setPen(QColor(tp.accent.red(), tp.accent.green(), tp.accent.blue(), 140));
            pa.drawText(QRect(0, 0, 112, 112), Qt::AlignCenter, "AUX");
        }
        art.setDevicePixelRatio(2.0);
        m_artLabel->setPixmap(art);
    }

    // -- Metadata + stats column ------------------------------------------
    auto* infoCol = new QVBoxLayout;
    infoCol->setSpacing(1);

    // Artist -- Title
    const QString metaQss = QString(
        "QLabel { color:%1; font-size:14px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.text.name());
    const QString metaMutedQss = QString(
        "QLabel { color:%1; font-size:12px;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.textMuted.name());

    auto* metaRow1 = new QHBoxLayout;
    metaRow1->setSpacing(4);
    m_artistLabel = new QLabel("", container);
    m_artistLabel->setStyleSheet(metaQss);
    m_artistLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_titleLabel = new QLabel(QString::fromUtf8("\xe2\x80\x94 no track \xe2\x80\x94"), container);
    m_titleLabel->setStyleSheet(metaQss);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    metaRow1->addWidget(m_artistLabel, 1);
    metaRow1->addWidget(m_titleLabel, 2);

    m_albumLabel = new QLabel("", container);
    m_albumLabel->setStyleSheet(metaMutedQss);

    infoCol->addLayout(metaRow1);
    infoCol->addWidget(m_albumLabel);

    // Stats grid: Cur/Tot/Rem + kbps/kHz/Stereo + BPM
    auto* statsGrid = new QGridLayout;
    statsGrid->setContentsMargins(0, 0, 0, 0);
    statsGrid->setHorizontalSpacing(3);
    statsGrid->setVerticalSpacing(1);

    const QString captionQss = QString(
        "QLabel { color:%1; font-size:12px;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.textMuted.name());
    const QString valueQss = QString(
        "QLabel { color:%1; font-size:12px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.text.name());
    const QString bpmValQss = QString(
        "QLabel { color:%1; font-size:12px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.accent.name());

    auto mkCap = [&](const QString& t) {
        auto* l = new QLabel(t, container);
        l->setStyleSheet(captionQss);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };
    auto mkVal = [&](const QString& t, const QString& qss = {}) {
        auto* l = new QLabel(t, container);
        l->setStyleSheet(qss.isEmpty() ? valueQss : qss);
        l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        l->setMinimumWidth(34);
        return l;
    };

    statsGrid->addWidget(mkCap("Cur"), 0, 0);
    m_curLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_curLabel, 0, 1);

    statsGrid->addWidget(mkCap("Tot"), 1, 0);
    m_totLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_totLabel, 1, 1);

    statsGrid->addWidget(mkCap("Rem"), 2, 0);
    m_remLabel = mkVal("--:--.-");
    statsGrid->addWidget(m_remLabel, 2, 1);

    // BPM row with nudge buttons
    statsGrid->addWidget(mkCap("BPM"), 3, 0);
    auto* bpmRow = new QHBoxLayout;
    bpmRow->setSpacing(2);
    m_bpmLabel = mkVal("---", bpmValQss);
    m_bpmMinusBtn = new QPushButton(QString::fromUtf8("\xe2\x88\x92"), container);
    m_bpmPlusBtn  = new QPushButton("+", container);
    for (auto* b : {m_bpmMinusBtn, m_bpmPlusBtn}) {
        b->setFixedSize(20, 20);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2;"
            "  border:1px solid %3; border-radius:3px;"
            "  font-size:12px; font-weight:900; }"
            "QPushButton:hover { border-color:%2; }"
            "QPushButton:pressed { background:%4; }"
        ).arg(tp.cardBg.name(), tp.accent.name(), tp.border.name(),
              tp.cardBg.darker(110).name()));
    }
    m_bpmMinusBtn->setToolTip("Decrease playback speed by 2%");
    m_bpmPlusBtn->setToolTip("Increase playback speed by 2%");
    connect(m_bpmMinusBtn, &QPushButton::clicked, this, [this]() {
        if (!m_player) return;
        const float s = std::clamp(m_player->speed() - 0.02f, 0.5f, 1.5f);
        m_player->setSpeed(s);
        updateBpmDisplay();
    });
    connect(m_bpmPlusBtn, &QPushButton::clicked, this, [this]() {
        if (!m_player) return;
        const float s = std::clamp(m_player->speed() + 0.02f, 0.5f, 1.5f);
        m_player->setSpeed(s);
        updateBpmDisplay();
    });
    bpmRow->addWidget(m_bpmLabel);
    bpmRow->addWidget(m_bpmMinusBtn);
    bpmRow->addWidget(m_bpmPlusBtn);
    statsGrid->addLayout(bpmRow, 3, 1, 1, 3);

    // Vertical divider in stats grid
    auto* vdiv = new QFrame(container);
    vdiv->setFrameShape(QFrame::VLine);
    vdiv->setStyleSheet(QString("QFrame { color:%1; }").arg(tp.border.name()));
    statsGrid->addWidget(vdiv, 0, 2, 3, 1);

    // Right sub-columns: kbps / kHz / Stereo
    m_bitrateLabel = mkVal("---");
    m_kHzLabel     = mkVal("--.-");
    m_stereoLabel  = mkVal("---");
    statsGrid->addWidget(m_bitrateLabel, 0, 3);
    statsGrid->addWidget(mkCap("kbps"), 0, 4);
    statsGrid->addWidget(m_kHzLabel,    1, 3);
    statsGrid->addWidget(mkCap("kHz"),  1, 4);
    statsGrid->addWidget(m_stereoLabel, 2, 3, 1, 2);
    statsGrid->setColumnStretch(5, 1);

    infoCol->addLayout(statsGrid);
    infoCol->addStretch(1);

    midRow->addWidget(m_artLabel, 0, Qt::AlignTop);
    midRow->addLayout(infoCol, 1);

    // -- Volume/Pitch fader column ----------------------------------------
    auto* volCol = new QVBoxLayout;
    volCol->setSpacing(2);
    volCol->setContentsMargins(2, 0, 2, 0);

    auto* volCap = new QLabel("VOL", container);
    volCap->setStyleSheet(QString("QLabel { color:%1; font-size:12px; font-weight:700;"
                           "  font-family:'Consolas','Courier New',monospace; }")
                           .arg(tp.textMuted.name()));
    volCap->setAlignment(Qt::AlignCenter);

    m_volModeBtn   = makeFlatBtn("V", container, true, 22, 18);
    m_pitchModeBtn = makeFlatBtn("P", container, true, 22, 18);
    m_volModeBtn->setToolTip("Volume mode");
    m_pitchModeBtn->setToolTip("Pitch mode");
    m_volModeBtn->setChecked(true);
    connect(m_volModeBtn,   &QPushButton::clicked, this, [this]() { onSliderMode(0); });
    connect(m_pitchModeBtn, &QPushButton::clicked, this, [this]() { onSliderMode(1); });
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(2);
    modeRow->addWidget(m_volModeBtn);
    modeRow->addWidget(m_pitchModeBtn);

    m_fader = new QSlider(Qt::Vertical, container);
    m_fader->setRange(0, 100);
    m_fader->setValue(100);
    m_fader->setMinimumHeight(55);
    m_fader->setToolTip("Volume / Pitch fader");
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
    connect(m_fader, &QSlider::valueChanged, this, &AuxDeckModule::onVolumeChanged);

    m_faderLabel = new QLabel("100%", container);
    m_faderLabel->setStyleSheet(QString("QLabel { color:%1; font-size:12px; font-weight:700;"
                                  "  font-family:'Consolas','Courier New',monospace; }")
                                  .arg(tp.accent.name()));
    m_faderLabel->setAlignment(Qt::AlignCenter);

    // AIR toggle — left of mute
    m_airToggle = new QPushButton("AIR", container);
    m_airToggle->setCheckable(true);
    m_airToggle->setChecked(true);  // AIR on by default
    m_airToggle->setFixedSize(32, 22);
    m_airToggle->setToolTip("Toggle AIR output (on-air bus)");
    m_airToggle->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:3px; font-size:12px; font-weight:900; padding:0; }"
        "QPushButton:checked { background:%4; color:#ffffff; border-color:%4; }"
        "QPushButton:hover { border-color:%4; }")
        .arg(tp.cardBg.name(), tp.textMuted.name(), tp.border.name(), tp.success.name()));
    connect(m_airToggle, &QPushButton::toggled, this, [this](bool on) {
        // Toggle opens/closes the dedicated AIR stream.
        // Routing is handled in onAudioBlock based on toggle state.
        if (on && m_airOutDevice >= 0) {
            m_airBus.open(m_airOutDevice, this);
        } else if (!on) {
            m_airBus.close();
        }
        qInfo() << "[AuxDeck] AIR toggled:" << (on ? "ON" : "OFF")
                << "device:" << m_airOutDevice;
    });

    m_muteBtn = makeFlatBtn("M", container, true, 26, 22);
    m_muteBtn->setToolTip("Mute");
    connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_player) m_player->setGain(on ? 0.0f : m_fader->value() / 100.0f);
    });

    // CUE toggle — right of mute
    m_cueToggle = new QPushButton("CUE", container);
    m_cueToggle->setCheckable(true);
    m_cueToggle->setChecked(false);  // CUE off by default
    m_cueToggle->setFixedSize(32, 22);
    m_cueToggle->setToolTip("Toggle CUE output (headphone preview bus)");
    m_cueToggle->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:3px; font-size:12px; font-weight:900; padding:0; }"
        "QPushButton:checked { background:%4; color:#ffffff; border-color:%4; }"
        "QPushButton:hover { border-color:%4; }")
        .arg(tp.cardBg.name(), tp.textMuted.name(), tp.border.name(), tp.accent.name()));
    connect(m_cueToggle, &QPushButton::toggled, this, [this](bool on) {
        setCueActive(on);
        if (on && m_cueOutDevice >= 0) {
            m_cueBus.open(m_cueOutDevice, this);
        } else if (!on) {
            m_cueBus.close();
        }
        qInfo() << "[AuxDeck] CUE toggled:" << (on ? "ON" : "OFF")
                << "device:" << m_cueOutDevice;
    });

    // AIR | MUTE | CUE button row
    auto* busRow = new QHBoxLayout;
    busRow->setSpacing(2);
    busRow->addWidget(m_airToggle);
    busRow->addWidget(m_muteBtn);
    busRow->addWidget(m_cueToggle);

    volCol->addWidget(volCap, 0, Qt::AlignHCenter);
    volCol->addLayout(modeRow);
    volCol->addWidget(m_fader, 1, Qt::AlignHCenter);
    volCol->addWidget(m_faderLabel, 0, Qt::AlignHCenter);
    volCol->addLayout(busRow);

    midRow->addLayout(volCol);

    // -- VU Meter ---------------------------------------------------------
    m_vuMeter = new DeckInlineMeter(container);
    midRow->addWidget(m_vuMeter);

    root->addLayout(midRow, 0);

    // ======================================================================
    // ROW 3: Seek slider (full width)
    // ======================================================================
    m_seekSlider = new QSlider(Qt::Horizontal, container);
    m_seekSlider->setObjectName("AuxDeckSeekSlider");
    m_seekSlider->setRange(0, 10000);
    m_seekSlider->setValue(0);
    m_seekSlider->setFixedHeight(14);
    m_seekSlider->setToolTip("Seek position");
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
    connect(m_seekSlider, &QSlider::sliderMoved, this, &AuxDeckModule::onSeekMoved);

    auto* seekWidget = new QWidget(container);
    seekWidget->setStyleSheet(QString("QWidget { background:%1; }")
        .arg(tp.cardBg.darker(103).name()));
    auto* seekLay = new QHBoxLayout(seekWidget);
    seekLay->setContentsMargins(5, 2, 5, 2);
    seekLay->addWidget(m_seekSlider);
    root->addWidget(seekWidget);

    // ======================================================================
    // ROW 4: Transport -- [Play][Stop][Rec][Eject] | [CP][CUE][LOOP] | [H1][H2][H3][H4] | [Load]
    // ======================================================================
    auto* transWidget = new QWidget(container);
    transWidget->setObjectName("AuxDeckTransport");
    transWidget->setStyleSheet(QString(
        "QWidget#AuxDeckTransport {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  border-top:1px solid %3; border-bottom:1px solid %3;"
        "}"
    ).arg(tp.cardBg.lighter(103).name(), tp.cardBg.darker(102).name(),
          tp.border.name()));
    auto* transLay = new QHBoxLayout(transWidget);
    transLay->setContentsMargins(4, 3, 4, 3);
    transLay->setSpacing(3);

    // Play button
    m_playBtn = makeTransBtn(QString::fromUtf8("\xe2\x96\xb6"), tp.success, container);
    m_playBtn->setCheckable(true);
    m_playBtn->setToolTip("Play / Pause");
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (m_player) m_player->togglePlayPause();
    });

    // Stop button
    m_stopBtn = makeTransBtn(QString::fromUtf8("\xe2\x96\xa0"), tp.error, container);
    m_stopBtn->setToolTip("Stop");
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        if (m_player) m_player->stop();
    });

    // Record button (red circle, checkable)
    m_recBtn = makeTransBtn(QString::fromUtf8("\xe2\x97\x8f"), tp.error, container);
    m_recBtn->setCheckable(true);
    m_recBtn->setToolTip("Record deck audio output");
    m_recBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 %1, stop:1 %2);"
        "  color:%3; border:1px solid %4; border-radius:4px;"
        "  font-size:14px; font-weight:900; padding:2px;"
        "}"
        "QPushButton:hover { background:%5; border-color:%3; }"
        "QPushButton:pressed { background:%6; }"
        "QPushButton:checked { background:%3; color:#ffffff; border-color:%7; }"
        "QPushButton:disabled { color:%8; }"
    ).arg(tp.cardBg.lighter(105).name(), tp.cardBg.name(),
          tp.error.name(), tp.border.name(),
          tp.cardBg.darker(103).name(), tp.cardBg.darker(108).name(),
          tp.error.darker(120).name(), tp.textDisabled.name()));
    connect(m_recBtn, &QPushButton::toggled, this, [this](bool on) {
        if (on) startRecording();
        else    stopRecording();
    });

    // Eject button
    m_ejectBtn = makeTransBtn(QString::fromUtf8("\xe2\x8f\x8f"), tp.accent, container);
    m_ejectBtn->setToolTip("Eject / Unload");
    connect(m_ejectBtn, &QPushButton::clicked, this, [this]() {
        if (m_player) m_player->unload();
    });

    transLay->addWidget(m_playBtn);
    transLay->addWidget(m_stopBtn);
    transLay->addWidget(m_recBtn);
    transLay->addWidget(m_ejectBtn);

    // Divider
    auto* div1 = new QFrame(container);
    div1->setFrameShape(QFrame::VLine);
    div1->setStyleSheet(QString("QFrame { color:%1; }").arg(tp.border.name()));
    transLay->addWidget(div1);

    // Cue Point button
    m_cpBtn = makeFlatBtn("CP", container);
    m_cpBtn->setMinimumSize(34, 30);
    m_cpBtn->setToolTip("Cue Point: set while playing, jump while stopped");
    connect(m_cpBtn, &QPushButton::clicked, this, [this]() {
        if (!m_player) return;
        if (m_player->state() == DeckPlayer::State::Playing)
            m_player->setCuePoint();
        else
            m_player->jumpToCue();
    });

    // Jump to Cue button
    m_cueJmpBtn = makeFlatBtn(QString::fromUtf8("\xe2\x96\xb2""CUE"), container);
    m_cueJmpBtn->setMinimumSize(44, 30);
    m_cueJmpBtn->setToolTip("Jump to Cue Point immediately");
    connect(m_cueJmpBtn, &QPushButton::clicked, this, [this]() {
        if (m_player) m_player->jumpToCue();
    });

    // Loop button
    m_loopBtn = makeFlatBtn("LOOP", container, true);
    m_loopBtn->setMinimumSize(42, 30);
    m_loopBtn->setToolTip("Toggle loop");
    connect(m_loopBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_player) m_player->setLoop(on);
    });

    transLay->addWidget(m_cpBtn);
    transLay->addWidget(m_cueJmpBtn);
    transLay->addWidget(m_loopBtn);
    transLay->addStretch(1);

    // Hot cue pads
    const QColor kHotColors[4] = {
        tp.error,      // red
        tp.success,    // green
        tp.accent,     // blue / gold
        tp.warning,    // amber
    };
    for (int i = 0; i < 4; ++i) {
        m_hotBtns[i] = makeHotBtn(QString("H%1").arg(i + 1), kHotColors[i], container);
        m_hotBtns[i]->setToolTip(
            QString("Hot Cue %1: click to set, click again to jump").arg(i + 1));
        connect(m_hotBtns[i], &QPushButton::clicked, this, [this, i]() {
            if (!m_player) return;
            if (m_player->hotCue(i) < 0) m_player->setHotCue(i);
            else                         m_player->jumpToHotCue(i);
        });
        transLay->addWidget(m_hotBtns[i]);
    }

    // Divider
    auto* div2 = new QFrame(container);
    div2->setFrameShape(QFrame::VLine);
    div2->setStyleSheet(QString("QFrame { color:%1; }").arg(tp.border.name()));
    transLay->addWidget(div2);

    // Load button
    m_loadBtn = makeFlatBtn("LOAD", container);
    m_loadBtn->setMinimumSize(44, 30);
    m_loadBtn->setToolTip("Load audio file");
    connect(m_loadBtn, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            m_widget, "Load Audio File", QString(),
            "Audio Files (*.mp3 *.wav *.ogg *.flac *.aac *.opus *.m4a *.wma *.aiff);;"
            "All Files (*)");
        if (!file.isEmpty())
            loadFile(file);
    });
    transLay->addWidget(m_loadBtn);

    root->addWidget(transWidget);

    // Waveform removed — tabs go directly below transport controls

    // ======================================================================
    // ROW 5: Tabbed panel (Track History / Recordings / Config)
    // ======================================================================
    m_tabWidget = new QTabWidget(container);
    m_tabWidget->setObjectName("AuxDeckTabs");
    m_tabWidget->setMinimumHeight(120);
    m_tabWidget->setStyleSheet(QString(
        "QTabWidget::pane {"
        "  background:%1; border:1px solid %2; border-top:none;"
        "}"
        "QTabBar::tab {"
        "  background:%3; color:%4; border:1px solid %2;"
        "  border-bottom:none; padding:4px 10px;"
        "  font-size:12px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  min-width:80px;"
        "}"
        "QTabBar::tab:selected {"
        "  background:%1; color:%5; border-bottom:2px solid %5;"
        "}"
        "QTabBar::tab:hover {"
        "  background:%6;"
        "}"
    ).arg(tp.cardBg.name(), tp.border.name(),
          tp.cardBg.darker(105).name(), tp.textMuted.name(),
          tp.accent.name(), tp.cardBg.darker(102).name()));

    // -- Tab 1: Track History ---------------------------------------------
    auto* historyPage = new QWidget;
    auto* histLay = new QVBoxLayout(historyPage);
    histLay->setContentsMargins(4, 4, 4, 4);
    histLay->setSpacing(2);

    m_historyList = new QListWidget(historyPage);
    m_historyList->setObjectName("AuxDeckHistoryList");
    m_historyList->setStyleSheet(QString(
        "QListWidget {"
        "  background:%1; color:%2; border:none;"
        "  font-size:12px; font-family:'Consolas','Courier New',monospace;"
        "}"
        "QListWidget::item { padding:2px 4px; }"
        "QListWidget::item:hover { background:%3; }"
        "QListWidget::item:selected { background:%4; color:#fff; }"
    ).arg(tp.cardBg.name(), tp.text.name(),
          tp.cardBg.darker(105).name(), tp.accent.name()));
    m_historyList->setAlternatingRowColors(false);
    histLay->addWidget(m_historyList);

    m_tabWidget->addTab(historyPage, "Track History");

    // -- Tab 2: Recordings ------------------------------------------------
    auto* recPage = new QWidget;
    auto* recLay = new QVBoxLayout(recPage);
    recLay->setContentsMargins(4, 4, 4, 4);
    recLay->setSpacing(2);

    m_recordingsList = new QListWidget(recPage);
    m_recordingsList->setObjectName("AuxDeckRecordingsList");
    m_recordingsList->setStyleSheet(QString(
        "QListWidget {"
        "  background:%1; color:%2; border:none;"
        "  font-size:12px; font-family:'Consolas','Courier New',monospace;"
        "}"
        "QListWidget::item { padding:2px 4px; }"
        "QListWidget::item:hover { background:%3; }"
        "QListWidget::item:selected { background:%4; color:#fff; }"
    ).arg(tp.cardBg.name(), tp.text.name(),
          tp.cardBg.darker(105).name(), tp.accent.name()));

    // Double-click to play
    connect(m_recordingsList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            loadFile(path);
    });

    // Right-click context menu
    m_recordingsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_recordingsList, &QWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        auto* item = m_recordingsList->itemAt(pos);
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) return;

        QMenu menu;
        menu.setStyleSheet(QString(
            "QMenu { background:%1; color:%2; border:1px solid %3; font-size:12px; }"
            "QMenu::item:selected { background:%4; color:#fff; }"
        ).arg(ThemePalette::forCurrentTheme().cardBg.name(),
              ThemePalette::forCurrentTheme().text.name(),
              ThemePalette::forCurrentTheme().border.name(),
              ThemePalette::forCurrentTheme().accent.name()));

        auto* actPlay = menu.addAction("Play");
        auto* actExplore = menu.addAction("Open in Explorer");
        auto* actDelete = menu.addAction("Delete");

        auto* chosen = menu.exec(m_recordingsList->mapToGlobal(pos));
        if (chosen == actPlay) {
            loadFile(path);
        } else if (chosen == actExplore) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        } else if (chosen == actDelete) {
            if (QFile::remove(path)) {
                delete item;
                emit statusChanged("Deleted: " + QFileInfo(path).fileName());
            }
        }
    });

    recLay->addWidget(m_recordingsList);
    m_tabWidget->addTab(recPage, "Recordings");

    // -- Tab 3: Config ----------------------------------------------------
    auto* cfgPage = new QWidget;
    auto* cfgLay = new QGridLayout(cfgPage);
    cfgLay->setContentsMargins(8, 8, 8, 8);
    cfgLay->setHorizontalSpacing(8);
    cfgLay->setVerticalSpacing(6);

    const QString cfgLblQss = QString(
        "QLabel { color:%1; font-size:12px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace; }").arg(tp.text.name());
    const QString cfgInputQss = QString(
        "QComboBox, QLineEdit, QSpinBox { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:2px; font-size:12px; padding:2px 4px; }"
        "QComboBox::drop-down { border:none; width:14px; }"
    ).arg(tp.inputBg.name(), tp.text.name(), tp.border.name());

    int row = 0;

    // Recording format
    auto* fmtLbl = new QLabel("Recording Format:", cfgPage);
    fmtLbl->setStyleSheet(cfgLblQss);
    m_recFormatCombo = new QComboBox(cfgPage);
    m_recFormatCombo->setStyleSheet(cfgInputQss);
    m_recFormatCombo->addItems({"WAV", "FLAC", "MP3", "Opus", "OGG Vorbis"});
    m_recFormatCombo->setCurrentIndex(m_recFormatIndex);
    connect(m_recFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { m_recFormatIndex = idx; });
    cfgLay->addWidget(fmtLbl, row, 0);
    cfgLay->addWidget(m_recFormatCombo, row, 1, 1, 2);
    ++row;

    // Recording output path
    auto* pathLbl = new QLabel("Output Path:", cfgPage);
    pathLbl->setStyleSheet(cfgLblQss);
    m_recPathEdit = new QLineEdit(m_recOutputPath, cfgPage);
    m_recPathEdit->setStyleSheet(cfgInputQss);
    m_recPathEdit->setToolTip("Directory where recordings are saved");
    connect(m_recPathEdit, &QLineEdit::textChanged,
            this, [this](const QString& t) { m_recOutputPath = t; });
    auto* browseBtn = makeFlatBtn("...", cfgPage, false, 30, 22);
    browseBtn->setToolTip("Browse for output directory");
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            m_widget, "Select Recording Output Directory", m_recOutputPath);
        if (!dir.isEmpty()) {
            m_recOutputPath = dir;
            if (m_recPathEdit) m_recPathEdit->setText(dir);
        }
    });
    cfgLay->addWidget(pathLbl, row, 0);
    cfgLay->addWidget(m_recPathEdit, row, 1);
    cfgLay->addWidget(browseBtn, row, 2);
    ++row;

    // Auto-naming
    auto* nameLbl = new QLabel("Auto-Naming:", cfgPage);
    nameLbl->setStyleSheet(cfgLblQss);
    m_autoNamingCheck = new QCheckBox("Enabled", cfgPage);
    m_autoNamingCheck->setStyleSheet(QString(
        "QCheckBox { color:%1; font-size:12px; }"
        "QCheckBox::indicator { width:16px; height:16px; }"
    ).arg(tp.text.name()));
    m_autoNamingCheck->setChecked(m_recAutoNaming);
    connect(m_autoNamingCheck, &QCheckBox::toggled,
            this, [this](bool on) { m_recAutoNaming = on; });
    m_autoNamingPattern = new QLineEdit(m_recPattern, cfgPage);
    m_autoNamingPattern->setStyleSheet(cfgInputQss);
    m_autoNamingPattern->setToolTip("Pattern: {name}=deck name, {date}=YYYY-MM-DD, {time}=HH-MM-SS");
    connect(m_autoNamingPattern, &QLineEdit::textChanged,
            this, [this](const QString& t) { m_recPattern = t; });
    cfgLay->addWidget(nameLbl, row, 0);
    cfgLay->addWidget(m_autoNamingCheck, row, 1);
    cfgLay->addWidget(m_autoNamingPattern, row, 2);
    ++row;

    // Default volume on load
    auto* volLbl = new QLabel("Default Volume:", cfgPage);
    volLbl->setStyleSheet(cfgLblQss);
    m_defaultVolSpin = new QSpinBox(cfgPage);
    m_defaultVolSpin->setStyleSheet(cfgInputQss);
    m_defaultVolSpin->setRange(0, 100);
    m_defaultVolSpin->setValue(m_defaultVolume);
    m_defaultVolSpin->setSuffix("%");
    connect(m_defaultVolSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { m_defaultVolume = v; });
    cfgLay->addWidget(volLbl, row, 0);
    cfgLay->addWidget(m_defaultVolSpin, row, 1);
    ++row;

    cfgLay->setRowStretch(row, 1);
    m_tabWidget->addTab(cfgPage, "Config");

    root->addWidget(m_tabWidget, 1);

    // Repaint on theme change
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            container, [container]() { container->update(); });

    // Populate recordings list from disk
    refreshRecordingsList();

    // Populate track history from cached list
    for (const QString& entry : m_trackHistory) {
        m_historyList->addItem(entry);
    }
}

void AuxDeckModule::refreshDeviceList()
{
    if (!m_airOutCombo || !m_cueOutCombo) return;

    m_airOutCombo->blockSignals(true);
    m_cueOutCombo->blockSignals(true);

    m_airOutCombo->clear();
    m_cueOutCombo->clear();

    // Default = follow global
    m_airOutCombo->addItem("(Use Global Default)", -1);
    m_cueOutCombo->addItem("(Use Global Default)", -1);

    // Ensure PortAudio is initialized before enumerating
    Pa_Initialize();  // safe to call multiple times
    auto devices = enumerateOutputDevices();
    qInfo() << "[AuxDeck] Enumerated" << devices.size() << "output devices for combos"
            << "(PA device count:" << Pa_GetDeviceCount() << ")";
    for (const auto& dev : devices) {
        const QString label = QString("%1 \xe2\x80\x94 %2").arg(dev.name, dev.hostApi);
        m_airOutCombo->addItem(label, dev.index);
        m_cueOutCombo->addItem(label, dev.index);
    }

    // Restore selections
    for (int i = 0; i < m_airOutCombo->count(); ++i) {
        if (m_airOutCombo->itemData(i).toInt() == m_airOutDevice) {
            m_airOutCombo->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < m_cueOutCombo->count(); ++i) {
        if (m_cueOutCombo->itemData(i).toInt() == m_cueOutDevice) {
            m_cueOutCombo->setCurrentIndex(i);
            break;
        }
    }

    m_airOutCombo->blockSignals(false);
    m_cueOutCombo->blockSignals(false);
}

// --- Playback --------------------------------------------------------------------

void AuxDeckModule::loadFile(const QString& filePath)
{
    // Add current track to history before loading new one
    if (m_player && m_player->state() != DeckPlayer::State::Empty) {
        addTrackHistoryEntry(m_player->tagArtist(), m_player->tagTitle(),
                             m_player->durationSeconds());
    }

    if (m_player)
        m_player->loadFile(filePath);

    emit statusChanged("Loaded: " + QFileInfo(filePath).fileName());
}

void AuxDeckModule::loadAndPlayOnCue(const QString& filePath)
{
    if (!m_player) return;

    // Always keep AIR on so audio plays through the global output.
    // CUE routing to a dedicated device is handled separately via the ring buffer.
    m_player->setAirOn(true);
    m_player->setCueOn(true);
    setCueActive(true);

    loadFile(filePath);

    // Auto-play: use a short timer to ensure loading has started,
    // then connect to loadingFinished for the actual play trigger.
    // Also set a fallback timer in case loadingFinished already fired.
    connect(m_player, &DeckPlayer::loadingFinished,
            this, [this]() {
                if (m_player && m_player->state() == DeckPlayer::State::Ready)
                    m_player->play();
            }, Qt::SingleShotConnection);

    // Fallback: if track loads very fast (cached), play after 500ms
    QTimer::singleShot(500, this, [this]() {
        if (m_player && m_player->state() == DeckPlayer::State::Ready)
            m_player->play();
    });

    emit statusChanged("Playing: " + QFileInfo(filePath).fileName());
}

void AuxDeckModule::play()
{
    if (m_player) m_player->play();
    emit playbackStateChanged(1);
}

void AuxDeckModule::pause()
{
    if (m_player) m_player->pause();
    emit playbackStateChanged(2);
}

void AuxDeckModule::stop()
{
    if (m_player) m_player->stop();
    emit playbackStateChanged(0);
}

void AuxDeckModule::setCueActive(bool active)
{
    m_cueActive.store(active);
}

void AuxDeckModule::setVolume(float v)
{
    m_volume.store(std::clamp(v, 0.0f, 1.0f));
    if (m_player) m_player->setGain(std::clamp(v, 0.0f, 1.0f));
}

void AuxDeckModule::setDeckName(const QString& name)
{
    if (m_deckName != name) {
        m_deckName = name;
        emit deckNameChanged(name);
        emit statusChanged(name);
    }
}

void AuxDeckModule::setAirOutDevice(int deviceIndex)
{
    m_airOutDevice = deviceIndex;
    // Immediately apply AIR routing to DeckPlayer
    if (m_player) m_player->setAirOn(deviceIndex >= 0);
    emit deviceConfigChanged();
}

void AuxDeckModule::setCueOutDevice(int deviceIndex)
{
    m_cueOutDevice = deviceIndex;
    if (m_player) m_player->setCueOn(deviceIndex >= 0);

    // Open a dedicated PortAudio stream on the selected CUE device
    // so this AuxDeck outputs independently of the global audio bus.
    if (deviceIndex >= 0)
        m_cueBus.open(deviceIndex, this);
    else
        m_cueBus.close();

    emit deviceConfigChanged();
}

// ── Per-bus PortAudio output streams (AIR + CUE independent) ─────────────────

void AuxDeckModule::AudioBus::open(int deviceIndex, void* userData)
{
    close();
    if (deviceIndex < 0) return;

    ring.resize(kBusRingFrames * 2, 0.0f);
    ringW.store(0, std::memory_order_relaxed);
    ringR.store(0, std::memory_order_relaxed);

    Pa_Initialize();

    const PaDeviceInfo* d = Pa_GetDeviceInfo(deviceIndex);
    if (!d || d->maxOutputChannels < 1) {
        qWarning() << "[AuxDeck Bus] Invalid device:" << deviceIndex;
        return;
    }

    PaStreamParameters outParams = {};
    outParams.device                    = deviceIndex;
    outParams.channelCount              = std::min(2, d->maxOutputChannels);
    outParams.sampleFormat              = paFloat32;
    outParams.suggestedLatency          = d->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    const double sr = d->defaultSampleRate > 0 ? d->defaultSampleRate : 48000.0;

    PaError err = Pa_OpenStream(&stream, nullptr, &outParams, sr, 512,
                                 paClipOff, &AuxDeckModule::paBusCallback, userData);
    if (err != paNoError) {
        qWarning() << "[AuxDeck Bus] Pa_OpenStream failed:" << Pa_GetErrorText(err)
                    << "device:" << d->name;
        stream = nullptr;
        return;
    }
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        qWarning() << "[AuxDeck Bus] Pa_StartStream failed:" << Pa_GetErrorText(err);
        Pa_CloseStream(stream);
        stream = nullptr;
        return;
    }
    qInfo() << "[AuxDeck Bus] Opened stream on:" << d->name;
}

void AuxDeckModule::AudioBus::close()
{
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
    }
}

void AuxDeckModule::AudioBus::write(const float* data, int frames, int channels)
{
    if (!stream || ring.empty()) return;
    const int w = ringW.load(std::memory_order_relaxed);
    for (int i = 0; i < frames; ++i) {
        const int off = ((w + i) & kBusRingMask) * 2;
        ring[off]     = data[i * channels];
        ring[off + 1] = (channels > 1) ? data[i * channels + 1] : data[i * channels];
    }
    ringW.store(w + frames, std::memory_order_release);
}

int AuxDeckModule::paBusCallback(const void* /*in*/, void* out, unsigned long frames,
                                   const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                   PaStreamCallbackFlags /*flags*/, void* userData)
{
    // userData points to the AuxDeckModule — we need to figure out which bus
    // this callback is for. Since both buses share the same callback, we check
    // which stream matches. Simple approach: try both ring buffers.
    auto* self = static_cast<AuxDeckModule*>(userData);
    auto* outBuf = static_cast<float*>(out);
    const int n = static_cast<int>(frames) * 2;

    // Determine which bus this callback is for by checking stream pointer
    AudioBus* bus = nullptr;
    // We can't directly compare PaStream* in the callback, so we try the AIR bus first
    // and if it has data, use it. This is a simplification — for proper identification
    // we'd need per-bus userData. For now, mix both buses into the output.

    // Actually, each bus opens its own stream with the same userData.
    // We need separate userData per bus. Let's use a struct:
    // For now, just read from BOTH ring buffers and mix.
    std::fill_n(outBuf, n, 0.0f);

    auto readBus = [&](AudioBus& b) {
        if (b.ring.empty()) return;
        const int w = b.ringW.load(std::memory_order_acquire);
        const int r = b.ringR.load(std::memory_order_relaxed);
        const int avail = w - r;
        if (avail <= 0) return;
        const int toRead = std::min(avail, static_cast<int>(frames));
        for (int i = 0; i < toRead; ++i) {
            const int off = ((r + i) & kBusRingMask) * 2;
            outBuf[i * 2]     += b.ring[off];
            outBuf[i * 2 + 1] += b.ring[off + 1];
        }
        b.ringR.store(r + toRead, std::memory_order_release);
    };

    readBus(self->m_airBus);
    readBus(self->m_cueBus);
    return paContinue;
}

// --- Audio RT callback -----------------------------------------------------------

void AuxDeckModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out)
{
    (void)in;
    if (!m_player) return;
    if (!out.isValid || out.frames == 0) return;

    m_cueMixFrames = 0;
    const int n = out.frames * out.channels;
    if (n > static_cast<int>(m_tmpBuf.size())) return;

    // Clear temp and CUE buffers
    std::fill_n(m_tmpBuf.data(), n, 0.0f);
    std::fill_n(m_cueBuf.data(), n, 0.0f);

    // Always render audio — volume applied by processBlock
    const float vol = m_volume.load(std::memory_order_relaxed);
    // Force DeckPlayer to always render (we handle routing ourselves)
    m_player->setAirOn(true);
    m_player->processBlock(m_tmpBuf.data(), out.frames, out.channels, vol);

    // Read toggle state from our buttons (NOT from DeckPlayer flags)
    const bool airOn = m_airToggle ? m_airToggle->isChecked() : true;
    const bool cueOn = m_cueToggle ? m_cueToggle->isChecked() : false;
    const bool airHasStream = (m_airBus.stream != nullptr);
    const bool cueHasStream = (m_cueBus.stream != nullptr);

    // Peak metering (always, regardless of routing)
    float pL = 0.0f, pR = 0.0f;
    for (int i = 0; i < n; ++i) {
        const float absVal = fabsf(m_tmpBuf[i]);
        if (i % out.channels == 0) { if (absVal > pL) pL = absVal; }
        else                        { if (absVal > pR) pR = absVal; }
    }
    m_peakL.store(pL, std::memory_order_relaxed);
    m_peakR.store(pR, std::memory_order_relaxed);

    // ── ROUTING ──────────────────────────────────────────────────────────
    // AIR bus: if AIR is ON and has a dedicated stream → write to that stream ONLY.
    //          if AIR is ON and NO dedicated stream → write to global out.data.
    //          if AIR is OFF → don't write anywhere for AIR.
    if (airOn) {
        if (airHasStream) {
            m_airBus.write(m_tmpBuf.data(), out.frames, out.channels);
        } else {
            for (int i = 0; i < n; ++i)
                out.data[i] += m_tmpBuf[i];
        }
    }

    // CUE bus: same logic — dedicated stream or global CUE buffer.
    m_cueMixFrames = 0;
    if (cueOn) {
        if (cueHasStream) {
            m_cueBus.write(m_tmpBuf.data(), out.frames, out.channels);
        } else {
            for (int i = 0; i < n; ++i)
                m_cueBuf[i] += m_tmpBuf[i];
            m_cueMixFrames = out.frames;
        }
    }

    // Recording: append samples to buffer.
    // NOTE: resize() may allocate on the RT thread when the pre-reserved capacity is
    // exhausted. This is acceptable for a v1 recording implementation -- the reserve
    // in startRecording() covers ~30 seconds, and std::vector doubles capacity on
    // growth, so allocations become increasingly rare after the first realloc.
    if (m_recording.load(std::memory_order_relaxed)) {
        const size_t oldSize = m_recBuffer.size();
        // Only grow if within a reasonable limit (~1 hour at 48kHz stereo)
        if (oldSize + n < 400000000) {
            m_recBuffer.resize(oldSize + n);
            std::memcpy(m_recBuffer.data() + oldSize, m_tmpBuf.data(), n * sizeof(float));
        }
    }
}

// --- Recording -------------------------------------------------------------------

void AuxDeckModule::startRecording()
{
    // Capture sample rate and channels from player
    if (m_player) {
        m_recSampleRate = m_player->sampleRate();
        if (m_recSampleRate <= 0) m_recSampleRate = 48000;
        m_recChannels = m_player->channels();
        if (m_recChannels <= 0) m_recChannels = 2;
    }

    // Reserve buffer for recording (~30 seconds initial)
    const size_t initialCapacity = static_cast<size_t>(m_recSampleRate) * m_recChannels * 30;
    m_recBuffer.clear();
    m_recBuffer.reserve(initialCapacity);

    m_recording.store(true, std::memory_order_relaxed);
    emit statusChanged("Recording started...");
}

void AuxDeckModule::stopRecording()
{
    m_recording.store(false, std::memory_order_relaxed);

    if (m_recBuffer.empty()) {
        emit statusChanged("Recording stopped (no audio captured).");
        return;
    }

    // Ensure output directory exists
    QDir dir(m_recOutputPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Generate file path
    const QString filePath = recordingFilePath();

    // Always save as WAV
    const int totalSamples = static_cast<int>(m_recBuffer.size());
    const bool ok = writeWavFile(filePath, m_recBuffer.data(), totalSamples,
                                  m_recSampleRate, m_recChannels);

    m_recBuffer.clear();
    m_recBuffer.shrink_to_fit();

    if (ok) {
        emit statusChanged("Recording saved: " + QFileInfo(filePath).fileName());

        // If format is not WAV, inform user
        if (m_recFormatIndex != 0) {
            const QStringList fmtNames = {"WAV", "FLAC", "MP3", "Opus", "OGG Vorbis"};
            const QString fmtName = (m_recFormatIndex < fmtNames.size())
                                    ? fmtNames[m_recFormatIndex] : "selected format";
            QMessageBox::information(m_widget, "Recording Saved",
                QString("WAV recording saved.\nConvert to %1 in a future update.\n\nFile: %2")
                    .arg(fmtName, filePath));
        }

        // Add to recordings list
        refreshRecordingsList();
    } else {
        emit statusChanged("Recording FAILED: could not write " + filePath);
    }
}

QString AuxDeckModule::recordingFilePath() const
{
    const QDateTime now = QDateTime::currentDateTime();
    QString fileName;

    if (m_recAutoNaming) {
        fileName = m_recPattern;
        fileName.replace("{name}", m_deckName.isEmpty() ? "AuxDeck" : m_deckName);
        fileName.replace("{date}", now.toString("yyyy-MM-dd"));
        fileName.replace("{time}", now.toString("HH-mm-ss"));
        // Sanitize file name
        fileName.replace(QRegularExpression("[^a-zA-Z0-9_\\-.]"), "_");
    } else {
        fileName = QString("AuxDeck_%1").arg(now.toString("yyyyMMdd_HHmmss"));
    }

    return m_recOutputPath + "/" + fileName + ".wav";
}

void AuxDeckModule::refreshRecordingsList()
{
    if (!m_recordingsList) return;
    m_recordingsList->clear();

    QDir dir(m_recOutputPath);
    if (!dir.exists()) return;

    const QStringList filters = {"*.wav", "*.flac", "*.mp3", "*.opus", "*.ogg"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    for (const QFileInfo& fi : files) {
        // Format file size
        QString sizeStr;
        qint64 sz = fi.size();
        if (sz < 1024)
            sizeStr = QString("%1 B").arg(sz);
        else if (sz < 1024 * 1024)
            sizeStr = QString("%1 KB").arg(sz / 1024.0, 0, 'f', 1);
        else
            sizeStr = QString("%1 MB").arg(sz / (1024.0 * 1024.0), 0, 'f', 1);

        // Estimate duration for WAV (assumes 16-bit stereo 48kHz)
        QString durStr = "---";
        if (fi.suffix().toLower() == "wav" && sz > 44) {
            const double estDur = (sz - 44.0) / (48000.0 * 2 * 2); // 16-bit stereo 48kHz
            const int mins = static_cast<int>(estDur / 60);
            const double secs = estDur - mins * 60;
            durStr = QString("%1:%2").arg(mins).arg(secs, 4, 'f', 1, '0');
        }

        const QString display = QString("%1  |  %2  |  %3  |  %4")
            .arg(fi.fileName(), -40)
            .arg(durStr, 8)
            .arg(fi.suffix().toUpper(), -4)
            .arg(sizeStr, 10);

        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath());
        m_recordingsList->addItem(item);
    }
}

void AuxDeckModule::addTrackHistoryEntry(const QString& artist, const QString& title,
                                          double durationSec)
{
    if (title.isEmpty() && artist.isEmpty()) return;

    const QString now = QDateTime::currentDateTime().toString("HH:mm");
    const int mins = static_cast<int>(durationSec / 60);
    const double secs = durationSec - mins * 60;
    const QString dur = QString("%1:%2").arg(mins).arg(secs, 4, 'f', 1, '0');

    const QString entry = QString("%1 -- %2 -- %3 (%4)")
        .arg(now,
             artist.isEmpty() ? "Unknown Artist" : artist,
             title.isEmpty() ? "Unknown Title" : title,
             dur);

    // Prepend (most recent at top)
    m_trackHistory.prepend(entry);
    while (m_trackHistory.size() > kMaxHistory)
        m_trackHistory.removeLast();

    // Update UI if available
    if (m_historyList) {
        m_historyList->insertItem(0, entry);
        while (m_historyList->count() > kMaxHistory)
            delete m_historyList->takeItem(m_historyList->count() - 1);
    }
}

// --- Poll timer (Qt thread) ------------------------------------------------------

void AuxDeckModule::onPollTimer()
{
    if (!m_player) return;

    // Update VU meter
    if (m_vuMeter)
        m_vuMeter->setLevels(m_player->levelL(), m_player->levelR());

    // Update time and seek if playing
    if (m_player->state() == DeckPlayer::State::Playing) {
        updateTimeDisplay();
        updateSeekSlider();
    }
}

void AuxDeckModule::onStateChanged()
{
    if (!m_player) return;
    updateTransportButtons();
}

void AuxDeckModule::onTagsLoaded()
{
    if (!m_player) return;

    const QString artist = m_player->tagArtist();
    const QString title  = m_player->tagTitle();
    const QString album  = m_player->tagAlbum();
    const int     br     = m_player->bitrate();
    const int     sr     = m_player->sampleRate();
    const int     ch     = m_player->channels();

    if (m_artistLabel)
        m_artistLabel->setText(artist);
    if (m_titleLabel)
        m_titleLabel->setText(!title.isEmpty() ? title
                              : QFileInfo(m_player->loadedPath()).baseName());
    if (m_albumLabel)
        m_albumLabel->setText(album);

    if (m_bitrateLabel)
        m_bitrateLabel->setText(br > 0 ? QString::number(br) : "PCM");
    if (m_kHzLabel)
        m_kHzLabel->setText(sr > 0 ? QString("%1").arg(sr / 1000.0, 0, 'f', 1) : "--.-");
    if (m_stereoLabel)
        m_stereoLabel->setText(ch == 1 ? "Mono" : ch == 2 ? "Stereo" : "---");

    // Kick off album art fetch
    if (!artist.isEmpty() && !title.isEmpty())
        fetchAlbumArt(artist, title);
}

void AuxDeckModule::onBpmDetected(float bpm)
{
    m_baseBpm = bpm;
    updateBpmDisplay();
}

void AuxDeckModule::onHotCuesChanged()
{
    if (!m_player) return;
    for (int i = 0; i < 4; ++i) {
        bool set = m_player->hotCue(i) >= 0;
        m_hotBtns[i]->setText(set ? QString("H%1\xe2\x97\x8f").arg(i + 1)
                                   : QString("H%1").arg(i + 1));
    }
}

void AuxDeckModule::onSeekMoved(int value)
{
    if (!m_player) return;
    const qint64 total = m_player->totalSamples();
    if (total > 0)
        m_player->seek(total * (qint64)value / 10000LL);
}

void AuxDeckModule::onVolumeChanged(int value)
{
    if (m_sliderMode == 0) {
        if (m_player) m_player->setGain(value / 100.0f);
        m_volume.store(value / 100.0f, std::memory_order_relaxed);
        if (m_faderLabel) m_faderLabel->setText(QString("%1%").arg(value));
    } else {
        onPitchChanged(value);
    }
}

void AuxDeckModule::onPitchChanged(int value)
{
    if (!m_player) return;
    const float pct  = (value - 50) / 50.0f * 50.0f;
    const float rate = 1.0f + pct / 100.0f;
    m_player->setSpeed(std::clamp(rate, 0.5f, 1.5f));
    if (m_faderLabel)
        m_faderLabel->setText(QString("%1%2%").arg(pct >= 0 ? "+" : "").arg(pct, 0, 'f', 1));
    updateBpmDisplay();
}

void AuxDeckModule::onSliderMode(int mode)
{
    m_sliderMode = mode;
    if (m_volModeBtn) m_volModeBtn->setChecked(mode == 0);
    if (m_pitchModeBtn) m_pitchModeBtn->setChecked(mode == 1);
    if (mode == 0) {
        m_fader->setRange(0, 100);
        const int v = m_player ? qRound(m_player->gain() * 100.0f) : 100;
        m_fader->setValue(v);
        if (m_faderLabel) m_faderLabel->setText(QString("%1%").arg(v));
    } else {
        m_fader->setRange(0, 100);
        const float spd = m_player ? m_player->speed() : 1.0f;
        const int v = qRound((spd - 1.0f) / 0.5f * 50.0f + 50.0f);
        m_fader->setValue(std::clamp(v, 0, 100));
    }
}

// --- Display helpers -------------------------------------------------------------

void AuxDeckModule::updateTimeDisplay()
{
    if (!m_player) return;
    const double elapsed   = m_player->positionSeconds();
    const double total     = m_player->durationSeconds();
    const double remaining = total - elapsed;
    if (m_curLabel) m_curLabel->setText(formatTime(elapsed));
    if (m_totLabel) m_totLabel->setText(formatTime(total));
    if (m_remLabel) m_remLabel->setText("-" + formatTime(remaining));
}

void AuxDeckModule::updateSeekSlider()
{
    if (!m_player || !m_seekSlider) return;
    const qint64 total = m_player->totalSamples();
    const qint64 pos   = m_player->positionSamples();
    if (total > 0) {
        const int v = (int)(pos * 10000LL / total);
        m_seekSlider->blockSignals(true);
        m_seekSlider->setValue(v);
        m_seekSlider->blockSignals(false);
    }
}

void AuxDeckModule::updateBpmDisplay()
{
    if (!m_player || !m_bpmLabel) return;
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

void AuxDeckModule::updateTransportButtons()
{
    if (!m_player) return;
    const auto state = m_player->state();
    const auto tp = ThemePalette::forCurrentTheme();
    const QString baseLblQss = "QLabel { color:%1; font-size:14px; font-weight:900;"
                               "  font-family:'Consolas','Courier New',monospace; padding:0 6px; }";

    const bool hasTrack = (state != DeckPlayer::State::Empty &&
                           state != DeckPlayer::State::Loading);
    if (m_stopBtn)   m_stopBtn->setEnabled(hasTrack);
    if (m_cpBtn)     m_cpBtn->setEnabled(hasTrack);
    if (m_loopBtn)   m_loopBtn->setEnabled(hasTrack);
    if (m_seekSlider) m_seekSlider->setEnabled(hasTrack);
    if (m_bpmMinusBtn) m_bpmMinusBtn->setEnabled(hasTrack);
    if (m_bpmPlusBtn)  m_bpmPlusBtn->setEnabled(hasTrack);
    if (m_recBtn)    m_recBtn->setEnabled(hasTrack);

    const bool playing = (state == DeckPlayer::State::Playing);
    if (m_playBtn) {
        m_playBtn->setChecked(playing);
        m_playBtn->setText(playing ? QString::fromUtf8("\xe2\x8f\xb8")
                                   : QString::fromUtf8("\xe2\x96\xb6"));
    }

    // Update state label
    if (m_stateLabel) {
        switch (state) {
        case DeckPlayer::State::Empty:
            m_stateLabel->setText("EMPTY");
            m_stateLabel->setStyleSheet(baseLblQss.arg(tp.textDisabled.name()));
            break;
        case DeckPlayer::State::Loading:
            m_stateLabel->setText("LOADING");
            m_stateLabel->setStyleSheet(baseLblQss.arg(tp.warning.name()));
            break;
        case DeckPlayer::State::Ready:
            m_stateLabel->setText("CUE");
            m_stateLabel->setStyleSheet(baseLblQss.arg(tp.info.name()));
            break;
        case DeckPlayer::State::Playing:
            m_stateLabel->setText(QString::fromUtf8("\xe2\x96\xb6 PLAY"));
            m_stateLabel->setStyleSheet(baseLblQss.arg(tp.success.name()));
            break;
        case DeckPlayer::State::Paused:
            m_stateLabel->setText(QString::fromUtf8("\xe2\x8f\xb8 PAUSE"));
            m_stateLabel->setStyleSheet(baseLblQss.arg(tp.textMuted.name()));
            break;
        }
    }
}

QString AuxDeckModule::formatTime(double seconds)
{
    if (seconds < 0) seconds = 0;
    int m = (int)(seconds / 60);
    double s = seconds - m * 60;
    return QString("%1:%2").arg(m).arg(s, 4, 'f', 1, '0');
}

// --- Album art via MusicBrainz ---------------------------------------------------

void AuxDeckModule::fetchAlbumArt(const QString& artist, const QString& title)
{
    if (!m_nam) return;
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
    req.setAttribute(QNetworkRequest::User, QString("mb_search"));
    m_nam->get(req);
}

void AuxDeckModule::onArtworkReply(QNetworkReply* reply)
{
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
        // Load album art image
        if (reply->error() != QNetworkReply::NoError) return;
        QPixmap pix;
        if (!pix.loadFromData(reply->readAll())) return;
        pix = pix.scaled(96, 96, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        // Center-crop to 96x96
        QPixmap cropped(96, 96);
        cropped.fill(Qt::black);
        {
            QPainter p(&cropped);
            const int xOff = (pix.width()  - 96) / 2;
            const int yOff = (pix.height() - 96) / 2;
            p.drawPixmap(0, 0, pix, xOff, yOff, 96, 96);
        }
        cropped.setDevicePixelRatio(2.0);
        if (m_artLabel) {
            m_artLabel->setPixmap(cropped);
            m_artLabel->setStyleSheet(QString("QLabel { border:1px solid %1; }")
                .arg(ThemePalette::forCurrentTheme().border.darker(110).name()));
        }
    }
}

// --- State persistence -----------------------------------------------------------

void AuxDeckModule::saveState(QSettings& s)
{
    s.setValue("auxdeck/name", m_deckName);
    s.setValue("auxdeck/airOutDevice", m_airOutDevice);
    s.setValue("auxdeck/cueOutDevice", m_cueOutDevice);
    s.setValue("auxdeck/volume", static_cast<double>(m_volume.load()));

    // Save speed
    if (m_player)
        s.setValue("auxdeck/speed", static_cast<double>(m_player->speed()));

    // Save track history
    s.setValue("auxdeck/trackHistory", m_trackHistory);

    // Save recording config
    s.setValue("auxdeck/recFormatIndex", m_recFormatIndex);
    s.setValue("auxdeck/recOutputPath", m_recOutputPath);
    s.setValue("auxdeck/recAutoNaming", m_recAutoNaming);
    s.setValue("auxdeck/recPattern", m_recPattern);
    s.setValue("auxdeck/defaultVolume", m_defaultVolume);
}

void AuxDeckModule::loadState(QSettings& s)
{
    m_deckName     = s.value("auxdeck/name", "AUX Deck").toString();
    m_airOutDevice = s.value("auxdeck/airOutDevice", -1).toInt();
    m_cueOutDevice = s.value("auxdeck/cueOutDevice", -1).toInt();
    float vol      = s.value("auxdeck/volume", 1.0).toFloat();
    m_volume.store(vol);

    if (m_nameEdit)     m_nameEdit->setText(m_deckName);
    if (m_fader) {
        if (m_sliderMode == 0)
            m_fader->setValue(static_cast<int>(vol * 100));
    }
    refreshDeviceList();

    // Restore speed
    float spd = s.value("auxdeck/speed", 1.0).toFloat();
    if (m_player) m_player->setSpeed(std::clamp(spd, 0.5f, 1.5f));

    // Apply device routing immediately
    if (m_player) {
        m_player->setAirOn(m_airOutDevice >= 0);
        m_player->setCueOn(m_cueOutDevice >= 0);
    }

    // Restore track history
    m_trackHistory = s.value("auxdeck/trackHistory").toStringList();
    if (m_historyList) {
        m_historyList->clear();
        for (const QString& entry : m_trackHistory)
            m_historyList->addItem(entry);
    }

    // Restore recording config
    m_recFormatIndex = s.value("auxdeck/recFormatIndex", 0).toInt();
    m_recOutputPath  = s.value("auxdeck/recOutputPath",
                               QCoreApplication::applicationDirPath() + "/recordings").toString();
    m_recAutoNaming  = s.value("auxdeck/recAutoNaming", true).toBool();
    m_recPattern     = s.value("auxdeck/recPattern", "AuxDeck_{name}_{date}_{time}").toString();
    m_defaultVolume  = s.value("auxdeck/defaultVolume", 100).toInt();

    // Update config UI
    if (m_recFormatCombo)   m_recFormatCombo->setCurrentIndex(m_recFormatIndex);
    if (m_recPathEdit)      m_recPathEdit->setText(m_recOutputPath);
    if (m_autoNamingCheck)  m_autoNamingCheck->setChecked(m_recAutoNaming);
    if (m_autoNamingPattern) m_autoNamingPattern->setText(m_recPattern);
    if (m_defaultVolSpin)   m_defaultVolSpin->setValue(m_defaultVolume);

    // Refresh recordings list
    refreshRecordingsList();
}

} // namespace M1
