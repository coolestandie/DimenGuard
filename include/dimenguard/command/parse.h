#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace dimenguard {

[[nodiscard]] inline std::optional<int> parseInteger(std::string_view text)
{
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] inline bool isCanonicalUuid(std::string_view text)
{
    if (text.size() != 36) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') {
                return false;
            }
        }
        else if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) {
            return false;
        }
    }
    return text != "00000000-0000-0000-0000-000000000000";
}

}  // namespace dimenguard
