#include "providers/dmm.hpp"
#include "library.hpp"
#include "log.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {

    /*
     * Digital releases are numbered "<label><prefix><5 digit suffix>", where the
     * label number is usually absent ("mkmp00705") but sometimes a single digit
     * ("1svhot00002"). Anything else is left to the search.
     */
    constexpr int LABEL_GUESSES = 9;

    const QString CONTENT_FIELDS = R"(
        id
        title
        makerContentId
        makerReleasedAt
        deliveryStartDate
        duration
        packageImage { largeUrl mediumUrl }
        sampleImages { number imageUrl largeImageUrl }
        actresses { id name imageUrl }
        directors { id name }
        maker { id name }
        genres { id name }
    )";

    /* Kunrei-shiki (DMM's picture names) to Hepburn (JavLibrary), longest first */
    const std::vector<std::pair<QString, QString>> KUNREI_TO_HEPBURN{
        {"sya", "sha"}, {"syu", "shu"}, {"syo", "sho"}, {"tya", "cha"}, {"tyu", "chu"},
        {"tyo", "cho"}, {"zya", "ja"},  {"zyu", "ju"},  {"zyo", "jo"},  {"tti", "cchi"},
        {"si", "shi"},  {"ti", "chi"},  {"tu", "tsu"},  {"hu", "fu"},   {"zi", "ji"}};

    QString toHepburn(const QString &kunrei)
    {
        /* One pass, so "syu" -> "shu" is not turned into "sfu" by "hu" -> "fu" */
        static const QRegularExpression syllables = [] {
            QStringList alternatives;
            for (const auto &[from, to] : KUNREI_TO_HEPBURN)
                alternatives.append(from);
            return QRegularExpression(alternatives.join('|'));
        }();

        QString out;
        qsizetype last = 0;
        for (auto it = syllables.globalMatch(kunrei); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            out += kunrei.mid(last, m.capturedStart() - last);
            for (const auto &[from, to] : KUNREI_TO_HEPBURN)
                if (from == m.captured())
                    out += to;
            last = m.capturedEnd();
        }
        return out + kunrei.mid(last);
    }

    /* "2026-02-09T15:00:00Z" -> 2026-02-10: DMM dates are midnight in Japan */
    std::chrono::year_month_day japanDate(const QString &timestamp)
    {
        const QDate date = QDateTime::fromString(timestamp, Qt::ISODateWithMs).toOffsetFromUtc(9 * 3600).date();
        if (!date.isValid())
            return {};
        return std::chrono::year_month_day{std::chrono::year(date.year()),
                                           std::chrono::month(static_cast<unsigned>(date.month())),
                                           std::chrono::day(static_cast<unsigned>(date.day()))};
    }

    model::Video videoFromJson(const QJsonObject &content, const QJsonObject &review)
    {
        model::Video video;
        video.provider = model::provider::DMM;
        video.dmmId = content["id"].toString();
        video.id = content["makerContentId"].toString().toUpper();
        video.titleJa = content["title"].toString();

        const QString released = content["makerReleasedAt"].toString();
        video.releaseDate = japanDate(released.isEmpty() ? content["deliveryStartDate"].toString() : released);
        video.lengthMin = static_cast<int>(std::lround(content["duration"].toDouble() / 60));

        if (const QJsonArray directors = content["directors"].toArray(); !directors.isEmpty()) {
            model::Director director;
            director.id = directors.first()["id"].toString();
            director.nameJa = directors.first()["name"].toString();
            video.director = director;
        }
        if (const QJsonObject json = content["maker"].toObject(); !json.isEmpty()) {
            model::Maker maker;
            maker.id = json["id"].toString();
            maker.nameJa = json["name"].toString();
            video.maker = maker;
        }
        for (const QJsonValue value : content["genres"].toArray()) {
            model::Genre genre;
            genre.id = value["id"].toString();
            genre.titleJa = value["name"].toString();
            video.genres.push_back(genre);
        }

        /* The picture belongs to this exact actress: no name matching needed */
        for (const QJsonValue value : content["actresses"].toArray()) {
            model::Actress actress;
            actress.id = value["id"].toString();
            actress.nameJa = value["name"].toString();
            const QUrl picture(value["imageUrl"].toString());
            if (picture.isValid() && !picture.isEmpty()) {
                actress.nameRo = DmmClient::romanizedName(picture.fileName().section('.', 0, 0));
                actress.thumbnails.push_back({{}, {}, picture});
            }
            video.cast.push_back(actress);
        }

        const QJsonObject package = content["packageImage"].toObject();
        if (const QUrl large(package["largeUrl"].toString()); !large.isEmpty())
            video.covers.push_back({{}, {}, large});
        if (const QUrl medium(package["mediumUrl"].toString()); !medium.isEmpty())
            video.posters.push_back({{}, {}, medium});

        QJsonArray samples = content["sampleImages"].toArray();
        std::vector<QJsonObject> sorted;
        for (const auto &value : samples)
            sorted.push_back(value.toObject());
        std::sort(sorted.begin(), sorted.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a["number"].toInt() < b["number"].toInt();
        });
        for (const QJsonObject &sample : sorted) {
            std::vector<model::Thumbnail> sizes;
            for (const char *key : {"imageUrl", "largeImageUrl"})
                if (const QUrl url(sample[key].toString()); !url.isEmpty())
                    sizes.push_back({{}, {}, url});
            if (!sizes.empty())
                video.previews.push_back(sizes);
        }

        /* Reviews are out of 5, JavLibrary's ratings out of 10 */
        if (review["total"].toInt() > 0)
            video.rating = model::UserRating{{}, review["average"].toDouble() * 2};

        return video;
    }

} /* namespace */

