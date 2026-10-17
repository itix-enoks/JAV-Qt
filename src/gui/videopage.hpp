#pragma once

#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "gui/imagecache.hpp"
#include "gui/librarypage.hpp"
#include "models/video.hpp"

/**
 * An image scaled to fit, centered, keeping its aspect ratio.
 */
class ImageView : public QWidget {
    Q_OBJECT

public:
    ImageView(ImageCache &images, QWidget *parent = nullptr);
    void setPath(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr QSize LOAD_SIZE{2400, 1600};  /* loaded once, scaled when painted */

    ImageCache &m_images;
    QString m_path;
};

/**
 * One video: its jacket, a Play button, preview stills, metadata and the
 * user's notes.
 */
class VideoPage : public QWidget {
    Q_OBJECT

public:
    VideoPage(ImageCache &images, QWidget *parent = nullptr);

    /* `files`: the video's files in the library, `notes`: the stored notes */
    void showVideo(const model::Video &video, const QStringList &files, const QString &notes);
    const QString &videoId() const { return m_id; }

    /* Emit notesEdited() now for unsaved notes, e.g. before leaving the page */
    void flushNotes();

signals:
    void back();
    void filterRequested(LibraryPage::Filter filter, const QString &value);
    /* Debounced while typing */
    void notesEdited(const QString &id, const QString &notes);

private:
    void showMetadata(const model::Video &video, int fileCount);
    void showPreviews(const model::Video &video);
    void setFiles(const QStringList &files);
    void openLink(const QString &link);

    ImageCache &m_images;
    QString m_id;
    bool m_notesDirty = false;

    ImageView *m_cover;
    QPushButton *m_play;
    QListWidget *m_previews;
    QLabel *m_metadata;
    QPlainTextEdit *m_notes;
    QTimer m_saveNotes;
};
