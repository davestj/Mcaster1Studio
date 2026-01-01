#pragma once
#include <QWidget>
#include <QString>
#include "ISurface.h"

class QPushButton;
class QLabel;
class QHBoxLayout;

/// SurfaceRibbon — thin horizontal strip at the top of each surface.
///
/// Layout:  [Surface icon] [Surface name]  [quick actions...]  [widget slot]
///
/// Each surface has its own dedicated ribbon. Small module widgets (e.g. a
/// ClockModule in compact mode) can be embedded in the widget slot on the right.
class SurfaceRibbon : public QWidget {
    Q_OBJECT

public:
    explicit SurfaceRibbon(M1::SurfaceType type,
                           const QString& name,
                           QWidget* parent = nullptr);

    void setSurfaceName(const QString& name);

    /// Embed a compact widget in the right-hand widget slot (e.g. ClockModule).
    void setRightWidget(QWidget* w);

    /// Add a quick-action button to the ribbon (surface-specific).
    QPushButton* addAction(const QString& text,
                           const QString& iconPath = QString(),
                           const QString& tooltip  = QString());

signals:
    void onAirToggled(bool onAir);

private:
    void buildUi(M1::SurfaceType type);

    QLabel*      m_nameLabel    = nullptr;
    QHBoxLayout* m_actionsLayout = nullptr;
    QWidget*     m_rightSlot    = nullptr;
    QPushButton* m_onAirBtn     = nullptr;
};
