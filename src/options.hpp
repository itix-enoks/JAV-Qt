#pragma once

#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>

class Options {
public:
    Options();
    void loadOptions();
    QUrl dockerAddress();

    /**
     * Options
     */
    QString m_dockerHost;
    int m_dockerPort = -1;
};
