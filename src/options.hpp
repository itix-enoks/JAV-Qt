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
     * Options
     */
    QString m_dockerHost;
    int m_dockerPort = -1;
    QString m_libraryDir;  /* root of the local video collection */
};
