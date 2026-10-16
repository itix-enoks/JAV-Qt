#include <chrono>
#include <vector>
#include <optional>

namespace model {

    struct Director {
        QString id;
        QString nameRo;
    };
    struct Maker {
        QString id;
        QString nameRo;
    };
    struct Genre {
        QString id;
        QString titleEn;
    };
    struct UserRating {
        int value;  /* value in { 0.5 * n | n in N ^ x <= 10 } */
    };
    struct Actress {
        QString id;
        QString nameRo;
    };

    struct Video {
        QString titleEn;
        QString id;
        std::chrono::year_month_day releaseDate;
        int lengthMin;
        std::optional<Director> director;
        std::optional<Maker> maker;
        std::vector<Genre> genres;
        std::optional<UserRating> rating;
        std::vector<Actress> cast;
    };

} /* namespace model */
