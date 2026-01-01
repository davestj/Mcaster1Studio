#include "SurfaceConfigYaml.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

// ─── save ─────────────────────────────────────────────────────────────────────
bool SurfaceConfigYaml::save(const QString& filePath,
                              const QMap<QString, SurfaceLayoutSnapshot>& data)
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[SurfaceConfigYaml] Cannot write:" << filePath;
        return false;
    }

    QTextStream s(&f);
    s << "# Mcaster1Studio Layout Configuration\n";
    s << "# Auto-generated on exit. Do not edit while the application is running.\n";
    s << "version: 1\n";
    s << "surfaces:\n";

    for (auto it = data.cbegin(); it != data.cend(); ++it) {
        const QString&               name = it.key();
        const SurfaceLayoutSnapshot& snap = it.value();

        s << "  " << name << ":\n";

        // Column widths
        QStringList wParts;
        for (int w : snap.columnWidths) wParts << QString::number(w);
        s << "    column_widths: " << (wParts.isEmpty() ? "0" : wParts.join(',')) << "\n";

        // Per-column row heights
        for (int ci = 0; ci < snap.columnHeights.size(); ++ci) {
            QStringList hParts;
            for (int h : snap.columnHeights[ci]) hParts << QString::number(h);
            s << "    column_" << ci << "_heights: "
              << (hParts.isEmpty() ? "0" : hParts.join(',')) << "\n";
        }

        // Floating docks
        s << "    floating_count: " << snap.floatingItems.size() << "\n";
        for (int fi = 0; fi < snap.floatingItems.size(); ++fi) {
            const auto& item = snap.floatingItems[fi];
            const QString pfx = QString("    floating_%1_").arg(fi);
            s << pfx << "module_id: " << item.moduleId << "\n";
            s << pfx << "x: "         << item.x        << "\n";
            s << pfx << "y: "         << item.y        << "\n";
            s << pfx << "width: "     << item.w        << "\n";
            s << pfx << "height: "    << item.h        << "\n";
        }
    }

    qInfo() << "[SurfaceConfigYaml] Saved layout to" << filePath;
    return true;
}

// ─── load ─────────────────────────────────────────────────────────────────────
QMap<QString, SurfaceLayoutSnapshot> SurfaceConfigYaml::load(const QString& filePath)
{
    QMap<QString, SurfaceLayoutSnapshot> result;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qInfo() << "[SurfaceConfigYaml] No layout file at" << filePath << "— using defaults";
        return result;
    }

    QTextStream in(&f);
    QString currentSurface;
    bool    inSurfaces = false;

    while (!in.atEnd()) {
        QString line = in.readLine();

        // Strip inline comments
        const int commentIdx = line.indexOf('#');
        if (commentIdx >= 0) line = line.left(commentIdx);
        if (line.trimmed().isEmpty()) continue;

        const int     indent  = line.length() - line.trimmed().length();
        const QString content = line.trimmed();

        if (indent == 0) {
            inSurfaces    = (content == "surfaces:");
            currentSurface.clear();
            continue;
        }

        if (indent == 2 && inSurfaces) {
            // Surface name line: "  Alpha:"
            if (content.endsWith(':')) {
                currentSurface = content.chopped(1);
                result[currentSurface]; // ensure entry exists
            }
            continue;
        }

        if (indent == 4 && !currentSurface.isEmpty()) {
            const int colonIdx = content.indexOf(':');
            if (colonIdx < 0) continue;
            const QString key = content.left(colonIdx).trimmed();
            const QString val = content.mid(colonIdx + 1).trimmed();

            auto& snap = result[currentSurface];

            // ── column_widths ──────────────────────────────────────────────
            if (key == "column_widths") {
                snap.columnWidths.clear();
                for (const QString& p : val.split(',', Qt::SkipEmptyParts)) {
                    bool ok; const int v = p.trimmed().toInt(&ok);
                    if (ok) snap.columnWidths << v;
                }
                continue;
            }

            // ── column_N_heights ───────────────────────────────────────────
            if (key.startsWith("column_") && key.endsWith("_heights")) {
                // key = "column_N_heights" → extract N
                const QString mid = key.mid(7, key.length() - 7 - 8);
                bool ok; const int ci = mid.toInt(&ok);
                if (!ok) continue;
                while (snap.columnHeights.size() <= ci)
                    snap.columnHeights << QList<int>();
                snap.columnHeights[ci].clear();
                for (const QString& p : val.split(',', Qt::SkipEmptyParts)) {
                    bool ok2; const int v = p.trimmed().toInt(&ok2);
                    if (ok2) snap.columnHeights[ci] << v;
                }
                continue;
            }

            // ── floating_count ─────────────────────────────────────────────
            if (key == "floating_count") {
                bool ok; const int n = val.toInt(&ok);
                if (ok) {
                    while (snap.floatingItems.size() < n)
                        snap.floatingItems << SurfaceLayoutSnapshot::FloatingItem{};
                }
                continue;
            }

            // ── floating_N_field ───────────────────────────────────────────
            if (key.startsWith("floating_")) {
                const QString rest = key.mid(9); // strip "floating_"
                const int underIdx = rest.indexOf('_');
                if (underIdx < 0) continue;
                bool ok; const int fi = rest.left(underIdx).toInt(&ok);
                if (!ok) continue;
                const QString field = rest.mid(underIdx + 1);

                while (snap.floatingItems.size() <= fi)
                    snap.floatingItems << SurfaceLayoutSnapshot::FloatingItem{};
                auto& item = snap.floatingItems[fi];

                if      (field == "module_id") item.moduleId = val;
                else if (field == "x")         { bool ok2; item.x = val.toInt(&ok2); }
                else if (field == "y")         { bool ok2; item.y = val.toInt(&ok2); }
                else if (field == "width")     { bool ok2; item.w = val.toInt(&ok2); }
                else if (field == "height")    { bool ok2; item.h = val.toInt(&ok2); }
            }
        }
    }

    qInfo() << "[SurfaceConfigYaml] Loaded layout for"
            << result.size() << "surface(s) from" << filePath;
    return result;
}
