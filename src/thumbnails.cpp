#include "thumbnails.hpp"
#include "log.hpp"

#include <algorithm>
#include <utility>

#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QRegularExpression>
#include <QSaveFile>

namespace {

    const QStringList DMM_HOSTS{"pics.dmm.co.jp", "awsimgsrc.dmm.co.jp"};

    /* Actress pictures: 65 px, 100 px and 125 px squares, named after the actress */
    const QString ACTRESS_BASE_URL = "https://pics.dmm.co.jp/mono/actjpgs/";
    const QStringList ACTRESS_SIZE_DIRS{"thumbnail/", "medium/", ""};

    /* Hepburn (javlibrary) to Kunrei-shiki (DMM's actress picture names), longest first */
    const std::vector<std::pair<QString, QString>> HEPBURN_TO_KUNREI{
        {"cch", "tch"}, {"sha", "sya"}, {"shu", "syu"}, {"sho", "syo"}, {"shi", "si"},
        {"cha", "tya"}, {"chu", "tyu"}, {"cho", "tyo"}, {"chi", "ti"},  {"tsu", "tu"},
        {"ja", "zya"},  {"ju", "zyu"},  {"jo", "zyo"},  {"ji", "zi"},   {"fu", "hu"}};

    bool isDmm(const QUrl &url)
    {
        return DMM_HOSTS.contains(url.host());
    }

    /* "Kuraki Shiori" -> "kuraki_shiori" */
    QString actressFileName(const QString &nameRo)
    {
        static const QRegularExpression notLetter("[^a-z ]");
        return nameRo.toLower().remove(notLetter).simplified().replace(' ', '_');
    }

    QString toKunrei(QString name)
    {
        for (const auto &[hepburn, kunrei] : HEPBURN_TO_KUNREI)
            name.replace(hepburn, kunrei);
        return name;
    }

    /*
     * DMM's id for the digital release, e.g. "mkmp00705": the high resolution
     * jacket only exists under that id. Known when the video came from DMM,
     * else taken from an image on the page, else guessed from the physical
     * release's "mkmp705".
     */
    QString digitalCid(const model::Video &video)
    {
        static const QRegularExpression digitalRe("/digital/video/([a-z0-9_]+)/");
        static const QRegularExpression monoRe("/mono/movie/adult/([a-z]+)(\\d+)/");

        if (!video.dmmId.isEmpty())
            return video.dmmId;

        for (const auto &sizes : video.previews)
            for (const model::Thumbnail &t : sizes)
                if (const auto m = digitalRe.match(t.url.path()); m.hasMatch())
                    return m.captured(1);
        for (const model::Thumbnail &t : video.covers) {
            if (const auto m = digitalRe.match(t.url.path()); m.hasMatch())
                return m.captured(1);
            if (const auto m = monoRe.match(t.url.path()); m.hasMatch())
                return m.captured(1) + m.captured(2).rightJustified(5, '0');
        }
        return {};
    }

    /* Video ids become directory names: keep them to [A-Za-z0-9_-] */
    QString safeDirName(QString id)
    {
        static const QRegularExpression unsafe("[^A-Za-z0-9_-]");
        return id.replace(unsafe, "_");
    }

} /* namespace */

struct ThumbnailFetcher::Job {
    struct Candidate {
        Candidate(QUrl url, int group) : url(std::move(url)), group(group) {}

        QUrl url;
        int group = 0;  /* lower is preferred, see Target */
        QByteArray data;  /* empty until downloaded */
        QSize size;
        QByteArray format;  /* "jpeg" or "png" */
    };

    /*
     * One image in several sizes, e.g. the jacket or one actress. Only the
     * lowest group with any downloaded candidate is kept, so a mirror (or an
     * alias that may be a different picture) is used only when nothing better
     * exists. Within it, one candidate per distinct size is kept.
     */
    struct Target {
        QString name;  /* file name prefix, e.g. "cover" or "cast-aegtu" */
        std::vector<model::Thumbnail> *thumbnails;  /* in Job::video */
        std::vector<Candidate> candidates;
    };

    model::Video video;
    std::vector<Target> targets;
    int pending = 0;  /* replies still outstanding */
};

