#pragma once

#include <map>
#include <vector>
#include <optional>

#include <QString>
#include <QStringList>

#include "options.hpp"
#include "models/video.hpp"

/**
 * The user's local JAV collection, rooted at "library" in options.json.
 */
class Library {
public:
    Library(const Options &options);

    /**
     * Recursively scan the library directory for video files and return the
     * distinct JAV ids found in their names (e.g. "hhd800.com@MKMP-705.mp4"),
     * falling back to the parent directory's name (e.g. "MKMP-705/").
     * Also remembers each id's files, see files().
     */
    std::vector<model::JavId> searchJav();

    /* Video files of an id found by the last searchJav(), sorted (CD1, CD2, ...) */
    QStringList files(const QString &id) const;

    QString root() const { return m_root; }

    /* Parse "MKMP-705" (or any name containing it) into a JavId */
    static std::optional<model::JavId> parseJavId(const QString &name);

    /* "SVHOT-002": suffixes are zero-padded to at least 3 digits */
    static QString toString(const model::JavId &id);

private:
    QString m_root;  /* absolute path, empty if not configured */
    std::map<QString, QStringList> m_files;  /* by id, e.g. "MKMP-705" */
};
