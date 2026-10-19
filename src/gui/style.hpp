#pragma once

#include <vector>

#include <QColor>
#include <QString>

/**
 * Color themes: Default Dark (after Gitea's dark mode) and Yotsuba B, the
 * blue 4chan/Discourse theme.
 */
namespace style {

    struct Theme {
        QString name;

        /* Page background: a fade from fadeTop at the top to background at the bottom */
        QColor fadeTop;
        QColor background;
        QColor bar;  /* behind the top bar, transparent to show the page */

        /* Posts: filled boxes with a border on their right and bottom only */
        QColor post;
        QColor postBorder;
        QColor highlight;  /* a hovered/selected post */
        QColor highlightBorder;

        QColor text;
        QColor muted;  /* secondary text, e.g. Japanese names next to romanized ones */
        QColor disabled;
        QColor subject;
        QColor label;  /* field names, e.g. "Released" (poster names on 4chan) */
        QColor link;
        QColor linkHover;
        QColor title;  /* a checked button */
        QColor star;  /* stars and bookmark ribbons */
        QColor starHover;

        QColor input;  /* text fields, lists */
        QColor inputBorder;
        QColor blank;  /* behind an image not loaded yet, an unfilled star */
    };

    /* Every theme, Default Dark first */
    const std::vector<Theme> &themes();

    /* The theme in use */
    const Theme &theme();

    /*
     * Use the theme named `name` (Default Dark if there is none) for the
     * application's palette and style sheet. Widgets that paint with theme()
     * colors themselves must be repainted, rich text rebuilt.
     */
    void apply(const QString &name);

    /*
     * The only font family the GUI uses, loaded from fonts/inter: the
     * static fonts cut for text sizes. Qt only registers the default instance
     * of the variable fonts, which would leave bold faked.
     */
    inline const QString FONT_FAMILY = "Inter 18pt";

    /*
     * For the Japanese text Inter has no glyphs for (titles, names), loaded
     * from fonts/noto-sans-cjk-jp. Listed after Inter so it only fills
     * in what Inter lacks.
     */
    inline const QString CJK_FONT_FAMILY = "Noto Sans CJK JP";

    /*
     * Load the bundled fonts and make Inter, with CJK_FONT_FAMILY behind it,
     * the application font. Call before
     * apply(). Returns false if they could not be loaded.
     */
    bool loadFonts();

    /* <a> styled as a link of the theme, for rich text QLabels */
    QString link(const QString &href, const QString &text);

    /* <span> in the theme's muted color */
    QString muted(const QString &html);

} /* namespace style */
