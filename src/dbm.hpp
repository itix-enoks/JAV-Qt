#pragma once

#include <vector>

#include <QString>
#include <QJsonObject>

#include "models/video.hpp"

/**
 * Manager of the database in <project root>/db.json.
 */
class DBManager {
public:
    DBManager();
    model::Video loadDB(model::Type objType, QString id);
    std::vector<model::Video> loadAllVideos();
    /* Replaces the stored video, but keeps its user notes */
    void storeDB(const model::Video &video);

    /* The user's own notes on a video, kept apart so a re-scrape never loses them */
    QString loadNotes(const QString &id);
    void storeNotes(const QString &id, const QString &notes);

private:
    QJsonObject readRoot();
    void writeRoot(const QJsonObject &root);

    QString m_path;  /* db.json in the project root, next to build/ */
};
