#include "gui/imagecache.hpp"
#include "log.hpp"

#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QtConcurrent/QtConcurrentRun>

namespace {

    constexpr int CACHE_KIB = 512 * 1024;

    QImage loadScaled(const QString &path, const QSize &devicePixels)
    {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (size.isValid() && (size.width() > devicePixels.width() || size.height() > devicePixels.height()))
            reader.setScaledSize(size.scaled(devicePixels, Qt::KeepAspectRatio));
        return reader.read();
    }

} /* namespace */

ImageCache::ImageCache(QObject *parent) : QObject(parent)
{
    m_pixmaps.setMaxCost(CACHE_KIB);
}

QString ImageCache::key(const QString &path, const QSize &bound)
{
    return QString("%1|%2x%3").arg(path).arg(bound.width()).arg(bound.height());
}

QPixmap ImageCache::get(const QString &path, const QSize &bound)
{
    const QString k = key(path, bound);
    if (const QPixmap *pixmap = m_pixmaps.object(k))
        return *pixmap;
    if (path.isEmpty() || m_loading.contains(k) || m_failed.contains(k))
        return {};

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
        }
        emit this->loaded(path);
    });
    watcher->setFuture(QtConcurrent::run(loadScaled, path, devicePixels));
    return {};
}
