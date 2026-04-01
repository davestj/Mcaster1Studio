#include "DatabaseModule.h"
#include "DatabaseWidget.h"
#include <QDebug>

namespace M1 {

DatabaseModule::DatabaseModule(QObject* parent)
    : IModule(parent)
{
    qInfo() << "[DatabaseModule] Created.";
}

DatabaseModule::~DatabaseModule() = default;

QWidget* DatabaseModule::createWidget(QWidget* parent) {
    if (!m_widget) {
        m_widget = new DatabaseWidget(this, parent);
    }
    return m_widget;
}

void DatabaseModule::saveState(QSettings& s) {
    s.setValue("dbServerId",  m_dbCtx.serverId);
    s.setValue("dbSchema",    m_dbCtx.schemaName);
    s.setValue("dbPrefix",    m_dbCtx.tablePrefix);
}

void DatabaseModule::loadState(QSettings& s) {
    m_dbCtx.serverId    = s.value("dbServerId").toString();
    m_dbCtx.schemaName  = s.value("dbSchema").toString();
    m_dbCtx.tablePrefix = s.value("dbPrefix").toString();
    if (m_widget)
        m_widget->refreshConnectionInfo();
}

void DatabaseModule::setSurfaceDbContext(const SurfaceDbContext& ctx) {
    m_dbCtx = ctx;
    qInfo() << "[DatabaseModule] DB context set — server:" << ctx.serverId
            << "schema:" << ctx.schemaName;
    if (m_widget)
        m_widget->refreshConnectionInfo();
    emit connectionStateChanged(ctx.isValid());
}

} // namespace M1
