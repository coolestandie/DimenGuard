#include "dimenguard/rules/damage_rules.h"

namespace dimenguard {

std::optional<Flag> damageFlag(std::string_view damage_type)
{
    if (damage_type == "fall") {
        return Flag::FallDamage;
    }
    if (damage_type == "fireworks") {
        return Flag::FireworkDamage;
    }
    return std::nullopt;
}

}
