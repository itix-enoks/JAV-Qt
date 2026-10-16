#pragma once

#include <iostream>

#include <QObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

#include "options.hpp"

/**
 * Interface between this process and FlareSolverr's container in Docker.
 */
class NetworkManager : public QNetworkAccessManager {
public:
    NetworkManager(Options options, QObject *parent = nullptr);
    void replyFinished(QNetworkReply *reply);
    void fetchSite(QUrl url);
    static QByteArray buildRequestData(QUrl resourceUrl) {
        QVariantHash requestDataHash;
        requestDataHash.insert("cmd", "request.get");
        requestDataHash.insert("url", resourceUrl.toString());
        requestDataHash.insert("maxTimeout", 20000);
        QJsonDocument requestData(QJsonObject::fromVariantHash(requestDataHash));
        return requestData.toJson();
    }

    /**
     * Internals
     */
    QUrl m_dockerUrl;
    QNetworkRequest m_httpRequest;
};
