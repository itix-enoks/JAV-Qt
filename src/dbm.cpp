#include "dbm.hpp"
#include "log.hpp"
#include "paths.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace {

    const QString VIDEOS_KEY = "videos";
    const QString NOTES_KEY = "notes";
    const QString STARS_KEY = "stars";
    const QString BOOKMARKED_KEY = "bookmarked";
    /* Written by the user, not by scraping: storeDB() keeps them */
    const QStringList USER_KEYS{NOTES_KEY, STARS_KEY, BOOKMARKED_KEY};

    /* Pair of id and romanized name, shared by Director, Maker and Actress */
    template <typename T>
    QJsonObject namedToJson(const T &obj)
    {
        return QJsonObject{{"id", obj.id}, {"nameRo", obj.nameRo}, {"nameJa", obj.nameJa}};
    }

    template <typename T>
    T namedFromJson(const QJsonObject &json)
    {
        T obj;
        obj.id = json["id"].toString();
        obj.nameRo = json["nameRo"].toString();
        obj.nameJa = json["nameJa"].toString();
        return obj;
    }

    QJsonArray thumbnailsToJson(const std::vector<model::Thumbnail> &thumbnails)
    {
        QJsonArray json;
        for (const model::Thumbnail &t : thumbnails)
            json.append(QJsonObject{{"path", t.path},
                                    {"url", t.url.toString()},
                                    {"width", t.width},
                                    {"height", t.height}});
        return json;
    }

    std::vector<model::Thumbnail> thumbnailsFromJson(const QJsonValue &json)
    {
        std::vector<model::Thumbnail> thumbnails;
        for (const auto &value : json.toArray()) {
            const QJsonObject t = value.toObject();
            thumbnails.push_back({{}, t["path"].toString(), QUrl(t["url"].toString()),
                                  t["width"].toInt(), t["height"].toInt()});
        }
        return thumbnails;
    }

    QString dateToString(const std::chrono::year_month_day &ymd)
    {
        QDate date(int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day()));
        return date.toString(Qt::ISODate);
    }

    std::chrono::year_month_day dateFromString(const QString &str)
    {
        QDate date = QDate::fromString(str, Qt::ISODate);
        if (!date.isValid())
            return {};
        return std::chrono::year_month_day{
            std::chrono::year(date.year()),
            std::chrono::month(date.month()),
            std::chrono::day(date.day())};
    }

    QJsonObject videoToJson(const model::Video &video)
    {
        QJsonObject json;
        json["provider"] = video.provider;
        json["dmmId"] = video.dmmId;
        json["titleEn"] = video.titleEn;
        json["titleJa"] = video.titleJa;
        json["id"] = video.id;
        json["releaseDate"] = dateToString(video.releaseDate);
        json["lengthMin"] = video.lengthMin;
        json["director"] = video.director ? QJsonValue(namedToJson(*video.director)) : QJsonValue();
        json["maker"] = video.maker ? QJsonValue(namedToJson(*video.maker)) : QJsonValue();
        json["rating"] = video.rating ? QJsonValue(video.rating->value) : QJsonValue();

        QJsonArray genres;
        for (const auto &genre : video.genres)
            genres.append(QJsonObject{{"id", genre.id}, {"titleEn", genre.titleEn}, {"titleJa", genre.titleJa}});
        json["genres"] = genres;

        QJsonArray cast;
        for (const auto &actress : video.cast) {
            QJsonObject actressJson = namedToJson(actress);
            actressJson["thumbnails"] = thumbnailsToJson(actress.thumbnails);
            cast.append(actressJson);
        }
        json["cast"] = cast;

        /* Paths are relative to paths::dataDir(), like this file */
        QJsonArray previews;
        for (const auto &sizes : video.previews)
            previews.append(thumbnailsToJson(sizes));
        json["thumbnails"] = QJsonObject{{"covers", thumbnailsToJson(video.covers)},
                                         {"posters", thumbnailsToJson(video.posters)},
                                         {"previews", previews}};

        return json;
    }

    model::Video videoFromJson(const QJsonObject &json)
    {
        model::Video video;
        video.provider = json["provider"].toString();
        video.dmmId = json["dmmId"].toString();
        video.titleEn = json["titleEn"].toString();
        video.titleJa = json["titleJa"].toString();
        video.id = json["id"].toString();
        video.releaseDate = dateFromString(json["releaseDate"].toString());
        video.lengthMin = json["lengthMin"].toInt();
        if (json["director"].isObject())
            video.director = namedFromJson<model::Director>(json["director"].toObject());
        if (json["maker"].isObject())
            video.maker = namedFromJson<model::Maker>(json["maker"].toObject());
        if (json["rating"].isDouble()) {
            model::UserRating rating;
            rating.value = json["rating"].toDouble();
            video.rating = rating;
        }

        for (const auto &value : json["genres"].toArray()) {
            QJsonObject genreJson = value.toObject();
            model::Genre genre;
            genre.id = genreJson["id"].toString();
            genre.titleEn = genreJson["titleEn"].toString();
            genre.titleJa = genreJson["titleJa"].toString();
            video.genres.push_back(genre);
        }

        for (const auto &value : json["cast"].toArray()) {
            model::Actress actress = namedFromJson<model::Actress>(value.toObject());
            actress.thumbnails = thumbnailsFromJson(value.toObject()["thumbnails"]);
            video.cast.push_back(actress);
        }

        const QJsonObject thumbnails = json["thumbnails"].toObject();
        video.covers = thumbnailsFromJson(thumbnails["covers"]);
        video.posters = thumbnailsFromJson(thumbnails["posters"]);
        for (const auto &sizes : thumbnails["previews"].toArray())
            video.previews.push_back(thumbnailsFromJson(sizes));

        video.stars = json[STARS_KEY].toDouble();
        video.bookmarked = json[BOOKMARKED_KEY].toBool();

        return video;
    }

} /* namespace */

