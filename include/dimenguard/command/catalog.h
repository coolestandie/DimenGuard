#pragma once

#include "dimenguard/i18n/translator.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {

enum class CommandSection {
    Selection,
    Regions,
    Protection,
    General
};
enum class ParameterKind {
    Word,
    Integer,
    Player,
    Message,
    Choice
};

struct CommandParameter {
    std::string_view name;
    ParameterKind kind;
    bool optional = false;
    std::vector<std::string_view> choices;
};

struct CommandSpec {
    std::string_view path;
    CommandSection section;
    Message description;
    bool requires_admin;
    std::vector<CommandParameter> parameters;
    bool optional_path = false;
};

[[nodiscard]] std::span<const CommandSpec> commandCatalog();
[[nodiscard]] std::vector<CommandSpec> clientCommandCatalog();
[[nodiscard]] std::string helpUsage(const CommandSpec &command);
[[nodiscard]] std::vector<std::string> nativeUsages();

}
