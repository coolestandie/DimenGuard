#pragma once

#include "dimenguard/listener/actor_listener.h"
#include "dimenguard/listener/block_listener.h"
#include "dimenguard/listener/explosion_listener.h"
#include "dimenguard/listener/mob_listener.h"
#include "dimenguard/listener/player_interaction_listener.h"
#include "dimenguard/listener/player_movement_listener.h"
#include "dimenguard/listener/player_session_listener.h"
#include "dimenguard/listener/world_block_listener.h"
#include "dimenguard/protection/protection_context.h"

namespace dimenguard {

class DimenGuardPlugin;

class ProtectionListener {
public:
    explicit ProtectionListener(DimenGuardPlugin &plugin);
    void registerEvents();

private:
    ProtectionContext context_;
    BlockListener blocks_;
    PlayerInteractionListener player_interactions_;
    ActorListener actors_;
    PlayerSessionListener sessions_;
    WorldBlockListener world_blocks_;
    ExplosionListener explosions_;
    MobListener mobs_;
    PlayerMovementListener movements_;
};

}
