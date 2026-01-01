/// @file   PodSoundboardModule.cpp
/// @path   Modules/PodSoundboardModule/PodSoundboardModule.cpp

#include "PodSoundboardModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QFileDialog>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPainter>
#include <QMenu>

namespace {

// ─── PadButton — styled grid button ────────────────────────────────────────
class PadButton : public QPushButton {
    Q_OBJECT
public:
    PadButton(int padIndex, QWidget* parent = nullptr)
        : QPushButton(parent), m_index(padIndex)
    {
        setMinimumSize(70, 60);
        setObjectName("PodPadBtn");
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        setFont(f);
        setContextMenuPolicy(Qt::CustomContextMenu);
    }
    int padIndex() const { return m_index; }
    void setPadColor(const QColor& c) { m_color = c; update(); }
    void setPlaying(bool on) { m_playing = on; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor bg = m_color;
        if (m_playing) bg = bg.lighter(140);
        if (isDown()) bg = bg.darker(120);

        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

        // Border glow when playing
        if (m_playing) {
            p.setPen(QPen(QColor(0x22, 0xc5, 0x5e), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 5, 5);
        }

        // Text
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, text());
    }
private:
    int    m_index;
    QColor m_color = QColor(0x33, 0x33, 0x33);
    bool   m_playing = false;
};

// ─── PodSoundboardWidget ────────────────────────────────────────────────────
class PodSoundboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodSoundboardWidget(M1::PodSoundboardModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodSoundboardWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // Bank selector
        auto* bankRow = new QHBoxLayout;
        bankRow->addWidget(new QLabel("Bank:"));
        m_bankCombo = new QComboBox;
        for (int i = 0; i < mod->bankCount(); ++i)
            m_bankCombo->addItem(mod->bank(i).name);
        connect(m_bankCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                mod, &M1::PodSoundboardModule::switchBank);
        bankRow->addWidget(m_bankCombo, 1);

        auto* addBankBtn = new QPushButton("+");
        addBankBtn->setFixedWidth(30);
        addBankBtn->setToolTip("Add new bank");
        connect(addBankBtn, &QPushButton::clicked, this, [this]() {
            int idx = m_mod->addBank(QString("Bank %1").arg(m_mod->bankCount() + 1));
            m_bankCombo->addItem(m_mod->bank(idx).name);
        });
        bankRow->addWidget(addBankBtn);

        auto* stopAllBtn = new QPushButton("Stop All");
        stopAllBtn->setObjectName("PodStopAllBtn");
        connect(stopAllBtn, &QPushButton::clicked, mod, &M1::PodSoundboardModule::stopAll);
        bankRow->addWidget(stopAllBtn);
        root->addLayout(bankRow);

        // Pad grid
        m_gridLayout = new QGridLayout;
        m_gridLayout->setSpacing(4);
        root->addLayout(m_gridLayout, 1);

        rebuildGrid();

        connect(mod, &M1::PodSoundboardModule::bankChanged, this, &PodSoundboardWidget::rebuildGrid);
        connect(mod, &M1::PodSoundboardModule::padTriggered, this, &PodSoundboardWidget::onPadStateChanged);
        connect(mod, &M1::PodSoundboardModule::padStopped, this, &PodSoundboardWidget::onPadStateChanged);
    }

private slots:
    void rebuildGrid() {
        // Clear existing
        for (auto* btn : m_padButtons) {
            m_gridLayout->removeWidget(btn);
            delete btn;
        }
        m_padButtons.clear();

        const int rows = m_mod->rows();
        const int cols = m_mod->cols();
        const auto& bank = m_mod->currentBank();

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const int idx = r * cols + c;
                auto* btn = new PadButton(idx);

                if (idx < bank.pads.size()) {
                    const auto& pad = bank.pads[idx];
                    btn->setText(pad.label.isEmpty() ? QString::number(idx + 1) : pad.label);
                    btn->setPadColor(pad.color);
                    btn->setPlaying(m_mod->isPadPlaying(idx));
                } else {
                    btn->setText(QString::number(idx + 1));
                }

                connect(btn, &QPushButton::clicked, this, [this, idx]() {
                    m_mod->triggerPad(idx);
                });
                connect(btn, &QPushButton::customContextMenuRequested, this, [this, idx]() {
                    onPadContextMenu(idx);
                });

                m_gridLayout->addWidget(btn, r, c);
                m_padButtons.append(btn);
            }
        }
    }

    void onPadStateChanged(int) { rebuildGrid(); }

    void onPadContextMenu(int padIndex) {
        QMenu menu(this);
        menu.addAction("Load Audio File...", [this, padIndex]() {
            const QString path = QFileDialog::getOpenFileName(
                this, "Load Audio", {}, "Audio (*.wav *.mp3 *.ogg *.flac)");
            if (path.isEmpty()) return;

            M1::SoundPad pad;
            pad.filePath = QUrl::fromLocalFile(path);
            pad.label = QFileInfo(path).completeBaseName();
            pad.color = QColor::fromHsl(padIndex * 37 % 360, 180, 100);
            m_mod->setPad(padIndex, pad);
            rebuildGrid();
        });
        menu.addAction("Stop", [this, padIndex]() { m_mod->stopPad(padIndex); });
        menu.exec(QCursor::pos());
    }

