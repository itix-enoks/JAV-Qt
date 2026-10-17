#pragma once

#include <QColor>
#include <QString>

/**
 * Yotsuba B, the blue 4chan/Discourse theme.
 */
namespace style {

    /* Page background: a fade from FADE_TOP at the top to BACKGROUND at the bottom */
    inline const QColor FADE_TOP{0xd1, 0xd5, 0xee};
    inline const QColor BACKGROUND{0xee, 0xf2, 0xff};

    /* Posts: filled boxes with a border on their right and bottom only */
    inline const QColor POST{0xd6, 0xda, 0xf0};
    inline const QColor POST_BORDER{0xb7, 0xc5, 0xd9};
    inline const QColor HIGHLIGHT{0xd6, 0xba, 0xd0};  /* a hovered/selected post */
    inline const QColor HIGHLIGHT_BORDER{0xba, 0x9d, 0xbf};

    inline const QColor SUBJECT{0x0f, 0x0c, 0x5d};
    inline const QColor NAME{0x11, 0x77, 0x43};
    inline const QColor LINK{0x34, 0x34, 0x5c};
    inline const QColor LINK_HOVER{0xdd, 0x00, 0x00};
    inline const QColor TITLE{0xaf, 0x0a, 0x0f};

    /*
     * The only font family the GUI uses, loaded from assets/fonts/inter: the
     * static fonts cut for text sizes. Qt only registers the default instance
     * of the variable fonts, which would leave bold faked.
     */
    inline const QString FONT_FAMILY = "Inter 18pt";

    /*
     * For the Japanese text Inter has no glyphs for (titles, names), loaded
     * from assets/fonts/noto-sans-cjk-jp. Listed after Inter so it only fills
     * in what Inter lacks.
     */
    inline const QString CJK_FONT_FAMILY = "Noto Sans CJK JP";

    /*
     * Load the bundled fonts and make Inter, with CJK_FONT_FAMILY behind it,
     * the application font. Call before
     * applying sheet(). Returns false if they could not be loaded.
     */
    bool loadFonts();

    /* Application-wide style sheet */
    QString sheet();

    /* <a> styled as a Yotsuba B link, for rich text QLabels */
    QString link(const QString &href, const QString &text);

} /* namespace style */
