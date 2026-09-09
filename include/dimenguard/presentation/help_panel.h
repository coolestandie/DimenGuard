#pragma once

#include "dimenguard/i18n/translator.h"

#include <string>
#include <vector>

namespace dimenguard {

[[nodiscard]] std::vector<std::string> renderHelp(Locale locale, bool can_manage);

}