private:
    M1::PodSoundboardModule* m_mod;
    QGridLayout* m_gridLayout;
    QComboBox*   m_bankCombo;
    QList<PadButton*> m_padButtons;
};

} // anonymous namespace

#include "PodSoundboardModule.moc"

namespace M1 {

// ─── SoundPad JSON ──────────────────────────────────────────────────────────
QJsonObject SoundPad::toJson() const {
    QJsonObject obj;
    obj["id"]       = id;
    obj["label"]    = label;
    obj["filePath"] = filePath.toString();
    obj["color"]    = color.name();
    obj["volume"]   = static_cast<double>(volume);
    obj["fadeInMs"]  = static_cast<double>(fadeInMs);
    obj["fadeOutMs"] = static_cast<double>(fadeOutMs);
    obj["mode"]     = static_cast<int>(mode);
    obj["shortcut"] = shortcutKey;
    return obj;
}

SoundPad SoundPad::fromJson(const QJsonObject& obj) {
    SoundPad p;
    p.id        = obj["id"].toInt();
    p.label     = obj["label"].toString();
    p.filePath  = QUrl(obj["filePath"].toString());
    p.color     = QColor(obj["color"].toString("#333333"));
    p.volume    = static_cast<float>(obj["volume"].toDouble(1.0));
    p.fadeInMs  = static_cast<float>(obj["fadeInMs"].toDouble(0));
    p.fadeOutMs = static_cast<float>(obj["fadeOutMs"].toDouble(0));
    p.mode      = static_cast<PadTriggerMode>(obj["mode"].toInt(0));
    p.shortcutKey = obj["shortcut"].toString();
    return p;
}

QJsonObject SoundBank::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    QJsonArray arr;
    for (const auto& p : pads) arr.append(p.toJson());
    obj["pads"] = arr;
    return obj;
}

SoundBank SoundBank::fromJson(const QJsonObject& obj) {
    SoundBank b;
    b.name = obj["name"].toString();
    const auto arr = obj["pads"].toArray();
    for (const auto& v : arr) b.pads.append(SoundPad::fromJson(v.toObject()));
    return b;
}

// ─── PodSoundboardModule ────────────────────────────────────────────────────
PodSoundboardModule::PodSoundboardModule(QObject* parent) : IModule(parent) {
    // Create default banks
    SoundBank intro;
    intro.name = "Intro/Outro";
    m_banks.append(intro);

    SoundBank sfx;
    sfx.name = "Sound FX";
    m_banks.append(sfx);

    SoundBank music;
    music.name = "Music Beds";
    m_banks.append(music);
}

PodSoundboardModule::~PodSoundboardModule() {
    stopAll();
}

void PodSoundboardModule::initialize() {}
void PodSoundboardModule::shutdown() { stopAll(); }

QWidget* PodSoundboardModule::createWidget(QWidget* parent) {
    return new PodSoundboardWidget(this, parent);
}

