#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace dimenguard {

enum class FlagState {
    Inherit,
    Allow,
    Deny
};
enum class FlagType {
    State,
    String,
    Integer,
    Double,
    Location,
    Boolean,
    Set
};

using FlagSet = std::set<std::string>;

struct FlagLocation {
    std::string level;
    std::string dimension;
    double x = 0;
    double y = 0;
    double z = 0;
    double yaw = 0;
    double pitch = 0;

    bool operator==(const FlagLocation &) const = default;
};

inline constexpr std::size_t maximum_flag_text_bytes = 256;
inline constexpr std::size_t maximum_flag_set_entries = 64;
inline constexpr std::size_t maximum_flag_set_entry_bytes = 128;
inline constexpr std::size_t maximum_encoded_flag_bytes = 8192;

class FlagValue {
public:
    FlagValue() = default;
    FlagValue(FlagState value) : value_(value) {}
    explicit FlagValue(std::string value) : value_(std::move(value)) {}
    explicit FlagValue(std::string_view value) : FlagValue(std::string(value)) {}
    explicit FlagValue(const char *value) : FlagValue(std::string(value)) {}
    explicit FlagValue(std::int64_t value) : value_(value) {}
    explicit FlagValue(double value) : value_(value) {}
    explicit FlagValue(bool value) : value_(value) {}
    explicit FlagValue(FlagLocation value) : value_(std::move(value)) {}
    explicit FlagValue(FlagSet value) : value_(std::move(value)) {}

    template <typename T>
    [[nodiscard]] const T *get() const
    {
        return std::get_if<T>(&value_);
    }

    [[nodiscard]] FlagType type() const;
    [[nodiscard]] std::optional<FlagState> state() const;
    bool operator==(const FlagValue &) const = default;
    [[nodiscard]] bool operator==(FlagState state) const { return this->state() == state; }

private:
    std::variant<FlagState, std::string, std::int64_t, double, FlagLocation, bool, FlagSet> value_ = FlagState::Inherit;
};

[[nodiscard]] std::string_view valueTypeName(FlagType type);
[[nodiscard]] std::optional<FlagType> parseFlagType(std::string_view name);
[[nodiscard]] std::optional<FlagValue> parseFlagValue(FlagType type, std::string_view text);
void validateFlagValue(FlagType type, const FlagValue &value);
[[nodiscard]] std::string formatFlagValue(const FlagValue &value);

}
