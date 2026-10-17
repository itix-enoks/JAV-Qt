#pragma once

#include <vector>

#include <QSize>
#include <QString>

#include "models/video.hpp"

/**
 * What the GUI shows for the model: romanized/English text when known (from
 * JavLibrary or DMM's picture names), else DMM's Japanese.
 */
namespace display {

    template <typename T>
    QString name(const T &person)
    {
        return person.nameRo.isEmpty() ? person.nameJa : person.nameRo;
    }

    inline QString name(const model::Genre &genre)
    {
        return genre.titleEn.isEmpty() ? genre.titleJa : genre.titleEn;
    }

    inline QString title(const model::Video &video)
    {
        return video.titleEn.isEmpty() ? video.titleJa : video.titleEn;
    }

    /*
     * Smallest thumbnail at least `minWidth` wide, else the largest one; the
     * lists are stored smallest first. Null if there are none.
     */
    inline const model::Thumbnail *pick(const std::vector<model::Thumbnail> &sizes, int minWidth)
    {
        for (const model::Thumbnail &t : sizes)
            if (t.width >= minWidth)
                return &t;
        return sizes.empty() ? nullptr : &sizes.back();
    }

} /* namespace display */
