/// @file   PodRemoteModule.cpp
/// @path   Modules/PodRemoteModule/PodRemoteModule.cpp

#include "PodRemoteModule.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QFrame>
#include <QSettings>
#include <QPainter>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>

namespace {

// ─── Audio Level Bar ───────────────────────────────────────────────────
class GuestLevelBar : public QWidget {
    Q_OBJECT
public:
    explicit GuestLevelBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("PodRemoteLevelBar");
        setFixedHeight(8);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setLevel(float level) {
        m_level = std::clamp(level, 0.0f, 1.0f);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.setBrush(QColor(0x33, 0x33, 0x33));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 3, 3);

        // Level fill
        if (m_level > 0.0f) {
            const int w = static_cast<int>(width() * m_level);
            QColor c;
            if (m_level < 0.6f)
                c = QColor(0x22, 0xc5, 0x5e); // green
            else if (m_level < 0.85f)
                c = QColor(0xf5, 0x9e, 0x0b); // amber
            else
                c = QColor(0xef, 0x44, 0x44); // red
            p.setBrush(c);
            p.drawRoundedRect(0, 0, w, height(), 3, 3);
        }
    }

private:
    float m_level = 0.0f;
};

// ─── Status LED ────────────────────────────────────────────────────────
class StatusLed : public QWidget {
public:
    explicit StatusLed(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("PodRemoteStatusLed");
        setFixedSize(14, 14);
    }

    void setStatus(M1::RemoteGuest::Status s) {
        m_status = s;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c;
        switch (m_status) {
        case M1::RemoteGuest::Status::Disconnected: c = QColor(0x66, 0x66, 0x66); break;
        case M1::RemoteGuest::Status::Waiting:      c = QColor(0xf5, 0x9e, 0x0b); break;
        case M1::RemoteGuest::Status::Connected:    c = QColor(0x22, 0xc5, 0x5e); break;
        case M1::RemoteGuest::Status::Error:        c = QColor(0xef, 0x44, 0x44); break;
        }
        p.setBrush(c);
        p.setPen(QPen(c.darker(140), 1));
        p.drawEllipse(rect().adjusted(1, 1, -1, -1));
    }

private:
    M1::RemoteGuest::Status m_status = M1::RemoteGuest::Status::Disconnected;
};

// ─── Guest Card ────────────────────────────────────────────────────────
class GuestCard : public QFrame {
    Q_OBJECT
public:
    GuestCard(int guestId, const M1::RemoteGuest& guest, M1::PodRemoteModule* mod,
              QWidget* parent = nullptr)
        : QFrame(parent), m_id(guestId), m_mod(mod)
    {
        setObjectName("PodRemoteGuestCard");
        setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 6, 8, 6);
        lay->setSpacing(4);

        // Top row: LED + name + latency
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(6);

        m_led = new StatusLed;
        topRow->addWidget(m_led);

        m_nameLabel = new QLabel(guest.name);
        m_nameLabel->setObjectName("PodRemoteGuestName");
        QFont nf = m_nameLabel->font();
        nf.setPixelSize(14);
        nf.setBold(true);
        m_nameLabel->setFont(nf);
        topRow->addWidget(m_nameLabel, 1);

        m_latencyLabel = new QLabel;
        m_latencyLabel->setObjectName("PodRemoteLatency");
        QFont lf = m_latencyLabel->font();
        lf.setPixelSize(11);
        m_latencyLabel->setFont(lf);
        topRow->addWidget(m_latencyLabel);

        lay->addLayout(topRow);

        // Status text
        m_statusLabel = new QLabel;
        m_statusLabel->setObjectName("PodRemoteGuestStatus");
        QFont sf = m_statusLabel->font();
        sf.setPixelSize(12);
        sf.setItalic(true);
        m_statusLabel->setFont(sf);
        lay->addWidget(m_statusLabel);

        // Audio level bar
        m_levelBar = new GuestLevelBar;
        lay->addWidget(m_levelBar);

        // Buttons row
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(4);

