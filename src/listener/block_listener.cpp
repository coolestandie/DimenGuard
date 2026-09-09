#include "dimenguard/listener/block_listener.h"

#include "dimenguard/protection/protection_context.h"

#include <endstone/block/block.h>
#include <endstone/event/block/block_break_event.h>
#include <endstone/event/block/block_place_event.h>

namespace dimenguard {

BlockListener::BlockListener(ProtectionContext &context) : context_(context) {}

void BlockListener::registerEvents()
{
    context_.registerGuarded(&BlockListener::onBreak, *this);
    context_.registerGuarded(&BlockListener::onPlace, *this);
}

bool BlockListener::onBreak(endstone::BlockBreakEvent &event)
{
    return context_.allowed(*event.getPlayer(), event.getBlock()->getLocation(), Flag::BlockBreak);
}

bool BlockListener::onPlace(endstone::BlockPlaceEvent &event)
{
    const auto &block = *event.getBlockPlaced();
    return context_.allowed(*event.getPlayer(), block.getLocation(), Flag::BlockPlace) &&
           context_.chestNeighbors(*event.getPlayer(), block);
}

}
