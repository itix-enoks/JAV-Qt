#include "gui/videopage.hpp"
#include "gui/display.hpp"
#include "gui/style.hpp"
#include "thumbnails.hpp"

#include <QDate>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace {

    constexpr int SIDEBAR_WIDTH = 300;
    constexpr int INFO_WIDTH = 460;
    constexpr QSize PREVIEW_ICON{160, 120};
    constexpr int CAST_PHOTO = 100;
    constexpr int CAST_COLUMNS = 3;

    /* Qt's rich text paints an <hr> with its background color, ignoring borders */
    QString divider()
    {
        return QString("<hr style=\"background-color: %1;\"/>").arg(style::POST_BORDER.name());
    }

    QFrame *post()
    {
        auto *frame = new QFrame;
        frame->setObjectName("post");
        frame->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
        return frame;
    }

    QString date(const std::chrono::year_month_day &ymd)
    {
        if (!ymd.ok())
            return {};
        return QDate(int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day())).toString(Qt::ISODate);
    }

    /* "Kuraki Shiori (倉木しおり)", or just one of them if that's all there is */
    template <typename T>
    QString bothNames(const T &person)
    {
        if (person.nameRo.isEmpty() || person.nameJa.isEmpty() || person.nameRo == person.nameJa)
            return display::name(person).toHtmlEscaped();
        return QString("%1 <span style=\"color: #707070;\">(%2)</span>")
            .arg(person.nameRo.toHtmlEscaped(), person.nameJa.toHtmlEscaped());
    }

    template <typename T>
    QString personLink(const char *kind, const T &person)
    {
        const QString name = display::name(person);
        QString html = style::link(QString("%1:%2").arg(kind, name), name);
        if (!person.nameRo.isEmpty() && !person.nameJa.isEmpty() && person.nameRo != person.nameJa)
            html += QString(" <span style=\"color: #707070;\">(%1)</span>").arg(person.nameJa.toHtmlEscaped());
        return html;
    }

} /* namespace */

ImageView::ImageView(ImageCache &images, QWidget *parent) : QWidget(parent), m_images(images)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMinimumSize(320, 220);
    connect(&m_images, &ImageCache::loaded, this, [this](const QString &path) {
        if (path == m_path)
            this->update();
    });
}

void ImageView::setPath(const QString &path)
{
    m_path = path;
    this->update();
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QPixmap pixmap = m_images.get(m_path, LOAD_SIZE);
    QSize size = pixmap.isNull() ? QSize(800, 538) : pixmap.size() / pixmap.devicePixelRatio();
    size = size.scaled(this->size() - QSize(2, 2), Qt::KeepAspectRatio);
    QRect target(QPoint(), size);
    target.moveCenter(this->rect().center());

    painter.fillRect(target, Qt::white);
    if (!pixmap.isNull())
        painter.drawPixmap(target, pixmap);
    else if (m_path.isEmpty())
        painter.drawText(target, Qt::AlignCenter, tr("No jacket image"));
    painter.setPen(style::POST_BORDER);
    painter.drawRect(target.adjusted(-1, -1, 0, 0));
}

