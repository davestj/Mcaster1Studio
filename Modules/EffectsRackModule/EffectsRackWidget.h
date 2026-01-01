#pragma once
#include <QWidget>
#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>

class QPushButton;
class QMenu;

namespace M1 { class IEffectUnit; }
namespace M1 { class EffectsRackModule; }

/// EffectsRackWidget — vertical rack-mount UI for the EffectsRackModule.
///
/// Modern design with toolbar (Add Effect, Bypass All, Reset All) and
/// card-style slot panels for each effect unit.
class EffectsRackWidget : public QWidget {
    Q_OBJECT

public:
    explicit EffectsRackWidget(M1::EffectsRackModule* module,
                                QWidget* parent = nullptr);
    ~EffectsRackWidget() override = default;

    /// Add an IEffectUnit: registers it with the module AND rebuilds the display.
    void addUnit(M1::IEffectUnit* unit);

    /// Remove the unit at slotIndex from the module AND rebuilds the display.
    void removeUnit(int slotIndex);

    /// Rebuild the slot display from the current module state (no module mutation).
    void refresh();

private:
    void rebuildSlots();
    void applyTheme();
    void buildToolbar();
    void populateAddMenu();

    M1::EffectsRackModule* m_module = nullptr;
    QWidget*               m_toolbar = nullptr;
    QPushButton*           m_addBtn = nullptr;
    QPushButton*           m_bypassAllBtn = nullptr;
    QMenu*                 m_addMenu = nullptr;
    QScrollArea*           m_scroll = nullptr;
    QWidget*               m_container = nullptr;
    QVBoxLayout*           m_layout = nullptr;
};
