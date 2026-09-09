# Public-event coverage audit

This is a static source audit, performed on 2026-09-08 against Endstone API 0.12.0, checkout `2572cd304b5ca2d094e02a1b2f969f7632ea44f6`. No Bedrock server was started for this audit. A hook found in source is evidence of an intended event path, not a successful runtime protection test or a guarantee about every Bedrock interaction.

Paths below are relative to the Endstone SDK repository, not DimenGuard; `bedrock_hooks/` abbreviates `src/endstone/runtime/bedrock_hooks/`. The integration contracts describe the implemented public-event adapters. Native hooks remain outside this plugin.

## World and movement adapters

These nine additional state flags are implemented but not yet accepted in-game. They default
to allow so upgrading does not silently disable autonomous world behavior in existing regions.

| Flag | Concrete source path | Implemented scope and remaining gap |
| --- | --- | --- |
| `explosions` | `bedrock_hooks/script_block_gameplay_handler.cpp`, block/actor explosion handlers; `script_actor_gameplay_handler.cpp`, before-hurt | Check origin and each exposed affected block; cancel the whole explosion on denial. Also check victims of `block_explosion` and `entity_explosion` damage. Secondary effects/knockback are not guaranteed. |
| `fluid-flow` | `bedrock_hooks/liquid_block.cpp`, `_trySpreadTo` | Check both from/to blocks. Instant-ticking paths bypass this hook. |
| `block-form` | `bedrock_hooks/liquid_block.cpp`, `_solidify` | Lava solidification into cobblestone, obsidian and basalt. Header examples for snow/ice are not evidence of additional firing paths. |
| `leaf-decay` | `bedrock_hooks/leaves_block.cpp`, `_die` | Cancel the leaf-decay path before drops/removal. |
| `actor-griefing` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, `ActorGriefingBlockEvent` | Check exposed target block. Falling blocks and sheep grass consumption are excluded; resulting block state is absent. |
| `mob-spawning` | `bedrock_hooks/script_level_gameplay_handler.cpp`, actor-added event | Restrict non-player Mob instances only. Cancellation despawns an actor already added; no spawn reason is exposed. |
| `mob-damage` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, before-hurt | Attributed non-player Mob attacking Player; decide at victim. Missing attribution cannot be reconstructed. |
| `entry`, `exit` | `bedrock_hooks/packet.cpp`, move threshold; `bedrock_hooks/player.cpp`, same-dimension teleport | Resolve changed region sets at reported from/to locations, preserving cancellations. Small movements may not fire. Dimension-change and respawn are after-events, and cross-dimension teleport bypasses the before-event. No complete exclusion or rollback is claimed. |

The shared environmental resolver accepts only environmental flags and has no player identity
or permission bypass. The move adapter skips unchanged block coordinates/dimension, but does
not infer that all movement was reported. Actual-position greetings and advanced session rules
belong to the next planned expansion.

## Missing contracts

- Piston events expose base and direction, not every moved/broken block. Base activation
  control could be added with that narrower name; it is not full cross-boundary protection.
- `BlockGrowEvent` is declared but no concrete firing construction was found.
- Inventory open lacks holder/location, and no concrete inventory click/drag transaction event
  was found. Opening guards do not protect ongoing or automated transfers.
- Bed/door multi-block placement exposes one target, not every affected block.

## Live command suggestions

