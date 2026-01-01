#include "ClockTemplate.h"
#include <algorithm>

// =============================================================================
// ClockSlot
// =============================================================================

void ClockSlot::save(QSettings& s, const QString& prefix) const
{
    s.setValue(prefix + "/cartNumber",     cartNumber);
    s.setValue(prefix + "/categoryFilter", categoryFilter);
    s.setValue(prefix + "/durationSecs",   durationSecs);
    s.setValue(prefix + "/exact",          exact);
}

ClockSlot ClockSlot::load(QSettings& s, const QString& prefix)
{
    ClockSlot slot;
    slot.cartNumber     = s.value(prefix + "/cartNumber").toString();
    slot.categoryFilter = s.value(prefix + "/categoryFilter").toString();
    slot.durationSecs   = s.value(prefix + "/durationSecs", 0).toInt();
    slot.exact          = s.value(prefix + "/exact", false).toBool();
    return slot;
}

// =============================================================================
// ClockTemplate
// =============================================================================

void ClockTemplate::save(QSettings& s) const
{
    s.beginGroup("ClockTemplate_" + name);
    s.setValue("name",      name);
    s.setValue("slotCount", clockSlots.size());

    for (int i = 0; i < clockSlots.size(); ++i)
        clockSlots[i].save(s, QString("slot_%1").arg(i));

    s.endGroup();
}

ClockTemplate ClockTemplate::load(QSettings& s)
{
    ClockTemplate tpl;
    tpl.name = s.value("name").toString();

    const int count = s.value("slotCount", 0).toInt();
    tpl.clockSlots.reserve(count);

    for (int i = 0; i < count; ++i)
        tpl.clockSlots.append(ClockSlot::load(s, QString("slot_%1").arg(i)));

    return tpl;
}

int ClockTemplate::totalDurationSecs() const
{
    int total = 0;
    for (const ClockSlot& slot : clockSlots)
        total += slot.durationSecs;
    return total;
}
