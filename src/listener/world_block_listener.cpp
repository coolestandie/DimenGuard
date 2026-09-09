#include "dimenguard/listener/world_block_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/block/block.h>
#include <endstone/event/block/block_form_event.h>
#include <endstone/event/block/block_from_to_event.h>
#include <endstone/event/block/leaves_decay_event.h>

namespace dimenguard {

WorldBlockListener::WorldBlockListener(ProtectionContext &context) : context_(context) {}

void WorldBlockListener::registerEvents()
{
    context_.registerGuarded(&WorldBlockListener::onFlow, *this);
    context_.registerGuarded(&WorldBlockListener::onForm, *this);
    context_.registerGuarded(&WorldBlockListener::onLeafDecay, *this);
}

bool WorldBlockListener::onFlow(endstone::BlockFromToEvent &event)
{
    return context_.allowed(event.getBlock()->getLocation(), Flag::FluidFlow) &&
           context_.allowed(event.getToBlock().getLocation(), Flag::FluidFlow);
}

bool WorldBlockListener::onForm(endstone::BlockFormEvent &event)
{
    return context_.allowed(event.getBlock()->getLocation(), Flag::BlockForm);
}

bool WorldBlockListener::onLeafDecay(endstone::LeavesDecayEvent &event)
{
    return context_.allowed(event.getBlock()->getLocation(), Flag::LeafDecay);
}

}