DmmClient::DmmClient(QObject *parent) : QObject(parent) {}

QString DmmClient::romanizedName(const QString &pictureName)
{
    /* Digits tell namesakes apart: "tukino_runa2", "..._2" */
    static const QRegularExpression digits("\\d+");

    QStringList parts;
    for (const QString &part : pictureName.toLower().remove(digits).split('_', Qt::SkipEmptyParts)) {
        QString hepburn = toHepburn(part);
        hepburn[0] = hepburn[0].toUpper();
        parts.append(hepburn);
    }
    return parts.join(' ');
}

void DmmClient::lookup(const model::JavId &id)
{
    const QString base = id.prefix.toLower() + QString::number(id.suffix).rightJustified(5, '0');
    QStringList guesses{base};
    for (int label = 1; label <= LABEL_GUESSES; ++label)
        guesses.append(QString::number(label) + base);

    QString declarations = "$q: String";
    QString fields;
    QJsonObject variables{{"q", QString("%1 %2").arg(id.prefix).arg(id.suffix, 3, 10, QChar('0'))}};
    for (qsizetype i = 0; i < guesses.size(); ++i) {
        declarations += QString(", $g%1: ID!").arg(i);
        fields += QString("g%1: ppvContent(id: $g%1) { id makerContentId }\n").arg(i);
        variables[QString("g%1").arg(i)] = guesses[i];
    }
    const QString lookupQuery = QString(R"(query Lookup(%1) {
        %2
        search: legacySearchPPV(limit: 10, sort: SALES_RANK_SCORE, queryWord: $q,
                                includeExplicit: true, excludeUndelivered: false) {
            result { contents { id } }
        }
    })").arg(declarations, fields);

    const QString wanted = Library::toString(id);
    this->query(lookupQuery, variables, [this, id, wanted, guesses](const QJsonObject &data) {
        for (qsizetype i = 0; i < guesses.size(); ++i) {
            const QJsonObject content = data[QString("g%1").arg(i)].toObject();
            if (content["makerContentId"].toString().compare(wanted, Qt::CaseInsensitive) == 0) {
                this->fetchDetails(id, content["id"].toString());
                return;
            }
        }

        /* The search is fuzzy ("URE 067" also finds JUQ-675): check every hit */
        QStringList hits;
        for (const QJsonValue value : data["search"]["result"]["contents"].toArray())
            if (const QString hit = value["id"].toString(); !hit.isEmpty() && !guesses.contains(hit))
                hits.append(hit);
        this->matchCandidates(id, hits);
    });
}

void DmmClient::matchCandidates(const model::JavId &id, const QStringList &contentIds)
{
    const QString wanted = Library::toString(id);
    if (contentIds.isEmpty()) {
        spdlog::info("{} not found on DMM", wanted);
        emit this->notFound(id);
        return;
    }

    QStringList declarations;
    QString fields;
    QJsonObject variables;
    for (qsizetype i = 0; i < contentIds.size(); ++i) {
        declarations.append(QString("$c%1: ID!").arg(i));
        fields += QString("c%1: ppvContent(id: $c%1) { id makerContentId }\n").arg(i);
        variables[QString("c%1").arg(i)] = contentIds[i];
    }
    const QString matchQuery = QString("query Match(%1) {\n%2}").arg(declarations.join(", "), fields);

    this->query(matchQuery, variables, [this, id, wanted, contentIds](const QJsonObject &data) {
        for (qsizetype i = 0; i < contentIds.size(); ++i) {
            const QJsonObject content = data[QString("c%1").arg(i)].toObject();
            if (content["makerContentId"].toString().compare(wanted, Qt::CaseInsensitive) == 0) {
                this->fetchDetails(id, content["id"].toString());
                return;
            }
        }
        spdlog::info("{} not found on DMM ({} search hit(s), none with that id)", wanted,
                     contentIds.size());
        emit this->notFound(id);
    });
}

void DmmClient::fetchDetails(const model::JavId &id, const QString &contentId)
{
    const QString detailsQuery = QString(R"(query Details($id: ID!) {
        ppvContent(id: $id) { %1 }
        reviewSummary(contentId: $id) { average total }
    })").arg(CONTENT_FIELDS);

    this->query(detailsQuery, {{"id", contentId}}, [this, id, contentId](const QJsonObject &data) {
        const QJsonObject content = data["ppvContent"].toObject();
        if (content.isEmpty()) {
            spdlog::warn("DMM returned no details for {}", contentId);
            emit this->notFound(id);
            return;
        }
        const model::Video video = videoFromJson(content, data["reviewSummary"].toObject());
        spdlog::info("Found {} on DMM as {}: {} cast, {} preview(s)", video.id, video.dmmId,
                     video.cast.size(), video.previews.size());
        emit this->found(id, video);
    });
}

void DmmClient::query(const QString &query, const QJsonObject &variables, Handler handler)
{
    QNetworkRequest request{QUrl(API_URL)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(30000);
    const QByteArray body = QJsonDocument(QJsonObject{{"query", query}, {"variables", variables}}).toJson();

    QNetworkReply *reply = m_network.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [reply, handler = std::move(handler)] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            spdlog::warn("DMM request failed: {}", reply->errorString());
            handler({});
            return;
        }

        QJsonParseError error;
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll(), &error).object();
        if (error.error != QJsonParseError::NoError)
            spdlog::warn("DMM reply is not JSON: {}", error.errorString());
        /* Partial errors (e.g. one unknown id) still come with usable data */
        for (const QJsonValue e : root["errors"].toArray())
            spdlog::debug("DMM GraphQL error: {}", e["message"].toString());
        handler(root["data"].toObject());
    });
}
