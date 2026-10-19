#include "providers/javlib.hpp"
#include "library.hpp"
#include "log.hpp"

#include <gumbo.h>

#include <cstring>
#include <optional>

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {

    /**
     * Gumbo tree helpers
     */

    bool isElement(const GumboNode *node)
    {
        return node && node->type == GUMBO_NODE_ELEMENT;
    }

    const char *attr(const GumboNode *node, const char *name)
    {
        if (!isElement(node))
            return nullptr;
        const GumboAttribute *a = gumbo_get_attribute(&node->v.element.attributes, name);
        return a ? a->value : nullptr;
    }

    bool hasClass(const GumboNode *node, const QString &cls)
    {
        const char *classes = attr(node, "class");
        if (!classes)
            return false;
        return QString::fromUtf8(classes).split(' ', Qt::SkipEmptyParts).contains(cls);
    }

    template <typename Pred>
    const GumboNode *findFirst(const GumboNode *node, Pred pred)
    {
        if (!isElement(node))
            return nullptr;
        if (pred(node))
            return node;
        const GumboVector &children = node->v.element.children;
        for (unsigned int i = 0; i < children.length; ++i) {
            auto *child = static_cast<const GumboNode *>(children.data[i]);
            if (const GumboNode *found = findFirst(child, pred))
                return found;
        }
        return nullptr;
    }

    template <typename Pred>
    void findAll(const GumboNode *node, Pred pred, std::vector<const GumboNode *> &out)
    {
        if (!isElement(node))
            return;
        if (pred(node)) {
            out.push_back(node);
            return;  /* matches are never nested on these pages */
        }
        const GumboVector &children = node->v.element.children;
        for (unsigned int i = 0; i < children.length; ++i)
            findAll(static_cast<const GumboNode *>(children.data[i]), pred, out);
    }

    const GumboNode *byId(const GumboNode *node, const char *id)
    {
        return findFirst(node, [id](const GumboNode *n) {
            const char *v = attr(n, "id");
            return v && std::strcmp(v, id) == 0;
        });
    }

    const GumboNode *byClass(const GumboNode *node, const QString &cls)
    {
        return findFirst(node, [&cls](const GumboNode *n) { return hasClass(n, cls); });
    }

    std::vector<const GumboNode *> allByClass(const GumboNode *node, const QString &cls)
    {
        std::vector<const GumboNode *> out;
        findAll(node, [&cls](const GumboNode *n) { return hasClass(n, cls); }, out);
        return out;
    }

    const GumboNode *byTag(const GumboNode *node, GumboTag tag)
    {
        return findFirst(node, [tag](const GumboNode *n) { return n->v.element.tag == tag; });
    }

    void appendText(const GumboNode *node, QString &out)
    {
        if (!node)
            return;
        if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE) {
            out += QString::fromUtf8(node->v.text.text);
        } else if (node->type == GUMBO_NODE_ELEMENT) {
            const GumboVector &children = node->v.element.children;
            for (unsigned int i = 0; i < children.length; ++i)
                appendText(static_cast<const GumboNode *>(children.data[i]), out);
        }
    }

    /* Concatenated, whitespace-normalized text content of a node */
    QString textOf(const GumboNode *node)
    {
        QString out;
        appendText(node, out);
        return out.simplified();
    }

    /* Value of `key` in an href like "vl_director.php?d=pnkq" */
    QString queryParam(const GumboNode *anchor, const QString &key)
    {
        const char *href = attr(anchor, "href");
        if (!href)
            return {};
        return QUrlQuery(QUrl(QString::fromUtf8(href))).queryItemValue(key);
    }

    /* Text of the first ".text" element inside div#<itemId> */
    QString itemText(const GumboNode *root, const char *itemId)
    {
        return textOf(byClass(byId(root, itemId), "text"));
    }

    /* (id, name) pair from the first <a> inside each ".<cls>" element of div#<itemId> */
    template <typename T>
    std::vector<T> linkedEntries(const GumboNode *root, const char *itemId, const QString &cls,
                                 const QString &key)
    {
        std::vector<T> out;
        const GumboNode *item = byId(root, itemId);
        if (!item)
            return out;
        for (const GumboNode *entry : allByClass(item, cls)) {
            const GumboNode *a = byTag(entry, GUMBO_TAG_A);
            if (!a)
                continue;
            T t;
            t.id = queryParam(a, key);
            if constexpr (requires { t.titleEn; })
                t.titleEn = textOf(a);
            else
                t.nameRo = textOf(a);
            out.push_back(t);
        }
        return out;
    }

    /* Absolute http(s) URL from an attribute, invalid otherwise */
    QUrl absoluteUrl(const char *value)
    {
        const QUrl url(QString::fromUtf8(value ? value : ""));
        return url.isValid() && url.scheme().startsWith("http") ? url : QUrl();
    }

    /* Jacket from img#video_jacket_img, plus the mirror given to its ThumbError handler */
    std::vector<model::Thumbnail> jacket(const GumboNode *root)
    {
        std::vector<model::Thumbnail> out;
        const GumboNode *img = byId(root, "video_jacket_img");
        if (const QUrl src = absoluteUrl(attr(img, "src")); !src.isEmpty())
            out.push_back({{}, {}, src});

        /* onerror="ThumbError(this, 'https://img2.pixhost.to/...jpg');" (or "ThumbError(this, 2);") */
        static const QRegularExpression fallbackRe(R"('(https?://[^']+)')");
        const char *onerror = attr(img, "onerror");
        const QRegularExpressionMatch match = fallbackRe.match(QString::fromUtf8(onerror ? onerror : ""));
        if (match.hasMatch())
            out.push_back({{}, {}, QUrl(match.captured(1))});
        return out;
    }

    /* div.previewthumbs holds <a href="large"><img src="small"></a> per still */
    std::vector<std::vector<model::Thumbnail>> previews(const GumboNode *root)
    {
        std::vector<std::vector<model::Thumbnail>> out;
        std::vector<const GumboNode *> anchors;
        findAll(byClass(root, "previewthumbs"),
                [](const GumboNode *n) { return n->v.element.tag == GUMBO_TAG_A; }, anchors);
        for (const GumboNode *a : anchors) {
            std::vector<model::Thumbnail> sizes;
            if (const QUrl small = absoluteUrl(attr(byTag(a, GUMBO_TAG_IMG), "src")); !small.isEmpty())
                sizes.push_back({{}, {}, small});
            if (const QUrl large = absoluteUrl(attr(a, "href")); !large.isEmpty())
                sizes.push_back({{}, {}, large});
            if (!sizes.empty())
                out.push_back(sizes);
        }
        return out;
    }

    /* Aliases are rendered as <span id="alias55465">(Otsu Alice)</span> after each star */
    void castAliases(const GumboNode *root, std::vector<model::Actress> &cast)
    {
        const std::vector<const GumboNode *> entries = allByClass(byId(root, "video_cast"), "cast");
        for (size_t i = 0; i < entries.size() && i < cast.size(); ++i) {
            const GumboVector &children = entries[i]->v.element.children;
            for (unsigned int c = 0; c < children.length; ++c) {
                auto *child = static_cast<const GumboNode *>(children.data[c]);
                const char *id = attr(child, "id");
                if (!id || std::strncmp(id, "alias", 5) != 0)
                    continue;
                QString alias = textOf(child);
                alias.remove('(').remove(')');
                if (!alias.trimmed().isEmpty())
                    cast[i].aliasesRo.append(alias.trimmed());
            }
        }
    }

    /* FlareSolverr wraps the page as {"solution": {"response": "<html>..."}} */
    std::optional<QString> unwrapFlareSolverr(const QString &payload)
    {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            spdlog::warn("Payload is not a FlareSolverr JSON reply: {}", error.errorString());
            return std::nullopt;
        }

        const QJsonObject root = doc.object();
        const QString status = root.value("status").toString();
        if (status != "ok")
            spdlog::warn("FlareSolverr status \"{}\": {}", status, root.value("message").toString());

        const QJsonObject solution = root.value("solution").toObject();
        const QJsonValue response = solution.value("response");
        if (!response.isString()) {
            spdlog::warn("FlareSolverr reply has no solution.response");
            return std::nullopt;
        }
        spdlog::debug("FlareSolverr solved {} (HTTP {})", solution.value("url").toString(),
                      solution.value("status").toInt());
        return response.toString();
    }

    /*
     * A search result list holds a div.video per match:
     * <div class="video"><a href="./javme3jfsq.html" title="IPZZ-782 ..."><div class="id">IPZZ-782</div>...
     * Href of the match whose id is `id` (also "DSVR-01660" for DSVR-1660), preferring the regular
     * edition over the "(Blu-ray Disc)" one; empty if there is none.
     */
    QString resultFor(const GumboNode *root, const model::JavId &id)
    {
        const QString wanted = Library::toString(id);
        QString found;
        for (const GumboNode *entry : allByClass(root, "video")) {
            const GumboNode *a = byTag(entry, GUMBO_TAG_A);
            const char *href = attr(a, "href");
            const std::optional<model::JavId> entryId = Library::parseJavId(textOf(byClass(entry, "id")));
            if (!href || !entryId || Library::toString(*entryId) != wanted)
                continue;
            const char *title = attr(a, "title");
            if (!QString::fromUtf8(title ? title : "").endsWith("(Blu-ray Disc)"))
                return QString::fromUtf8(href);
            if (found.isEmpty())
                found = QString::fromUtf8(href);
        }
        return found;
    }

} /* namespace */

