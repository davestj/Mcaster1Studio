#pragma once
#include "IModule.h"
#include <QList>

namespace M1 {

class CartPlayer;
class CartWallWidget;

/// CartWallModule — instant-play audio cart grid for jingles, drops, SFX.
///
/// Each cart holds a pre-decoded audio file that plays instantly on click.
/// Multiple carts can play simultaneously; all audio is summed into the
/// main output bus in onAudioBlock() (RT-safe — no allocation, atomic positions).
///
/// This is a SINGLETON module: one cart wall shared across all surfaces.
/// It participates in the RT audio callback (after decks, before effects).
class CartWallModule : public IModule {
    Q_OBJECT

public:
    explicit CartWallModule(QObject* parent = nullptr);
    ~CartWallModule() override;

    // ── IModule interface ────────────────────────────────────────────
    QString moduleId()      const override { return "com.mcaster1.cartwall"; }
    QString displayName()   const override { return "Cart Wall"; }
    QSize   preferredSize() const override { return {480, 360}; }
    QSize   minimumModuleSize() const override { return {300, 200}; }

    void initialize() override;
    void shutdown()   override;

    QWidget* createWidget(QWidget* parent) override;

    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Public API ───────────────────────────────────────────────────
    int          cartCount() const { return m_players.size(); }
    CartPlayer*  cartPlayer(int index) const;

    /// Stop all currently playing carts.
    void stopAll();

private:
    QList<CartPlayer*>  m_players;
    CartWallWidget*     m_widget = nullptr;
};

} // namespace M1
