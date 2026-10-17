#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QTimer>
#include <QWidget>

#include "gui/videolist.hpp"

/**
 * Landing page: a sidebar to search and filter, and a grid of posters.
 */
class LibraryPage : public QWidget {
    Q_OBJECT

public:
    /* What showOnly() filters on; also used in links from the video page */
    enum class Filter { Actress, Director, Maker, Genre };

    LibraryPage(VideoListModel &videos, QWidget *parent = nullptr);

    /* Clear every filter, then filter on just this */
    void showOnly(Filter filter, const QString &value);

    /*
     * Poster size, 1 = 100%: rounded to the nearest of ZOOM_LEVELS. Changed
     * by the user with Ctrl + mouse wheel, Ctrl+= / Ctrl+- and Ctrl+0.
     */
    double zoom() const { return ZOOM_LEVELS[m_zoomLevel]; }
    void setZoom(double zoom);

signals:
    void videoActivated(const model::Video &video);
    /* Only when changed by the user */
    void zoomChanged(double zoom);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static constexpr double ZOOM_LEVELS[] = {0.5, 0.625, 0.75, 0.875, 1, 1.125, 1.25, 1.5, 1.75, 2, 2.5};
    static constexpr int DEFAULT_ZOOM_LEVEL = 4;

    /* Show ZOOM_LEVELS[level], keeping the first visible poster in view */
    void applyZoomLevel(int level);
    void zoomBy(int steps);

    void applyFilters();
    void clearFilters();
    /* Refill the filter choices from the videos, keeping the current choices */
    void refreshChoices();
    static void refill(QComboBox *combo, const QStringList &values);

    VideoListModel &m_videos;
    VideoFilter m_filter;

    QLineEdit *m_search;
    QComboBox *m_sortBy;
    QComboBox *m_actress;
    QComboBox *m_director;
    QComboBox *m_maker;
    QListWidget *m_genres;
    QLabel *m_count;
    QListView *m_grid;
    QLabel *m_empty;  /* instead of the grid while there are no videos */
    QTimer m_refreshChoices;  /* coalesces model changes */
    PosterDelegate *m_delegate;
    int m_zoomLevel = DEFAULT_ZOOM_LEVEL;
    int m_wheelDelta = 0;  /* Ctrl + wheel rotation not yet turned into zoom steps */
};
