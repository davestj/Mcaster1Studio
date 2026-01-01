#include "CartWallModule.h"
#include "CartWallWidget.h"
#include "CartPlayer.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace M1 {

CartWallModule::CartWallModule(QObject* parent)
    : IModule(parent)
{
    // Pre-create initial cart players (widget may create more as grid grows)
    for (int i = 0; i < CartWallWidget::kDefaultRows * CartWallWidget::kDefaultCols; ++i)
        m_players.append(new CartPlayer(this));
}

CartWallModule::~CartWallModule() {
    // Players are QObject children — auto-deleted
}

void CartWallModule::initialize() {
    qInfo() << "[CartWallModule] Initialized.";
}

void CartWallModule::shutdown() {
    stopAll();
    qInfo() << "[CartWallModule] Shutdown.";
}

QWidget* CartWallModule::createWidget(QWidget* parent) {
    m_widget = new CartWallWidget(m_players, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    return m_widget;
}

// ── RT audio processing ──────────────────────────────────────────────────────

void CartWallModule::onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& out) {
    if (!out.isValid || out.frames == 0) return;

    // Sum all playing carts into the output buffer (additive — don't silence first)
    for (auto* player : m_players) {
        if (player->isPlaying()) {
            player->processBlock(out.data, out.frames, out.channels);
        }
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

CartPlayer* CartWallModule::cartPlayer(int index) const {
    if (index < 0 || index >= m_players.size()) return nullptr;
    return m_players[index];
}

void CartWallModule::stopAll() {
    for (auto* player : m_players) {
        if (player->isPlaying()) player->stop();
    }
}

// ── Persistence ──────────────────────────────────────────────────────────────

void CartWallModule::saveState(QSettings& s) {
    s.beginGroup("CartWallModule");
    if (m_widget)
        m_widget->saveState(s);
    s.endGroup();
}

void CartWallModule::loadState(QSettings& s) {
    s.beginGroup("CartWallModule");
    if (m_widget)
        m_widget->loadState(s);
    s.endGroup();
}

} // namespace M1
