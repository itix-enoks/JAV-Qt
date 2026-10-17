#include "gui/videolist.hpp"
#include "gui/display.hpp"
#include "gui/style.hpp"
#include "thumbnails.hpp"

#include <algorithm>

#include <QPainter>

VideoListModel::VideoListModel(ImageCache &images, QObject *parent) : QAbstractListModel(parent), m_images(images)
{
    /* Repaint the posters whose image just finished loading */
    connect(&m_images, &ImageCache::loaded, this, [this](const QString &path) {
        for (size_t row = 0; row < m_videos.size(); ++row)
            if (posterPath(m_videos[row]) == path)
                emit this->dataChanged(this->index(int(row)), this->index(int(row)), {Qt::DecorationRole});
    });
}

void VideoListModel::setVideos(std::vector<model::Video> videos)
{
    this->beginResetModel();
    m_videos = std::move(videos);
    this->endResetModel();
}

void VideoListModel::upsert(const model::Video &video)
{
    const auto it = std::find_if(m_videos.begin(), m_videos.end(),
                                 [&video](const model::Video &v) { return v.id == video.id; });
    if (it != m_videos.end()) {
        *it = video;
        const QModelIndex changed = this->index(int(it - m_videos.begin()));
        emit this->dataChanged(changed, changed);
        return;
    }
    this->beginInsertRows({}, int(m_videos.size()), int(m_videos.size()));
    m_videos.push_back(video);
    this->endInsertRows();
}

int VideoListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_videos.size());
}

QString VideoListModel::posterPath(const model::Video &video)
{
    /* The front of the jacket; the whole jacket if there is no poster */
    const model::Thumbnail *t = display::pick(video.posters, POSTER_SIZE.width());
    if (!t)
        t = display::pick(video.covers, POSTER_SIZE.width());
    return t ? ThumbnailFetcher::absolutePath(t->path) : QString();
}

QVariant VideoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_videos.size()))
        return {};
    const model::Video &video = m_videos[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        return video.id;
    case Qt::ToolTipRole:
        return QString("<b>%1</b><br>%2").arg(video.id.toHtmlEscaped(), display::title(video).toHtmlEscaped());
    case Qt::DecorationRole:
        return m_images.get(posterPath(video), POSTER_SIZE);
    case PosterRole:
        return posterPath(video);
    default:
        return {};
    }
}

void VideoFilter::setCriteria(const Criteria &criteria)
{
    this->beginFilterChange();
    m_criteria = criteria;
    this->endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool VideoFilter::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    const auto *source = static_cast<const VideoListModel *>(this->sourceModel());
    const model::Video &video = source->video(sourceRow);
    const Criteria &c = m_criteria;

    if (!c.text.isEmpty() && !video.id.contains(c.text, Qt::CaseInsensitive)
        && !display::title(video).contains(c.text, Qt::CaseInsensitive))
        return false;
    if (!c.actress.isEmpty()
        && std::none_of(video.cast.begin(), video.cast.end(),
                        [&c](const model::Actress &a) { return display::name(a) == c.actress; }))
        return false;
    if (!c.director.isEmpty() && (!video.director || display::name(*video.director) != c.director))
        return false;
    if (!c.maker.isEmpty() && (!video.maker || display::name(*video.maker) != c.maker))
        return false;
    for (const QString &genre : c.genres)
        if (std::none_of(video.genres.begin(), video.genres.end(),
                         [&genre](const model::Genre &g) { return display::name(g) == genre; }))
            return false;
    return true;
}

QSize PosterDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {VideoListModel::POSTER_SIZE.width() + 2 * MARGIN,
            VideoListModel::POSTER_SIZE.height() + CAPTION_HEIGHT + 2 * MARGIN};
}

void PosterDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const bool hot = option.state & (QStyle::State_MouseOver | QStyle::State_Selected);
    const QRect card = option.rect.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN);
    const QRect image(card.topLeft(), VideoListModel::POSTER_SIZE);
    const QRect caption(image.left(), image.bottom() + 1, image.width(), CAPTION_HEIGHT);

    /* Poster, centered in its box; a white page until it is loaded */
    painter->fillRect(image, Qt::white);
    const QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
    if (!pixmap.isNull()) {
        const QSize size = (pixmap.size() / pixmap.devicePixelRatio()).scaled(image.size(), Qt::KeepAspectRatio);
        QRect target(QPoint(), size);
        target.moveCenter(image.center());
        painter->drawPixmap(target, pixmap);
    } else if (index.data(VideoListModel::PosterRole).toString().isEmpty()) {
        painter->setPen(QColor(0x70, 0x70, 0x70));
        painter->drawText(image, Qt::AlignCenter, tr("No image"));
    }
    painter->setPen(style::POST_BORDER);
    painter->drawRect(image.adjusted(0, 0, -1, -1));

    /* Id bar, drawn like a post: border on the right and bottom */
    painter->fillRect(caption, hot ? style::HIGHLIGHT : style::POST);
    painter->setPen(hot ? style::HIGHLIGHT_BORDER : style::POST_BORDER);
    painter->drawLine(caption.topRight(), caption.bottomRight());
    painter->drawLine(caption.bottomLeft(), caption.bottomRight());

    QFont font = option.font;
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(hot ? style::LINK_HOVER : style::SUBJECT);
    painter->drawText(caption, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());

    painter->restore();
}
