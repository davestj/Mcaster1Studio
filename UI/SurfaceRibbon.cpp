#include "SurfaceRibbon.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QSvgRenderer>
#include <QPixmap>
#include <QPainter>

static QIcon ribbonSvgIcon(const QString& path, int sz = 16) {
    QSvgRenderer r(path);
    if (!r.isValid()) return QIcon();
    QPixmap pm(sz * 2, sz * 2);
    pm.fill(Qt::transparent);
    { QPainter p(&pm); r.render(&p); }
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

SurfaceRibbon::SurfaceRibbon(M1::SurfaceType type,
                              const QString& name,
                              QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SurfaceRibbon");
    setFixedHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    buildUi(type);
    setSurfaceName(name);
}

void SurfaceRibbon::buildUi(M1::SurfaceType type) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 2, 8, 2);
    root->setSpacing(4);

    // ── Surface icon + name ───────────────────────────────────────
    auto* iconLabel = new QLabel(this);
    iconLabel->setObjectName("RibbonSurfaceIcon");
    iconLabel->setFixedSize(18, 18);
    // Icon path based on surface type
    const QString iconPath = [type]() -> QString {
        using T = M1::SurfaceType;
        switch (type) {
        case T::Alpha:         return ":/resources/icons/surface-alpha.svg";
        case T::Beta:          return ":/resources/icons/surface-beta.svg";
        case T::DJ:            return ":/resources/icons/surface-dj.svg";
        case T::Company:       return ":/resources/icons/surface-company.svg";
        case T::Entertainment: return ":/resources/icons/surface-entertainment.svg";
        case T::Social:        return ":/resources/icons/surface-social.svg";
        case T::Podcast:       return ":/resources/icons/surface-podcast.svg";
        case T::Church:        return ":/resources/icons/surface-church.svg";
        default:               return ":/resources/icons/surface-custom.svg";
        }
    }();
    {
        QSvgRenderer r(iconPath);
        if (r.isValid()) {
            QPixmap pm(36, 36);
            pm.fill(Qt::transparent);
            { QPainter p(&pm); r.render(&p); }
            pm.setDevicePixelRatio(2.0);
            iconLabel->setPixmap(pm);
        }
    }

    m_nameLabel = new QLabel(this);
    m_nameLabel->setObjectName("RibbonSurfaceName");

    root->addWidget(iconLabel);
    root->addWidget(m_nameLabel);

    // ── Separator ─────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setObjectName("RibbonSeparator");
    root->addWidget(sep);

    // ── Quick actions layout ──────────────────────────────────────
    auto* actionsContainer = new QWidget(this);
    actionsContainer->setObjectName("RibbonActionsArea");
    m_actionsLayout = new QHBoxLayout(actionsContainer);
    m_actionsLayout->setContentsMargins(0, 0, 0, 0);
    m_actionsLayout->setSpacing(3);
    root->addWidget(actionsContainer);

    // On-Air toggle (all surfaces)
    m_onAirBtn = new QPushButton("● ON AIR", this);
    m_onAirBtn->setObjectName("RibbonOnAirBtn");
    m_onAirBtn->setCheckable(true);
    m_onAirBtn->setCursor(Qt::PointingHandCursor);
    m_onAirBtn->setToolTip("Toggle On-Air indicator");
    m_onAirBtn->setFixedHeight(22);
    connect(m_onAirBtn, &QPushButton::toggled, this, &SurfaceRibbon::onAirToggled);
    m_actionsLayout->addWidget(m_onAirBtn);

    root->addStretch(1);

    // ── Right widget slot ──────────────────────────────────────────
    m_rightSlot = new QWidget(this);
    m_rightSlot->setObjectName("RibbonRightSlot");
    m_rightSlot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* slotLayout = new QHBoxLayout(m_rightSlot);
    slotLayout->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_rightSlot);
}

void SurfaceRibbon::setSurfaceName(const QString& name) {
    if (m_nameLabel) m_nameLabel->setText(name);
}

void SurfaceRibbon::setRightWidget(QWidget* w) {
    if (!m_rightSlot || !w) return;
    auto* layout = qobject_cast<QHBoxLayout*>(m_rightSlot->layout());
    if (layout) {
        // Clear old widget
        while (QLayoutItem* item = layout->takeAt(0)) {
            if (item->widget()) item->widget()->setParent(nullptr);
            delete item;
        }
        w->setParent(m_rightSlot);
        layout->addWidget(w);
    }
}

QPushButton* SurfaceRibbon::addAction(const QString& text,
                                       const QString& iconPath,
                                       const QString& tooltip) {
    auto* btn = new QPushButton(text, this);
    btn->setObjectName("RibbonActionBtn");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(22);
    if (!iconPath.isEmpty())
        btn->setIcon(ribbonSvgIcon(iconPath, 14));
    if (!tooltip.isEmpty())
        btn->setToolTip(tooltip);
    m_actionsLayout->addWidget(btn);
    return btn;
}
