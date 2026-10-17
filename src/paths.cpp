#include "paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace paths {

    QString dataDir()
    {
        static const QString dir = [] {
            QString path = qEnvironmentVariable("JAVQT_DATA_DIR");
            if (path.isEmpty())
                path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            path = QDir(path).absolutePath();
            QDir().mkpath(path);
            return path;
        }();
        return dir;
    }

    QString dataPath(const QString &relative)
    {
        return QDir(dataDir()).filePath(relative);
    }

    QString fontsDir()
    {
        const QString app = QCoreApplication::applicationDirPath();
        const QStringList candidates{
            app + "/../Resources/fonts",     /* macOS bundle */
            app + "/fonts",                  /* Windows */
            app + "/../share/jav-qt/fonts",  /* Linux install, AppImage */
#ifdef JAVQT_SOURCE_DIR
            QStringLiteral(JAVQT_SOURCE_DIR "/assets/fonts"),  /* running from the build tree */
#endif
        };
        for (const QString &candidate : candidates)
            if (QDir(candidate).exists())
                return QDir::cleanPath(candidate);
        return {};
    }

} /* namespace paths */
