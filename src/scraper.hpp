#pragma once

#include <optional>

#include <QHash>
#include <QObject>

#include "dbm.hpp"
#include "network.hpp"
#include "thumbnails.hpp"
#include "providers/dmm.hpp"
#include "providers/javlib.hpp"

#include "models/video.hpp"

/**
 * Scrapes one video from both providers, merges them (see model::Video),
 * downloads its thumbnails and stores it.
 * DMM is the primary provider; JavLibrary adds the English text, or provides
 * everything when DMM doesn't have the video.
 */
class Scraper : public QObject {
    Q_OBJECT

public:
    Scraper(NetworkManager &flareSolverr, DBManager &db, QObject *parent = nullptr);

    /*
     * Look `id` up on both providers: DMM answers right away, JavLibrary at
     * FlareSolverr's pace.
     */
    void scrape(const model::JavId &id);

    /* Videos queued but not yet stored or skipped */
    int pendingCount() const { return m_pending.size() + m_fetching; }

signals:
    void stored(const model::Video &video);
    /* Found on neither provider, or could not be stored */
    void skipped(const QString &id);

private:
    struct Pending {
        std::optional<model::Video> dmm;
        std::optional<model::Video> javLibrary;
        bool dmmDone = false;
        bool javLibraryDone = false;
    };

    void dmmDone(const model::JavId &id, std::optional<model::Video> video);
    void javLibraryDone(const model::JavId &id, std::optional<model::Video> video);
    /* Once both providers answered: merge, then fetch thumbnails */
    void settle(const QString &key);

    static model::Video merge(const model::Video &dmm, const std::optional<model::Video> &javLibrary);

    DBManager &m_db;
    DmmClient m_dmm;
    JavLibClient m_javLibrary;
    ThumbnailFetcher m_thumbnails;
    QHash<QString, Pending> m_pending;  /* by id, e.g. "MKMP-705" */
    int m_fetching = 0;  /* merged, thumbnails still downloading */
};