VideoPage::VideoPage(ImageCache &images, QWidget *parent) : QWidget(parent), m_images(images)
{
    /* Sidebar: Back */
    auto *sideBar = new QFrame;
    sideBar->setObjectName("sideBar");
    sideBar->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
    sideBar->setFixedWidth(SIDEBAR_WIDTH);
    auto *side = new QVBoxLayout(sideBar);
    side->setContentsMargins(16, 16, 16, 16);
    auto *backButton = new QPushButton(tr("Back"));
    side->addWidget(backButton);
    side->addStretch(1);

    /* Center: jacket, Play, preview stills */
    m_cover = new ImageView(m_images);
    m_play = new QPushButton(tr("▶  Play"));
    m_play->setObjectName("play");
    m_play->setCursor(Qt::PointingHandCursor);

    m_previews = new QListWidget;
    m_previews->setObjectName("previews");
    m_previews->setViewMode(QListView::IconMode);
    m_previews->setFlow(QListView::LeftToRight);
    m_previews->setWrapping(false);
    m_previews->setMovement(QListView::Static);
    m_previews->setIconSize(PREVIEW_ICON);
    m_previews->setSpacing(4);
    m_previews->setFixedHeight(PREVIEW_ICON.height() + 30);
    m_previews->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_previews->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previews->setToolTip(tr("Double-click to open the full size still"));

    auto *center = new QVBoxLayout;
    center->setSpacing(16);
    center->addWidget(m_cover, 1);
    center->addWidget(m_play, 0, Qt::AlignHCenter);
    center->addWidget(m_previews);

    /* Right: metadata and notes, each in a post box */
    m_metadata = new QLabel;
    m_metadata->setTextFormat(Qt::RichText);
    m_metadata->setWordWrap(true);
    m_metadata->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_metadata->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_metadata->setOpenExternalLinks(false);

    auto *metadataPost = post();
    auto *metadataLayout = new QVBoxLayout(metadataPost);
    metadataLayout->setContentsMargins(14, 12, 14, 14);
    metadataLayout->addWidget(m_metadata);
    metadataLayout->addStretch(1);

    auto *metadataScroll = new QScrollArea;
    metadataScroll->setWidgetResizable(true);
    metadataScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    metadataScroll->setWidget(metadataPost);
    metadataPost->setAutoFillBackground(false);

    auto *notesPost = post();
    auto *notesLayout = new QVBoxLayout(notesPost);
    notesLayout->setContentsMargins(14, 10, 14, 14);
    auto *notesTitle = new QLabel(tr("Notes"));
    notesTitle->setObjectName("sectionTitle");
    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(tr("Write your notes about this video here"));
    notesLayout->addWidget(notesTitle);
    notesLayout->addWidget(m_notes);

    auto *info = new QVBoxLayout;
    info->setSpacing(16);
    info->addWidget(metadataScroll, 3);
    info->addWidget(notesPost, 2);
    auto *infoWidget = new QWidget;
    infoWidget->setFixedWidth(INFO_WIDTH);
    infoWidget->setLayout(info);
    info->setContentsMargins(0, 0, 0, 0);

    auto *main = new QHBoxLayout;
    main->setContentsMargins(32, 32, 32, 24);
    main->setSpacing(32);
    main->addLayout(center, 1);
    main->addWidget(infoWidget);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(sideBar);
    layout->addLayout(main, 1);

    /* Wiring */
    connect(backButton, &QPushButton::clicked, this, [this] {
        this->flushNotes();
        emit this->back();
    });
    connect(m_metadata, &QLabel::linkActivated, this, &VideoPage::openLink);
    connect(m_previews, &QListWidget::itemActivated, this, [](QListWidgetItem *item) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));
    });

    m_saveNotes.setSingleShot(true);
    m_saveNotes.setInterval(700);
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] {
        m_notesDirty = true;
        m_saveNotes.start();
    });
    connect(&m_saveNotes, &QTimer::timeout, this, &VideoPage::flushNotes);

    connect(&m_images, &ImageCache::loaded, this, [this](const QString &path) {
        for (int i = 0; i < m_previews->count(); ++i) {
            QListWidgetItem *item = m_previews->item(i);
            if (item->data(Qt::UserRole + 1).toString() == path)
                item->setIcon(QIcon(m_images.get(path, PREVIEW_ICON)));
        }
    });
}

void VideoPage::flushNotes()
{
    m_saveNotes.stop();
    if (!m_notesDirty || m_id.isEmpty())
        return;
    m_notesDirty = false;
    emit this->notesEdited(m_id, m_notes->toPlainText());
}

void VideoPage::showVideo(const model::Video &video, const QStringList &files, const QString &notes)
{
    this->flushNotes();
    m_id = video.id;

    const model::Thumbnail *cover = display::pick(video.covers, 100000);  /* the largest */
    m_cover->setPath(cover ? ThumbnailFetcher::absolutePath(cover->path) : QString());

    this->setFiles(files);
    this->showPreviews(video);
    this->showMetadata(video, int(files.size()));

    const QSignalBlocker blocker(m_notes);
    m_notes->setPlainText(notes);
    m_notesDirty = false;
}

void VideoPage::setFiles(const QStringList &files)
{
    if (QMenu *old = m_play->menu()) {
        m_play->setMenu(nullptr);
        old->deleteLater();
    }
    m_play->disconnect(this);
    m_play->setEnabled(!files.isEmpty());
    m_play->setToolTip(files.isEmpty() ? tr("Not found in the library folder") : files.join('\n'));

    if (files.size() == 1) {
        connect(m_play, &QPushButton::clicked, this,
                [file = files.first()] { QDesktopServices::openUrl(QUrl::fromLocalFile(file)); });
    } else if (files.size() > 1) {
        /* Multi-part video: pick the part */
        auto *menu = new QMenu(m_play);
        for (qsizetype i = 0; i < files.size(); ++i) {
            const QString file = files[i];
            menu->addAction(tr("Part %1: %2").arg(i + 1).arg(QFileInfo(file).fileName()), this,
                            [file] { QDesktopServices::openUrl(QUrl::fromLocalFile(file)); });
        }
        m_play->setMenu(menu);
    }
}

void VideoPage::showPreviews(const model::Video &video)
{
    m_previews->clear();
    for (const auto &sizes : video.previews) {
        const model::Thumbnail *small = display::pick(sizes, PREVIEW_ICON.width());
        const model::Thumbnail *large = display::pick(sizes, 100000);
        if (!small)
            continue;
        const QString iconPath = ThumbnailFetcher::absolutePath(small->path);
        auto *item = new QListWidgetItem(m_previews);
        item->setData(Qt::UserRole, ThumbnailFetcher::absolutePath(large->path));
        item->setData(Qt::UserRole + 1, iconPath);
        item->setSizeHint(PREVIEW_ICON + QSize(8, 8));
        if (const QPixmap pixmap = m_images.get(iconPath, PREVIEW_ICON); !pixmap.isNull())
            item->setIcon(QIcon(pixmap));
    }
    m_previews->setVisible(m_previews->count() > 0);
}

