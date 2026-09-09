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

/** Native player arguments can retain their outer quotes; only individual names are supported. */
[[nodiscard]] inline std::optional<std::string_view> parsePlayerName(std::string_view text)
{
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }
    if (text.empty() || text.front() == '@' || text.front() == ' ' || text.back() == ' ') {
        return std::nullopt;
    }
    for (const unsigned char character : text) {
        if (character == '"' || character < 32 || character == 127) {
            return std::nullopt;
        }
    }
    return text;
}

[[nodiscard]] inline bool playerNamesMatch(std::string_view first, std::string_view second)
{
    if (first.size() != second.size()) {
        return false;
    }
    const auto lower = [](unsigned char character) {
        return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
    };
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (lower(static_cast<unsigned char>(first[i])) != lower(static_cast<unsigned char>(second[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace dimenguard
