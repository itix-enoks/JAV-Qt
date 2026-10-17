#include <string>
#include <cstdlib>
#include <stdexcept>

#include <QApplication>
#include <QTimer>

#include <spdlog/cfg/env.h>

#include "dbm.hpp"
#include "library.hpp"
#include "log.hpp"
#include "network.hpp"
#include "options.hpp"
#include "scraper.hpp"

#include "models/video.hpp"

int main(int argc, char *argv[]) {
    QApplication javQt(argc, argv);

    /* Logging: level defaults to info, override with e.g. SPDLOG_LEVEL=debug */
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    spdlog::cfg::load_env_levels();
    spdlog::info("JAV-Qt starting");

    /* Load options */
    Options options;

    DBManager db;
    NetworkManager manager(options);

    /* DMM first, JavLibrary for English text or as the fallback */
    Scraper scraper(manager, db);

    /*
     * Scrape every id in the library that isn't stored yet. Entries without a
     * provider predate DMM support (their cast may not match its pictures),
     * and entries without a jacket may have been stored before thumbnails.
     */
    Library library(options);
    int queued = 0;
    for (const model::JavId &javId : library.searchJav()) {
        const QString id = Library::toString(javId);
        const model::Video stored = db.loadDB(model::Type::Video, id);
        if (!stored.provider.isEmpty() && !stored.covers.empty()) {
            spdlog::debug("{} already in database, skipping", id);
            continue;
        }
        scraper.scrape(javId);
        ++queued;
    }
    spdlog::info("Queued {} video(s), one JavLibrary download every {} s", queued,
                 std::chrono::duration_cast<std::chrono::seconds>(NetworkManager::START_INTERVAL).count());

    /* Start one download now, then one every START_INTERVAL */
    QTimer downloadTimer;
    downloadTimer.setTimerType(Qt::PreciseTimer);  /* coarse timers may fire early */
    downloadTimer.setInterval(NetworkManager::START_INTERVAL);
    QObject::connect(&downloadTimer, &QTimer::timeout, &manager, &NetworkManager::startDownload);
    manager.startDownload();
    downloadTimer.start();

    return javQt.exec();
}
