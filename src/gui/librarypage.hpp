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

signals:
    void videoActivated(const model::Video &video);

private:
    void applyFilters();
    void clearFilters();
    /* Refill the filter choices from the videos, keeping the current choices */
    void refreshChoices();
    static void refill(QComboBox *combo, const QStringList &values);

    VideoListModel &m_videos;
    VideoFilter m_filter;

    QLineEdit *m_search;
    QComboBox *m_actress;
    QComboBox *m_director;
    QComboBox *m_maker;
    QListWidget *m_genres;
    QLabel *m_count;
    QListView *m_grid;
    QLabel *m_empty;  /* instead of the grid while there are no videos */
    QTimer m_refreshChoices;  /* coalesces model changes */
};
