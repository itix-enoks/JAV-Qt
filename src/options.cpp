#include "options.hpp"

#include "log.hpp"

#include <QSaveFile>

namespace {
    const QString OPTIONS_PATH = "../options.json";
}

Options::Options()
{
    this->loadOptions();
}

void Options::loadOptions()
{
    QFile file(OPTIONS_PATH);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        spdlog::warn("Could not open {}: {}, using defaults", file.fileName(), file.errorString());
        return;
    }

    QByteArray jsonBytes = file.readAll();
    QJsonParseError jsonParseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonBytes, &jsonParseError);
    if (jsonParseError.error != 0) {
        spdlog::error("Could not parse {} at offset {}: {}", file.fileName(),
                      jsonParseError.offset, jsonParseError.errorString());
        throw std::runtime_error("Could not parse options.json");
    }

    if (!jsonDoc.isObject())
        throw std::runtime_error("Unexpected structure in options.json");

    QJsonObject jsonObj = jsonDoc.object();
    this->m_dockerHost = jsonObj["host"].toString();
    this->m_dockerPort = jsonObj["port"].toInt();
    this->m_libraryDir = jsonObj["library"].toString();
    spdlog::info("Loaded options: FlareSolverr at {}:{}, library \"{}\"", this->m_dockerHost,
                 this->m_dockerPort, this->m_libraryDir);
}

void Options::saveOptions()
{
    QJsonObject jsonObj;
    QFile in(OPTIONS_PATH);
    if (in.open(QIODevice::ReadOnly | QIODevice::Text))
        jsonObj = QJsonDocument::fromJson(in.readAll()).object();

    jsonObj["host"] = this->m_dockerHost;
    jsonObj["port"] = this->m_dockerPort;
    jsonObj["library"] = this->m_libraryDir;

    QSaveFile out(OPTIONS_PATH);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text) || out.write(QJsonDocument(jsonObj).toJson()) < 0
        || !out.commit())
        throw std::runtime_error("Could not write options.json");
    spdlog::info("Saved options: library \"{}\"", this->m_libraryDir);
}
