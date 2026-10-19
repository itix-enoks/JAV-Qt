#include "gui/style.hpp"
#include "log.hpp"
#include "paths.hpp"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyle>

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

} /* namespace style */

namespace {

    using namespace style;

    /* "transparent" for a fully transparent color, which name() would make black */
    QString css(const QColor &color)
    {
        return color.alpha() == 0 ? QString("transparent") : color.name();
    }

    /* Application-wide style sheet; ${token}s are filled in below */
    QString sheet(const Theme &t)
    {
        QString sheet = R"(
* {
    font-family: "${font}", "${cjkFont}";
    font-size: 10pt;
    color: ${text};
}

/* The page fade: most color at the top */
#central {
    background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 ${fadeTop}, stop: 1 ${background});
}
QStackedWidget, QScrollArea, #scrollContents, QStatusBar {
    background: transparent;
    border: none;
}

#topBar {
    background: ${bar};
    border: none;
    border-bottom: 1px solid ${postBorder};
}
#status {
    color: ${label};
}
#sideBar {
    border: none;
    border-right: 1px solid ${postBorder};
}

QFrame#post {
    background: ${post};
    border: 1px solid ${postBorder};
    border-left: none;
    border-top: none;
}
QLabel#subject {
    color: ${subject};
    font-weight: bold;
    font-size: 13pt;
}
QLabel#sectionTitle {
    color: ${subject};
    font-weight: bold;
}
QLabel#name {
    color: ${label};
    font-weight: bold;
}
QLabel#muted {
    color: ${muted};
}

QPushButton {
    background: ${post};
    color: ${link};
    border: 1px solid ${postBorder};
    border-left-color: ${post};
    border-top-color: ${post};
    padding: 5px 12px;
}
QPushButton:hover {
    background: ${highlight};
    border-color: ${highlightBorder};
    color: ${linkHover};
}
QPushButton:pressed {
    background: ${postBorder};
}
QPushButton:checked {
    background: ${highlight};
    border-color: ${highlightBorder};
    color: ${title};
    font-weight: bold;
}
QPushButton:disabled {
    color: ${disabled};
}
QPushButton::menu-indicator {
    width: 0;
}
QPushButton#play {
    font-size: 15pt;
    font-weight: bold;
    padding: 10px 36px;
}

QLineEdit, QComboBox, QPlainTextEdit, QListWidget#genres {
    background: ${input};
    border: 1px solid ${inputBorder};
    padding: 3px;
    selection-background-color: ${highlight};
    selection-color: ${text};
}
QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
    border-color: ${highlightBorder};
}
QComboBox QAbstractItemView {
    background: ${input};
    selection-background-color: ${highlight};
}

QMenu {
    background: ${input};
    border: 1px solid ${postBorder};
}
QMenu::item:selected {
    background: ${highlight};
    color: ${linkHover};
}

QListView#posterGrid, QListWidget#previews {
    background: transparent;
    border: none;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: ${background};
    border: none;
    width: 12px;
    height: 12px;
}
QScrollBar::handle {
    background: ${postBorder};
    min-height: 24px;
    min-width: 24px;
}
QScrollBar::handle:hover {
    background: ${highlightBorder};
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: transparent;
}

QToolTip {
    background: ${post};
    border: 1px solid ${postBorder};
    color: ${text};
}
)";
        const std::vector<std::pair<const char *, QString>> tokens{
            {"font", FONT_FAMILY}, {"cjkFont", CJK_FONT_FAMILY},
            {"fadeTop", css(t.fadeTop)}, {"background", css(t.background)}, {"bar", css(t.bar)},
            {"post", css(t.post)}, {"postBorder", css(t.postBorder)},
            {"highlight", css(t.highlight)}, {"highlightBorder", css(t.highlightBorder)},
            {"text", css(t.text)}, {"muted", css(t.muted)}, {"disabled", css(t.disabled)},
            {"subject", css(t.subject)}, {"label", css(t.label)}, {"link", css(t.link)},
            {"linkHover", css(t.linkHover)}, {"title", css(t.title)},
            {"input", css(t.input)}, {"inputBorder", css(t.inputBorder)},
        };
        for (const auto &[token, value] : tokens)
            sheet.replace(QString("${%1}").arg(token), value);
        return sheet;
    }

    /* For what the style sheet doesn't reach: dialogs, Fusion's own drawing */
    QPalette palette(const Theme &t)
    {
        if (t.background.lightness() >= 128)
            return QApplication::style()->standardPalette();

        QPalette p;
        p.setColor(QPalette::Window, t.background);
        p.setColor(QPalette::WindowText, t.text);
        p.setColor(QPalette::Base, t.input);
        p.setColor(QPalette::AlternateBase, t.post);
        p.setColor(QPalette::Text, t.text);
        p.setColor(QPalette::PlaceholderText, t.muted);
        p.setColor(QPalette::Button, t.post);
        p.setColor(QPalette::ButtonText, t.text);
        p.setColor(QPalette::BrightText, t.subject);
        p.setColor(QPalette::Highlight, t.link);
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::ToolTipBase, t.post);
        p.setColor(QPalette::ToolTipText, t.text);
        p.setColor(QPalette::Link, t.link);
        p.setColor(QPalette::LinkVisited, t.link);
        p.setColor(QPalette::Light, t.postBorder.lighter(130));
        p.setColor(QPalette::Midlight, t.postBorder);
        p.setColor(QPalette::Mid, t.postBorder.darker(130));
        p.setColor(QPalette::Dark, t.post.darker(150));
        p.setColor(QPalette::Shadow, Qt::black);
        for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            p.setColor(QPalette::Disabled, role, t.disabled);
        return p;
    }

    size_t current = 0;  /* index into themes() */

} /* namespace */

