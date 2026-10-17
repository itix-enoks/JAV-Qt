#include "gui/librarypage.hpp"
#include "gui/display.hpp"

#include <set>

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
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
    m_filter.setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filter.sort(0);

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
    m_grid->setItemDelegate(new PosterDelegate(m_grid));
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
    layout->addWidget(m_grid, 1);

    /* Wiring */
    connect(m_grid, &QListView::activated, this, [this](const QModelIndex &index) {
        emit this->videoActivated(m_videos.video(m_filter.mapToSource(index).row()));
    });
    connect(m_search, &QLineEdit::textChanged, this, &LibraryPage::applyFilters);
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
