#include "network.hpp"
#include "log.hpp"

#include <QTimer>

NetworkManager::NetworkManager(Options options, QObject *parent) : QNetworkAccessManager(parent)
{
    /* Get Flaresolverr URL */
    if (!options.m_dockerHost.isEmpty() && options.m_dockerPort > 0 && options.m_dockerPort <= 65535) {
        this->m_dockerUrl.setScheme("http");
        this->m_dockerUrl.setHost(options.m_dockerHost);
        this->m_dockerUrl.setPort(options.m_dockerPort);
        this->m_dockerUrl.setPath("/v1");
        spdlog::info("Using FlareSolverr at {}", this->m_dockerUrl);
    } else {
        spdlog::error("FlareSolverr host/port not configured (host \"{}\", port {}), downloads will fail",
                      options.m_dockerHost, options.m_dockerPort);
    }

    /* Prepare HTTP headers */
    QHttpHeaders headers;
    headers.append("Content-Type", "application/json");

    /* Create reusable HTTP request for Flaresolverr */
    QNetworkRequest httpRequest;
    httpRequest.setHeaders(headers);
    httpRequest.setUrl(this->m_dockerUrl);  /* Reuse previous URL */
    this->m_httpRequest = httpRequest;

    /* Run ::replyfinished each time a request finishes with! a reply */
    QObject::connect(this, &QNetworkAccessManager::finished, this, &NetworkManager::replyFinished);
}

void NetworkManager::replyFinished(QNetworkReply *reply)
{
    const QUrl url = reply->property(TARGET_URL_PROPERTY).toUrl();

    if (reply->error() != QNetworkReply::NoError) {
        spdlog::error("Download of {} failed: {} (is flaresolverr running?)", url, reply->errorString());
        emit this->downloadFailed(url);
    } else {
        const QByteArray payload = reply->readAll();
        spdlog::info("Downloaded {} ({} bytes)", url, payload.size());
        emit this->downloadReady(url, QString(payload));
    }

    reply->deleteLater();
}

void NetworkManager::push(QUrl url)
{
    QMutexLocker locker(&this->m_queueMutex);
    this->m_queue.push(url);
    spdlog::debug("Queued {} ({} in queue)", url, this->m_queue.size());
}

void NetworkManager::startDownload()
{
    /*
     * Hard limit: never start two requests less than START_INTERVAL apart.
     * A call that comes too early (e.g. a timer tick a few ms ahead of the
     * previous, late start) is deferred to the earliest allowed moment rather
     * than dropped, which would leave a gap of a whole extra interval.
     */
    if (this->m_sinceLastStart.isValid()
        && this->m_sinceLastStart.durationElapsed() < START_INTERVAL) {
        if (!this->m_startDeferred) {
            using namespace std::chrono;
            const auto wait = ceil<milliseconds>(START_INTERVAL - this->m_sinceLastStart.durationElapsed())
                              + milliseconds(1);
            spdlog::debug("startDownload called {} ms after the previous start, deferring by {} ms",
                          this->m_sinceLastStart.elapsed(), wait.count());
            this->m_startDeferred = true;
            QTimer::singleShot(wait, Qt::PreciseTimer, this, [this] {
                this->m_startDeferred = false;
                this->startDownload();
            });
        }
        return;
    }

    QUrl url;
    size_t remaining;
    {
        QMutexLocker locker(&this->m_queueMutex);
        if (this->m_queue.empty()) {
            spdlog::trace("Download queue empty");
            return;
        }
        url = this->m_queue.front();
        this->m_queue.pop();
        remaining = this->m_queue.size();
    }

    this->m_sinceLastStart.start();
    spdlog::info("Starting download of {} ({} left in queue)", url, remaining);
    QByteArray data = NetworkManager::buildRequestData(url);
    QNetworkReply *reply = this->post(this->m_httpRequest, data);
    reply->setProperty(TARGET_URL_PROPERTY, url);
}
