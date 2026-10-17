#pragma once

#include <chrono>
#include <vector>
#include <optional>

#include <QUrl>
#include <QString>
#include <QStringList>

namespace model {

    struct Base {};

    enum class Type {
        Director,
        Maker,
        Genre,
        UserRating,
        Actress,
        Video
    };

    struct Director : model::Base {
        QString id;
        QString nameRo;
        QString nameJa;  /* DMM only */
    };

    struct Maker : model::Base {
        QString id;
        QString nameRo;
        QString nameJa;  /* DMM only */
    };

    struct Genre : model::Base {
        QString id;
        QString titleEn;  /* JavLibrary only */
        QString titleJa;  /* DMM only */
    };

    struct UserRating : model::Base {
        double value;  /* value in { 0.5 * n | n in N ^ x <= 10 } */
    };

    /**
     * One downloaded image. Until ThumbnailFetcher has downloaded it, only url
     * is set (a candidate scraped from the page, path still empty).
     */
    struct Thumbnail : model::Base {
        QString path;  /* relative to paths::dataDir(), e.g. "assets/thumbnails/MKMP-705/cover-800x537.jpg" */
        QUrl url;      /* where it was downloaded from */
        int width = 0;
        int height = 0;
    };

    struct Actress : model::Base {
        QString id;
        QString nameRo;
        QString nameJa;  /* DMM only */
        QStringList aliasesRo;  /* e.g. "Otsu Alice"; only parsed from the page, not stored */
        std::vector<Thumbnail> thumbnails;  /* one per available size, smallest first */
    };

    /* Where a video's ids (cast, director, maker, genres) come from */
    namespace provider {
        inline const QString DMM = "dmm";
        inline const QString JAVLIBRARY = "javlibrary";
    }

    /**
     * Found on DMM: cast, director, maker, dates and images come from DMM, with
     * the English title and genres (and romanized director/maker names, rating)
     * added from JavLibrary when it has the video too. Otherwise everything
     * comes from JavLibrary.
     */
    struct Video : model::Base {
        QString provider;  /* model::provider::*, empty for entries stored before providers existed */
        QString dmmId;     /* DMM's content id, e.g. "mkmp00705" */
        QString titleEn;
        QString titleJa;
        QString id;
        std::chrono::year_month_day releaseDate;
        int lengthMin;
        std::optional<Director> director;
        std::optional<Maker> maker;
        std::vector<Genre> genres;
        std::optional<UserRating> rating;
        std::vector<Actress> cast;

        /* Each one per available size, smallest first */
        std::vector<Thumbnail> covers;   /* full jacket, front and back */
        std::vector<Thumbnail> posters;  /* front of the jacket only */
        std::vector<std::vector<Thumbnail>> previews;  /* sample stills */
    };

    struct JavId : model::Base {
        QString prefix;  /* e.g. XVSR in [XVSR-851] */
        int suffix;  /* e.g. 851 in [XVSR-851] */
    };

} /* namespace model */