namespace style {

    const std::vector<Theme> &themes()
    {
        /* Gitea's dark mode (theme-gitea-dark.css): body, nav, card, secondary and primary colors */
        static const Theme defaultDark{
            .name = "Default Dark",
            .fadeTop{0x1e, 0x1f, 0x20},
            .background{0x1e, 0x1f, 0x20},
            .bar{0x18, 0x19, 0x1b},
            .post{0x19, 0x1a, 0x1c},
            .postBorder{0x3f, 0x42, 0x48},
            .highlight{0x23, 0x25, 0x28},
            .highlightBorder{0x41, 0x83, 0xc4},
            .text{0xd2, 0xd4, 0xd8},
            .muted{0x96, 0x9a, 0xa1},
            .disabled{0x6f, 0x74, 0x7d},
            .subject{0xf8, 0xf8, 0xf8},
            .label{0xaa, 0xad, 0xb4},
            .link{0x41, 0x83, 0xc4},
            .linkHover{0x7a, 0xa8, 0xd6},
            .title{0x67, 0x9c, 0xd0},
            .star{0xcc, 0x99, 0x03},
            .starHover{0xea, 0xaf, 0x03},
            .input{0x19, 0x1a, 0x1c},
            .inputBorder{0x46, 0x49, 0x4f},
            .blank{0x2c, 0x2e, 0x32},
        };
        static const Theme yotsubaB{
            .name = "Yotsuba B",
            .fadeTop{0xd1, 0xd5, 0xee},
            .background{0xee, 0xf2, 0xff},
            .bar = Qt::transparent,
            .post{0xd6, 0xda, 0xf0},
            .postBorder{0xb7, 0xc5, 0xd9},
            .highlight{0xd6, 0xba, 0xd0},
            .highlightBorder{0xba, 0x9d, 0xbf},
            .text{0x00, 0x00, 0x00},
            .muted{0x70, 0x70, 0x70},
            .disabled{0x8a, 0x8a, 0xa0},
            .subject{0x0f, 0x0c, 0x5d},
            .label{0x11, 0x77, 0x43},
            .link{0x34, 0x34, 0x5c},
            .linkHover{0xdd, 0x00, 0x00},
            .title{0xaf, 0x0a, 0x0f},
            .star{0xaf, 0x0a, 0x0f},
            .starHover{0xdd, 0x00, 0x00},
            .input{0xff, 0xff, 0xff},
            .inputBorder{0xaa, 0xaa, 0xaa},
            .blank{0xff, 0xff, 0xff},
        };
        static const std::vector<Theme> all{defaultDark, yotsubaB};
        return all;
    }

    const Theme &theme()
    {
        return themes()[current];
    }

    void apply(const QString &name)
    {
        current = 0;
        for (size_t i = 0; i < themes().size(); ++i)
            if (themes()[i].name == name)
                current = i;
        if (!name.isEmpty() && theme().name != name)
            spdlog::warn("No theme \"{}\", using {}", name, theme().name);

        QApplication::setPalette(palette(theme()));
        qApp->setStyleSheet(sheet(theme()));
    }

    QString link(const QString &href, const QString &text)
    {
        return QString("<a href=\"%1\" style=\"color: %2; text-decoration: none;\">%3</a>")
            .arg(href.toHtmlEscaped(), theme().link.name(), text.toHtmlEscaped());
    }

    QString muted(const QString &html)
    {
        return QString("<span style=\"color: %1;\">%2</span>").arg(theme().muted.name(), html);
    }

} /* namespace style */
