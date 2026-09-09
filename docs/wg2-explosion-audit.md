# WG-2 explosion protection audit

This source audit targets Endstone API 0.12.0 at
`46eff9f125f52eac76472d84339ead8fbf51fcd2`. It establishes the adapter contract,
not successful gameplay testing. No native hooks are added by DimenGuard.

## Verified public event paths

| Evidence | Consequence for DimenGuard |
| --- | --- |
| [`ExplosionStartedEvent` adapter](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_block_gameplay_handler.cpp) resolves the event source actor and constructs `ActorExplodeEvent` when it exists. | The actor's exact public type identifies vanilla TNT, TNT minecarts and creepers; another identified actor is `other-explosion`. No nearby player, owner UUID or inferred shooter is used. |
| [`ActorExplodeEvent`](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/actor/actor_explode_event.h) and [`BlockExplodeEvent`](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/block/block_explode_event.h) expose mutable affected-block vectors. The adapter clears native `event.blocks` and repopulates it from those vectors after listener dispatch. | Removing denied blocks is effective on this runtime route. Allowed terrain remains in the event; a single denied target no longer cancels all allowed terrain. |
| The same adapter constructs `BlockExplodeEvent` whenever its actor lookup fails, using the position recorded by [`Explosion::explode`](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/explosion.cpp). | A block event does not prove that a bed or respawn anchor caused the explosion. It may represent a missing actor. DimenGuard treats that source as unknown. If Endstone cannot recover a position, it logs an error and allows the native operation without a public block event; DimenGuard cannot intercept that missing event. |
| [`ActorBeforeHurtEvent` adapter](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/script_actor_gameplay_handler.cpp) fires `ActorDamageEvent` for a mob victim and returns before native damage on cancellation. | Explosion damage is checked independently at the victim's location. Terrain filtering is not an assertion that entity damage was prevented. Coverage is reported mob/player damage, not every actor type. |
| [`DamageSource`](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/damage/damage_source.h) separates the direct actor from the credited actor. Its [implementation](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/core/damage/damage_source.cpp) resolves each by a different native entity ID and can return no actor. | Exact `block_explosion` and `entity_explosion` damage causes use the direct actor when present; otherwise only a credited TNT/TNT-minecart/creeper supplies a known explosive source. A credited player or another unidentified explosive remains unknown. |

## Enforced rules

All three state flags default to allow and fall back to the existing `explosions`
aggregate when no specific value resolves. The shared resolver retains hierarchy,
priority and deny-on-equal-priority rules. A specific value is an override of its
aggregate, not a second independent flag that must also allow.

| Source | State gate at origin and each affected block | Additional terrain gate |
| --- | --- | --- |
| Exact `minecraft:tnt` or `minecraft:tnt_minecart` | `tnt` | Non-player `block-break`, falling back to `build`, associated with the current explosion position. |
| Exact `minecraft:creeper` | `creeper-explosion` | None; preserves the existing environmental default. |
| Other identified actor, including custom actor namespaces | `other-explosion` | None; preserves the existing environmental default. |
| Unidentified actor/block source or invalid actor identifier | All three state gates must allow | Non-player `block-break` with no source membership. |

If the origin state gate denies, the explosion event is cancelled. Otherwise each
affected block independently passes its terrain and state gates. Filtering keeps
the original relative order and does not add blocks, load chunks or modify the world
directly. An empty filtered list is valid. Exceptions still cancel the guarded event.

For identified TNT, a protected region outside the current source position denies
its terrain by default. TNT already inside a region has that region's non-player
association, subject to explicit flags, hierarchy and shared protection domains.
`block-break deny` can therefore stop internal TNT terrain damage even when `tnt`
allows it. No player bypass is applied to an autonomous explosion.
An explicit `tnt allow` does not grant a source membership in a target region;
the terrain gate is independent. A known source and target in different dimensions
are rejected, including matching coordinates or an explicit target `build allow`.

For unknown sources, the conservative state intersection prevents `other-explosion
allow` from bypassing a possible `tnt deny`. An unknown source never acquires region
membership from an unreliable location. Consequently, beds/anchors or lost-source
explosions may be restricted more broadly than an identified TNT/creeper explosion.
This is an explicit safety trade-off, not exact WorldGuard parity.

Victim damage uses only the source-specific state gate at the victim, separately
from the terrain membership gate. The existing `invincible`, PvP and mob-damage
listeners remain additional independent restrictions. Permitting terrain damage
does not override those listeners, and filtering terrain does not confer immunity.

## WorldGuard comparison and deliberate limits

WorldGuard uses regional association for non-player actions, with optional shared
protection domains. Its [scope documentation](https://worldguard.enginehub.org/en/latest/regions/scope/)
also distinguishes an actor's original location from its later movement. DimenGuard
uses the position supplied by this explosion event: it does not reconstruct a TNT
cannon, retain an ignition position or attribute thrown TNT to a player. TNT which
moves inside a region before exploding may therefore gain local association. This
is not complete anti-cannon protection.

WorldGuard's [flag catalog](https://worldguard.enginehub.org/en/latest/regions/flags/)
includes distinct TNT, creeper and other explosion controls. DimenGuard adapts those
names to the verified public routes above. It does not claim full priming prevention,
fire ignition control, arbitrary actor destruction protection or byte-for-byte
WorldGuard semantics. In particular, identified creeper/other explosions retain
environmental allow defaults unless their flags or `explosions` deny them; merely
creating a cuboid does not disable all mob explosions.

Piston region-boundary enforcement remains unavailable. The pinned
[`PistonBlockActor::tick` hook](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/src/endstone/runtime/bedrock_hooks/piston_block_actor.cpp)
fires cancellable extend/retract events, but their
[`BlockPistonEvent` base](https://github.com/EndstoneMC/endstone/blob/46eff9f125f52eac76472d84339ead8fbf51fcd2/include/endstone/event/block/block_piston_event.h)
supplies only the piston block and direction, not moved, destroyed and destination
blocks. A base-position flag would not prove slime/honey-branch or retract boundary
protection. WG-2 therefore does not register a decorative piston flag or claim that
the new non-player resolver makes piston interception complete.

## Verification

`tests/wg2_explosion_test.cpp` covers exact namespaced classification, missing/direct
damage attribution, the unknown-source intersection, stable filtering and its
single per-target terrain gate, negative/inclusive boundaries, independent
dimensions and cross-dimension rejection, inherited aggregate/subtype overrides,
equal-priority deny ties, existing aggregate compatibility, internal/external TNT
and independent victim-damage decisions. Shared domain/hierarchy behavior is covered by the
non-player policy tests rather than reimplemented in the listener.

Before publishing an updated binary, gameplay acceptance must include outside,
inside and moved TNT; TNT minecarts; creepers; beds/anchors; mixed protected and
unprotected affected blocks; explicit subtype/aggregate conflicts; simultaneous
victim damage; and cancellation by another plugin. No server was started or plugin
deployed for this source audit.