The version-gated adapter rewrites only the plugin-owned `dg` command in packet 76 through
`PacketSendEvent`. It preserves other commands, aliases, metadata, enum indices and constraints.
It uses the [protocol-2169 schema](https://github.com/EndstoneMC/protocol-docs/tree/r26_u4):
fixed uint32 enum-value indices and string command permissions. Malformed, oversized,
unsupported or conflicting packets pass through without cancellation. Unsupported protocols
retain native command parsing with less detailed hints. Offline codec tests do not prove client
autocomplete display; that remains part of final acceptance.

## Event contracts

| Public event | Hook evidence | Protection decision and limits |
| --- | --- | --- |
| `BlockBreakEvent` | `src/endstone/runtime/bedrock_hooks/script_block_gameplay_handler.cpp`, `handleEvent(BlockTryDestroyByPlayerEvent&)` and `ScriptBlockGameplayHandler::handleEvent4` | Query `build` at `getBlock()` using `getPlayer()`. Cancellation returns `CoordinatorResult::Cancel`. This covers the player destruction path; explosions, commands, pistons, fluids and plugin block edits are different paths. |
| `BlockPlaceEvent` | Same file, `handleEvent(const BlockTryPlaceByPlayerEvent&)` and `handleEvent2` | Query `build` at `getBlockPlaced()`, not only `getBlockAgainst()` or the player's location. The target is a pre-mutation snapshot with the permutation about to be placed. Secondary blocks of doors, beds and similar placements require boundary tests; the event exposes one target, not an affected-block list. |
| `PlayerBucketFillEvent` | `bedrock_hooks/bucket.cpp`, `BucketItem::_useOn`; `bedrock_hooks/cauldron.cpp`, `CauldronBlock::use`; `bedrock_hooks/script_player_gameplay_handler.cpp`, entity interaction before-event | Query `build` at the affected `getBlock()` and handle a null result explicitly. `getBlockClicked()` is available as a conservative fallback. The current paths cover recognized source liquids, powder snow, supported cauldron operations and milking supported animals. Later fluid spread and automation are separate operations. |
| `PlayerBucketEmptyEvent` | `bedrock_hooks/bucket.cpp`, `BucketItem::_useOn`; `bedrock_hooks/cauldron.cpp`, `CauldronBlock::use` | Use `getBlock()` for the affected position. The hook resolves waterlogging versus adjacent placement; recomputing the destination from the clicked face would discard that work. Test water, lava, powder snow and creature buckets at region edges. |
| `PlayerInteractEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, block before-event; `bedrock_hooks/script_item_gameplay_handler.cpp`, item use; `bedrock_hooks/packet.cpp`, destroy-start and missed swing input | Inspect `getAction()` and the nullable `getBlock()`. For block actions, query the target block. A captured `Container` can select `container-access` for right-clicks; other targeted interactions use `interact`. Left-click destruction should retain the separate `build` decision. Air interactions have no target block; deciding at the player's position is a separate documented policy, not remote target protection. |
| `PlayerInteractActorEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, `handleEvent(const PlayerInteractWithEntityBeforeEvent&)` | Query the relevant action at `getActor()->getLocation()` using `getPlayer()`. Cancellation returns `CoordinatorResult::Cancel`. Actor inventories do not have an exposed inventory-holder mapping; a generic actor interaction guard is not complete inventory transaction protection. |
| `PlayerArmorStandManipulateEvent` | `bedrock_hooks/armor_stand.cpp` | A distinct event name despite inheriting from `PlayerInteractActorEvent`. If enabled, register a separate listener and call the same actor-policy helper. Do not assume a parent-class listener receives it. |
| `ActorDamageEvent` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, `handleEvent(ActorBeforeHurtEvent&)` and `handleEvent4` | Victim is `NotNull<Mob>`. For PvP, resolve both the victim and the responsible source as players. `getDamageSource()->getActor()` returns the attributed attacker, including a resolved projectile shooter. `getDamagingActor()` instead returns the direct projectile. Query attacker and victim positions if the policy protects both sides of a region boundary. An unloaded or missing shooter produces a null handle; historical shooter position is not available. Damage cancellation is not proof that every knockback, potion-effect or delayed environmental consequence is suppressed. |
| `InventoryOpenEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, `handleEvent(const PlayerOpenContainerEvent&)` | The hook knows `block_pos` but exposes only `Inventory&` and player. The public API has no holder or location accessor. A precise region decision cannot be made from this event alone. The event is dispatched through a void gameplay event, so screen-opening cancellation timing also needs runtime verification. |
| `InventoryInteractEvent` | Header `include/endstone/event/inventory/inventory_interact_event.h`; no construction or concrete firing path found | This is an abstract base without `ENDSTONE_EVENT` or a `NAME`; it is not a usable concrete listener in this checkout. No public inventory click/drag subclass was found. Do not claim ongoing transaction protection by registering the base. |
| `PlayerQuitEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, disconnect-event handler | Remove transient selection and notification state keyed by `getPlayer()->getUniqueId()`. This is cleanup, not a cancellable protection action. The hook calls the event before disconnecting the Endstone player wrapper. |

## Inventory and double-chest boundaries

`Block::captureState(false).as<Container>()` identifies supported block containers through the public API without creating an independent block-entity snapshot. `Container::getInventory()` returns `Inventory&`; it does not expose the owner, block position or paired chest. `BlockActorState` has no public NBT accessor for recovering pairing data either. The core excludes data-driven block actors from its generic container conversion, so custom containers cannot be assumed to satisfy this detection.

Each captured container state owns a newly created inventory wrapper. Comparing a newly captured inventory's address with the event inventory is therefore not a reliable holder lookup. Saving the event's inventory reference for later would also outlive the temporary state that owns it.

A chest spanning a region edge can expose its protected half when the player clicks the other half. The public-only conservative approach is to check the clicked chest and all four horizontal adjacent blocks of the same chest type. Apply the same centralized region policy to each candidate and deny the interaction if any candidate denies access. This can also deny access beside an adjacent chest that is not actually paired. Document that behavior; exact pairing requires an appropriate public Endstone API. Check both placement next to an existing chest and opening either half during runtime verification.

The first adapter can protect targeted player opening interactions. It cannot guarantee protection of inventories already open when permissions change, remote/plugin-opened inventories, every entity container, hoppers, droppers or other automated transfers. Using the player's location as a substitute for an unknown inventory's location gives a false sense of protection and must not be represented as holder-aware protection.

Future Endstone API work should expose stable inventory ownership/affected locations, paired-container locations and cancellable concrete inventory transaction events. That work belongs in Endstone with independent runtime tests; it does not justify private memory access in DimenGuard's basic adapter.

## Handle, identity and listener conventions

- Event players and block targets generally use `NotNull<T>`; nullable targets and damage sources use `Nullable<T>`. Dereference with `*handle`, access with `handle->method()`, and test a `Nullable<T>` before use. `.get()` returns a shared pointer, not a raw pointer.
- Narrow supported `Object` handles with `.as<Player>()` or `.as<Container>()`, checking the returned nullable handle. Avoid unchecked downcasts.
- `Player::getUniqueId()` returns `UUID`; `.str()` provides the persistent string identity. `Player::getLocale()` returns a string such as `en_US`. Select Spanish by a normalized `es` language component and fall back to English for unknown locales. Do not identify members by display name or runtime actor ID.
- `Block::getDimension()` and `Actor::getDimension()` return `NotNull<Dimension>`. `Dimension::getLevel()` returns `Level&`; `Dimension::getId()` returns a namespaced `DimensionId`. Preserve both the selected level identity and the complete dimension identifier, not only its local key.
- `Location::getBlockX/Y/Z()` floor coordinates. Plain integer casts truncate negative values and select the wrong block. A location's dimension reference may be missing or unloaded; handle that case explicitly.
- Register concrete events through `Plugin::registerEvent(handler, instance, EventPriority::High, true)`. The final argument skips already cancelled events. Set cancellation only on denial and never undo another plugin's cancellation. `Monitor` is observation-only. A later plugin can still override cancellation; no event priority provides isolation from other plugins.
- Registration is keyed by `EventType::NAME`, not C++ inheritance. Keep every concrete listener thin and route its targets through shared position, actor and policy helpers. Resolve targets and apply cancellation synchronously during the event; do not retain borrowed inventory references or perform persistence writes in the listener.

## Pending in-server verification

Every item below is pending. Record the exact BDS build, Endstone commit, DimenGuard commit and observed outcome when executing it.

1. Create a small region with an owner, a member and an outsider. Check `allow`, `deny`, unset defaults, explicit bypass, operator without bypass, equal-priority overlap and a higher-priority region. Repeat at inclusive corners, negative coordinates and matching coordinates in another dimension.
2. Break and place in survival and creative. Stand outside while targeting inside, and reverse the positions. Test door/bed secondary blocks and neighboring chest placement across the boundary. Check actual server state after reconnecting, not only the immediate client animation.
3. Fill and empty water, lava and powder-snow buckets; waterlog a block; use a supported creature bucket; fill/empty a cauldron; milk an animal. Confirm target positions, inventory counts and absence of duplicated items after denial.
4. Interact with doors, buttons, levers, signs, item frames, lecterns, furnaces, shulker boxes and other containers. Confirm which actions select `interact` or `container-access`, and whether denial blocks both the UI and server-side mutation.
5. Open each half of a normal and trapped double chest straddling a region edge. Check all orientations and chunk edges. Place same-type unpaired chests next to a protected chest and record the conservative over-denial behavior.
6. Interact with mobs, storage actors and armor stands. Verify armor-stand equipment manipulation separately because its concrete event has a different name. Inventory access through an already open, remote or plugin-created UI remains unsupported until a suitable event path is established.
7. Test melee and projectile PvP with the attacker inside, victim inside and both outside. Verify damage, shooter attribution and knockback separately. Repeat with the shooter disconnected before impact; record the unresolved-attribution limitation. Potion effects and environmental damage are separate checks, not implied by melee success.
8. Change trust/flags while a container screen is open, and attempt shift transfers, drag actions and automated extraction. Record the current transaction coverage gap; do not label these paths protected by the opening listener.
9. Reconnect, change locale between English and Spanish, and repeat rapid denied actions. Verify selection cleanup, localization fallback and notification throttling without masking protection decisions.

## Policy risk review and test strategy

The engine's chosen precedence is deliberate: search priority tiers for explicit decisions first; `deny` wins within a tier, and any explicit decision at a higher tier takes precedence over lower tiers. Membership defaults apply only when every matching tier inherits. Consequently, a lower-priority explicit `allow` opens access through a higher-priority region that inherits, and explicit `deny` also denies owners. Document and test those examples so administrators do not interpret ownership or priority as unconditional access.

The independent flags require adapter tests in addition to engine tests. A container click must not accidentally use a generic `interact allow` to skip a `container-access deny`. Conversely, a build permission should not by itself grant container access. Test an interact-allowed/build-denied tool action, a bucket targeting the other side of a boundary and left-clicking a protected item frame. Their correct target and concrete event path cannot be established solely through `RegionManager::isAllowed()` tests.

Use two online identities for cross-boundary PvP checks. A bypass permission on the victim must not grant bypass to an attacker, and the projectile's runtime identity must not be mistaken for its owner. Check both attacker and victim locations with the same responsible-player policy, and record unresolved attribution separately. Reusing one position for both sides creates a sanctuary-boundary escape even when the region engine works correctly.

Keep authority tests separate from membership tests. Administrative permission, region ownership, trusted membership and protection bypass have different purposes. Test an operator without the explicit bypass permission, a trusted player without administration permission and unauthorized attempts to mutate a region. Confirm every administrative mutation goes through the service, so a failed save cannot produce new in-memory permissions that disappear on restart.

Offline verification should cover boundaries and negative coordinates, deterministic overlaps, independent dimensions, invalid enum values, full-width priorities, index correctness against a brute-force oracle and snapshots that outlive their originals. Service tests should inject a database error after a replacement has begun, then verify both the persisted regions and live protection index still contain the previous snapshot. A rejected reload must retain the last known valid protection state. Reopen the database after successful mutations to verify durability; do not infer persistence from the in-memory result.

The database is a single-writer application store. External changes should be read through an explicit successful reload before subsequent administrative writes, because saves replace a complete snapshot. Two live plugin instances or manual database edits during operation are not a coordinated editing interface. Tests for persistence failures should therefore use controlled failure injection, not imply multi-writer conflict resolution.
