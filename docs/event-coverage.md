# Public-event coverage audit

This audit describes the preview baseline. Unreleased WG-2 updates explosion filtering,
source association and actor spawn sets; see [flag semantics](flag-semantics.md) and
[the pinned explosion audit](wg2-explosion-audit.md) for those superseding contracts.

This is a static source audit, performed on 2026-09-08 against Endstone API 0.12.0, checkout `2572cd304b5ca2d094e02a1b2f969f7632ea44f6`. No Bedrock server was started for this audit. A hook found in source is evidence of an intended event path, not a successful runtime protection test or a guarantee about every Bedrock interaction.

A source comparison with the unmodified upstream pin
`46eff9f125f52eac76472d84339ead8fbf51fcd2` found no differences in the 14 checked flag-hook/damage
files or the complete `include/endstone/event` tree. Those files cover script gameplay/chat,
player/packet, bucket, cauldron, armor-stand, liquid/leaf and damage-source paths. This establishes
equivalence of those audited routes only, **not the whole SDK or binary ABI**: the fork's expanded
`Dimension` interface differs, so public and custom-fork DLLs still require separate compatible builds.

Paths below are relative to the Endstone SDK repository, not DimenGuard; `bedrock_hooks/` abbreviates `src/endstone/runtime/bedrock_hooks/`. The integration contracts describe the implemented public-event adapters. Native hooks remain outside this plugin.

## Release-candidate scope: 26 state flags

The 0.2.0 public-preview candidate implements the following flags. None has completed the
gameplay acceptance checklist for this candidate; this is not complete WorldGuard protection.

| Family | Implemented flags |
| --- | --- |
| Existing player rules | `build`, `interact`, `container-access`, `pvp` |
| Granular player actions | `block-break`, `block-place`, `use`, `use-anvil`, `sleep` |
| Items and chat | `item-drop`, `item-pickup`, `send-chat` |
| World and mobs | `explosions`, `fluid-flow`, `water-flow`, `lava-flow`, `block-form`, `leaf-decay`, `actor-griefing`, `mob-spawning`, `mob-damage` |
| Damage | `fall-damage`, `firework-damage`, `invincible` |
| Reported transitions | `entry`, `exit` |

Granular fallbacks are `block-break`/`block-place` to `build`, `use`/`sleep` to `interact`,
`use-anvil` to `use`, and `water-flow`/`lava-flow` to `fluid-flow`. The resolver first considers
explicit values for the requested flag across priority tiers, then follows its aggregate only
if none decides. An explicit granular decision can therefore override an aggregate in another
priority tier. Setting the granular flag to `inherit` restores aggregate resolution.

`build`, `interact` and `container-access` retain membership defaults. Independent flags default
to allow **except `invincible`, which defaults to deny**. Its value enables or disables an
immunity feature, rather than directly permitting damage. See the damage contract below.
Typed strings/sets/numbers/locations, greetings and advanced session behavior remain future
work in [flag-roadmap.md](flag-roadmap.md), not additional working flags.

## World and movement adapters

These original world/transition rules remain implemented but not yet accepted in-game. They
default to allow; granular liquid rules retain their aggregate when not explicitly configured.

| Flag | Concrete source path | Implemented scope and remaining gap |
| --- | --- | --- |
| `explosions` | `bedrock_hooks/script_block_gameplay_handler.cpp`, block/actor explosion handlers; `script_actor_gameplay_handler.cpp`, before-hurt | Check origin and each exposed affected block; cancel the whole explosion on denial. Also check victims of `block_explosion` and `entity_explosion` damage. Secondary effects/knockback are not guaranteed. |
| `fluid-flow`, `water-flow`, `lava-flow` | `bedrock_hooks/liquid_block.cpp`, `_trySpreadTo` | Classify the source block's exact namespace/type, then check both from/to blocks. Vanilla still/flowing water and lava select their granular flag; unknown/custom types select `fluid-flow`. Instant-ticking paths bypass this hook. |
| `block-form` | `bedrock_hooks/liquid_block.cpp`, `_solidify` | Lava solidification into cobblestone, obsidian and basalt. Header examples for snow/ice are not evidence of additional firing paths. |
| `leaf-decay` | `bedrock_hooks/leaves_block.cpp`, `_die` | Cancel the leaf-decay path before drops/removal. |
| `actor-griefing` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, `ActorGriefingBlockEvent` | Check exposed target block. Falling blocks and sheep grass consumption are excluded; resulting block state is absent. |
| `mob-spawning` | `bedrock_hooks/script_level_gameplay_handler.cpp`, actor-added event | Restrict non-player Mob instances only. Cancellation despawns an actor already added; no spawn reason is exposed. |
| `mob-damage` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, before-hurt | Attributed non-player Mob attacking Player; decide at victim. Missing attribution cannot be reconstructed. |
| `entry`, `exit` | `bedrock_hooks/packet.cpp`, move threshold; `bedrock_hooks/player.cpp`, same-dimension teleport | Resolve changed region sets at reported from/to locations, preserving cancellations. Small movements may not fire. Dimension-change and respawn are after-events, and cross-dimension teleport bypasses the before-event. No complete exclusion or rollback is claimed. |