DBManager::DBManager()
{
    m_path = paths::dataPath("db.json");

    if (QFile::exists(m_path)) {
        spdlog::info("Using database {}", m_path);
        return;
    }

    this->writeRoot(QJsonObject{{VIDEOS_KEY, QJsonObject()}});
    spdlog::info("Created empty database {}", m_path);
}

model::Video DBManager::loadDB(model::Type objType, QString id)
{
    if (objType != model::Type::Video)
        throw std::invalid_argument("DBManager::loadDB only supports model::Type::Video");

    QJsonObject videos = this->readRoot()[VIDEOS_KEY].toObject();
    if (!videos.contains(id)) {
        spdlog::debug("Video {} not in database", id);
        return {};
    }  /* not found: returned video has an empty id */

    return videoFromJson(videos[id].toObject());
}

std::vector<model::Video> DBManager::loadAllVideos()
{
    std::vector<model::Video> out;
    const QJsonObject videos = this->readRoot()[VIDEOS_KEY].toObject();
    for (auto it = videos.begin(); it != videos.end(); ++it)
        out.push_back(videoFromJson(it.value().toObject()));
    return out;
}

QString DBManager::loadNotes(const QString &id)
{
    return this->readRoot().value(VIDEOS_KEY)[id][NOTES_KEY].toString();
}

void DBManager::storeNotes(const QString &id, const QString &notes)
{
    this->storeUserField(id, NOTES_KEY, notes);
}

void DBManager::storeStars(const QString &id, double stars)
{
    this->storeUserField(id, STARS_KEY, std::round(std::clamp(stars, 0.0, 5.0) * 2) / 2);
}

void DBManager::storeBookmarked(const QString &id, bool bookmarked)
{
    this->storeUserField(id, BOOKMARKED_KEY, bookmarked);
}

void DBManager::storeUserField(const QString &id, const QString &key, const QJsonValue &value)
{
    QJsonObject root = this->readRoot();
    QJsonObject videos = root[VIDEOS_KEY].toObject();
    if (!videos.contains(id))
        throw std::invalid_argument("Cannot store " + key.toStdString() + " for a video that isn't stored");
    QJsonObject video = videos[id].toObject();
    video[key] = value;
    videos[id] = video;
    root[VIDEOS_KEY] = videos;
    this->writeRoot(root);
}

void DBManager::storeDB(const model::Video &video)
{
    if (video.id.isEmpty())
        throw std::invalid_argument("Cannot store a video without an id");

    QJsonObject root = this->readRoot();
    QJsonObject videos = root[VIDEOS_KEY].toObject();
    QJsonObject json = videoToJson(video);
    if (videos.contains(video.id)) {
        spdlog::debug("Overwriting existing entry for {}", video.id);
        const QJsonObject stored = videos.value(video.id).toObject();
        for (const QString &key : USER_KEYS)
            if (stored.contains(key))
                json[key] = stored[key];
    }
    videos[video.id] = json;
    root[VIDEOS_KEY] = videos;
    this->writeRoot(root);
}

QJsonObject DBManager::readRoot()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw std::runtime_error("Could not open db.json");

    QJsonParseError jsonParseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(file.readAll(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError)
        throw std::runtime_error("Could not parse db.json");

    if (!jsonDoc.isObject())
        throw std::runtime_error("Unexpected structure in db.json");

    return jsonDoc.object();
}

void DBManager::writeRoot(const QJsonObject &root)
{
    /* QSaveFile writes to a temp file and renames, so a crash can't corrupt the db */
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        throw std::runtime_error("Could not open db.json for writing");

    file.write(QJsonDocument(root).toJson());
    if (!file.commit())
        throw std::runtime_error("Could not write db.json");
}
