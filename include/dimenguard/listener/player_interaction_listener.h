#pragma once

namespace endstone {
class PlayerArmorStandManipulateEvent;
class PlayerBucketEmptyEvent;
class PlayerBucketEvent;
class PlayerBucketFillEvent;
class PlayerInteractActorEvent;
class PlayerInteractEvent;
}

namespace dimenguard {

class ProtectionContext;

class PlayerInteractionListener {
public:
    explicit PlayerInteractionListener(ProtectionContext &context);
    void registerEvents();

private:
    bool bucketAllowed(endstone::PlayerBucketEvent &event);
    bool onBucketFill(endstone::PlayerBucketFillEvent &event);
    bool onBucketEmpty(endstone::PlayerBucketEmptyEvent &event);
    bool onInteract(endstone::PlayerInteractEvent &event);
    bool onInteractActor(endstone::PlayerInteractActorEvent &event);
    bool onArmorStandManipulate(endstone::PlayerArmorStandManipulateEvent &event);
    ProtectionContext &context_;
};

}
