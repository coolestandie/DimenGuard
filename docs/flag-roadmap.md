# WorldGuard-inspired flag roadmap

For the current unreleased branch, [WG-2](flag-semantics.md) supersedes the typed-value,
source-aware explosion and supported nonplayer-domain backlog below. It registers 33
functional flags with schema 3; the following 26-flag matrix remains the published preview
baseline. Unsupported piston and session-effect paths are still not claimed as implemented.

Static audit dated 2026-09-08. The reference is the [WorldGuard flag catalog](https://worldguard.enginehub.org/en/latest/regions/flags/), not a compatibility specification for DimenGuard. The evidence below is the local Endstone API 0.12 checkout `2572cd304b5ca2d094e02a1b2f969f7632ea44f6` and the current DimenGuard source. It does not establish runtime acceptance or support on a different Endstone build.

**The 0.2.0 preview candidate implements the 26 state flags in the first table. Runtime acceptance is still pending.** Later rows marked **Implemented** describe that same bounded scope, not additional features; **Next**, **Partial**, **Typed** and **Blocked** rows remain future work. A header declaration is not a firing hook, and a cancellable after-event is not pre-mutation prevention. See [event coverage](event-coverage.md) and [runtime tests](testing.md). This is a coherent public preview, not a complete WorldGuard replacement.

## Preserve existing policy

The unreleased [WG-1 model](region-model.md) now supplies parent/global/template regions,
state-flag groups and schema 2 internally. Its migration preserves existing region rules;
new command controls, broader WorldGuard build semantics, typed flags and non-player association
remain later phases. The release-candidate table below still describes the published preview.

- Keep the current level/dimension identity, region names, ownership, members and saved flag values.
- `build`, `interact` and `container-access` default to membership. Granular flags use their documented aggregates when unset; independent flags default to allow except `invincible`, whose default deny adds no immunity. Explicit values resolve by descending priority, with deny winning ties. Environmental actions do not invent a responsible player or inherit a player's bypass.
- Keep action checks independent: allowing one flag does not erase another intercepted stage's denial. Granular flags first resolve their own explicit decisions across all priority tiers, then consult their aggregate only if all inherit. Thus a configured granular value may override a higher-priority aggregate value; leaving the granular flag unset preserves existing policy.
- WorldGuard's broad `build`, region groups, parent/global regions and non-player association model are not current DimenGuard behavior. Introducing equivalents requires an explicit policy design and migration, not aliases that silently change existing meaning.

## Implemented release candidate: 26 state flags

| DimenGuard flag | Current source behavior | Important boundary |
| --- | --- | --- |
| `build` | Aggregate fallback for player break/place; direct rule for supported bucket operations. | Not every indirect mutation or the secondary blocks of every placement. |
| `interact` | Reported right-click block/actor actions, separate armor-stand manipulation and granular-use fallback. | Not physical trampling, every left-click mutation or every item-use path. |
| `container-access` | Recognized block-container opening interactions; conservative adjacent chests. | No remote/ongoing transactions, exact chest pairing or hopper transfer protection. |
| `pvp` | Attributed player damage; attacker and victim locations. | Missing projectile shooter attribution remains a gap. |
| `explosions` | Origin and all reported blocks must allow; separate explosion-caused mob/player damage check. | Cancels the whole reported explosion if any checked position denies. Does not classify its source. |
| `fluid-flow` | Aggregate fallback for reported liquid spread; direct rule for unknown/custom source types. | Both endpoints are checked; the hook excludes instant-ticking paths. |
| `block-form` | Lava solidification reported by the liquid hook. | Not snow, ice, frost-walker or arbitrary formation. |
| `leaf-decay` | Reported leaf removal through the decay hook. | Not an umbrella growth flag. |
| `actor-griefing` | Reported actor block-griefing before-event. | Not every block mutation caused by an actor. |
| `mob-spawning` | Cancellation of added non-player mobs requests their removal. | The native notification is after addition, without a spawn reason; existing-actor re-addition needs testing. |
| `mob-damage` | Attributed non-player mob damage to a player at the victim. | Not unidentified shooters, every effect or damage to all actors. |
| `entry`, `exit` | Regions actually crossed by reported movement and same-dimension teleports. | Portal event is a separate concrete event and is not registered here; no complete movement/portal guarantee. |
| `block-break`, `block-place` | Reported player break/place targets; fallback to `build`. | A granular allow does not bypass separate interaction or container checks; buckets still use `build`. |
| `use` | Reviewed vanilla doors, trapdoors, gates, buttons and levers; fallback to `interact`. | Exact namespaced classifier, not all block/item use or physical activation. |
| `use-anvil` | Reviewed anvil right-click opening; fallback through `use` to `interact`. | Does not override `container-access` for a captured container or protect ongoing inventory transactions. |
| `sleep` | Valid reported bed-enter attempts; fallback to `interact`. | `sleep allow` still needs the prior bed click to pass `interact`. Exploding beds do not use this sleep event. |
| `item-drop` | Active drops by alive initialized players, at the player's location. | Not death drops, XP drops or dispenser output. |
| `item-pickup` | Reported player/non-player actor pickup; actor and item endpoints. | Player events include items and arrows/tridents; autonomous pickup has no player bypass. Not inventory transfer protection. |
| `send-chat` | Reported chat sent by a player at the sender's position. | Not receive filtering, proxy handling or arbitrary plugin broadcasts. |
| `water-flow`, `lava-flow` | Exact vanilla still/flowing source-type classification, both endpoints; fallback to `fluid-flow`. | Other types retain the aggregate; instant-ticking exclusion remains. |
| `fall-damage`, `firework-damage` | Reported `fall` / `fireworks` causes at a mob/player victim. | Not similarly named causes, all actors or every secondary effect. |
| `invincible` | `allow` cancels reported damage only to player victims; default `deny` adds no immunity. | `deny` does not force damage through other rules. No bypass-based immunity, direct-health/removal guarantee or immunity for non-player mobs. |

The executable command catalog exposes these state values through `/dg flags [page]`: six
descriptions per page, five pages, with administrative instructions omitted for non-managers.
`/dg flag` remains the administrative first-page shortcut. Typed values, session effects and
region groups are not implemented by the current state registry or SQLite schema.

## Feasibility key

- **Implemented:** present in this release candidate, within the stated event boundaries; gameplay acceptance remains pending.
- **Next:** public event and concrete firing/cancellation path support the stated adapted scope. Implementation and gameplay tests are still required.
- **Partial:** a useful narrower behavior is possible, but the familiar flag name would overstate coverage without qualification.
- **Typed:** public behavior is feasible after the common typed-value registry, persistence and session services exist.
- **Blocked:** required event data, cancellation timing or setter is unavailable in this checkout. Do not register a decorative flag that cannot enforce its contract.

### Player actions and damage

| Candidate family | Assessment | Concrete route and required adaptation |
| --- | --- | --- |
| `block-break`, `block-place` | Implemented | `BlockBreakEvent` / `BlockPlaceEvent` in **E1**. Granular target rules retain `build` fallback and the existing bucket policy. They do not cover piston/explosion changes under these names. |
| `use`, `use-anvil` | Implemented | `PlayerInteractEvent::RightClickBlock` in **E2**, with a central exact vanilla-type classifier. Captured containers still require `container-access`; a granular allow does not erase that independent check. Anvil opening control is not ongoing inventory enforcement. |
| `chest-access` | Existing adaptation | Keep `container-access` as the canonical name; a compatibility alias is optional, not a second independent policy. Holder/transaction gaps in **E3** remain. |
| `sleep` | Implemented | `PlayerBedEnterEvent` from `Player::startSleepInBed` in **E4** exposes the bed and returns before sleeping on cancellation. The prior bed click must also pass `interact`, even with `sleep allow`. Retaining that stage prevents a sleep exception from permitting denied explosive bed interactions in Nether/End. |
| `respawn-anchors` | Partial | Block right-click filtering plus `PlayerSetSpawnEvent::Cause::RespawnAnchor` in **E4** can restrict use and spawn assignment. Charge consumption, explosion and wrong-dimension behavior need distinct tests; spawn assignment alone is insufficient. |
| `lighter` | Next, player use only | Inspect the event item and clicked block/face in **E2** for flint-and-steel/fire-charge use. Check the destination when identifiable. This does not prevent dispenser fire or later natural ignition. |
| `damage-animals` | Next, explicit type set | `ActorDamageEvent` in **E5**, attributed player plus victim `ActorTypeId`. The public API has no friendly-animal category query; define a reviewed namespaced type set and custom-actor fallback rather than assuming every non-player mob is an animal. |
| `invincible`, `fall-damage`, `firework-damage` | Implemented, reported damage | **E5/E6** expose `Mob` victims and exact `fall` / `fireworks` causes. Cause flags default allow; the inverse `invincible` feature defaults deny and activates only with resolved allow at a player victim. It does not bypass other denials or promise immunity to direct `setHealth`, removal or all server/plugin actions. |
| `ride` | Partial | `PlayerInteractActorEvent` in **E2** can block targeted interaction with a configured mount set. There is no public before-mount event; `ActorDismountEvent` in **E5** handles the opposite operation. Commands, forced mounting and all automatic mounting are not covered. |
| `vehicle-place` | Partial | Item-use filtering and actor-add cancellation in **E2/E7** can restrict known boat/minecart cases. The add event lacks placer, reason and pre-add timing. Never attribute a spawned vehicle to the nearest player. |
| `vehicle-destroy`, `entity-painting-destroy` | Blocked for complete damage protection | The damage adapter exposes `Mob`, not general vehicle/painting actors. `ActorRemoveEvent` is observational after removal and cannot undo destruction or drops. A cancellable hanging/vehicle damage/removal path is needed. |
| `item-frame-rotation`, `entity-item-frame-destroy` | Partial / model mismatch | Bedrock item frames use block data rather than Java's item-frame entity model. Reported block right-clicks can restrict rotation; break and explosion checks cover their own paths. Dedicated left-click, support-loss and non-player destruction coverage is unproven. |
| `block-trampling`, `use-dripleaf` | Blocked for physical activation | **E2** offers four click actions and no physical/pressure/trample action. General movement cancellation is not a reliable target-aware substitute for farmland, egg or dripleaf mutation. |

### Mobs, explosions and natural changes

| Candidate family | Assessment | Concrete route and required adaptation |
| --- | --- | --- |
| `tnt`, `creeper-explosion`, `ghast-fireball`, `wither-damage`, `other-explosion` | Next for identified source; partial overall | Classify `ActorExplodeEvent::getActor()->getType()` in **E1**, then inspect both responsible/direct damage actors in **E5/E6**. Preserve the aggregate `explosions` rule and all affected positions. Unknown or removed source identities must have an explicit fallback. TNT detonation/priming and fireball ignition are not fully represented by the explosion's block list. |
| `enderdragon-block-damage`, `enderman-grief`, `snowman-trails`, `ravager-grief` | Partial | Specialize the identified actor in `ActorChangeBlockEvent` (**E5**). The native source is `ActorGriefingBlockEvent`, not a universal actor mutation stream. Verify each actor behavior independently before advertising that subtype. |
| `deny-spawn` | Typed, with existing spawn limitation | A bounded set of registry-validated `ActorTypeId` values can reuse **E7**. It would remove matching actors on addition, not prevent all spawn work or distinguish natural/egg/command/load reasons. Keep `mob-spawning` aggregation explicit. |
| `water-flow`, `lava-flow` | Implemented, reported liquid spread | **E8** is classified by the source block's exact namespaced type, retaining source/destination checks and `fluid-flow` fallback. Still/flowing variants are included; the instant-tick exclusion remains. |
| `pistons` | Partial | **E9** has concrete cancellable extend/retract events with piston block and direction. A flag at the piston base is feasible. Exact protection of every moved/destroyed block, slime/honey branches and destination region is blocked by the missing affected-block list. Do not present base-only denial as boundary protection. |
| `fire-spread`, `lava-fire` | Blocked | No public ignition/spread event or concrete firing hook was found. Blocking a player's lighter does not intercept later fire ticks, lava ignition or dispenser ignition. |
| `lightning` | Partial / blocked for strike prevention | **E7** can observe/remove an added lightning actor, and **E6** identifies resulting mob damage. Addition is too late to establish prevention of every strike effect, fire or transformation. Level-wide thunder cancellation cannot implement a region flag. |
| `snow-fall`, `snow-melt`, `ice-form`, `ice-melt`, `frosted-ice-form`, `frosted-ice-melt` | Blocked | `BlockGrowEvent` is declared but no direct firing construction was found. `BlockFormEvent` in **E8** fires only during lava solidification; it does not cover these mechanics. |
| `mushroom-growth`, `grass-growth`, `mycelium-spread`, `vine-growth`, `rock-growth`, `sculk-growth`, `crop-growth` | Blocked | No concrete public growth/spread hooks were found for these families. Listening to a shared base class also does not subscribe to separately named event types. |
| `soil-dry`, `coral-fade`, `copper-fade` | Blocked | No public fade/oxidation/hydration mutation event with cancellable before-state was found. Polling and replacing blocks after mutation would introduce different behavior and overhead. |

### Items, chat, commands and effects

| Candidate family | Assessment | Concrete route and required adaptation |
| --- | --- | --- |
| `item-drop` | Implemented, active player drops | `PlayerDropItemEvent` in **E4** checks the player's location and cancels `Player::drop` for alive initialized players. Death drops, dispenser drops and other actors are separate paths. |
| `item-pickup` | Implemented | `PlayerPickupItemEvent` and `PlayerPickupArrowEvent` in **E4**, plus `ActorPickupItemEvent` in **E5**, check actor and object positions. Only the player path has a player identity/bypass; this is not automated inventory transfer protection. |
| `exp-drops` | Blocked for exact drop prevention | Death events expose no drop list/XP amount. `PlayerExpChangeEvent` and `PlayerPickupExperienceEvent` control acquisition, not generation. Removing an XP orb through **E7** is an after-add, all-source approximation, not equivalent death-drop behavior. |
| `send-chat` | Implemented | `PlayerChatEvent` in **E10** checks the sender's position and returns cancellation before the original native chat route. This applies to that server's reported chat, not proxy/plugin broadcasts. |
| `receive-chat` | Partial, separate delivery design | **E10** provides `getRecipients()` by value and no setter. The native hook does not apply recipient edits to `event.targets`. Safe filtering requires owning cancellation and redistributing the original permitted audience with defined formatting/inter-plugin behavior, or a separately audited packet adapter. Merely erasing from the returned vector does nothing. |
| `blocked-cmds`, `allowed-cmds` | Typed | A bounded normalized command set can cancel `PlayerCommandEvent` in **E11**. Resolve aliases/namespaced roots consistently; specify root versus full-command matching, allow-list conflicts and an explicit administrative recovery path. Console, command blocks, proxy handling and direct plugin dispatch are not necessarily player-origin events. |
| `potion-splash` | Blocked for exact splash-only semantics | `ActorEffectEvent` in **E5** identifies an added effect but exposes no source or splash/drink/beacon cause, impacted-actor list or instant-effect contract. A differently named typed effect-denial set could control reported additions; it would affect more than splashes. |
| `natural-health-regen`, `natural-hunger-drain` | Blocked for source-specific prevention | No health-regain cause event or `FoodLevelChangeEvent` exists in this audited checkout. A similarly named event on another fork branch is not a dependency available here. Polling cannot distinguish natural regeneration/drain from food, effects and plugin changes. |

### Movement and typed/session values

| Candidate family and value type | Assessment | Concrete route and required adaptation |
| --- | --- | --- |
| `exit-via-teleport` (state), `exit-override` (boolean) | Typed policy extension | **E12** distinguishes concrete movement/teleport events, allowing separate policy for reported teleports. Define precedence with existing entry/exit before implementing. Neither value repairs unreported crossings. |
| `enderpearl`, `chorus-fruit-teleport` (state) | Partial | **E2** can restrict ender-pearl use; `PlayerItemConsumeEvent` in **E4** can deny chorus consumption, including its food benefit. `PlayerTeleportEvent` has no cause field, so it cannot selectively identify their later teleport. Already-thrown pearls crossing into a region need separate evidence. |
| `greeting`, `farewell`, `entry-deny-message`, `exit-deny-message`, `deny-message`, `teleport-message` (string) | Typed | Shared localized rendering and **E12/E13** can send bounded messages on decisions or observed state transitions. Use an allow-listed placeholder grammar, not arbitrary format strings or commands. Deduplicate equal effective values across overlaps. |
| `greeting-title`, `farewell-title` (string/title pair) | Typed | Public `Player::sendTitle` in **E13**. Define title/subtitle splitting, size/color limits and cooldowns; do not copy Java-only text-component assumptions. |
| `notify-enter`, `notify-leave` (boolean) | Typed | Observe transitions, then notify only recipients with a dedicated permission via **E13**. Reconcile join/quit, dimension changes, reload and deleted regions. Notifications are observations, not proof entry was prevented. |
| `teleport` (location) | Typed | Add an explicitly authorized plugin command using public `Actor::teleport(Location)`. Persist level/dimension identity, finite coordinates and orientation; validate destination availability and the shared transition policy. The flag alone does not create a command. |
| `spawn` (location) | Partial typed adaptation | **E12** exposes player death/respawn notifications, but `PlayerRespawnEvent` has no destination setter. Saving the death-region choice then teleporting after respawn is possible; it is a post-respawn relocation, not native spawn selection. Distinguish death from End-portal respawn and avoid silently overwriting the player's permanent bed/anchor location. |
| `game-mode` (enum) | Typed | Public `getGameMode` / `setGameMode` and concrete `PlayerGameModeChangeEvent` (**E2/E13**). Maintain session ownership and restore only values still owned by DimenGuard; handle recursive events, death, quit, reload, region deletion and plugin disable. |
| `heal-delay`, `heal-amount`, `heal-min-health`, `heal-max-health` (bounded numbers) | Typed, integer-health adaptation | Use one synchronous scheduler and `Mob::getHealth/setHealth` (**E13/E14**). Clamp against live maximum health, skip dead players, define interval units and rounding: the public convenience setter accepts integer health. Negative healing is a direct health write, not an attributed damage event; shared damage/invincibility policy must be explicit. |
| `feed-delay`, `feed-amount`, `feed-min-hunger`, `feed-max-hunger` (bounded numbers) | Partial typed candidate | Hunger/saturation/exhaustion attributes exist in **E14**, but there is no public direct current-hunger setter. `setBaseValue` changes the default/base and may do nothing when the same base is requested again. Do not implement repeated exact feeding by pretending it is `setFoodLevel`. A tested attribute adaptation or upstream setter is required. |
| `time-lock` (validated time/offset), `weather-lock` (enum) | Partial, protocol extension | No per-player time/weather public convenience API exists. Level time/weather changes affect other regions. Public `Player::sendPacket` / `PacketSendEvent` permit a separately bounded, version-gated client-visual adapter; it must handle native packet updates, dimension changes and restoration. This is not region-local world simulation. |

### Policy and platform differences

| Candidate | Assessment | Required design |
| --- | --- | --- |
| `passthrough` | Planned policy feature | Excluding a region from implicit membership protection is implementable in the engine-independent resolver. It is not equivalent to setting `build allow`. Preserve explicit rules and define overlapping/default behavior with migration tests. |
| `nonplayer-protection-domains` (string set) | Partial / dependent | Domain intersection is possible in the resolver, but requires a documented non-player association model plus reliable source/destination data for every participating mechanism. Missing piston affected positions remain a blocker to the broad contract. |
| Region groups | Planned policy feature | Public UUID/ownership data can support owners/members/non-members groups. Define which subject applies to damage, environment and transitions. Existing entry/exit denials currently affect owners too; changing this silently would be a security regression. |
| Java entity/text/event compatibility | Not directly portable | Bukkit vehicle/hanging events, Java item-frame entities, Adventure text and WorldGuard's platform/session internals cannot be imported as Endstone capabilities. Most listed gameplay mechanics also exist in Bedrock; do not incorrectly label a missing Endstone hook as a Java-only mechanic. Adapt the contract or mark it unavailable. |

## Implementation order and acceptance gates

1. Accept the implemented 26-state-flag candidate in a controlled server session: granular player actions, damage causes/immunity, reported liquid subtypes, drops/pickups and sending chat alongside the earlier rules. Check the independent bed-click/container stages and unchanged legacy aggregate behavior. Do not convert offline success into a gameplay pass.
2. Add identified-source explosion/griefing subtypes only with captured runtime cases for each subtype and unknown-source behavior. Keep partial capabilities clearly labeled; do not broaden `build` retroactively.
3. Introduce a single typed registry shared by parsing, bounds, defaults, resolution, storage, suggestions and bilingual presentation. Define deterministic equal-priority resolution for strings, sets, numbers and locations rather than relying on unordered iteration. Use an atomic schema migration; old snapshots must retain their decisions and roll back unchanged on failure.
4. Add command/type sets and bounded message/title values, then session-owned mode, teleport and health features. Keep no disk I/O in event handlers, one bounded scheduler, player-state cleanup and transition deduplication. Add feeding or visual time/weather only after their separate adapter contracts pass tests.
5. Keep blocked capabilities discoverable in documentation, not in the executable supported-flag list. Re-audit after an Endstone upgrade; do not add private memory hooks or change Endstone as an incidental plugin implementation step.

For each delivered family, test allow/deny/inherit, legacy aggregate fallback, owner/member/outsider, explicit bypass, equal/high priorities, source and destination on opposite sides, negative coordinates and independent dimensions. Record the actual event, affected positions, server mutation and item/XP counts. Then test joins/reloads/deaths/teleports and restoration where applicable. Offline policy tests alone cannot establish the event route or client result.

## Primary evidence index

Paths below link to the pinned upstream revision, not a moving branch. The cited event, hook, player and attribute paths are unchanged in the reviewed local fork; only the Chunk and Dimension headers differ within the compared public API/hooks. The WorldGuard page supplies the candidate taxonomy; feasibility and limitations are derived independently from these Endstone implementations.

- **E1:** [block break/place and explosion dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_block_gameplay_handler.cpp).
- **E2:** [block/actor interaction and game-mode dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_player_gameplay_handler.cpp), [item-use dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_item_gameplay_handler.cpp), [public interaction actions](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/player/player_interact_event.h).
- **E3:** [inventory-open dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_player_gameplay_handler.cpp) and the detailed [inventory/holder audit](event-coverage.md#inventory-and-double-chest-boundaries).
- **E4:** [sleep, consume, drop, pickup, spawn assignment and teleport hooks](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/player.cpp).
- **E5:** [damage, effects, actor pickup, dismount and griefing dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_actor_gameplay_handler.cpp), [public damage victim contract](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/actor/actor_damage_event.h).
- **E6:** [damage causes and responsible/direct actor lookup](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/core/damage/damage_source.cpp).
- **E7:** [actor-added dispatch and cancellation removal](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_level_gameplay_handler.cpp).
- **E8:** [liquid spread and lava solidification hooks](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/liquid_block.cpp), [leaf decay hook](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/leaves_block.cpp).
- **E9:** [piston tick hooks](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/piston_block_actor.cpp).
- **E10:** [chat dispatch and delivery branches](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_server_network_event_handler.cpp), [public chat recipient contract](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/player/player_chat_event.h).
- **E11:** [player/console command dispatch](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/minecraft_commands.cpp).
- **E12:** [movement packet hook](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/packet.cpp), [portal hook](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/server_player.cpp), [public respawn event](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/player/player_respawn_event.h).
- **E13:** [public player setters, messages, titles and packets](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/player.h), [synchronous scheduler](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/scheduler/scheduler.h).
- **E14:** [mob health and attributes implementation](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/core/actor/mob.h), [public attribute adapter](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/core/attribute/attribute_instance.h), [base-versus-current attribute writes](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/bedrock/world/attribute/attribute_instance.cpp).
