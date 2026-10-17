#include "gui/style.hpp"

namespace style {

    QString sheet()
    {
        /* %1.. are filled in below, in the order of the arg() calls */
        return QString(R"(
* {
    font-family: Arial, Helvetica, sans-serif;
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
    font-family: Tahoma, sans-serif;
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
            .arg(HIGHLIGHT.name(), HIGHLIGHT_BORDER.name());
    }

    QString link(const QString &href, const QString &text)
    {
        return QString("<a href=\"%1\" style=\"color: %2; text-decoration: none;\">%3</a>")
            .arg(href.toHtmlEscaped(), LINK.name(), text.toHtmlEscaped());
    }

} /* namespace style */