JavLibClient::JavLibClient(NetworkManager &flareSolverr, QObject *parent)
    : QObject(parent), m_flareSolverr(flareSolverr)
{
    connect(&m_flareSolverr, &NetworkManager::downloadReady, this, &JavLibClient::downloadReady);
    connect(&m_flareSolverr, &NetworkManager::downloadFailed, this, &JavLibClient::downloadFailed);
}

QUrl JavLibClient::searchUrl(const model::JavId &id)
{
    QUrl url("https://www.javlibrary.com/en/vl_searchbyid.php");
    url.setQuery(QUrlQuery{{"keyword", Library::toString(id)}});
    return url;
}

void JavLibClient::lookup(const model::JavId &id)
{
    const QUrl url = searchUrl(id);
    m_pending.insert(url, id);
    m_flareSolverr.push(url);
}

void JavLibClient::downloadReady(const QUrl &url, const QString &payload)
{
    const auto it = m_pending.constFind(url);
    if (it == m_pending.constEnd())
        return;  /* not one of ours */
    const model::JavId id = *it;
    m_pending.erase(it);

    const model::Video video = parse(payload);
    if (!video.id.isEmpty()) {
        emit this->found(id, video);
        return;
    }

    /* Several matches (other ids starting alike, a Blu-ray edition) give a list to pick from */
    if (url.path().endsWith("/vl_searchbyid.php")) {
        if (const QUrl page = resultUrl(payload, id, url); page.isValid()) {
            spdlog::info("JavLibrary search for {} gave several matches, following {}", Library::toString(id), page);
            m_pending.insert(page, id);
            m_flareSolverr.push(page);
            return;
        }
    }
    spdlog::warn("JavLibrary has no video page for {}", Library::toString(id));
    emit this->notFound(id);
}

