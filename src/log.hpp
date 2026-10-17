#pragma once

#include <string_view>

#include <QString>
#include <QUrl>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

/**
 * Lets Qt strings and URLs be passed directly to spdlog/fmt format strings.
 */
template <>
struct fmt::formatter<QString> : fmt::formatter<std::string_view> {
    auto format(const QString &str, fmt::format_context &ctx) const
    {
        const QByteArray utf8 = str.toUtf8();
        return fmt::formatter<std::string_view>::format(
            std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), ctx);
    }
};

template <>
struct fmt::formatter<QUrl> : fmt::formatter<QString> {
    auto format(const QUrl &url, fmt::format_context &ctx) const
    {
        return fmt::formatter<QString>::format(url.toString(), ctx);
    }
};
