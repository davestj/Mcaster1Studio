#include "MetricsServer.h"
#include "HealthModule.h"
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

MetricsServer::MetricsServer(M1::HealthModule* health, QObject* parent)
    : QTcpServer(parent)
    , m_health(health)
{
}

bool MetricsServer::startListening(quint16 port) {
    m_port = port;
    if (isListening()) close();
    if (!listen(QHostAddress::Any, port)) {
        qWarning() << "[MetricsServer] Failed to listen on port" << port
                   << ":" << errorString();
        return false;
    }
    qInfo() << "[MetricsServer] Listening on port" << port;
    return true;
}

void MetricsServer::stopListening() {
    if (isListening()) {
        close();
        qInfo() << "[MetricsServer] Stopped.";
    }
}

void MetricsServer::incomingConnection(qintptr socketDescriptor) {
    auto* socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray request = socket->readAll();

        // Parse HTTP request line
        const int lineEnd = request.indexOf("\r\n");
        const QByteArray requestLine = (lineEnd > 0) ? request.left(lineEnd) : request;

        // Check for GET /metrics
        if (requestLine.startsWith("GET /metrics")) {
            const QByteArray body = buildMetricsBody();
            QByteArray response;
            response += "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n";
            response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += body;
            socket->write(response);
        } else {
            QByteArray response;
            response += "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Type: text/plain\r\n";
            response += "Content-Length: 9\r\n";
            response += "Connection: close\r\n\r\n";
            response += "Not Found";
            socket->write(response);
        }

        socket->flush();
        socket->disconnectFromHost();
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);

    // Timeout protection: close if no data within 5s
    QTimer::singleShot(5000, socket, [socket]() {
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->disconnectFromHost();
        }
    });
}

QByteArray MetricsServer::buildMetricsBody() const {
    if (!m_health) return "# No health data available\n";

    const auto& snap = m_health->snapshot();

    QByteArray body;

    body += "# HELP mcaster1_cpu_percent Process CPU usage percentage\n";
    body += "# TYPE mcaster1_cpu_percent gauge\n";
    body += "mcaster1_cpu_percent " + QByteArray::number(snap.cpuPercent, 'f', 2) + "\n\n";

    body += "# HELP mcaster1_memory_bytes Process working set size in bytes\n";
    body += "# TYPE mcaster1_memory_bytes gauge\n";
    body += "mcaster1_memory_bytes " + QByteArray::number(snap.memoryBytes) + "\n\n";

    body += "# HELP mcaster1_memory_peak_bytes Process peak working set size in bytes\n";
    body += "# TYPE mcaster1_memory_peak_bytes gauge\n";
    body += "mcaster1_memory_peak_bytes " + QByteArray::number(snap.peakMemory) + "\n\n";

    body += "# HELP mcaster1_encoder_slots_total Total encoder slots configured\n";
    body += "# TYPE mcaster1_encoder_slots_total gauge\n";
    body += "mcaster1_encoder_slots_total " + QByteArray::number(snap.encoderTotal) + "\n\n";

    body += "# HELP mcaster1_encoder_slots_live Currently streaming encoder slots\n";
    body += "# TYPE mcaster1_encoder_slots_live gauge\n";
    body += "mcaster1_encoder_slots_live " + QByteArray::number(snap.encoderLive) + "\n\n";

    body += "# HELP mcaster1_encoder_slots_idle Idle (non-streaming) encoder slots\n";
    body += "# TYPE mcaster1_encoder_slots_idle gauge\n";
    body += "mcaster1_encoder_slots_idle " + QByteArray::number(snap.encoderTotal - snap.encoderLive) + "\n\n";

    body += "# HELP mcaster1_deck_playing Deck playback status (1=playing, 0=stopped)\n";
    body += "# TYPE mcaster1_deck_playing gauge\n";
    body += "mcaster1_deck_playing{deck=\"A\"} " + QByteArray::number(snap.deckAPlaying ? 1 : 0) + "\n";
    body += "mcaster1_deck_playing{deck=\"B\"} " + QByteArray::number(snap.deckBPlaying ? 1 : 0) + "\n\n";

    return body;
}
