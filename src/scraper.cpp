#include "scraper.hpp"
#include "library.hpp"
#include "log.hpp"

#include <stdexcept>

Scraper::Scraper(NetworkManager &flareSolverr, DBManager &db, QObject *parent)
    : QObject(parent), m_db(db), m_javLibrary(flareSolverr)
{
    connect(&m_dmm, &DmmClient::found, this,
            [this](const model::JavId &id, const model::Video &video) { this->dmmDone(id, video); });
    connect(&m_dmm, &DmmClient::notFound, this,
            [this](const model::JavId &id) { this->dmmDone(id, std::nullopt); });

    connect(&m_javLibrary, &JavLibClient::found, this,
            [this](const model::JavId &id, const model::Video &video) { this->javLibraryDone(id, video); });
    connect(&m_javLibrary, &JavLibClient::notFound, this,
            [this](const model::JavId &id) { this->javLibraryDone(id, std::nullopt); });

    connect(&m_thumbnails, &ThumbnailFetcher::finished, this, [this](const model::Video &video) {
        --m_fetching;
        try {
            m_db.storeDB(video);
            spdlog::info("Stored {} from {} \"{}\"", video.id, video.provider,
                         video.titleEn.isEmpty() ? video.titleJa : video.titleEn);
            emit this->stored(video);
        } catch (const std::exception &e) {
            spdlog::error("Could not store {}: {}", video.id, e.what());
            emit this->skipped(video.id);
        }
    });
}

void Scraper::scrape(const model::JavId &id)
{
    const QString key = Library::toString(id);
    if (m_pending.contains(key)) {
        spdlog::debug("{} is already being scraped", key);
        return;
    }
    m_pending.insert(key, Pending{});
    m_dmm.lookup(id);
    m_javLibrary.lookup(id);
}

void Scraper::dmmDone(const model::JavId &id, std::optional<model::Video> video)
{
    const QString key = Library::toString(id);
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;
    it->dmm = std::move(video);
    it->dmmDone = true;
    this->settle(key);
}

void Scraper::javLibraryDone(const model::JavId &id, std::optional<model::Video> video)
{
    const QString key = Library::toString(id);
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;
    it->javLibrary = std::move(video);
    it->javLibraryDone = true;
    this->settle(key);
}

void Scraper::settle(const QString &key)
{
    auto it = m_pending.find(key);
    if (it == m_pending.end() || !it->dmmDone || !it->javLibraryDone)
        return;
    const Pending pending = *it;
    m_pending.erase(it);

    std::optional<model::Video> javLibrary = pending.javLibrary;
    const bool mismatch = javLibrary && javLibrary->id.compare(key, Qt::CaseInsensitive) != 0;

    if (pending.dmm) {
        if (mismatch) {
            spdlog::warn("JavLibrary search for {} led to {}, ignoring it", key, javLibrary->id);
            javLibrary.reset();
        } else if (!javLibrary) {
            spdlog::info("{}: not on JavLibrary, storing DMM's Japanese text only", key);
        }
        ++m_fetching;
        m_thumbnails.fetch(merge(*pending.dmm, javLibrary));
    } else if (javLibrary) {
        if (mismatch)
            spdlog::warn("Searched for {} but got {}, storing it as {}", key, javLibrary->id, javLibrary->id);
        spdlog::info("{}: not on DMM, using JavLibrary", key);
        ++m_fetching;
        m_thumbnails.fetch(*javLibrary);
    } else {
        spdlog::warn("{} found on neither DMM nor JavLibrary, skipping", key);
        emit this->skipped(key);
    }
}

model::Video Scraper::merge(const model::Video &dmm, const std::optional<model::Video> &javLibrary)
{
    model::Video video = dmm;
    if (!javLibrary)
        return video;

    /*
     * Only fields that can't mismatch are taken: the cast stays DMM's, so each
     * actress keeps her own picture.
     */
    video.titleEn = javLibrary->titleEn;
    if (!javLibrary->genres.empty())
        video.genres = javLibrary->genres;
    if (video.director && javLibrary->director)
        video.director->nameRo = javLibrary->director->nameRo;
    if (video.maker && javLibrary->maker)
        video.maker->nameRo = javLibrary->maker->nameRo;
    if (javLibrary->rating)
        video.rating = javLibrary->rating;

    /* Extra jacket sources (other sizes, mirrors); ThumbnailFetcher drops duplicates */
    video.covers.insert(video.covers.end(), javLibrary->covers.begin(), javLibrary->covers.end());
    if (video.previews.empty())
        video.previews = javLibrary->previews;
    return video;
}
