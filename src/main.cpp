#include <string>
#include <cstdlib>
#include <stdexcept>

#include <QApplication>
#include <QTimer>

#include <spdlog/cfg/env.h>

#include "dbm.hpp"
#include "log.hpp"
#include "network.hpp"
#include "options.hpp"
#include "scraper.hpp"
#include "gui/mainwindow.hpp"
#include "gui/style.hpp"

#include "models/video.hpp"

int main(int argc, char *argv[]) {
    QApplication javQt(argc, argv);
    javQt.setStyle("Fusion");  /* the same base look everywhere, under the style sheet */
    javQt.setStyleSheet(style::sheet());

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

    MainWindow window(options, db, scraper);
    window.scanLibrary();

    /* Start one download now, then one every START_INTERVAL */
    QTimer downloadTimer;
    downloadTimer.setTimerType(Qt::PreciseTimer);  /* coarse timers may fire early */
    downloadTimer.setInterval(NetworkManager::START_INTERVAL);
    QObject::connect(&downloadTimer, &QTimer::timeout, &manager, &NetworkManager::startDownload);
    manager.startDownload();
    downloadTimer.start();

    window.show();

    return javQt.exec();
}
