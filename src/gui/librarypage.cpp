#include "gui/librarypage.hpp"
#include "gui/display.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>

#include <QFrame>
#include <QKeySequence>
#include <QShortcut>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace {

    constexpr int SIDEBAR_WIDTH = 300;

    QLabel *sectionTitle(const QString &text)
    {
        auto *label = new QLabel(text);
        label->setObjectName("sectionTitle");
        return label;
    }

} /* namespace */

LibraryPage::LibraryPage(VideoListModel &videos, QWidget *parent) : QWidget(parent), m_videos(videos)
{
    m_filter.setSourceModel(&m_videos);
    m_filter.sort(0);  /* see VideoFilter::lessThan() */

    /* Sidebar: search, then the filters in a post box */
    auto *sideBar = new QFrame;
    sideBar->setObjectName("sideBar");
    sideBar->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
    sideBar->setFixedWidth(SIDEBAR_WIDTH);
    auto *side = new QVBoxLayout(sideBar);
    side->setContentsMargins(16, 16, 16, 16);
    side->setSpacing(12);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search ID"));
    m_search->setClearButtonEnabled(true);
    side->addWidget(m_search);

    auto *sortRow = new QHBoxLayout;
    auto *sortLabel = new QLabel(tr("Sort by"));
    sortLabel->setObjectName("sectionTitle");
    m_sortBy = new QComboBox;
    m_sortBy->addItem(tr("ID"), int(VideoFilter::SortBy::Id));
    m_sortBy->addItem(tr("Stars"), int(VideoFilter::SortBy::Stars));
    m_sortBy->addItem(tr("Bookmarked"), int(VideoFilter::SortBy::Bookmarked));
    sortRow->addWidget(sortLabel);
    sortRow->addWidget(m_sortBy, 1);
    side->addLayout(sortRow);

    auto *post = new QFrame;
    post->setObjectName("post");
    post->setFrameShape(QFrame::StyledPanel);  /* else its style sheet border is not drawn */
    auto *filters = new QVBoxLayout(post);
    filters->setContentsMargins(12, 10, 12, 12);
    filters->setSpacing(6);

    auto *subject = new QLabel(tr("Filter"));
    subject->setObjectName("subject");
    filters->addWidget(subject);

    m_actress = new QComboBox;
    m_director = new QComboBox;
    m_maker = new QComboBox;
    for (auto [title, combo] : {std::pair{tr("Actress"), m_actress}, std::pair{tr("Director"), m_director},
                                std::pair{tr("Maker"), m_maker}}) {
        combo->setMaxVisibleItems(20);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        filters->addWidget(sectionTitle(title));
        filters->addWidget(combo);
    }

    filters->addWidget(sectionTitle(tr("Genres (all of)")));
    m_genres = new QListWidget;
    m_genres->setObjectName("genres");
    m_genres->setMinimumHeight(160);
    filters->addWidget(m_genres, 1);

    auto *clear = new QPushButton(tr("Clear filters"));
    filters->addWidget(clear);
    side->addWidget(post, 1);

    m_count = new QLabel;
    m_count->setObjectName("muted");
    side->addWidget(m_count);

    /* Poster grid */
    m_grid = new QListView;
    m_grid->setObjectName("posterGrid");
    m_grid->setModel(&m_filter);
    m_delegate = new PosterDelegate(m_grid);
    m_grid->setItemDelegate(m_delegate);
    m_grid->viewport()->installEventFilter(this);  /* Ctrl + wheel zooms */
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSpacing(4);
    m_grid->setMouseTracking(true);  /* hover highlight */
    m_grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_grid->verticalScrollBar()->setSingleStep(24);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setFrameShape(QFrame::NoFrame);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(sideBar);

    /* First run: nothing stored yet */
    m_empty = new QLabel(tr("<p style=\"font-size: 14pt; font-weight: bold;\">No videos yet</p>"
                            "<p>Click <b>Import From Folder (Library)</b> at the top right and pick the folder "
                            "with your videos.<br/>JAV-Qt finds the ID in each file or folder name (e.g. "
                            "MKMP-705) and downloads its details and pictures.</p>"));
    m_empty->setObjectName("muted");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);

    auto *content = new QStackedLayout;
    content->addWidget(m_grid);
    content->addWidget(m_empty);
    layout->addLayout(content, 1);
    auto showEmpty = [this, content] {
        content->setCurrentWidget(m_videos.rowCount() == 0 ? static_cast<QWidget *>(m_empty) : m_grid);
    };
    connect(&m_videos, &QAbstractItemModel::modelReset, this, showEmpty);
    connect(&m_videos, &QAbstractItemModel::rowsInserted, this, showEmpty);
    showEmpty();

    /* Wiring */
    connect(m_grid, &QListView::activated, this, [this](const QModelIndex &index) {
        emit this->videoActivated(m_videos.video(m_filter.mapToSource(index).row()));
    });
    connect(m_search, &QLineEdit::textChanged, this, &LibraryPage::applyFilters);
    connect(m_sortBy, &QComboBox::currentIndexChanged, this, [this] {
        m_filter.setSortBy(VideoFilter::SortBy(m_sortBy->currentData().toInt()));
    });
    for (QComboBox *combo : {m_actress, m_director, m_maker})
        connect(combo, &QComboBox::currentIndexChanged, this, &LibraryPage::applyFilters);
    connect(m_genres, &QListWidget::itemChanged, this, &LibraryPage::applyFilters);
    connect(clear, &QPushButton::clicked, this, &LibraryPage::clearFilters);

    m_refreshChoices.setSingleShot(true);
    connect(&m_refreshChoices, &QTimer::timeout, this, &LibraryPage::refreshChoices);
    auto refreshLater = [this] { m_refreshChoices.start(); };
    connect(&m_videos, &QAbstractItemModel::modelReset, this, refreshLater);
    connect(&m_videos, &QAbstractItemModel::rowsInserted, this, refreshLater);
    connect(&m_videos, &QAbstractItemModel::dataChanged, this, refreshLater);

    auto updateCount = [this] {
        m_count->setText(tr("%1 of %2 video(s)").arg(m_filter.rowCount()).arg(m_videos.rowCount()));
    };
    connect(&m_filter, &QAbstractItemModel::rowsInserted, this, updateCount);
    connect(&m_filter, &QAbstractItemModel::rowsRemoved, this, updateCount);
    connect(&m_filter, &QAbstractItemModel::modelReset, this, updateCount);

    /*
     * Zoom: the platform's zoom keys, plus Ctrl+= since Ctrl++ needs Shift on
     * most keyboards. Each key once, as a key bound twice triggers neither.
     * For the whole window (the focus may be in the top bar), but only while
     * this page is shown.
     */
    auto keysOf = [](QKeySequence::StandardKey standard, const QKeySequence &extra) {
        QList<QKeySequence> keys = QKeySequence::keyBindings(standard);
        if (!keys.contains(extra))
            keys.append(extra);
        return keys;
    };
    const std::pair<QList<QKeySequence>, int> zoomKeys[] = {
        {keysOf(QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)), +1},
        {keysOf(QKeySequence::ZoomOut, QKeySequence(Qt::CTRL | Qt::Key_Minus)), -1},
        {{QKeySequence(Qt::CTRL | Qt::Key_0)}, 0},
    };
    for (const auto &[keys, steps] : zoomKeys) {
        for (const QKeySequence &key : keys) {
            auto *shortcut = new QShortcut(key, this);
            shortcut->setContext(Qt::WindowShortcut);
            connect(shortcut, &QShortcut::activated, this, [this, steps] {
                if (steps == 0) {
                    this->applyZoomLevel(DEFAULT_ZOOM_LEVEL);
                    emit this->zoomChanged(this->zoom());
                } else {
                    this->zoomBy(steps);
                }
            });
        }
    }

    this->refreshChoices();
}

