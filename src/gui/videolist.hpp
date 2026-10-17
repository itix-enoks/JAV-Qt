#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

#include "gui/imagecache.hpp"
#include "models/video.hpp"

/**
 * Every stored video, in no particular order (see VideoFilter for that).
 */
class VideoListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role { PosterRole = Qt::UserRole + 1 };

    /* Poster size in the grid, in device independent pixels */
    static constexpr QSize POSTER_SIZE{176, 240};

    VideoListModel(ImageCache &images, QObject *parent = nullptr);

    void setVideos(std::vector<model::Video> videos);
    /* Add `video`, or replace the stored video with its id */
    void upsert(const model::Video &video);
    const model::Video &video(int row) const { return m_videos[row]; }
    const std::vector<model::Video> &videos() const { return m_videos; }

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    static QString posterPath(const model::Video &video);

    ImageCache &m_images;
    std::vector<model::Video> m_videos;
};

/**
 * The sidebar's search and filters; sorts by id.
 * Names are matched as displayed, since the same actress (or genre) has
 * different ids on DMM and JavLibrary.
 */
class VideoFilter : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    struct Criteria {
        QString text;  /* part of the id or title */
        QString actress;
        QString director;
        QString maker;
        QSet<QString> genres;  /* all of them */
    };

    void setCriteria(const Criteria &criteria);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    Criteria m_criteria;
};

/**
 * A poster with the video's id in a post-colored bar below it.
 */
class PosterDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    static constexpr int CAPTION_HEIGHT = 28;
    static constexpr int MARGIN = 8;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
