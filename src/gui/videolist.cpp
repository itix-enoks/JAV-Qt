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

const model::Video *VideoListModel::find(const QString &id) const
{
    const auto it = std::find_if(m_videos.begin(), m_videos.end(),
                                 [&id](const model::Video &v) { return v.id == id; });
    return it == m_videos.end() ? nullptr : &*it;
}

int VideoListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_videos.size());
}

QString VideoListModel::posterPath(const model::Video &video)
{
    /*
     * The front of the jacket, else the whole jacket. The largest one, so it
     * stays sharp at every zoom (it is loaded scaled down) and the path stays
     * the same while zooming, so ImageCache can show the previous size meanwhile.
     */
    constexpr int LARGEST = 1 << 20;
    const model::Thumbnail *t = display::pick(video.posters, LARGEST);
    if (!t)
        t = display::pick(video.covers, LARGEST);
    return t ? ThumbnailFetcher::absolutePath(t->path) : QString();
}

void VideoListModel::setPosterSize(const QSize &size)
{
    if (size == m_posterSize)
        return;
    m_posterSize = size;
    if (!m_videos.empty())
        emit this->dataChanged(this->index(0), this->index(int(m_videos.size()) - 1), {Qt::DecorationRole});
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
        return m_images.get(posterPath(video), m_posterSize);
    case PosterRole:
        return posterPath(video);
    case BookmarkedRole:
        return video.bookmarked;
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

void VideoFilter::setSortBy(SortBy sortBy)
{
    m_sortBy = sortBy;
    this->invalidate();  /* re-sorts */
}

bool VideoFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto *source = static_cast<const VideoListModel *>(this->sourceModel());
    const model::Video &a = source->video(left.row());
    const model::Video &b = source->video(right.row());

    switch (m_sortBy) {
    case SortBy::Stars:
        if (a.stars != b.stars)
            return a.stars > b.stars;
        break;
    case SortBy::Bookmarked:
        if (a.bookmarked != b.bookmarked)
            return a.bookmarked;
        break;
    case SortBy::Id:
        break;
    }
    return a.id.compare(b.id, Qt::CaseInsensitive) < 0;
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
    return {m_posterSize.width() + 2 * MARGIN, m_posterSize.height() + CAPTION_HEIGHT + 2 * MARGIN};
}

void PosterDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const bool hot = option.state & (QStyle::State_MouseOver | QStyle::State_Selected);
    const QRect card = option.rect.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN);
    const QRect image(card.topLeft(), m_posterSize);
    const QRect caption(image.left(), image.bottom() + 1, image.width(), CAPTION_HEIGHT);

    /* Poster, whole and centered in its box; a blank page until it is loaded */
    const style::Theme &theme = style::theme();
    painter->fillRect(image, theme.blank);
    const QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
    if (!pixmap.isNull()) {
        const QSize size = (pixmap.size() / pixmap.devicePixelRatio()).scaled(image.size(), Qt::KeepAspectRatio);
        QRect target(QPoint(), size);
        target.moveCenter(image.center());
        painter->drawPixmap(target, pixmap);
    } else if (index.data(VideoListModel::PosterRole).toString().isEmpty()) {
        painter->setPen(theme.muted);
        painter->drawText(image, Qt::AlignCenter, tr("No image"));
    }
    painter->setPen(theme.postBorder);
    painter->drawRect(image.adjusted(0, 0, -1, -1));

    /* Id bar, drawn like a post: border on the right and bottom */
    painter->fillRect(caption, hot ? theme.highlight : theme.post);
    painter->setPen(hot ? theme.highlightBorder : theme.postBorder);
    painter->drawLine(caption.topRight(), caption.bottomRight());
    painter->drawLine(caption.bottomLeft(), caption.bottomRight());

    /* Id, and a bookmark ribbon right of it; smaller when zoomed out too far for both */
    constexpr int GAP = 6, RIBBON_WIDTH = 9, RIBBON_HEIGHT = 12, PADDING = 6, MIN_POINT_SIZE = 6;
    const QString id = index.data(Qt::DisplayRole).toString();
    const bool bookmarked = index.data(VideoListModel::BookmarkedRole).toBool();
    QFont font = option.font;
    font.setBold(true);
    auto width = [&] { return QFontMetrics(font).horizontalAdvance(id) + (bookmarked ? GAP + RIBBON_WIDTH : 0); };
    while (width() > caption.width() - 2 * PADDING && font.pointSizeF() > MIN_POINT_SIZE)
        font.setPointSizeF(font.pointSizeF() - 0.5);
    painter->setFont(font);
    painter->setPen(hot ? theme.linkHover : theme.subject);
    if (!bookmarked) {
        painter->drawText(caption, Qt::AlignCenter, id);
    } else {
        /* Centered together */
        const int textWidth = QFontMetrics(font).horizontalAdvance(id);
        const int left = caption.center().x() - (textWidth + GAP + RIBBON_WIDTH) / 2;
        painter->drawText(QRect(left, caption.top(), textWidth, caption.height()), Qt::AlignCenter, id);

        const QRectF ribbon(left + textWidth + GAP, caption.center().y() - RIBBON_HEIGHT / 2.0 + 0.5,
                            RIBBON_WIDTH, RIBBON_HEIGHT);
        const QPolygonF shape{ribbon.topLeft(), ribbon.topRight(), ribbon.bottomRight(),
                              QPointF(ribbon.center().x(), ribbon.bottom() - RIBBON_HEIGHT / 3.0), ribbon.bottomLeft()};
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(hot ? theme.starHover : theme.star);
        painter->drawPolygon(shape);
    }

    painter->restore();
}
