#include <iostream>

#include "network.hpp"

NetworkManager::NetworkManager(Options options, QObject *parent) : QNetworkAccessManager(parent)
{
    /* Get Flaresolverr URL */
    if (!options.m_dockerHost.isEmpty() && options.m_dockerPort > 0 && options.m_dockerPort <= 65535) {
        this->m_dockerUrl.setScheme("http");
        this->m_dockerUrl.setHost(options.m_dockerHost);
        this->m_dockerUrl.setPort(options.m_dockerPort);
        this->m_dockerUrl.setPath("/v1");
    }

    /* Prepare HTTP headers */
    QHttpHeaders headers;
    headers.append("Content-Type", "application/json");

    /* Create reusable HTTP request for Flaresolverr */
    QNetworkRequest httpRequest;
    httpRequest.setHeaders(headers);
    httpRequest.setUrl(this->m_dockerUrl);  /* Reuse previous URL */
    std::cout << m_dockerUrl.toString().toStdString() << std::endl;
    this->m_httpRequest = httpRequest;

    /* Run ::replyfinished each time a request finishes with! a reply */
    QObject::connect(this, &QNetworkAccessManager::finished, this, &NetworkManager::replyFinished);
}

void NetworkManager::replyFinished(QNetworkReply *reply)
{
    QString qStr = QString(reply->readAll());
    std::string str = qStr.toStdString();
    std::cout << str << std::endl;
    // TODO send str to parser, which is responsible for converting to parsable HTMLObject
}

void NetworkManager::fetchSite(QUrl url)
{
    QByteArray data = NetworkManager::buildRequestData(url);
    this->post(this->m_httpRequest, data);
}
