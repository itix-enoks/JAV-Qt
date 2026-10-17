#pragma once

#include <queue>
#include <chrono>

#include <QMutex>
#include <QObject>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "options.hpp"

/**
 * Interface between this process and FlareSolverr's container in Docker.
 */
class NetworkManager : public QNetworkAccessManager {
    Q_OBJECT

public:
    /* FlareSolverr fails to solve protections if requests start closer together */
    static constexpr std::chrono::milliseconds START_INTERVAL{std::chrono::seconds(15)};

    NetworkManager(Options options, QObject *parent = nullptr);
    void replyFinished(QNetworkReply *reply);
    void push(QUrl url);  /* thread-safe */
    static QByteArray buildRequestData(QUrl resourceUrl) {
        QVariantHash requestDataHash;
        requestDataHash.insert("cmd", "request.get");
        requestDataHash.insert("url", resourceUrl.toString());
        requestDataHash.insert("maxTimeout", 20000);
        QJsonDocument requestData(QJsonObject::fromVariantHash(requestDataHash));
        return requestData.toJson();
    }

public slots:
    /**
     * Start downloading the next queued URL, if any.
     * Meant to be driven by a timer firing every START_INTERVAL; a call made
     * sooner than START_INTERVAL after the previous start is deferred until
     * START_INTERVAL has passed (further early calls meanwhile are dropped).
     * Must be called from the thread owning this NetworkManager.
     */
    void startDownload();

signals:
    /* url: the page that was requested, payload: FlareSolverr's reply */
    void downloadReady(const QUrl &url, const QString &payload);
    /* url: the page that was requested; it could not be downloaded */
    void downloadFailed(const QUrl &url);

private:
    /* Remembers which page a FlareSolverr reply belongs to */
    static constexpr const char *TARGET_URL_PROPERTY = "targetUrl";

    QMutex m_queueMutex;  /* guards m_queue */
    std::queue<QUrl> m_queue;
    QElapsedTimer m_sinceLastStart;
    bool m_startDeferred = false;  /* a deferred startDownload is pending */
    QUrl m_dockerUrl;
    QNetworkRequest m_httpRequest;
};
