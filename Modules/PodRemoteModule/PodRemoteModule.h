#pragma once
/// @file   PodRemoteModule.h
/// @path   Modules/PodRemoteModule/PodRemoteModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodRemote — Remote Guest Connection Module
/// @purpose Manages remote podcast guest connections: up to 4 guests with
///          name, status, audio level, mute, and green room staging.
///          v1 provides full UI and data model; WebRTC/VoIP is stubbed for v2.
/// @reason  Modern podcasting requires remote guest participation with
///          per-guest audio control and waiting room functionality.
/// @changelog
///   2026-03-09  Initial implementation — guest cards, status, green room stub

#include "IModule.h"
#include <QList>
#include <QString>

namespace M1 {

struct RemoteGuest {
    int     id          = 0;
    QString name;
    enum class Status { Disconnected, Waiting, Connected, Error };
    Status  status      = Status::Disconnected;
    int     latencyMs   = 0;
    float   audioLevel  = 0.0f;  // 0.0 – 1.0
    bool    muted       = false;
};

class PodRemoteModule : public IModule {
    Q_OBJECT

public:
    explicit PodRemoteModule(QObject* parent = nullptr);
    ~PodRemoteModule() override = default;

    QString moduleId()    const override { return "com.mcaster1.podcast.remote"; }
    QString displayName() const override { return "Remote Guests"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {500, 400}; }
    QSize minimumModuleSize() const override { return {350, 300}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Guest management
    int  addGuest(const QString& name);
    void removeGuest(int id);
    QList<RemoteGuest> guests() const { return m_guests; }

    // Guest control
    void setGuestMuted(int id, bool muted);
    void admitGuest(int id);
    void disconnectGuest(int id);
    QString generateGuestUrl(int id) const;
    int  maxGuests() const { return kMaxGuests; }

signals:
    void guestsChanged();
    void guestStatusChanged(int id, int newStatus);

private:
    static constexpr int kMaxGuests = 4;
    QList<RemoteGuest> m_guests;
    int m_nextId = 1;

    RemoteGuest* findGuest(int id);
    const RemoteGuest* findGuest(int id) const;
};

} // namespace M1
