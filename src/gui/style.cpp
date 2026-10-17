#include "gui/style.hpp"
#include "log.hpp"
#include "paths.hpp"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>

namespace {

    /* Register every font file matching `pattern` in paths::fontsDir()/<subdir>; false if any failed */
    bool addFonts(const QString &subdir, const QString &pattern)
    {
        const QDir dir(paths::fontsDir() + "/" + subdir);
        const QStringList files = dir.entryList({pattern}, QDir::Files);
        if (files.isEmpty()) {
            spdlog::error("No fonts {} in {}", pattern, dir.path());
            return false;
        }
        bool loaded = true;
        for (const QString &file : files) {
            const int id = QFontDatabase::addApplicationFont(dir.filePath(file));
            if (id < 0) {
                spdlog::error("Could not load font {}", dir.filePath(file));
                loaded = false;
                continue;
            }
            spdlog::debug("Loaded font {}: {}", file, QFontDatabase::applicationFontFamilies(id).join(", "));
        }
        return loaded;
    }

} /* namespace */

namespace style {

    bool loadFonts()
    {
        /* Every weight, upright and italic */
        bool loaded = addFonts("inter/static", "Inter_18pt-*.ttf");
        loaded = addFonts("noto-sans-cjk-jp", "NotoSansCJKjp-*.otf") && loaded;

        if (!QFontDatabase::hasFamily(FONT_FAMILY)) {
            spdlog::error("Font family {} not available, the GUI falls back to the system font", FONT_FAMILY);
            return false;
        }
        if (!QFontDatabase::hasFamily(CJK_FONT_FAMILY))
            spdlog::warn("Font family {} not available, Japanese text falls back to another font", CJK_FONT_FAMILY);

        QFont font;
        font.setFamilies({FONT_FAMILY, CJK_FONT_FAMILY});
        font.setPointSize(10);
        font.setHintingPreference(QFont::PreferNoHinting);  /* keep Inter's own shapes */
        QApplication::setFont(font);
        return loaded;
    }

    QString sheet()
    {
        /* %1.. are filled in below, in the order of the arg() calls */
        return QString(R"(
* {
    font-family: "%12", "%13";
    font-size: 10pt;
    color: #000000;
}

/* The Yotsuba B fade: most color at the top */
#central {
    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 %1, stop: 1 %2);
}
QStackedWidget, QScrollArea, #scrollContents, QStatusBar {
    background: transparent;
    border: none;
}

#topBar {
    border: none;
    border-bottom: 1px solid %4;
}
#boardTitle {
    color: %9;
    font-size: 18pt;
    font-weight: bold;
}
#status {
    color: %6;
}
#sideBar {
    border: none;
    border-right: 1px solid %4;
}

QFrame#post {
    background: %3;
    border: 1px solid %4;
    border-left: none;
    border-top: none;
}
QLabel#subject {
    color: %5;
    font-weight: bold;
    font-size: 13pt;
}
QLabel#sectionTitle {
    color: %5;
    font-weight: bold;
}
QLabel#name {
    color: %6;
    font-weight: bold;
}
QLabel#muted {
    color: #707070;
}

QPushButton {
    background: %3;
    color: %7;
    border: 1px solid %4;
    border-left-color: %3;
    border-top-color: %3;
    padding: 5px 12px;
}
QPushButton:hover {
    background: %10;
    border-color: %11;
    color: %8;
}
QPushButton:pressed {
    background: %4;
}
QPushButton:checked {
    background: %10;
    border-color: %11;
    color: %9;
    font-weight: bold;
}
QPushButton:disabled {
    color: #8a8aa0;
}
QPushButton#play {
    font-size: 15pt;
    font-weight: bold;
    padding: 10px 36px;
}

QLineEdit, QComboBox, QPlainTextEdit, QListWidget#genres {
    background: #ffffff;
    border: 1px solid #aaaaaa;
    padding: 3px;
    selection-background-color: %10;
    selection-color: #000000;
}
QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
    border-color: %5;
}
QComboBox QAbstractItemView {
    background: #ffffff;
    selection-background-color: %10;
}

QListView#posterGrid, QListWidget#previews {
    background: transparent;
    border: none;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: %2;
    border: none;
    width: 12px;
    height: 12px;
}
QScrollBar::handle {
    background: %4;
    min-height: 24px;
    min-width: 24px;
}
QScrollBar::handle:hover {
    background: %11;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: transparent;
}

QToolTip {
    background: %3;
    border: 1px solid %4;
    color: #000000;
}
)")
            .arg(FADE_TOP.name(), BACKGROUND.name(), POST.name(), POST_BORDER.name(), SUBJECT.name(),
                 NAME.name(), LINK.name(), LINK_HOVER.name(), TITLE.name())
            .arg(HIGHLIGHT.name(), HIGHLIGHT_BORDER.name(), FONT_FAMILY, CJK_FONT_FAMILY);
    }

    QString link(const QString &href, const QString &text)
    {
        return QString("<a href=\"%1\" style=\"color: %2; text-decoration: none;\">%3</a>")
            .arg(href.toHtmlEscaped(), LINK.name(), text.toHtmlEscaped());
    }

} /* namespace style */
