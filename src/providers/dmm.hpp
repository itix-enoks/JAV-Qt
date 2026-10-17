#pragma once

#include <functional>

#include <QJsonObject>
#include <QObject>
#include <QNetworkAccessManager>

#include "models/video.hpp"

/**
 * Looks videos up on DMM (FANZA) through the GraphQL API behind
 * video.dmm.co.jp. It needs no account and no FlareSolverr, and unlike the
 * DVD pages on www.dmm.co.jp it is not region locked. Its text is Japanese
 * only.
 */
class DmmClient : public QObject {
    Q_OBJECT

public:
    static constexpr const char *API_URL = "https://api.video.dmm.co.jp/graphql";

    DmmClient(QObject *parent = nullptr);

    /**
     * Find the video whose maker id (e.g. "MKMP-705") is `id`, then emit
     * found() or notFound() with `id` as given here.
     * Videos are matched on their exact maker id, so a fuzzy search hit for
     * another video is never returned.
     */
    void lookup(const model::JavId &id);

    /* "kuraki_siori" (a DMM actress picture name) -> "Kuraki Shiori" */
    static QString romanizedName(const QString &pictureName);

signals:
    void found(const model::JavId &id, const model::Video &video);
    void notFound(const model::JavId &id);

private:
    using Handler = std::function<void(const QJsonObject &data)>;

    /* POST a GraphQL query; `handler` gets its "data" (empty on any error) */
    void query(const QString &query, const QJsonObject &variables, Handler handler);
    void matchCandidates(const model::JavId &id, const QStringList &contentIds);
    void fetchDetails(const model::JavId &id, const QString &contentId);

    QNetworkAccessManager m_network;
};