void PodSoundboardModule::saveState(QSettings& s) {
    QJsonObject root;
    root["rows"] = m_rows;
    root["cols"] = m_cols;
    root["currentBank"] = m_currentBank;
    QJsonArray banks;
    for (const auto& b : m_banks) banks.append(b.toJson());
    root["banks"] = banks;
    s.setValue("soundboard", QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PodSoundboardModule::loadState(QSettings& s) {
    const QByteArray data = s.value("soundboard").toByteArray();
    if (data.isEmpty()) return;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    const QJsonObject root = doc.object();
    m_rows = root["rows"].toInt(4);
    m_cols = root["cols"].toInt(4);
    m_currentBank = root["currentBank"].toInt(0);
    m_banks.clear();
    const auto arr = root["banks"].toArray();
    for (const auto& v : arr) m_banks.append(SoundBank::fromJson(v.toObject()));
    if (m_banks.isEmpty()) {
        SoundBank b;
        b.name = "Default";
        m_banks.append(b);
        m_currentBank = 0;
    }
}

void PodSoundboardModule::setGridSize(int rows, int cols) {
    m_rows = std::clamp(rows, 2, 8);
    m_cols = std::clamp(cols, 2, 8);
    emit gridSizeChanged(m_rows, m_cols);
}

int PodSoundboardModule::addBank(const QString& name) {
    SoundBank b;
    b.name = name;
    m_banks.append(b);
    return m_banks.size() - 1;
}

void PodSoundboardModule::removeBank(int index) {
    if (index >= 0 && index < m_banks.size() && m_banks.size() > 1) {
        m_banks.removeAt(index);
        if (m_currentBank >= m_banks.size()) m_currentBank = 0;
    }
}

void PodSoundboardModule::switchBank(int index) {
    if (index >= 0 && index < m_banks.size()) {
        m_currentBank = index;
        emit bankChanged(index);
    }
}

void PodSoundboardModule::setPad(int padIndex, const SoundPad& pad) {
    auto& bank = m_banks[m_currentBank];
    while (bank.pads.size() <= padIndex) {
        SoundPad empty;
        empty.id = m_nextPadId++;
        bank.pads.append(empty);
    }
    bank.pads[padIndex] = pad;
    bank.pads[padIndex].id = m_nextPadId++;
}

const SoundPad* PodSoundboardModule::pad(int padIndex) const {
    const auto& bank = m_banks[m_currentBank];
    if (padIndex >= 0 && padIndex < bank.pads.size())
        return &bank.pads[padIndex];
    return nullptr;
}

void PodSoundboardModule::triggerPad(int padIndex) {
    const auto* p = pad(padIndex);
    if (!p || p->filePath.isEmpty()) return;

    ensurePlayers(padIndex + 1);
    auto& pp = m_players[padIndex];

    // Toggle mode: stop if already playing
    if (pp.playing && (p->mode == PadTriggerMode::Toggle || p->mode == PadTriggerMode::OneShot)) {
        stopPad(padIndex);
        return;
    }

    if (!pp.player) {
        pp.audio = new QAudioOutput(this);
        pp.player = new QMediaPlayer(this);
        pp.player->setAudioOutput(pp.audio);
        connect(pp.player, &QMediaPlayer::mediaStatusChanged, this,
                [this, padIndex](QMediaPlayer::MediaStatus st) {
            if (st == QMediaPlayer::EndOfMedia) {
                if (padIndex < m_players.size()) {
                    auto& ref = m_players[padIndex];
                    const auto* padPtr = pad(padIndex);
                    if (padPtr && padPtr->mode == PadTriggerMode::Loop) {
                        ref.player->setPosition(0);
                        ref.player->play();
                    } else {
                        ref.playing = false;
                        emit padStopped(padIndex);
                    }
                }
            }
        });
    }

    pp.audio->setVolume(p->volume);
    pp.player->setSource(p->filePath);
    pp.player->play();
    pp.playing = true;
    emit padTriggered(padIndex);
}

void PodSoundboardModule::stopPad(int padIndex) {
    if (padIndex >= 0 && padIndex < m_players.size()) {
        auto& pp = m_players[padIndex];
        if (pp.player) pp.player->stop();
        pp.playing = false;
        emit padStopped(padIndex);
    }
}

void PodSoundboardModule::stopAll() {
    for (int i = 0; i < m_players.size(); ++i)
        stopPad(i);
}

bool PodSoundboardModule::isPadPlaying(int padIndex) const {
    if (padIndex >= 0 && padIndex < m_players.size())
        return m_players[padIndex].playing;
    return false;
}

bool PodSoundboardModule::saveBank(int bankIndex, const QString& filePath) {
    if (bankIndex < 0 || bankIndex >= m_banks.size()) return false;
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(m_banks[bankIndex].toJson()).toJson(QJsonDocument::Indented));
    return true;
}

bool PodSoundboardModule::loadBank(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    m_banks.append(SoundBank::fromJson(doc.object()));
    return true;
}

void PodSoundboardModule::ensurePlayers(int count) {
    while (m_players.size() < count)
        m_players.append(PadPlayer{});
}

} // namespace M1
