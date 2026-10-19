#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

#include "network.hpp"
#include "models/video.hpp"

/**
 * Looks videos up on javlibrary.com. Its pages sit behind Cloudflare, so they
 * are downloaded through FlareSolverr (NetworkManager), which also sets the
 * pace: lookups are queued and answered one every START_INTERVAL at most.
 * Its text is English, its images are DMM's.
 */
class JavLibClient : public QObject {
    Q_OBJECT

public:
    JavLibClient(NetworkManager &flareSolverr, QObject *parent = nullptr);

    /**
     * Queue a search for `id`, then emit found() or notFound() with `id` as
     * given here. If the search gives a list, the match for `id` is followed
     * (one more download). Unlike DmmClient, a found video may carry another id than
     * the one searched for, if javlibrary redirected the search to it.
     */
    void lookup(const model::JavId &id);

    /**
     * javlibrary.com search page for an id. This is not the video page itself:
     * a single match redirects to it, several (or no) matches give a list.
     */
    static QUrl searchUrl(const model::JavId &id);

    /**
     * Parse a FlareSolverr JSON reply wrapping a javlibrary video page
     * (solution.response) into a model::Video.
     * Returns a default Video (empty id) if the payload isn't such a reply.
     */
    static model::Video parse(const QString &payload);

    /**
     * Video page of the match for `id` on a FlareSolverr reply wrapping a
     * search result list, resolved against `searchUrl`; invalid if `id`
     * isn't listed. The regular edition is preferred over a Blu-ray one.
     */
    static QUrl resultUrl(const QString &payload, const model::JavId &id, const QUrl &searchUrl);

signals:
    void found(const model::JavId &id, const model::Video &video);
    void notFound(const model::JavId &id);

private:
    void downloadReady(const QUrl &url, const QString &payload);
    void downloadFailed(const QUrl &url);

    NetworkManager &m_flareSolverr;
    QHash<QUrl, model::JavId> m_pending;  /* by search or followed video page url */
};
