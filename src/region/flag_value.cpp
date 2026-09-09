#include "dimenguard/region/flag_value.h"

#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace dimenguard {
namespace {
constexpr std::array type_names{std::string_view{"state"},  std::string_view{"string"},   std::string_view{"integer"},
                                std::string_view{"double"}, std::string_view{"location"}, std::string_view{"boolean"},
                                std::string_view{"set"}};

bool validText(std::string_view text, std::size_t maximum)
{
    if (text.size() > maximum) {
        return false;
    }
    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index++]);
        if (first < 0x20 || first == 0x7f) {
            return false;
        }
        if (first < 0x80) {
            continue;
        }
        std::uint32_t codepoint = 0;
        std::size_t trailing = 0;
        std::uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            codepoint = first & 0x1f;
            trailing = 1;
            minimum = 0x80;
        }
        else if (first >= 0xe0 && first <= 0xef) {
            codepoint = first & 0x0f;
            trailing = 2;
            minimum = 0x800;
        }
        else if (first >= 0xf0 && first <= 0xf4) {
            codepoint = first & 0x07;
            trailing = 3;
            minimum = 0x10000;
        }
        else {
            return false;
        }
        if (trailing > text.size() - index) {
            return false;
        }
        for (std::size_t remaining = 0; remaining < trailing; ++remaining) {
            const auto next = static_cast<unsigned char>(text[index++]);
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
            (codepoint >= 0x80 && codepoint <= 0x9f) || codepoint == 0x2028 || codepoint == 0x2029) {
            return false;
        }
    }
    return true;
}

template <typename T>
std::optional<T> number(std::string_view text)
{
    T result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return result;
}

std::string decimal(double value)
{
    std::array<char, 128> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general,
                                      std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{}) {
        throw std::invalid_argument("Cannot encode numeric flag value");
    }
    return {buffer.data(), result.ptr};
}

std::vector<std::string_view> split(std::string_view text)
{
    std::vector<std::string_view> parts;
    for (;;) {
        const auto separator = text.find(',');
        parts.push_back(text.substr(0, separator));
        if (separator == std::string_view::npos) {
            return parts;
        }
        text.remove_prefix(separator + 1);
    }
}

bool validToken(std::string_view text, std::size_t maximum)
{
    return !text.empty() && validText(text, maximum) && text.find_first_of(",[]") == std::string_view::npos &&
           text.front() != ' ' && text.back() != ' ';
}
}

FlagType FlagValue::type() const
{
    return static_cast<FlagType>(value_.index());
}

std::optional<FlagState> FlagValue::state() const
{
    const auto *value = get<FlagState>();
    return value ? std::optional{*value} : std::nullopt;
}

std::string_view valueTypeName(FlagType type)
{
    const auto index = static_cast<std::size_t>(type);
    if (index >= type_names.size()) {
        throw std::invalid_argument("Unknown flag value type");
    }
    return type_names[index];
}

std::optional<FlagType> parseFlagType(std::string_view name)
{
    for (std::size_t index = 0; index < type_names.size(); ++index) {
        if (type_names[index] == name) {
            return static_cast<FlagType>(index);
        }
    }
    return std::nullopt;
}

void validateFlagValue(FlagType type, const FlagValue &value)
{
    static_cast<void>(valueTypeName(type));
    if (type != value.type()) {
        throw std::invalid_argument("Flag value does not match its registered type");
    }
    switch (type) {
    case FlagType::State:
        if (const auto state = *value.get<FlagState>();
            state != FlagState::Inherit && state != FlagState::Allow && state != FlagState::Deny) {
            throw std::invalid_argument("Unknown flag state");
        }
        break;
    case FlagType::String:
        if (!validText(*value.get<std::string>(), maximum_flag_text_bytes)) {
            throw std::invalid_argument("Flag text must be bounded printable UTF-8");
        }
        break;
    case FlagType::Integer:
    case FlagType::Boolean:
        break;
    case FlagType::Double:
        if (!std::isfinite(*value.get<double>())) {
            throw std::invalid_argument("Numeric flag values must be finite");
        }
        break;
    case FlagType::Location: {
        const auto &location = *value.get<FlagLocation>();
        if (!validToken(location.level, maximum_flag_text_bytes) ||
            !validToken(location.dimension, maximum_flag_text_bytes) || !std::isfinite(location.x) ||
            !std::isfinite(location.y) || !std::isfinite(location.z) || !std::isfinite(location.yaw) ||
            !std::isfinite(location.pitch) || location.pitch < -90 || location.pitch > 90) {
            throw std::invalid_argument("Invalid flag location");
        }
        break;
    }
    case FlagType::Set: {
        const auto &entries = *value.get<FlagSet>();
        std::size_t encoded_size = entries.empty() ? 2 : entries.size() - 1;
        for (const auto &entry : entries) {
            if (!validToken(entry, maximum_flag_set_entry_bytes)) {
                throw std::invalid_argument("Set entries must be bounded printable tokens without commas or brackets");
            }
            encoded_size += entry.size();
        }
        if (entries.size() > maximum_flag_set_entries || encoded_size > maximum_encoded_flag_bytes) {
            throw std::invalid_argument("Flag set exceeds its size limit");
        }
        break;
    }
    }
}

