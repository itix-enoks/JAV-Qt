#include "gui/mainwindow.hpp"
#include "gui/style.hpp"
#include "log.hpp"

#include <stdexcept>

#include <QCloseEvent>
#include <QFileDialog>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QVBoxLayout>

MainWindow::MainWindow(Options &options, DBManager &db, Scraper &scraper, QWidget *parent)
    : QMainWindow(parent), m_options(options), m_db(db), m_scraper(scraper), m_videos(m_images)
{
    this->setWindowTitle("JAV-Qt");
    this->resize(1600, 900);

    /* Top bar: import, scraping status, theme */
    auto *topBar = new QFrame;
    topBar->setObjectName("topBar");
    topBar->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
    auto *top = new QHBoxLayout(topBar);
    top->setContentsMargins(16, 6, 16, 6);
    m_import = new QPushButton(tr("Import"));
    m_import->setToolTip(tr("Pick the folder with your videos"));
    m_status = new QLabel;
    m_status->setObjectName("status");
    m_theme = new QPushButton(tr("Select theme"));
    auto *themes = new QMenu(m_theme);
    auto *themeGroup = new QActionGroup(themes);
    for (const style::Theme &theme : style::themes()) {
        QAction *action = themes->addAction(theme.name, this, [this, name = theme.name] { this->setTheme(name); });
        action->setCheckable(true);
        action->setChecked(theme.name == style::theme().name);
        themeGroup->addAction(action);
    }
    m_theme->setMenu(themes);
    top->addWidget(m_import);
    top->addStretch(1);
    top->addWidget(m_status);
    top->addSpacing(16);
    top->addWidget(m_theme);

    m_libraryPage = new LibraryPage(m_videos);
    m_videoPage = new VideoPage(m_images);
    m_pages = new QStackedWidget;
    m_pages->addWidget(m_libraryPage);
    m_pages->addWidget(m_videoPage);

    /* The theme's fade is painted on this, behind everything (see style::sheet) */
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
    m_libraryPage->setZoom(m_options.m_posterZoom);
    connect(m_libraryPage, &LibraryPage::zoomChanged, this, [this](double zoom) {
        m_options.m_posterZoom = zoom;
        try {
            m_options.saveOptions();
        } catch (const std::exception &e) {
            spdlog::warn("Could not save the zoom: {}", e.what());
        }
    });
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
        const bool japaneseOnly = stored.provider == model::provider::DMM && stored.titleEn.isEmpty();
        if (!stored.provider.isEmpty() && !stored.covers.empty() && !japaneseOnly) {
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
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Import"),
                                                          m_library ? m_library->root() : QString());
    if (dir.isEmpty())
        return;

    m_options.m_libraryDir = dir;
    try {
        m_options.saveOptions();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Import"),
                             tr("The folder is used now, but could not be saved for next time:\n%1").arg(e.what()));
    }
    this->scanLibrary();
}

void MainWindow::setTheme(const QString &name)
{
    style::apply(name);  /* repolishes, so repaints, every widget */
    m_videoPage->restyle();

    m_options.m_theme = style::theme().name;
    try {
        m_options.saveOptions();
    } catch (const std::exception &e) {
        spdlog::warn("Could not save the theme: {}", e.what());
    }
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
