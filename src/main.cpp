#include <cstdio>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include <QApplication>
#include <QIcon>
#include <QTimer>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "dbm.hpp"
#include "log.hpp"
#include "network.hpp"
#include "options.hpp"
#include "paths.hpp"
#include "scraper.hpp"
#include "gui/mainwindow.hpp"
#include "gui/style.hpp"

#include "models/video.hpp"

#ifndef JAVQT_VERSION
#define JAVQT_VERSION "dev"
#endif

namespace {

    /*
     * Log to the terminal and to jav-qt.log in the data folder, which is the
     * only place the log can be found when started from a desktop.
     * The level defaults to info, override it with e.g. SPDLOG_LEVEL=debug.
     */
    void setUpLogging()
    {
        std::vector<spdlog::sink_ptr> sinks{std::make_shared<spdlog::sinks::stderr_color_sink_mt>()};
        try {
            constexpr size_t MAX_SIZE = 1024 * 1024, MAX_FILES = 3;
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                paths::dataPath("jav-qt.log").toStdString(), MAX_SIZE, MAX_FILES));
        } catch (const spdlog::spdlog_ex &e) {
            std::fprintf(stderr, "Could not open the log file: %s\n", e.what());
        }
        auto logger = std::make_shared<spdlog::logger>("jav-qt", sinks.begin(), sinks.end());
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        spdlog::cfg::load_env_levels();
    }

} /* namespace */

int main(int argc, char *argv[]) {
    QApplication javQt(argc, argv);
    QApplication::setApplicationName("JAV-Qt");  /* names the data folder, see paths::dataDir() */
    QApplication::setApplicationVersion(JAVQT_VERSION);
    QApplication::setWindowIcon(QIcon(":/icons/jav-qt.png"));

    setUpLogging();
    spdlog::info("JAV-Qt {} starting, data in {}", JAVQT_VERSION, paths::dataDir());

    javQt.setStyle("Fusion");  /* the same base look everywhere, under the style sheet */
    style::loadFonts();
    javQt.setStyleSheet(style::sheet());

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
