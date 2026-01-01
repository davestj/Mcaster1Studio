#include "ISurface.h"
#include "IModule.h"
#include <QList>
#include <QDebug>

namespace M1 {

/// Phase 1 stub — surface manager will be expanded in Phase 2.
/// Tracks which surfaces are open and which modules are loaded per surface.

struct SurfaceState {
    SurfaceType       type;
    QString           customName;
    QList<IModule*>   modules;
};

static QList<SurfaceState> s_surfaces;

void addSurface(SurfaceType type, const QString& customName = {}) {
    SurfaceState st;
    st.type       = type;
    st.customName = customName.isEmpty() ? surfaceTypeName(type) : customName;
    s_surfaces.append(st);
    qInfo() << "[SurfaceManager] Added surface:" << st.customName;
}

void removeSurface(int index) {
    if (index < 0 || index >= s_surfaces.size()) return;
    qInfo() << "[SurfaceManager] Removing surface:" << s_surfaces[index].customName;
    s_surfaces.removeAt(index);
}

int surfaceCount() { return s_surfaces.size(); }

QString surfaceName(int index) {
    if (index < 0 || index >= s_surfaces.size()) return {};
    const auto& st = s_surfaces[index];
    return st.customName.isEmpty() ? surfaceTypeName(st.type) : st.customName;
}

void shutdownSurfaces() {
    for (auto& st : s_surfaces) {
        for (auto* m : st.modules) {
            m->shutdown();
        }
        qDeleteAll(st.modules);
        st.modules.clear();
    }
    s_surfaces.clear();
}

} // namespace M1
