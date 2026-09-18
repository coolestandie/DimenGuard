#pragma once

#include "dimenguard/rules/explosion_rules.h"

#include <endstone/util/pointers.h>
#include <vector>

namespace endstone {
class ActorDamageEvent;
class ActorExplodeEvent;
class Block;
class BlockExplodeEvent;
class Location;
}

namespace dimenguard {

class ProtectionContext;

class ExplosionListener {
public:
    explicit ExplosionListener(ProtectionContext &context);
    void registerEvents();

private:
    bool protect(const endstone::Location &origin, ExplosionSource source,
                 std::vector<endstone::NotNull<endstone::Block>> &blocks);
    bool onActorExplosion(endstone::ActorExplodeEvent &event);
    bool onBlockExplosion(endstone::BlockExplodeEvent &event);
    bool onDamage(endstone::ActorDamageEvent &event);

    ProtectionContext &context_;
};

}
