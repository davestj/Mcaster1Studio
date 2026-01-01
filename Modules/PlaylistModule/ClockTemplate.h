#pragma once
#include <QString>
#include <QList>
#include <QSettings>

/// ClockSlot — one time slot within a broadcast clock hour structure.
struct ClockSlot {
    QString cartNumber;     ///< Cart identifier, e.g. "MUS001", "JNG002"
    QString categoryFilter; ///< Genre / category filter for AutoDJ selection
    int     durationSecs = 0; ///< Target duration in seconds
    bool    exact        = false; ///< true = must match exactly; false = approximate

    void        save(QSettings& s, const QString& prefix) const;
    static ClockSlot load(QSettings& s, const QString& prefix);
};

/// ClockTemplate — a named broadcast clock defining one hour's structure.
///
/// Each slot describes what type of content (category, cart) should air
/// and its target duration. The AutoDJ uses this to select tracks that
/// fit each slot when building the next hour's playlist.
///
/// Usage:
/// @code
///     ClockTemplate tpl;
///     tpl.name = "Morning Drive";
///     tpl.slots.append({"MUS001", "Hot AC", 210, false});
///     tpl.slots.append({"JNG001", "Jingles", 15, true});
///     QSettings s("mcaster1.ini", QSettings::IniFormat);
///     tpl.save(s);
/// @endcode
struct ClockTemplate {
    QString          name;
    QList<ClockSlot> clockSlots; ///< Ordered hour structure

    void              save(QSettings& s) const;
    static ClockTemplate load(QSettings& s);

    /// Total programmed duration across all slots, in seconds.
    int totalDurationSecs() const;
};
