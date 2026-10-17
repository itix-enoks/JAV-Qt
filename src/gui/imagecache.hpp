#pragma once

#include <QCache>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QString>

/**
 * Loads images scaled to fit a size, off the GUI thread, and keeps them.
 */
class ImageCache : public QObject {
    Q_OBJECT

public:
    ImageCache(QObject *parent = nullptr);

    /*
     * The image at `path` scaled to `bound` (in device independent pixels):
     * to fit in it, or with KeepAspectRatioByExpanding to cover it. Loading
     * starts on the first call and loaded() is emitted when done (also on
     * failure). Meanwhile this returns the image as last loaded in another
     * size, if still cached, else null.
     */
    QPixmap get(const QString &path, const QSize &bound, Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

signals:
    void loaded(const QString &path);

private:
    static QString key(const QString &path, const QSize &bound, Qt::AspectRatioMode mode);

    QCache<QString, QPixmap> m_pixmaps;  /* cost in KiB */
    QHash<QString, QString> m_latest;  /* path -> key it was last loaded with */
    QSet<QString> m_loading;
    QSet<QString> m_failed;
};
