#pragma once

#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>

class Options {
public:
    Options();
    void loadOptions();
    /* Write the options back to options.json, keeping keys this class doesn't know */
    void saveOptions();
    QUrl dockerAddress();

    /**
     * Options, in options.json in paths::dataDir(). The defaults match
     * FlareSolverr's own, as set up in the README.
     */
    QString m_dockerHost = "127.0.0.1";
    int m_dockerPort = 8191;
    QString m_libraryDir;  /* root of the local video collection */
    double m_posterZoom = 1;  /* library poster size, 1 = 100% */
};
