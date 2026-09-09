#pragma once

#include <string>
#include <string_view>

namespace dimenguard {

struct Theme {
    static constexpr std::string_view Amethyst = "\xc2\xa7u";
    static constexpr std::string_view White = "\xc2\xa7"
                                              "f";
    static constexpr std::string_view LightGray = "\xc2\xa7"
                                                  "7";
    static constexpr std::string_view Muted = "\xc2\xa7j";
    static constexpr std::string_view DarkGray = "\xc2\xa7"
                                                 "8";
    static constexpr std::string_view Reset = "\xc2\xa7r";

    [[nodiscard]] static std::string identity();
    [[nodiscard]] static std::string decorate(std::string_view text);
};

}