ThumbnailFetcher::ThumbnailFetcher(QObject *parent) : QObject(parent)
{
    /* Resolve against the executable, not the working directory */
    m_root = QDir(QDir::cleanPath(QCoreApplication::applicationDirPath() + "/.."));
}

QString ThumbnailFetcher::absolutePath(const QString &path)
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../" + path);
}

void ThumbnailFetcher::fetch(const model::Video &video)
{
    auto job = std::make_shared<Job>();
    job->video = video;
    model::Video &v = job->video;

    auto addTarget = [&job](const QString &name, std::vector<model::Thumbnail> &thumbnails,
                            const std::vector<Job::Candidate> &candidates) {
        Job::Target target{name, &thumbnails, {}};
        for (const Job::Candidate &c : candidates) {
            const bool seen = std::any_of(target.candidates.begin(), target.candidates.end(),
                                          [&c](const Job::Candidate &o) { return o.url == c.url; });
            if (c.url.isValid() && !seen)
                target.candidates.push_back(c);
        }
        thumbnails.clear();  /* refilled with what gets downloaded */
        if (!target.candidates.empty())
            job->targets.push_back(target);
    };

    /* Jacket: the page's, then the digital release's (sometimes far larger); mirrors last */
    std::vector<Job::Candidate> covers;
    for (const model::Thumbnail &t : v.covers)
        covers.push_back({t.url, isDmm(t.url) ? 0 : 1});
    if (const QString cid = digitalCid(v); !cid.isEmpty()) {
        covers.push_back({QUrl(QString("https://awsimgsrc.dmm.co.jp/pics_dig/digital/video/%1/%1pl.jpg").arg(cid)), 0});
        covers.push_back({QUrl(QString("https://pics.dmm.co.jp/digital/video/%1/%1pl.jpg").arg(cid)), 0});
    }

    /* Poster: DMM serves the jacket's front half next to it, "...pl.jpg" -> "...ps.jpg" */
    std::vector<Job::Candidate> posters;
    for (const model::Thumbnail &t : v.posters)
        posters.push_back({t.url, 0});
    for (const Job::Candidate &c : covers) {
        QString url = c.url.toString();
        if (c.group == 0 && url.endsWith("pl.jpg"))
            posters.push_back({QUrl(url.replace(url.size() - 6, 6, "ps.jpg")), 0});
    }

    addTarget("cover", v.covers, covers);
    addTarget("poster", v.posters, posters);

    for (size_t i = 0; i < v.previews.size(); ++i) {
        std::vector<Job::Candidate> sizes;
        for (const model::Thumbnail &t : v.previews[i])
            sizes.push_back({t.url, 0});
        addTarget(QString("preview-%1").arg(i + 1, 2, 10, QChar('0')), v.previews[i], sizes);
    }

    /*
     * Cast: a picture given by the provider (DMM) belongs to that actress, so
     * only its other sizes are added. Otherwise (JavLibrary) the picture is
     * looked up by name: DMM names pictures after the Kunrei-shiki name,
     * sometimes the Hepburn one.
     */
    static const QRegularExpression actressPictureRe("/actjpgs/(?:[a-z]+/)?([^/]+)\\.jpg$");
    for (model::Actress &actress : v.cast) {
        QStringList names;
        std::vector<Job::Candidate> candidates;
        if (!actress.thumbnails.empty()) {
            for (const model::Thumbnail &t : actress.thumbnails) {
                candidates.push_back({t.url, 0});
                if (const auto m = actressPictureRe.match(t.url.path()); m.hasMatch() && !names.contains(m.captured(1)))
                    names.append(m.captured(1));
            }
            for (const QString &name : names)
                for (const QString &sizeDir : ACTRESS_SIZE_DIRS)
                    candidates.push_back({QUrl(ACTRESS_BASE_URL + sizeDir + name + ".jpg"), 0});
        } else {
            for (const QString &nameRo : QStringList{actress.nameRo} + actress.aliasesRo) {
                const QString hepburn = actressFileName(nameRo);
                for (const QString &name : {toKunrei(hepburn), hepburn})
                    if (!name.isEmpty() && !names.contains(name))
                        names.append(name);
            }
            for (int group = 0; group < names.size(); ++group)
                for (const QString &sizeDir : ACTRESS_SIZE_DIRS)
                    candidates.push_back({QUrl(ACTRESS_BASE_URL + sizeDir + names[group] + ".jpg"), group});
        }
        addTarget("cast-" + safeDirName(actress.id), actress.thumbnails, candidates);
    }

    for (size_t t = 0; t < job->targets.size(); ++t) {
        for (size_t c = 0; c < job->targets[t].candidates.size(); ++c) {
            QNetworkRequest request(job->targets[t].candidates[c].url);
            /* DMM redirects missing pictures to a "now printing" placeholder */
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::ManualRedirectPolicy);
            request.setTransferTimeout(30000);
            QNetworkReply *reply = m_network.get(request);
            ++job->pending;
            connect(reply, &QNetworkReply::finished, this,
                    [this, job, t, c, reply] { this->replyFinished(job, t, c, reply); });
        }
    }
    spdlog::info("{}: downloading up to {} thumbnail(s)", v.id, job->pending);

    if (job->pending == 0)
        this->save(job);
}