void LibraryPage::refill(QComboBox *combo, const QStringList &values)
{
    const QString current = combo->currentIndex() > 0 ? combo->currentText() : QString();
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(tr("Any"));
    combo->addItems(values);
    const int index = combo->findText(current);
    combo->setCurrentIndex(index > 0 ? index : 0);
}

void LibraryPage::refreshChoices()
{
    std::set<QString> actresses, directors, makers, genres;
    for (const model::Video &video : m_videos.videos()) {
        for (const model::Actress &actress : video.cast)
            actresses.insert(display::name(actress));
        if (video.director)
            directors.insert(display::name(*video.director));
        if (video.maker)
            makers.insert(display::name(*video.maker));
        for (const model::Genre &genre : video.genres)
            genres.insert(display::name(genre));
    }
    auto list = [](const std::set<QString> &values) {
        QStringList out;
        for (const QString &v : values)
            if (!v.isEmpty())
                out.append(v);
        out.sort(Qt::CaseInsensitive);
        return out;
    };

    refill(m_actress, list(actresses));
    refill(m_director, list(directors));
    refill(m_maker, list(makers));

    QSet<QString> checked;
    for (int i = 0; i < m_genres->count(); ++i)
        if (m_genres->item(i)->checkState() == Qt::Checked)
            checked.insert(m_genres->item(i)->text());
    {
        const QSignalBlocker blocker(m_genres);
        m_genres->clear();
        for (const QString &genre : list(genres)) {
            auto *item = new QListWidgetItem(genre, m_genres);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(checked.contains(genre) ? Qt::Checked : Qt::Unchecked);
        }
    }
    this->applyFilters();
}

