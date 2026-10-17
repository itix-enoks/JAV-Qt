#include "library.hpp"
#include "log.hpp"

#include <set>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

    const QStringList VIDEO_NAME_FILTERS{
        "*.mp4", "*.mkv", "*.avi", "*.wmv", "*.mov", "*.m4v", "*.ts", "*.flv", "*.webm"};

    /* Letters, dash, digits, not glued to other letters/digits ("hhd800.com@MKMP-705") */
    const QRegularExpression JAV_ID_RE(R"((?<![A-Za-z0-9])([A-Za-z]{2,10})-(\d{2,6})(?![0-9]))");

} /* namespace */

Library::Library(const Options &options)
{
    /* Not QDir(""), which would silently mean the working directory */
    if (options.m_libraryDir.isEmpty()) {
        spdlog::warn("No \"library\" directory set in options.json");
        return;
    }

    m_root = QDir(options.m_libraryDir).absolutePath();
    if (!QDir(m_root).exists())
        spdlog::error("Library directory {} does not exist", m_root);
    else
        spdlog::info("Using library directory {}", m_root);
}

std::vector<model::JavId> Library::searchJav()
{
    std::vector<model::JavId> ids;
    m_files.clear();
    if (m_root.isEmpty() || !QDir(m_root).exists())
        return ids;

    std::set<QString> seen;  /* multi-part videos (CD1, CD2, ...) share one id */
    int videoFiles = 0;

    QDirIterator it(m_root, VIDEO_NAME_FILTERS, QDir::Files,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        const QFileInfo file(it.next());
        ++videoFiles;

        std::optional<model::JavId> id = parseJavId(file.completeBaseName());
        if (!id)
            id = parseJavId(file.dir().dirName());
        if (!id) {
            spdlog::warn("No JAV id found for {}", file.filePath());
            continue;
        }

        const QString key = toString(*id);
        m_files[key].append(file.absoluteFilePath());
        if (!seen.insert(key).second) {
            spdlog::debug("{}: id {} already found, skipping", file.filePath(), key);
            continue;
        }
        spdlog::debug("{}: found {}", file.filePath(), key);
        ids.push_back(*id);
    }

    spdlog::info("Found {} JAV id(s) in {} video file(s) under {}", ids.size(), videoFiles,
                 m_root);
    return ids;
}

QStringList Library::files(const QString &id) const
{
    const auto it = m_files.find(id);
    if (it == m_files.end())
        return {};
    QStringList files = it->second;
    files.sort(Qt::CaseInsensitive);
    return files;
}

std::optional<model::JavId> Library::parseJavId(const QString &name)
{
    const QRegularExpressionMatch match = JAV_ID_RE.match(name);
    if (!match.hasMatch())
        return std::nullopt;

    model::JavId id;
    id.prefix = match.captured(1).toUpper();
    id.suffix = match.captured(2).toInt();
    return id;
}

QString Library::toString(const model::JavId &id)
{
    return QString("%1-%2").arg(id.prefix).arg(id.suffix, 3, 10, QChar('0'));
}