void VideoPage::showMetadata(const model::Video &video, int fileCount)
{
    const QString label = QString("<td style=\"color: %1; font-weight: bold; padding: 2px 12px 2px 0;\" "
                                  "valign=\"top\">%2</td>")
                              .arg(style::NAME.name());
    auto row = [&label](const QString &name, const QString &valueHtml) {
        return valueHtml.isEmpty() ? QString()
                                   : QString("<tr>%1<td style=\"padding: 2px 0;\">%2</td></tr>")
                                         .arg(label.arg(name.toHtmlEscaped()), valueHtml);
    };

    QString html = QString("<p style=\"color: %1; font-weight: bold; font-size: 15pt; margin: 0 0 6px 0;\">%2</p>")
                       .arg(style::SUBJECT.name(), video.id.toHtmlEscaped());
    if (!video.titleEn.isEmpty())
        html += QString("<p style=\"color: %1; font-weight: bold; margin: 0 0 4px 0;\">%2</p>")
                    .arg(style::SUBJECT.name(), video.titleEn.toHtmlEscaped());
    if (!video.titleJa.isEmpty())
        html += QString("<p style=\"margin: 0 0 4px 0;\">%1</p>").arg(video.titleJa.toHtmlEscaped());

    QStringList genres;
    for (const model::Genre &genre : video.genres)
        genres.append(style::link("genre:" + display::name(genre), display::name(genre)));

    QString source = video.provider == model::provider::DMM ? QString("DMM (%1)").arg(video.dmmId.toHtmlEscaped())
                     : video.provider == model::provider::JAVLIBRARY ? QString("JavLibrary")
                                                                      : QString();

    html += divider() + "<table cellspacing=\"0\">";
    html += row(tr("Released"), date(video.releaseDate));
    html += row(tr("Length"), video.lengthMin > 0 ? tr("%1 min").arg(video.lengthMin) : QString());
    html += row(tr("Rating"), video.rating ? QString("%1 / 10").arg(video.rating->value, 0, 'f', 1) : QString());
    html += row(tr("Director"), video.director ? personLink("director", *video.director) : QString());
    html += row(tr("Maker"), video.maker ? personLink("maker", *video.maker) : QString());
    html += row(tr("Genres"), genres.join(", "));
    html += row(tr("Source"), source);
    html += row(tr("Files"), fileCount > 0 ? tr("%n file(s)", nullptr, fileCount)
                                           : QString("<span style=\"color: #707070;\">%1</span>")
                                                 .arg(tr("not in the library folder")));
    html += "</table>";

    /* Cast, with each actress's own picture */
    if (!video.cast.empty()) {
        html += divider();
        html += QString("<p style=\"color: %1; font-weight: bold; margin: 0 0 6px 0;\">%2</p>"
                        "<table cellspacing=\"6\"><tr>")
                    .arg(style::NAME.name(), tr("Cast"));
        for (size_t i = 0; i < video.cast.size(); ++i) {
            const model::Actress &actress = video.cast[i];
            if (i > 0 && i % CAST_COLUMNS == 0)
                html += "</tr><tr>";
            const model::Thumbnail *photo = display::pick(actress.thumbnails, CAST_PHOTO);
            const QString name = display::name(actress);
            QString cell = photo ? QString("<a href=\"actress:%1\"><img src=\"%2\" width=\"%3\" height=\"%3\"/></a><br/>")
                                       .arg(name.toHtmlEscaped(),
                                            QUrl::fromLocalFile(ThumbnailFetcher::absolutePath(photo->path)).toString())
                                       .arg(CAST_PHOTO)
                                 : QString();
            cell += style::link("actress:" + name, name);
            if (!actress.nameJa.isEmpty() && actress.nameJa != name)
                cell += QString("<br/><span style=\"color: #707070;\">%1</span>").arg(actress.nameJa.toHtmlEscaped());
            html += QString("<td align=\"center\" valign=\"top\" width=\"%1\">%2</td>").arg(CAST_PHOTO + 24).arg(cell);
        }
        html += "</tr></table>";
    }

    m_metadata->setText(html);
}

void VideoPage::openLink(const QString &link)
{
    const QString kind = link.section(':', 0, 0);
    const QString value = link.section(':', 1);
    if (kind == "actress")
        emit this->filterRequested(LibraryPage::Filter::Actress, value);
    else if (kind == "director")
        emit this->filterRequested(LibraryPage::Filter::Director, value);
    else if (kind == "maker")
        emit this->filterRequested(LibraryPage::Filter::Maker, value);
    else if (kind == "genre")
        emit this->filterRequested(LibraryPage::Filter::Genre, value);
}
