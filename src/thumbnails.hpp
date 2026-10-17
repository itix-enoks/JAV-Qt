#pragma once

#include <memory>

#include <QDir>
#include <QObject>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include "models/video.hpp"

/**
 * Downloads every available size of a video's jacket, preview stills and cast
 * pictures into <paths::dataDir()>/assets/thumbnails/<video id>/.
 * Images are fetched straight from DMM's CDN, which has no Cloudflare
 * protection, so FlareSolverr is not involved.
 */
class ThumbnailFetcher : public QObject {
    Q_OBJECT

public:
    /* Relative to paths::dataDir(); also the prefix of every stored Thumbnail::path */
    static constexpr const char *THUMBNAILS_DIR = "assets/thumbnails";

    ThumbnailFetcher(QObject *parent = nullptr);

    /* Absolute path of a stored Thumbnail::path */
    static QString absolutePath(const QString &path);

    /**
     * Download the image candidates the providers found in `video`, plus the other
     * sizes DMM serves for them, then emit finished() with each thumbnail list
     * holding only what was downloaded. Replaces this video's previous
     * thumbnail directory.
     */
    void fetch(const model::Video &video);

signals:
    void finished(const model::Video &video);

private:
    struct Job;

    void replyFinished(const std::shared_ptr<Job> &job, size_t target, size_t candidate,
                       QNetworkReply *reply);
    void save(const std::shared_ptr<Job> &job);

    QNetworkAccessManager m_network;
    QDir m_root;  /* paths::dataDir() */
};
