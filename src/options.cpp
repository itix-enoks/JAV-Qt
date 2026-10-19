#include "options.hpp"

#include "log.hpp"
#include "paths.hpp"

#include <QSaveFile>

namespace {
    QString optionsPath()
    {
        return paths::dataPath("options.json");
    }
}

Options::Options()
{
    this->loadOptions();
}

void Options::loadOptions()
{
    QFile file(optionsPath());
    if (!file.exists()) {
        spdlog::info("No {} yet, using defaults", file.fileName());
        return;
    }
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
    this->m_dockerHost = jsonObj["host"].toString(this->m_dockerHost);
    this->m_dockerPort = jsonObj["port"].toInt(this->m_dockerPort);
    this->m_libraryDir = jsonObj["library"].toString(this->m_libraryDir);
    this->m_posterZoom = jsonObj["posterZoom"].toDouble(this->m_posterZoom);
    this->m_theme = jsonObj["theme"].toString(this->m_theme);
    spdlog::info("Loaded options: FlareSolverr at {}:{}, library \"{}\"", this->m_dockerHost,
                 this->m_dockerPort, this->m_libraryDir);
}

void Options::saveOptions()
{
    QJsonObject jsonObj;
    QFile in(optionsPath());
    if (in.open(QIODevice::ReadOnly | QIODevice::Text))
        jsonObj = QJsonDocument::fromJson(in.readAll()).object();

    jsonObj["host"] = this->m_dockerHost;
    jsonObj["port"] = this->m_dockerPort;
    jsonObj["library"] = this->m_libraryDir;
    jsonObj["posterZoom"] = this->m_posterZoom;
    jsonObj["theme"] = this->m_theme;

    QSaveFile out(optionsPath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text) || out.write(QJsonDocument(jsonObj).toJson()) < 0
        || !out.commit())
        throw std::runtime_error("Could not write options.json");
    spdlog::debug("Saved options: library \"{}\", poster zoom {}, theme \"{}\"", this->m_libraryDir,
                  this->m_posterZoom, this->m_theme);
}
