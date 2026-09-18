#pragma once

#include "dimenguard/i18n/translator.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dimenguard {

using HelpPermissionCheck = std::function<bool(std::string_view)>;

[[nodiscard]] std::vector<std::string> renderHelp(Locale locale, const HelpPermissionCheck &can_use);
[[nodiscard]] std::vector<std::string> renderHelp(Locale locale, bool can_manage);

}
