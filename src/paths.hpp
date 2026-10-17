#pragma once

#include <QString>

/**
 * Where JAV-Qt keeps its files. Needs QCoreApplication's name to be set.
 */
namespace paths {

    /*
     * The user's data: db.json, options.json, downloaded thumbnails and the
     * log. $JAVQT_DATA_DIR if set, else the per-user application data folder:
     *   Linux:   ~/.local/share/JAV-Qt
     *   macOS:   ~/Library/Application Support/JAV-Qt
     *   Windows: %APPDATA%\JAV-Qt
     * Created if missing.
     */
    QString dataDir();

    /* `relative` (e.g. "db.json") inside dataDir() */
    QString dataPath(const QString &relative);

    /*
     * The bundled fonts, wherever the platform's package put them (next to
     * the executable, in the macOS bundle's Resources, in share/jav-qt), else
     * the source tree's assets/fonts. Empty if none exists.
     */
    QString fontsDir();

} /* namespace paths */
