#include "gui/mainwindow.hpp"
#include "gui/style.hpp"
#include "log.hpp"

#include <stdexcept>

#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QShortcut>
#include <QVBoxLayout>

MainWindow::MainWindow(Options &options, DBManager &db, Scraper &scraper, QWidget *parent)
    : QMainWindow(parent), m_options(options), m_db(db), m_scraper(scraper), m_videos(m_images)
{
    this->setWindowTitle("JAV-Qt");
    this->resize(1600, 900);

    /* Top bar: title, scraping status, import */
    auto *topBar = new QFrame;
    topBar->setObjectName("topBar");
    topBar->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
    auto *top = new QHBoxLayout(topBar);
    top->setContentsMargins(16, 6, 16, 6);
    auto *title = new QLabel("JAV-Qt");
    title->setObjectName("boardTitle");
    m_status = new QLabel;
    m_status->setObjectName("status");
    m_import = new QPushButton(tr("Import From Folder (Library)"));
    top->addWidget(title);
    top->addStretch(1);
    top->addWidget(m_status);
    top->addSpacing(16);
    top->addWidget(m_import);

    m_libraryPage = new LibraryPage(m_videos);
    m_videoPage = new VideoPage(m_images);
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_libraryPage);
    m_pages->addWidget(m_videoPage);

    /* The Yotsuba B fade is painted on this, behind everything (see style::sheet) */
    auto *central = new QWidget;
    central->setObjectName("central");
    central->setAttribute(Qt::WA_StyledBackground);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(topBar);
    layout->addWidget(m_pages, 1);
    this->setCentralWidget(central);

    /* Wiring */
    connect(m_import, &QPushButton::clicked, this, &MainWindow::importFolder);
    connect(m_libraryPage, &LibraryPage::videoActivated, this, &MainWindow::openVideo);
    connect(m_videoPage, &VideoPage::back, this, &MainWindow::showLibrary);
    connect(m_videoPage, &VideoPage::filterRequested, this, [this](LibraryPage::Filter filter, const QString &value) {
        m_libraryPage->showOnly(filter, value);
        this->showLibrary();
    });
    connect(m_videoPage, &VideoPage::notesEdited, this, [this](const QString &id, const QString &notes) {
        try {
            m_db.storeNotes(id, notes);
        } catch (const std::exception &e) {
            spdlog::error("Could not store notes of {}: {}", id, e.what());
        }
    });
    auto *escape = new QShortcut(Qt::Key_Escape, m_videoPage);
    connect(escape, &QShortcut::activated, m_videoPage, [this] {
        m_videoPage->flushNotes();
        this->showLibrary();
    });

    connect(m_videoPage, &VideoPage::starsEdited, this, [this](const QString &id, double stars) {
        this->editUserField(id, [&](model::Video &video) { video.stars = stars; },
                            [&] { m_db.storeStars(id, stars); });
    });
    connect(m_videoPage, &VideoPage::bookmarkedEdited, this, [this](const QString &id, bool bookmarked) {
        this->editUserField(id, [&](model::Video &video) { video.bookmarked = bookmarked; },
                            [&] { m_db.storeBookmarked(id, bookmarked); });
    });

    connect(&m_scraper, &Scraper::stored, this, [this](const model::Video &video) {
        /* Reloaded: the stored entry kept the user's stars and bookmark */
        m_videos.upsert(m_db.loadDB(model::Type::Video, video.id));
        this->updateStatus();
    });
    connect(&m_scraper, &Scraper::skipped, this, &MainWindow::updateStatus);

    try {
        m_videos.setVideos(m_db.loadAllVideos());
    } catch (const std::exception &e) {
        spdlog::error("Could not load the database: {}", e.what());
    }
    m_library = std::make_unique<Library>(m_options);
    this->updateStatus();
}

void MainWindow::scanLibrary()
{
    m_library = std::make_unique<Library>(m_options);
    int queued = 0;
    for (const model::JavId &javId : m_library->searchJav()) {
        const QString id = Library::toString(javId);
        const model::Video stored = m_db.loadDB(model::Type::Video, id);
        if (!stored.provider.isEmpty() && !stored.covers.empty()) {
            spdlog::debug("{} already in database, skipping", id);
            continue;
        }
        m_scraper.scrape(javId);
        ++queued;
    }
    spdlog::info("Queued {} video(s), one JavLibrary download every {} s", queued,
                 std::chrono::duration_cast<std::chrono::seconds>(NetworkManager::START_INTERVAL).count());
    this->updateStatus();
}

void MainWindow::importFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Import From Folder (Library)"),
                                                          m_library ? m_library->root() : QString());
    if (dir.isEmpty())
        return;

    m_options.m_libraryDir = dir;
    try {
        m_options.saveOptions();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Import From Folder"),
                             tr("The folder is used now, but could not be saved for next time:\n%1").arg(e.what()));
    }
    this->scanLibrary();
}

void MainWindow::openVideo(const model::Video &video)
{
    QString notes;
    try {
        notes = m_db.loadNotes(video.id);
    } catch (const std::exception &e) {
        spdlog::error("Could not load notes of {}: {}", video.id, e.what());
    }
    m_videoPage->showVideo(video, m_library->files(video.id), notes);
    m_import->hide();
    m_pages->setCurrentWidget(m_videoPage);
}

void MainWindow::showLibrary()
{
    m_import->show();
    m_pages->setCurrentWidget(m_libraryPage);
}

void MainWindow::editUserField(const QString &id, const std::function<void(model::Video &)> &edit,
                               const std::function<void()> &store)
{
    try {
        store();
    } catch (const std::exception &e) {
        spdlog::error("Could not store {}: {}", id, e.what());
        return;
    }
    if (const model::Video *stored = m_videos.find(id)) {
        model::Video video = *stored;
        edit(video);
        m_videos.upsert(video);  /* re-sorts the library */
    }
}

void MainWindow::updateStatus()
{
    const int pending = m_scraper.pendingCount();
    m_status->setText(pending > 0 ? tr("Scraping %n video(s)…", nullptr, pending) : QString());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_videoPage->flushNotes();
    QMainWindow::closeEvent(event);
}