QUrl JavLibClient::resultUrl(const QString &payload, const model::JavId &id, const QUrl &searchUrl)
{
    const std::optional<QString> page = unwrapFlareSolverr(payload);
    if (!page)
        return {};
    const QByteArray html = page->toUtf8();
    GumboOutput *output = gumbo_parse_with_options(&kGumboDefaultOptions, html.constData(),
                                                   static_cast<size_t>(html.size()));
    const QString href = resultFor(output->root, id);
    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return href.isEmpty() ? QUrl() : searchUrl.resolved(QUrl(href));
}

void JavLibClient::downloadFailed(const QUrl &url)
{
    const auto it = m_pending.constFind(url);
    if (it == m_pending.constEnd())
        return;
    const model::JavId id = *it;
    m_pending.erase(it);
    emit this->notFound(id);
}

model::Video JavLibClient::parse(const QString &payload)
{
    model::Video video{};

    const std::optional<QString> page = unwrapFlareSolverr(payload);
    if (!page)
        return video;

    const QByteArray html = page->toUtf8();
    GumboOutput *output = gumbo_parse_with_options(&kGumboDefaultOptions, html.constData(),
                                                   static_cast<size_t>(html.size()));
    const GumboNode *root = output->root;

    video.provider = model::provider::JAVLIBRARY;
    video.id = itemText(root, "video_id");
    if (video.id.isEmpty()) {
        spdlog::debug("No video id found, page is not a video page");
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        return video;
    }

    /* Title is "<ID> <english title>" */
    QString title = textOf(byTag(byId(root, "video_title"), GUMBO_TAG_A));
    if (!video.id.isEmpty() && title.startsWith(video.id))
        title = title.mid(video.id.size()).trimmed();
    video.titleEn = title;

    QDate date = QDate::fromString(itemText(root, "video_date"), Qt::ISODate);
    if (date.isValid()) {
        video.releaseDate = std::chrono::year_month_day{
            std::chrono::year(date.year()),
            std::chrono::month(static_cast<unsigned>(date.month())),
            std::chrono::day(static_cast<unsigned>(date.day()))};
    }

    video.lengthMin = itemText(root, "video_length").toInt();

    auto directors = linkedEntries<model::Director>(root, "video_director", "director", "d");
    if (!directors.empty())
        video.director = directors.front();

    auto makers = linkedEntries<model::Maker>(root, "video_maker", "maker", "m");
    if (!makers.empty())
        video.maker = makers.front();

    video.genres = linkedEntries<model::Genre>(root, "video_genres", "genre", "g");
    video.cast = linkedEntries<model::Actress>(root, "video_cast", "cast", "s");
    castAliases(root, video.cast);

    /* Image candidates only: ThumbnailFetcher downloads them and adds other sizes */
    video.covers = jacket(root);
    video.previews = previews(root);

    /* Score is rendered as "(8.40)" */
    QString score = textOf(byClass(byId(root, "video_review"), "score"));
    score.remove('(').remove(')');
    bool ok = false;
    double value = score.toDouble(&ok);
    if (ok)
        video.rating = model::UserRating{{}, value};

    /* Missing fields usually mean the page layout changed */
    if (video.titleEn.isEmpty())
        spdlog::warn("{}: no title found", video.id);
    if (!date.isValid())
        spdlog::warn("{}: no valid release date found", video.id);
    if (video.lengthMin <= 0)
        spdlog::warn("{}: no length found", video.id);
    if (video.cast.empty())
        spdlog::debug("{}: no cast listed", video.id);
    if (video.covers.empty())
        spdlog::warn("{}: no jacket image found", video.id);

    spdlog::debug("Parsed {}: {} jacket url(s), {} preview(s)", video.id, video.covers.size(),
                  video.previews.size());

    spdlog::debug("Parsed {}: {} min, {} genre(s), {} cast, rating {}", video.id, video.lengthMin,
                  video.genres.size(), video.cast.size(),
                  video.rating ? QString::number(video.rating->value) : QString("none"));

    gumbo_destroy_output(&kGumboDefaultOptions, output);
    return video;
}
