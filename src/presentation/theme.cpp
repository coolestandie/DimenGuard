#include "dimenguard/presentation/theme.h"

#include <format>

namespace dimenguard {

std::string Theme::identity()
{
    return std::format("{}Dimen{}Guard", Amethyst, LightGray);
}

std::string Theme::decorate(std::string_view text)
{
    return std::format("{}{} > {}{}{}", identity(), DarkGray, White, text, Reset);
}

}