void ThumbnailFetcher::replyFinished(const std::shared_ptr<Job> &job, size_t target, size_t candidate,
                                     QNetworkReply *reply)
{
    Job::Candidate &c = job->targets[target].candidates[candidate];
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || status != 200) {
        spdlog::debug("{} not available (HTTP {}, {})", c.url, status, reply->errorString());
    } else {
        QByteArray data = reply->readAll();
        QBuffer buffer(&data);
        QImageReader reader(&buffer);
        const QByteArray format = reader.format();
        const QSize size = reader.size();
        if ((format == "jpeg" || format == "png") && size.isValid()) {
            c.data = data;
            c.size = size;
            c.format = format;
        } else {
            spdlog::warn("{} is not a JPEG or PNG image, skipping", c.url);
        }
    }

    reply->deleteLater();
    if (--job->pending == 0)
        this->save(job);
}

void ThumbnailFetcher::save(const std::shared_ptr<Job> &job)
{
    model::Video &video = job->video;
    const QString relDir = QString("%1/%2").arg(THUMBNAILS_DIR, safeDirName(video.id));

    QDir dir(m_root.filePath(relDir));
    if (dir.exists() && !dir.removeRecursively())
        spdlog::warn("Could not clear old thumbnails in {}", dir.path());
    if (!m_root.mkpath(relDir)) {
        spdlog::error("Could not create {}, storing {} without thumbnails", dir.path(), video.id);
        emit this->finished(video);
        return;
    }

    int saved = 0;
    for (Job::Target &target : job->targets) {
        int best = -1;
        for (const Job::Candidate &c : target.candidates)
            if (!c.data.isEmpty() && (best < 0 || c.group < best))
                best = c.group;
        if (best < 0) {
            spdlog::info("{}: no {} image found", video.id, target.name);
            continue;
        }

        std::vector<const Job::Candidate *> chosen;
        for (const Job::Candidate &c : target.candidates) {
            const bool sameSize = std::any_of(chosen.begin(), chosen.end(),
                                              [&c](const Job::Candidate *o) { return o->size == c.size; });
            if (!c.data.isEmpty() && c.group == best && !sameSize)
                chosen.push_back(&c);
        }
        std::sort(chosen.begin(), chosen.end(), [](const Job::Candidate *a, const Job::Candidate *b) {
            return a->size.width() * a->size.height() < b->size.width() * b->size.height();
        });

        for (const Job::Candidate *c : chosen) {
            const QString fileName = QString("%1-%2x%3.%4")
                                         .arg(target.name)
                                         .arg(c->size.width())
                                         .arg(c->size.height())
                                         .arg(c->format == "jpeg" ? "jpg" : QString(c->format));
            QSaveFile file(dir.filePath(fileName));
            if (!file.open(QIODevice::WriteOnly) || file.write(c->data) != c->data.size() || !file.commit()) {
                spdlog::error("Could not write {}: {}", file.fileName(), file.errorString());
                continue;
            }
            target.thumbnails->push_back({{}, relDir + "/" + fileName, c->url, c->size.width(),
                                          c->size.height()});
            ++saved;
        }
    }

    spdlog::info("{}: saved {} thumbnail(s) to {}", video.id, saved, dir.path());
    emit this->finished(video);
}