The shared environmental resolver accepts only environmental flags and has no player identity
or permission bypass. `item-pickup` also has a player adapter, which supplies that player's
explicit bypass; autonomous pickup has no such identity. The move adapter skips unchanged
block coordinates/dimension, but does not infer that all movement was reported. Actual-position
greetings and advanced session rules belong to the next planned expansion.

## Missing contracts

- Piston events expose base and direction, not every moved/broken block. Base activation
  control could be added with that narrower name; it is not full cross-boundary protection.
- `BlockGrowEvent` is declared but no concrete firing construction was found.
- Inventory open lacks holder/location, and no concrete inventory click/drag transaction event
  was found. Opening guards do not protect ongoing or automated transfers.
- Bed/door multi-block placement exposes one target, not every affected block.
- Health-regeneration causes, current-hunger writes, precise potion sources and complete portal
  transitions are unavailable through the required public contracts in this checkout.

## Live command suggestions

The version-gated adapter rewrites only the plugin-owned `dg` command in packet 76 through
`PacketSendEvent`. It preserves other commands, aliases, metadata, enum indices and constraints.
It uses the [protocol-2169 schema](https://github.com/EndstoneMC/protocol-docs/tree/r26_u4):
fixed uint32 enum-value indices and string command permissions. Malformed, oversized,
unsupported or conflicting packets pass through without cancellation. Unsupported protocols
retain native command parsing with less detailed hints. Offline codec tests do not prove client
autocomplete display; that remains part of final acceptance.

Live region names are supplied only to administrators and only for their current level/dimension.
The public `/dg flags [page]` path has its own distinct root and optional native integer; discovery
shows six descriptions per page, with 26 flags across five pages. Non-administrators do not receive
mutation instructions in the help/catalog panels. Endstone's native fallback may still display
administrative subcommand syntax under `dg`; the server-side permission checks remain authoritative.

## Event contracts

| Public event | Hook evidence | Protection decision and limits |
| --- | --- | --- |
| `BlockBreakEvent` | `src/endstone/runtime/bedrock_hooks/script_block_gameplay_handler.cpp`, `handleEvent(BlockTryDestroyByPlayerEvent&)` and `ScriptBlockGameplayHandler::handleEvent4` | Query `block-break`, falling back to `build`, at `getBlock()` using `getPlayer()`. Cancellation returns `CoordinatorResult::Cancel`. This covers the player destruction path; explosions, commands, pistons, fluids and plugin block edits are different paths. |
| `BlockPlaceEvent` | Same file, `handleEvent(const BlockTryPlaceByPlayerEvent&)` and `handleEvent2` | Query `block-place`, falling back to `build`, at `getBlockPlaced()`, not only `getBlockAgainst()` or the player's location. The target is a pre-mutation snapshot with the permutation about to be placed. Secondary blocks of doors, beds and similar placements require boundary tests; the event exposes one target, not an affected-block list. Conservative neighboring-chest checks still apply. |
| `PlayerBucketFillEvent` | `bedrock_hooks/bucket.cpp`, `BucketItem::_useOn`; `bedrock_hooks/cauldron.cpp`, `CauldronBlock::use`; `bedrock_hooks/script_player_gameplay_handler.cpp`, entity interaction before-event | Query `build` at the affected `getBlock()` and handle a null result explicitly. `getBlockClicked()` is available as a conservative fallback. The current paths cover recognized source liquids, powder snow, supported cauldron operations and milking supported animals. Later fluid spread and automation are separate operations. |
| `PlayerBucketEmptyEvent` | `bedrock_hooks/bucket.cpp`, `BucketItem::_useOn`; `bedrock_hooks/cauldron.cpp`, `CauldronBlock::use` | Use `getBlock()` for the affected position. The hook resolves waterlogging versus adjacent placement; recomputing the destination from the clicked face would discard that work. Test water, lava, powder snow and creature buckets at region edges. |
| `PlayerInteractEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, block before-event; `bedrock_hooks/script_item_gameplay_handler.cpp`, item use; `bedrock_hooks/packet.cpp`, destroy-start and missed swing input | The adapter handles only `RightClickBlock` with a target. Reviewed vanilla door/trapdoor/gate/button/lever identifiers select `use`; reviewed anvils select `use-anvil`; other targets select `interact`. A captured `Container` additionally requires `container-access` and conservative neighboring-chest checks. Air and left-click events are ignored here; the separate break listener does not establish protection of every left-click mutation. |
| `PlayerInteractActorEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, `handleEvent(const PlayerInteractWithEntityBeforeEvent&)` | Query the relevant action at `getActor()->getLocation()` using `getPlayer()`. Cancellation returns `CoordinatorResult::Cancel`. Actor inventories do not have an exposed inventory-holder mapping; a generic actor interaction guard is not complete inventory transaction protection. |
| `PlayerArmorStandManipulateEvent` | `bedrock_hooks/armor_stand.cpp` | Registered separately despite inheriting from `PlayerInteractActorEvent`; delegates to the shared actor interaction decision. A parent-class registration alone would not receive it. |
| `PlayerBedEnterEvent` | `bedrock_hooks/player.cpp`, `Player::startSleepInBed` | Query `sleep` at `getBed()`, falling back to `interact`. Cancellation returns before sleeping. The hook fires only for a valid sleep attempt; the earlier bed right-click must also pass `interact`. It is not the route for exploding beds. |
| `PlayerDropItemEvent` | `bedrock_hooks/player.cpp`, `Player::drop` | Query `item-drop` at the player's location. Cancellation returns false before the native drop, for alive initialized players. Death drops, XP drops and other actors' drops are not covered. |
| `PlayerPickupItemEvent`, `PlayerPickupArrowEvent` | `bedrock_hooks/player.cpp`, `Player::take` | Query `item-pickup` at both the player and the actual item/arrow locations, using the player's identity/bypass. The arrow event also describes thrown-trident pickup. Cancellation prevents the reported take; it is not a hopper/inventory-transfer event. |
| `ActorPickupItemEvent` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, `ActorBeforeAcquireItemEvent` | The native filter selects non-player actors acquiring an item through `PickedUp`. Query `item-pickup` at both actor and item positions without a player identity or bypass. Other acquisition methods are separate. |
| `PlayerChatEvent` | `bedrock_hooks/script_server_network_event_handler.cpp`, `ChatEvent` | Query `send-chat` at the sender's location before native chat dispatch. Does not implement receive filtering, proxy chat, direct messages or arbitrary plugin broadcasts. |
| `ActorDamageEvent` | `bedrock_hooks/script_actor_gameplay_handler.cpp`, `handleEvent(ActorBeforeHurtEvent&)` and `handleEvent4` | Victim is `NotNull<Mob>`. Apply the player-only `invincible` feature and recognized damage-cause flag at the victim; separate listeners retain explosion and mob-damage rules. For PvP, `getDamageSource()->getActor()` identifies the responsible player, including a resolved projectile shooter; `getDamagingActor()` is the direct actor/projectile instead. Check attacker and victim positions using the attacker's identity. Missing/unloaded shooters and historical shooter positions cannot be reconstructed. |
| `InventoryOpenEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, `handleEvent(const PlayerOpenContainerEvent&)` | The hook knows `block_pos` but exposes only `Inventory&` and player. The public API has no holder or location accessor. A precise region decision cannot be made from this event alone. The event is dispatched through a void gameplay event, so screen-opening cancellation timing also needs runtime verification. |
| `InventoryInteractEvent` | Header `include/endstone/event/inventory/inventory_interact_event.h`; no construction or concrete firing path found | This is an abstract base without `ENDSTONE_EVENT` or a `NAME`; it is not a usable concrete listener in this checkout. No public inventory click/drag subclass was found. Do not claim ongoing transaction protection by registering the base. |
| `PlayerQuitEvent` | `bedrock_hooks/script_player_gameplay_handler.cpp`, disconnect-event handler | Remove transient selection and notification state keyed by `getPlayer()->getUniqueId()`. This is cleanup, not a cancellable protection action. The hook calls the event before disconnecting the Endstone player wrapper. |

## Granular action and damage boundaries

The exact vanilla block and liquid classifiers live in [block_rules.cpp](../src/rules/block_rules.cpp).
A custom namespace or unknown suffix does not acquire vanilla behavior automatically. `use` is
not a catch-all item-use flag, nor a physical pressure-plate/trampling event.

`sleep allow` does **not** bypass an earlier `interact deny` on the bed. The plugin deliberately
retains that separate interaction decision: treating every bed click as a sleep attempt could
permit a denied bed interaction in Nether/End, where it explodes instead of emitting a valid
bed-enter event. Public dimension identity alone cannot classify every custom dimension's bed
behavior. To permit ordinary sleeping, permit the relevant interaction as well as `sleep`.

Likewise, `use-anvil allow` never overrides `container-access` when the clicked block is actually
exposed as a `Container`. A vanilla anvil's temporary UI must not be assumed to be a persistent
block container; its supported opening route is the reviewed right-click classifier. Neither
rule adds ongoing inventory transaction coverage.

[damage_rules.cpp](../src/rules/damage_rules.cpp) selects `fall-damage` only for `fall`, and
`firework-damage` only for `fireworks`. Similar names such as `falling_block`, `fly_into_wall`,
`projectile` or generic explosion are not silently included. These cause rules apply to the
reported `Mob` victim, including players, and do not infer a responsible player's bypass.

`invincible allow` cancels intercepted damage only when the victim is a `Player`. `deny` or an
unconfigured value adds no immunity; it does not force damage through `pvp`, `mob-damage`,
`explosions`, `fall-damage` or another plugin's cancellation. Equal-priority deny still wins the
feature-value tie, disabling only this immunity. No player bypass is used to turn immunity on
or pierce it. Direct health writes, actor removal, secondary knockback and all potion effects
are not guaranteed to be intercepted. Storage unavailable at startup still locks intercepted
damage through the shared failure guard, independently of configured immunity.

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
4. Interact with doors, buttons, levers, anvils, signs, item frames, lecterns, furnaces, shulker boxes and other containers. Confirm which actions select `use`, `use-anvil`, `interact` or `container-access`, and whether denial blocks both the UI and server-side mutation.
5. Open each half of a normal and trapped double chest straddling a region edge. Check all orientations and chunk edges. Place same-type unpaired chests next to a protected chest and record the conservative over-denial behavior.
6. Interact with mobs, storage actors and armor stands. Verify armor-stand equipment manipulation separately because its concrete event has a different name. Inventory access through an already open, remote or plugin-created UI remains unsupported until a suitable event path is established.
7. Test melee and projectile PvP with the attacker inside, victim inside and both outside. Verify damage, shooter attribution and knockback separately. Repeat with the shooter disconnected before impact; record the unresolved-attribution limitation. Potion effects and environmental damage are separate checks, not implied by melee success.
8. Change trust/flags while a container screen is open, and attempt shift transfers, drag actions and automated extraction. Record the current transaction coverage gap; do not label these paths protected by the opening listener.
9. Reconnect, change locale between English and Spanish, and repeat rapid denied actions. Verify selection cleanup, localization fallback and notification throttling without masking protection decisions.
10. Execute the granular, sleep, item/chat, immunity and paged-discovery checks in [testing.md](testing.md). Every new action needs its actual event and server outcome recorded; a successful policy unit test does not establish an earlier event stage was permitted.

## Policy risk review and test strategy

The engine's chosen precedence is deliberate: search priority tiers for explicit decisions first; `deny` wins within a tier, and any explicit decision at a higher tier takes precedence over lower tiers. Membership defaults apply only when every matching tier inherits. Consequently, a lower-priority explicit `allow` opens access through a higher-priority region that inherits, and explicit `deny` also denies owners. Document and test those examples so administrators do not interpret ownership or priority as unconditional access.

The independent flags require adapter tests in addition to engine tests. A container click must not accidentally use a generic `interact allow` to skip a `container-access deny`. Conversely, a build permission should not by itself grant container access. Test an interact-allowed/build-denied tool action, a bucket targeting the other side of a boundary and left-clicking a protected item frame. Their correct target and concrete event path cannot be established solely through `RegionManager::isAllowed()` tests.

Use two online identities for cross-boundary PvP checks. A bypass permission on the victim must not grant bypass to an attacker, and the projectile's runtime identity must not be mistaken for its owner. Check both attacker and victim locations with the same responsible-player policy, and record unresolved attribution separately. Reusing one position for both sides creates a sanctuary-boundary escape even when the region engine works correctly.

Keep authority tests separate from membership tests. Administrative permission, region ownership, trusted membership and protection bypass have different purposes. Test an operator without the explicit bypass permission, a trusted player without administration permission and unauthorized attempts to mutate a region. Confirm every administrative mutation goes through the service, so a failed save cannot produce new in-memory permissions that disappear on restart.

Offline verification should cover boundaries and negative coordinates, deterministic overlaps, independent dimensions, invalid enum values, full-width priorities, index correctness against a brute-force oracle and snapshots that outlive their originals. Service tests should inject a database error after a replacement has begun, then verify both the persisted regions and live protection index still contain the previous snapshot. A rejected reload must retain the last known valid protection state. Reopen the database after successful mutations to verify durability; do not infer persistence from the in-memory result.

The database is a single-writer application store. External changes should be read through an explicit successful reload before subsequent administrative writes, because saves replace a complete snapshot. Two live plugin instances or manual database edits during operation are not a coordinated editing interface. Tests for persistence failures should therefore use controlled failure injection, not imply multi-writer conflict resolution.