void LibraryPage::applyFilters()
{
    auto chosen = [](const QComboBox *combo) { return combo->currentIndex() > 0 ? combo->currentText() : QString(); };

    VideoFilter::Criteria criteria;
    criteria.text = m_search->text().trimmed();
    criteria.actress = chosen(m_actress);
    criteria.director = chosen(m_director);
    criteria.maker = chosen(m_maker);
    for (int i = 0; i < m_genres->count(); ++i)
        if (m_genres->item(i)->checkState() == Qt::Checked)
            criteria.genres.insert(m_genres->item(i)->text());
    m_filter.setCriteria(criteria);
    m_count->setText(tr("%1 of %2 video(s)").arg(m_filter.rowCount()).arg(m_videos.rowCount()));
}

void LibraryPage::clearFilters()
{
    {
        const QSignalBlocker b1(m_search), b2(m_actress), b3(m_director), b4(m_maker), b5(m_genres);
        m_search->clear();
        for (QComboBox *combo : {m_actress, m_director, m_maker})
            combo->setCurrentIndex(0);
        for (int i = 0; i < m_genres->count(); ++i)
            m_genres->item(i)->setCheckState(Qt::Unchecked);
    }
    this->applyFilters();
}

void LibraryPage::showOnly(Filter filter, const QString &value)
{
    if (m_refreshChoices.isActive()) {
        m_refreshChoices.stop();
        this->refreshChoices();  /* `value` may be new */
    }
    this->clearFilters();
    switch (filter) {
    case Filter::Actress:
        m_actress->setCurrentIndex(qMax(0, m_actress->findText(value)));
        break;
    case Filter::Director:
        m_director->setCurrentIndex(qMax(0, m_director->findText(value)));
        break;
    case Filter::Maker:
        m_maker->setCurrentIndex(qMax(0, m_maker->findText(value)));
        break;
    case Filter::Genre:
        for (QListWidgetItem *item : m_genres->findItems(value, Qt::MatchExactly))
            item->setCheckState(Qt::Checked);
        break;
    }
}

void LibraryPage::setZoom(double zoom)
{
    int nearest = 0;
    for (int level = 0; level < int(std::size(ZOOM_LEVELS)); ++level)
        if (std::abs(ZOOM_LEVELS[level] - zoom) < std::abs(ZOOM_LEVELS[nearest] - zoom))
            nearest = level;
    this->applyZoomLevel(nearest);
}

void LibraryPage::zoomBy(int steps)
{
    const int level = std::clamp(m_zoomLevel + steps, 0, int(std::size(ZOOM_LEVELS)) - 1);
    if (level == m_zoomLevel)
        return;
    this->applyZoomLevel(level);
    emit this->zoomChanged(this->zoom());
}

void LibraryPage::applyZoomLevel(int level)
{
    const QModelIndex first = m_grid->indexAt(QPoint(PosterDelegate::MARGIN * 2, PosterDelegate::MARGIN * 2));
    m_zoomLevel = level;

    const QSize size = (QSizeF(VideoListModel::POSTER_SIZE) * ZOOM_LEVELS[level]).toSize();
    m_delegate->setPosterSize(size);
    m_videos.setPosterSize(size);
    m_grid->doItemsLayout();
    if (first.isValid())
        m_grid->scrollTo(first, QAbstractItemView::PositionAtTop);
}

bool LibraryPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_grid->viewport() && event->type() == QEvent::Wheel) {
        const auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            /* One step per notch (120); touchpads send many small deltas */
            m_wheelDelta += wheel->angleDelta().y();
            const int steps = m_wheelDelta / 120;
            m_wheelDelta -= steps * 120;
            if (steps != 0)
                this->zoomBy(steps);
            return true;
        }
        m_wheelDelta = 0;
    }
    return QWidget::eventFilter(watched, event);
}
