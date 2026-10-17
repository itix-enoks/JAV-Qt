#include "gui/imagecache.hpp"
#include "log.hpp"

#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QtConcurrent/QtConcurrentRun>

namespace {

    constexpr int CACHE_KIB = 512 * 1024;

    QImage loadScaled(const QString &path, const QSize &devicePixels, Qt::AspectRatioMode mode)
    {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        const QSize scaled = size.scaled(devicePixels, mode);
        if (size.isValid() && (scaled.width() < size.width() || scaled.height() < size.height()))
            reader.setScaledSize(scaled);  /* only ever smaller: JPEGs then decode faster */
        return reader.read();
    }

} /* namespace */

ImageCache::ImageCache(QObject *parent) : QObject(parent)
{
    m_pixmaps.setMaxCost(CACHE_KIB);
}

QString ImageCache::key(const QString &path, const QSize &bound, Qt::AspectRatioMode mode)
{
    return QString("%1|%2x%3|%4").arg(path).arg(bound.width()).arg(bound.height()).arg(int(mode));
}

QPixmap ImageCache::get(const QString &path, const QSize &bound, Qt::AspectRatioMode mode)
{
    const QString k = key(path, bound, mode);
    if (const QPixmap *pixmap = m_pixmaps.object(k))
        return *pixmap;
    const QPixmap *previous = m_pixmaps.object(m_latest.value(path));
    const QPixmap meanwhile = previous ? *previous : QPixmap();
    if (path.isEmpty() || m_loading.contains(k) || m_failed.contains(k))
        return meanwhile;

    m_loading.insert(k);
    const qreal ratio = qApp->devicePixelRatio();
    const QSize devicePixels = bound * ratio;

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path, k, ratio] {
        const QImage image = watcher->result();
        watcher->deleteLater();
        m_loading.remove(k);
        if (image.isNull()) {
            spdlog::warn("Could not load image {}", path);
            m_failed.insert(k);
        } else {
            auto *pixmap = new QPixmap(QPixmap::fromImage(image));
            pixmap->setDevicePixelRatio(ratio);
            m_pixmaps.insert(k, pixmap, qMax<qsizetype>(1, image.sizeInBytes() / 1024));
            m_latest.insert(path, k);
        }
        emit this->loaded(path);
    });
    watcher->setFuture(QtConcurrent::run(loadScaled, path, devicePixels, mode));
    return meanwhile;
}
