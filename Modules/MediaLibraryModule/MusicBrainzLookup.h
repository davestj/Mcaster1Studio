#pragma once
#include "MediaItem.h"
#include <QObject>
#include <QNetworkAccessManager>

namespace M1 {

/// MusicBrainzLookup — performs async MusicBrainz recording lookups.
///
/// Uses the MusicBrainz JSON API v2: https://musicbrainz.org/ws/2/
/// Requires User-Agent header per MB API policy.
///
/// Usage:
///   lookup(item)  →  [async]  →  lookupComplete(enriched) | lookupFailed(reason)
class MusicBrainzLookup : public QObject {
    Q_OBJECT

public:
    explicit MusicBrainzLookup(QObject* parent = nullptr);

    void lookup(const MediaItem& item);
    bool isBusy() const { return m_pending > 0; }

signals:
    void lookupComplete(const M1::MediaItem& enriched);
    void lookupFailed(const QString& reason);

private slots:
    void onReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_nam     = nullptr;
    MediaItem              m_pending_item;
    int                    m_pending = 0;
};

} // namespace M1
