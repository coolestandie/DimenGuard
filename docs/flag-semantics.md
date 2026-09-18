# Flag semantics: WG-2

This is the unreleased flag-model update built on [WG-1](region-model.md). It is not
full WorldGuard parity and does not change the published `0.2.0-rc1` download. Server
and client acceptance remain pending. No plugin-owned native hooks are introduced.

## Compatibility audit

The baseline is the official [WorldGuard flag contract](https://worldguard.enginehub.org/en/latest/regions/flags/)
and DimenGuard's pinned Endstone API/runtime revision
`46eff9f125f52eac76472d84339ead8fbf51fcd2`. Every existing flag is covered below.
Defaults, player groups, fallback order and stored values are preserved; the intentional
terrain change is identified separately. `all` remains the default group, including
entry/exit. Existing owners do not silently bypass explicit denials.

| Existing flags | Default or fallback | Evaluated subject / retained difference |
| --- | --- | --- |
| `build`, `interact`, `container-access` | Membership in highest protecting regions | Players; narrower than WorldGuard's broad build umbrella. WG-2 also uses build as TNT's terrain fallback. |
| `block-break`, `block-place` | `build` | Players. WG-2 extends block-break to identified TNT and unattributed explosion terrain, not arbitrary automation. |
| `use`, `sleep` | `interact` | Player interaction; earlier event checks still apply independently. |
| `use-anvil` | `use`, then `interact` | Player opening a recognized anvil. |
| `pvp`, `item-drop`, `send-chat` | Allow | Identified players. PvP checks both endpoints; drop excludes death drops. |
| `entry`, `exit` | Allow | Reported player transitions; only changed regions contribute. Default group remains `all`, not WorldGuard's `nonmembers`. |
| `explosions` | Allow | Environmental aggregate. WG-2 adds subtype overrides and per-block filtering instead of cancelling all terrain for one denied target. |
| `fluid-flow`, `block-form`, `leaf-decay` | Allow | Environment; same reported paths, including lava-only block formation. |
| `actor-griefing`, `mob-spawning`, `mob-damage` | Allow | Environment; actor changes, added nonplayer mobs, attributed mob damage to players. |
| `item-pickup`, `fall-damage`, `firework-damage` | Allow | Mixed actors/players; environmental policy, no invented player association. |
| `water-flow`, `lava-flow` | `fluid-flow` | Environment at both source and destination. |
| `invincible` | Deny (no immunity) | Environment queried for a player victim; bypass cannot turn on immunity. |

State resolution still uses inherited rules and groups at the child's priority,
descending priority, and deny on ties. A specific flag searches its explicit values
before following the aggregate chain. The global region remains the last tier.
Passthrough affects implicit membership, not explicit denial. Unsupported events in
[the event audit](event-coverage.md) have not gained protection merely from this model.

## One registry and typed values

`flags.inc` defines the 33 registered names, scopes, types, defaults, fallbacks and
localization keys. Parsing, validation, service mutations, storage, policy, client
grammar and descriptions consume these definitions. Every region has a single flag
value map, not parallel state and arbitrary-value stores.

The internal value codec supports state, string, integer, double, location, boolean
and set. Numeric, boolean and location consumer flags are deliberately not registered
yet: their codec support is not a promise of healing, spawning, teleporting or session
effects. There are no advertised no-op flags and no public custom-flag API.

| New registered flag | Type / default | Implemented consumer |
| --- | --- | --- |
| `tnt` | State / `explosions` fallback | Identified TNT/TNT-minecart explosions, filtered terrain and reported victim damage. |
| `creeper-explosion` | State / `explosions` fallback | Identified creeper explosions and reported victim damage. |
| `other-explosion` | State / `explosions` fallback | Other identified explosive actors and reported victim damage. |
| `deny-spawn` | Set / empty | Removes matching nonplayer actor types when Endstone reports them added. |
| `entry-deny-message` | String / no override | Plain text for a denied reported entry; normal localized denial otherwise. |
| `exit-deny-message` | String / no override | Plain text for a denied reported exit; normal localized denial otherwise. |
| `nonplayer-protection-domains` | Set / empty | Shared membership for the supported TNT terrain path. No piston enforcement. |

### Value resolution

Non-state values share hierarchy, independent groups, priority tiers and global
precedence with states. Within the winning explicit tier, sets are unioned; other
values use the lexicographically first canonical region name for a deterministic
tie. This is a deliberate DimenGuard rule, not WorldGuard's unspecified scalar tie.
Lower tiers are ignored once a tier supplies a value. An explicit empty set overrides
lower/inherited sets but does not erase an unrelated tied region's set.

Environment decisions accept only environmental flags and never inherit player
bypass or ownership. Relationship groups `members`, `owners`, `nonmembers` and
`nonowners` apply to player/transition flags; environmental flags only accept `all`.
Nonplayer membership is a separate subject, not an empty or fabricated player UUID.

Messages resolve only from regions involved in the denied transition, with exit
checked before entry. A message alone never denies movement. Administrator text is
literal, not a formatting template, command, translation key or placeholder program.
The standard prefix and shared one-second denial throttle still apply. Empty text
is a valid explicit override. Portal/event coverage has not expanded.

### Bounds and input

Strings are at most 256 UTF-8 bytes and reject invalid UTF-8/control characters.
Sets contain at most 64 unique entries, at most 128 bytes each. Actor IDs use canonical
`namespace:name` spelling (including custom namespaces); syntactic acceptance does
not prove the actor type is installed. Domains use 1-64 lowercase ASCII letters,
digits, underscores or hyphens. Numeric codecs reject overflow, trailing junk and
non-finite values. Locations encode explicit level/dimension and finite coordinates;
they are data only, not a teleport command. The complete encoded value is bounded.

## Commands and suggestions

Existing query forms remain read-only; omitting a value never erases it:

```text
/dg flag spawn
/dg flag spawn tnt
/dg flag spawn tnt deny
/dg flag spawn deny-spawn minecraft:zombie,minecraft:skeleton
/dg flag spawn deny-spawn []
/dg flag spawn entry-deny-message This area is private.
/dg flag spawn entry-deny-message "This area is private."
/dg flag spawn entry-deny-message --unset
/dg flag factory nonplayer-protection-domains district_a
```

`--unset` clears any stored value without clearing its independent group. State
`inherit` remains a clearing alias. For strings, `inherit` is literal text, and `[]`
is a literal string; for sets, `[]` is an explicitly empty set. `--unset` is reserved
by the command interface. Text with spaces accepts a bounded final message tail;
malformed outer quotes and controls are rejected. State flags still reject extra
words instead of partially accepting the first word.

Native server registration keeps its single authoritative message-tail overload to
avoid the previous repeated-root parsing bug. The existing version-checked protocol
adapter generates client paths per registered flag, keeps current-dimension region
suggestions, and offers state choices only where they are valid. Text and sets accept
free text; a single-flag query shows their type and shortcuts (`[]`, `--unset`).
Unsupported packet formats pass through unchanged; permissions and server parsing
remain authoritative. In-game autocomplete rendering must still be verified.

## Explosions and nonplayer association

The pinned runtime honors edited public affected-block lists. An origin denial
cancels the event; otherwise only denied target blocks are removed, retaining allowed
terrain. Entity damage is checked separately at each reported victim. Details and
pinned evidence are in [the explosion audit](wg2-explosion-audit.md).

Identified TNT additionally needs `block-break`/`build` permission as a nonplayer
subject. A source inside the same region/hierarchy has regional association; a source
outside a protected target does not. Explicit block-break/build rules still apply.
Shared non-global protection domains can connect source and target associations in
the same level/dimension; they never confer ownership or cross-dimension privileges.
Unknown sources receive no membership, so protected targets fail closed under the
normal membership default. Explicit allows remain intentional policy overrides.

**This uses the current explosion position, not the original ignition position.**
TNT moving into a region before exploding can acquire that region's association.
Do not advertise full anti-cannon protection. Identified creeper/other explosions
retain environmental defaults: creating a cuboid alone does not disable all mob
explosions. An unknown source must pass all subtype checks and the unassociated
terrain gate, which can restrict beds/anchors more than identified explosions.

Pistons lack a reliable public list of every moved/destroyed/destination block.
Domains do not magically protect piston, hopper, fluid or arbitrary actor automation.
Those adapters and ignition-origin tracking need a separate verified capability.

## Storage upgrade and recovery

Schema 3 stores each flag's explicit `type` and encoded `value`. Supported schema 1
and 2 databases migrate directly after validating their historic flag vocabulary.
A unique SQLite backup of the **original version** is created before transactional
DDL, including committed WAL contents. Hierarchy/groups, UUIDs, coordinates, legacy
`__global__` cuboids and old states are preserved. New data is validated before
commit; a failed migration rolls back. Backups are not overwritten.

Invalid types, unknown names, malformed values and oversized encoded columns fail
loading instead of silently dropping protection. Failed administrative writes keep
both the previous stored snapshot and the live index; failed reloads keep live state.
Protection queries do no database I/O. Administrative snapshots remain synchronous.

Older binaries cannot read schema 3. To downgrade, stop the server, preserve the
current complete data folder, then restore the matching old binary and the
pre-migration backup. Post-upgrade changes are not in that backup. Never edit
`user_version` to fake compatibility or overwrite an active database.

## Acceptance boundary

The complete Windows x64 upstream build passed 375 offline tests on 2026-09-09 with
Clang 19.1.5, CMake 3.31.6 and strict warnings. Formatting and benchmark result checks
also passed. This records local validation only; Linux CI and runtime acceptance are separate.

Offline policy, codec, persistence, command/protocol and adapter tests are necessary
but are not gameplay acceptance. Before any release/deployment, test mixed explosion
terrain and victims, missing attribution, domain/hierarchy boundaries, movement
denials/messages, actor additions, numeric region IDs, text/state suggestions and
restart/reload. Use a disposable backed-up world and a DLL built for the exact runtime
ABI; this repository's public SDK build is not the custom local chunk fork's DLL.
