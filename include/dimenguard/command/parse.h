#pragma once

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

/** Split a native message tail into bounded arguments; quotes preserve one whole argument. */
[[nodiscard]] inline std::optional<std::vector<std::string>> parseCommandArguments(std::string_view text,
                                                                                   std::size_t max_count)
{
    for (const unsigned char character : text) {
        if (character < 32 || character == 127) {
            return std::nullopt;
        }
    }
    std::vector<std::string> arguments;
    while (true) {
        const auto first = text.find_first_not_of(' ');
        if (first == text.npos) {
            return arguments;
        }
        text.remove_prefix(first);
        if (arguments.size() == max_count) {
            return std::nullopt;
        }
        if (text.front() == '"') {
            const auto closing = text.find('"', 1);
            if (closing == text.npos || (closing + 1 < text.size() && text[closing + 1] != ' ')) {
                return std::nullopt;
            }
            arguments.emplace_back(text.substr(1, closing - 1));
            text.remove_prefix(closing + 1);
        }
        else {
            const auto end = text.find(' ');
            const auto word = text.substr(0, end);
            if (word.find('"') != word.npos) {
                return std::nullopt;
            }
            arguments.emplace_back(word);
            text.remove_prefix(word.size());
        }
    }
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
