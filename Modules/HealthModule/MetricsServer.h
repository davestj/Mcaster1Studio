#pragma once
#include <QTcpServer>

namespace M1 {
class HealthModule;
}

/// MetricsServer — Prometheus-compatible HTTP endpoint for system health metrics.
///
/// Listens on a configurable port (default 9100) and responds to GET /metrics
/// with Prometheus text exposition format. All other paths return 404.
///
/// Usage:
///   auto* server = new MetricsServer(healthModule, this);
///   server->startListening(9100);
class MetricsServer : public QTcpServer {
    Q_OBJECT

public:
    explicit MetricsServer(M1::HealthModule* health, QObject* parent = nullptr);

    bool startListening(quint16 port = 9100);
    void stopListening();
    quint16 listenPort() const { return m_port; }

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QByteArray buildMetricsBody() const;

    M1::HealthModule* m_health = nullptr;
    quint16 m_port = 9100;
};