        m_muteBtn = new QPushButton("Mute");
        m_muteBtn->setObjectName("PodRemoteMuteBtn");
        m_muteBtn->setCheckable(true);
        m_muteBtn->setToolTip("Mute/unmute this guest");
        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool checked) {
            m_mod->setGuestMuted(m_id, checked);
        });
        btnRow->addWidget(m_muteBtn);

        m_admitBtn = new QPushButton("Admit");
        m_admitBtn->setObjectName("PodRemoteAdmitBtn");
        m_admitBtn->setToolTip("Move guest from waiting room to live");
        connect(m_admitBtn, &QPushButton::clicked, this, [this]() {
            m_mod->admitGuest(m_id);
        });
        btnRow->addWidget(m_admitBtn);

        m_disconnectBtn = new QPushButton("Disconnect");
        m_disconnectBtn->setObjectName("PodRemoteDisconnectBtn");
        m_disconnectBtn->setToolTip("Disconnect this guest");
        connect(m_disconnectBtn, &QPushButton::clicked, this, [this]() {
            m_mod->disconnectGuest(m_id);
        });
        btnRow->addWidget(m_disconnectBtn);

        auto* urlBtn = new QPushButton("Copy URL");
        urlBtn->setObjectName("PodRemoteCopyUrlBtn");
        urlBtn->setToolTip("Copy guest join URL to clipboard");
        connect(urlBtn, &QPushButton::clicked, this, [this]() {
            const QString url = m_mod->generateGuestUrl(m_id);
            QGuiApplication::clipboard()->setText(url);
        });
        btnRow->addWidget(urlBtn);

        auto* removeBtn = new QPushButton("X");
        removeBtn->setObjectName("PodRemoteRemoveBtn");
        removeBtn->setFixedWidth(28);
        removeBtn->setToolTip("Remove this guest");
        connect(removeBtn, &QPushButton::clicked, this, [this]() {
            m_mod->removeGuest(m_id);
        });
        btnRow->addWidget(removeBtn);

        lay->addLayout(btnRow);

        updateDisplay(guest);
    }

    void updateDisplay(const M1::RemoteGuest& guest) {
        m_nameLabel->setText(guest.name);
        m_led->setStatus(guest.status);
        m_levelBar->setLevel(guest.audioLevel);
        m_muteBtn->setChecked(guest.muted);
        m_muteBtn->setText(guest.muted ? "Unmute" : "Mute");

        switch (guest.status) {
        case M1::RemoteGuest::Status::Disconnected:
            m_statusLabel->setText("Disconnected");
            m_latencyLabel->setText("");
            m_admitBtn->setEnabled(false);
            m_disconnectBtn->setEnabled(false);
            break;
        case M1::RemoteGuest::Status::Waiting:
            m_statusLabel->setText("In Green Room");
            m_latencyLabel->setText(QString("%1 ms").arg(guest.latencyMs));
            m_admitBtn->setEnabled(true);
            m_disconnectBtn->setEnabled(true);
            break;
        case M1::RemoteGuest::Status::Connected:
            m_statusLabel->setText("Connected");
            m_latencyLabel->setText(QString("%1 ms").arg(guest.latencyMs));
            m_admitBtn->setEnabled(false);
            m_disconnectBtn->setEnabled(true);
            break;
        case M1::RemoteGuest::Status::Error:
            m_statusLabel->setText("Connection Error");
            m_latencyLabel->setText("");
            m_admitBtn->setEnabled(false);
            m_disconnectBtn->setEnabled(true);
            break;
        }
    }

    int guestId() const { return m_id; }

private:
    int m_id;
    M1::PodRemoteModule* m_mod;
    StatusLed*     m_led;
    QLabel*        m_nameLabel;
    QLabel*        m_statusLabel;
    QLabel*        m_latencyLabel;
    GuestLevelBar* m_levelBar;
    QPushButton*   m_muteBtn;
    QPushButton*   m_admitBtn;
    QPushButton*   m_disconnectBtn;
};

// ─── Main Remote Widget ────────────────────────────────────────────────
class PodRemoteWidget : public QWidget {
    Q_OBJECT
public:
    explicit PodRemoteWidget(M1::PodRemoteModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("PodRemoteWidget");
        m_rootLayout = new QVBoxLayout(this);
        m_rootLayout->setContentsMargins(8, 8, 8, 8);
        m_rootLayout->setSpacing(6);

        // Header
        auto* headerRow = new QHBoxLayout;
        auto* headerLabel = new QLabel("Remote Guests");
        headerLabel->setObjectName("PodRemoteHeader");
        QFont hf = headerLabel->font();
        hf.setPixelSize(16);
        hf.setBold(true);
        headerLabel->setFont(hf);
        headerRow->addWidget(headerLabel);

        m_countLabel = new QLabel;
        m_countLabel->setObjectName("PodRemoteCount");
        headerRow->addWidget(m_countLabel);

        headerRow->addStretch();

        m_addBtn = new QPushButton("+ Add Guest");
        m_addBtn->setObjectName("PodRemoteAddBtn");
        m_addBtn->setToolTip("Add a new remote guest (max 4)");
        connect(m_addBtn, &QPushButton::clicked, this, &PodRemoteWidget::onAddGuest);
        headerRow->addWidget(m_addBtn);

        m_rootLayout->addLayout(headerRow);

        // Guest cards container
        m_cardsLayout = new QVBoxLayout;
        m_cardsLayout->setSpacing(6);
        m_rootLayout->addLayout(m_cardsLayout);

        // Spacer
        m_rootLayout->addStretch();

        // v2 stub message
        m_stubLabel = new QLabel("WebRTC/VoIP integration available in v2 — "
                                 "guests will appear as Disconnected in v1");
        m_stubLabel->setObjectName("PodRemoteStubLabel");
        m_stubLabel->setWordWrap(true);
        QFont sf = m_stubLabel->font();
        sf.setPixelSize(11);
        sf.setItalic(true);
        m_stubLabel->setFont(sf);
        m_stubLabel->setAlignment(Qt::AlignCenter);
        m_rootLayout->addWidget(m_stubLabel);

        connect(mod, &M1::PodRemoteModule::guestsChanged, this, &PodRemoteWidget::rebuildCards);
        connect(mod, &M1::PodRemoteModule::guestStatusChanged, this, &PodRemoteWidget::rebuildCards);

        rebuildCards();
    }

private slots:
    void rebuildCards() {
        // Clear existing cards
        for (auto* card : m_cards)
            delete card;
        m_cards.clear();

        const auto guests = m_mod->guests();
        m_countLabel->setText(QString("(%1/%2)").arg(guests.size()).arg(m_mod->maxGuests()));
        m_addBtn->setEnabled(guests.size() < m_mod->maxGuests());

        for (const auto& guest : guests) {
            auto* card = new GuestCard(guest.id, guest, m_mod);
            m_cardsLayout->addWidget(card);
            m_cards.append(card);
        }
    }

