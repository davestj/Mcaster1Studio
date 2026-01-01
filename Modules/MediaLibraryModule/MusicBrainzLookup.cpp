#include "MusicBrainzLookup.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

namespace M1 {

MusicBrainzLookup::MusicBrainzLookup(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &MusicBrainzLookup::onReply);
}

void MusicBrainzLookup::lookup(const MediaItem& item) {
    if (item.title.isEmpty() && item.artist.isEmpty()) {
        emit lookupFailed("Title and artist are both empty");
        return;
    }

    m_pending_item = item;
    ++m_pending;

    // Lucene query for MusicBrainz recording search
    QStringList parts;
    if (!item.title.isEmpty())
        parts << QString("recording:\"%1\"").arg(item.title);
    if (!item.artist.isEmpty())
        parts << QString("artist:\"%1\"").arg(item.artist);

    QUrl url("https://musicbrainz.org/ws/2/recording/");
    QUrlQuery query;
    query.addQueryItem("query", parts.join(" AND "));
    query.addQueryItem("limit", "5");
    query.addQueryItem("fmt",   "json");
    url.setQuery(query);

    QNetworkRequest req(url);
    // MusicBrainz requires a User-Agent identifying the application
    req.setRawHeader("User-Agent",
        "Mcaster1Studio/1.0.0 ( contact@mcaster1.com )");
    req.setRawHeader("Accept", "application/json");

    m_nam->get(req);
    qInfo() << "[MusicBrainz] Lookup:" << parts.join(" AND ");
}

void MusicBrainzLookup::onReply(QNetworkReply* reply) {
    reply->deleteLater();
    --m_pending;

    if (reply->error() != QNetworkReply::NoError) {
        emit lookupFailed(reply->errorString());
        qWarning() << "[MusicBrainz] Network error:" << reply->errorString();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray recordings = doc.object().value("recordings").toArray();

    if (recordings.isEmpty()) {
        emit lookupFailed("No recordings found");
        return;
    }

    // Use the first (best-scored) result
    const QJsonObject rec = recordings.first().toObject();
    MediaItem enriched = m_pending_item;

    enriched.mbid  = rec.value("id").toString();
    enriched.title = rec.value("title").toString();

    // Artist credit → artist name
    const QJsonArray credits = rec.value("artist-credit").toArray();
    if (!credits.isEmpty()) {
        const QJsonObject credit = credits.first().toObject();
        const QJsonObject artist = credit.value("artist").toObject();
        enriched.artist = artist.value("name").toString();
    }

    // First release → album, year, track number
    const QJsonArray releases = rec.value("releases").toArray();
    if (!releases.isEmpty()) {
        const QJsonObject rel = releases.first().toObject();
        enriched.album = rel.value("title").toString();
        enriched.year  = rel.value("date").toString().left(4);

        // Track number within this release
        const QJsonArray mediaArr = rel.value("media").toArray();
        if (!mediaArr.isEmpty()) {
            const QJsonArray tracks = mediaArr.first().toObject()
                                              .value("track").toArray();
            if (!tracks.isEmpty()) {
                const int num = tracks.first().toObject()
                                              .value("number").toString().toInt();
                if (num > 0) enriched.trackNumber = QString::number(num);
            }
        }
    }

    emit lookupComplete(enriched);
    qInfo() << "[MusicBrainz] Found MBID" << enriched.mbid
            << "for" << enriched.artist << "-" << enriched.title;
}

} // namespace M1
