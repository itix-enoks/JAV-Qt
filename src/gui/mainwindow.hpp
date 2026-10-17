#pragma once

#include <memory>

#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStackedWidget>

#include "dbm.hpp"
#include "library.hpp"
#include "options.hpp"
#include "scraper.hpp"
#include "gui/imagecache.hpp"
#include "gui/librarypage.hpp"
#include "gui/videolist.hpp"
#include "gui/videopage.hpp"

/**
 * Top bar over two pages: the library (landing page) and a video.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(Options &options, DBManager &db, Scraper &scraper, QWidget *parent = nullptr);

    /*
     * Scan the library folder from the options and scrape every id that isn't
     * stored yet. Entries without a provider predate DMM support (their cast
     * may not match its pictures), and entries without a jacket may have been
     * stored before thumbnails.
     */
    void scanLibrary();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void importFolder();
    void openVideo(const model::Video &video);
    void showLibrary();
    void updateStatus();

    Options &m_options;
    DBManager &m_db;
    Scraper &m_scraper;
    std::unique_ptr<Library> m_library;

    ImageCache m_images;
    VideoListModel m_videos;

    QStackedWidget *m_pages;
    LibraryPage *m_libraryPage;
    VideoPage *m_videoPage;
    QPushButton *m_import;
    QLabel *m_status;
};