    void onAddGuest() {
        if (m_mod->guests().size() >= m_mod->maxGuests()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, "Add Guest",
            "Guest name:", QLineEdit::Normal, QString(), &ok);
        if (ok && !name.trimmed().isEmpty())
            m_mod->addGuest(name.trimmed());
    }

private:
    M1::PodRemoteModule* m_mod;
    QVBoxLayout*       m_rootLayout;
    QVBoxLayout*       m_cardsLayout;
    QLabel*            m_countLabel;
    QLabel*            m_stubLabel;
    QPushButton*       m_addBtn;
    QList<GuestCard*>  m_cards;
};

} // anonymous namespace

#include "PodRemoteModule.moc"

namespace M1 {

PodRemoteModule::PodRemoteModule(QObject* parent) : IModule(parent) {}

void PodRemoteModule::initialize() {}
void PodRemoteModule::shutdown() {}

QWidget* PodRemoteModule::createWidget(QWidget* parent) {
    return new PodRemoteWidget(this, parent);
}

int PodRemoteModule::addGuest(const QString& name) {
    if (m_guests.size() >= kMaxGuests) return -1;
    RemoteGuest g;
    g.id     = m_nextId++;
    g.name   = name;
    g.status = RemoteGuest::Status::Disconnected;
    m_guests.append(g);
    emit guestsChanged();
    return g.id;
}

void PodRemoteModule::removeGuest(int id) {
    for (int i = 0; i < m_guests.size(); ++i) {
        if (m_guests[i].id == id) {
            m_guests.removeAt(i);
            emit guestsChanged();
            return;
        }
    }
}

void PodRemoteModule::setGuestMuted(int id, bool muted) {
    if (auto* g = findGuest(id)) {
        g->muted = muted;
        emit guestStatusChanged(id, static_cast<int>(g->status));
    }
}

void PodRemoteModule::admitGuest(int id) {
    if (auto* g = findGuest(id)) {
        if (g->status == RemoteGuest::Status::Waiting) {
            g->status = RemoteGuest::Status::Connected;
            emit guestStatusChanged(id, static_cast<int>(g->status));
        }
    }
}

void PodRemoteModule::disconnectGuest(int id) {
    if (auto* g = findGuest(id)) {
        g->status = RemoteGuest::Status::Disconnected;
        g->audioLevel = 0.0f;
        g->latencyMs = 0;
        emit guestStatusChanged(id, static_cast<int>(g->status));
    }
}

QString PodRemoteModule::generateGuestUrl(int id) const {
    // v1 stub — return placeholder URL
    if (const auto* g = findGuest(id))
        return QString("https://studio.mcaster1.com/join/%1?guest=%2")
            .arg(id)
            .arg(QString(g->name.toUtf8().toBase64(QByteArray::Base64UrlEncoding)));
    return {};
}

RemoteGuest* PodRemoteModule::findGuest(int id) {
    for (auto& g : m_guests)
        if (g.id == id) return &g;
    return nullptr;
}

const RemoteGuest* PodRemoteModule::findGuest(int id) const {
    for (const auto& g : m_guests)
        if (g.id == id) return &g;
    return nullptr;
}

void PodRemoteModule::saveState(QSettings& s) {
    s.setValue("guestCount", m_guests.size());
    s.setValue("nextId", m_nextId);
    for (int i = 0; i < m_guests.size(); ++i) {
        const QString prefix = QString("guest/%1/").arg(i);
        s.setValue(prefix + "id",     m_guests[i].id);
        s.setValue(prefix + "name",   m_guests[i].name);
        s.setValue(prefix + "muted",  m_guests[i].muted);
    }
}

void PodRemoteModule::loadState(QSettings& s) {
    m_guests.clear();
    m_nextId = s.value("nextId", 1).toInt();
    const int count = s.value("guestCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("guest/%1/").arg(i);
        RemoteGuest g;
        g.id     = s.value(prefix + "id", i + 1).toInt();
        g.name   = s.value(prefix + "name").toString();
        g.muted  = s.value(prefix + "muted", false).toBool();
        g.status = RemoteGuest::Status::Disconnected; // v1: always start disconnected
        m_guests.append(g);
    }
    emit guestsChanged();
}

} // namespace M1
