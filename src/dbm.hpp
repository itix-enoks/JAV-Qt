#pragma once

#include <vector>

#include <QString>
#include <QJsonObject>

#include "models/video.hpp"

/**
 * Manager of the database, db.json in paths::dataDir().
 */
class DBManager {
public:
    DBManager();
    model::Video loadDB(model::Type objType, QString id);
    std::vector<model::Video> loadAllVideos();
    /* Replaces the stored video, but keeps the user's own fields (notes, stars, bookmark) */
    void storeDB(const model::Video &video);

    /*
     * The user's own fields of a stored video, each written on its own so a
     * re-scrape never loses them. Stars and the bookmark are also loaded into
     * model::Video; notes only here.
     */
    QString loadNotes(const QString &id);
    void storeNotes(const QString &id, const QString &notes);
    void storeStars(const QString &id, double stars);
    void storeBookmarked(const QString &id, bool bookmarked);

private:
    void storeUserField(const QString &id, const QString &key, const QJsonValue &value);
    QJsonObject readRoot();
    void writeRoot(const QJsonObject &root);

    QString m_path;  /* db.json in paths::dataDir() */
};