std::optional<FlagValue> parseFlagValue(FlagType type, std::string_view text)
{
    if (text.size() > maximum_encoded_flag_bytes) {
        return std::nullopt;
    }
    std::optional<FlagValue> value;
    switch (type) {
    case FlagType::State:
        if (text == "inherit") {
            value = FlagState::Inherit;
        }
        else if (text == "allow") {
            value = FlagState::Allow;
        }
        else if (text == "deny") {
            value = FlagState::Deny;
        }
        break;
    case FlagType::String:
        value = FlagValue(text);
        break;
    case FlagType::Integer:
        if (const auto parsed = number<std::int64_t>(text)) {
            value = FlagValue(*parsed);
        }
        break;
    case FlagType::Double:
        if (const auto parsed = number<double>(text)) {
            value = FlagValue(*parsed);
        }
        break;
    case FlagType::Boolean:
        if (text == "true" || text == "false") {
            value = FlagValue(text == "true");
        }
        break;
    case FlagType::Set: {
        FlagSet entries;
        if (text != "[]") {
            const auto parts = split(text);
            if (parts.size() > maximum_flag_set_entries) {
                return std::nullopt;
            }
            for (auto part : parts) {
                while (!part.empty() && part.front() == ' ') {
                    part.remove_prefix(1);
                }
                while (!part.empty() && part.back() == ' ') {
                    part.remove_suffix(1);
                }
                entries.emplace(part);
            }
        }
        value = FlagValue(std::move(entries));
        break;
    }
    case FlagType::Location: {
        const auto parts = split(text);
        if (parts.size() != 5 && parts.size() != 7) {
            return std::nullopt;
        }
        std::array<double, 5> coordinates{};
        for (std::size_t index = 2; index < parts.size(); ++index) {
            const auto parsed = number<double>(parts[index]);
            if (!parsed) {
                return std::nullopt;
            }
            coordinates[index - 2] = *parsed;
        }
        value = FlagValue(FlagLocation{std::string(parts[0]), std::string(parts[1]), coordinates[0], coordinates[1],
                                       coordinates[2], coordinates[3], coordinates[4]});
        break;
    }
    default:
        return std::nullopt;
    }
    if (value) {
        try {
            validateFlagValue(type, *value);
        }
        catch (const std::invalid_argument &) {
            return std::nullopt;
        }
    }
    return value;
}

std::string formatFlagValue(const FlagValue &value)
{
    validateFlagValue(value.type(), value);
    switch (value.type()) {
    case FlagType::State:
        switch (*value.get<FlagState>()) {
        case FlagState::Inherit:
            return "inherit";
        case FlagState::Allow:
            return "allow";
        case FlagState::Deny:
            return "deny";
        }
        break;
    case FlagType::String:
        return *value.get<std::string>();
    case FlagType::Integer:
        return std::to_string(*value.get<std::int64_t>());
    case FlagType::Double:
        return decimal(*value.get<double>());
    case FlagType::Boolean:
        return *value.get<bool>() ? "true" : "false";
    case FlagType::Location: {
        const auto &location = *value.get<FlagLocation>();
        return location.level + "," + location.dimension + "," + decimal(location.x) + "," + decimal(location.y) + "," +
               decimal(location.z) + "," + decimal(location.yaw) + "," + decimal(location.pitch);
    }
    case FlagType::Set: {
        std::string result;
        for (const auto &entry : *value.get<FlagSet>()) {
            if (!result.empty()) {
                result += ',';
            }
            result += entry;
        }
        return result.empty() ? "[]" : result;
    }
    }
    throw std::invalid_argument("Unknown flag value type");
}
}
