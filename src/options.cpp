#include "options.hpp"

#include <iostream>

Options::Options()
{
    this->loadOptions();
}

void Options::loadOptions()
{
    QFile file("../options.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QByteArray jsonBytes = file.readAll();
    QJsonParseError jsonParseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonBytes, &jsonParseError);
    if (jsonParseError.error != 0) {
        // std::cout << jsonParseError.errorString().toStdString() << std::endl;
        throw std::runtime_error("Could not parse options.json");
    }

    if (!jsonDoc.isObject())
        throw std::runtime_error("Unexpected structure in options.json");

    QJsonObject jsonObj = jsonDoc.object();
    this->m_dockerHost = jsonObj["host"].toString();
    this->m_dockerPort = jsonObj["port"].toInt();
}
