# Region model: WG-1

This document records the WG-1 baseline. [WG-2](flag-semantics.md) extends its values,
explosion behavior and storage to schema 3; use that contract for the current branch.

WG-1 is the unreleased domain and persistence foundation for the next update. It adds region
hierarchy, dimension-wide policy, templates and groups to the existing shared protection
resolver. The public `0.2.0-rc1` download is unchanged. Creating these new region types and
editing parents, groups or passthrough through game commands belongs to the subsequent command
phase. Existing commands continue to create cuboids. The service methods described below are
internal implementation details, not a public plugin API.

## Region identity and shape

A region is identified by its level, complete dimension identifier and lowercase name. Names
still accept 1-64 lowercase ASCII letters, digits, underscores or hyphens. WG-1 does not change
case matching or rewrite existing names, UUID owners, members, bounds, priorities or flag states.

| Kind | Spatial participation | Purpose |
| --- | --- | --- |
| `cuboid` | Inclusive minimum and maximum block coordinates. | Ordinary protected areas. |
| `global` | Every position in its own level and dimension. | Lowest-precedence dimension policy. |
| `template` | None; never returned by a position query. | Reusable parent rules and membership. |

Global and template bounds are not used for matching. Neither is indexed as a giant cuboid.
There is at most one global region per dimension, named `__global__`, with fixed minimum
integer priority. It sorts below even a cuboid whose priority is the minimum integer. A
dimension without a global region keeps its previous outside-region behavior.

Previous versions accepted `__global__` as an ordinary cuboid name. Those saved regions remain
cuboids with their original coverage. They are never silently converted into dimension-wide
protection. A conflicting legacy region must be explicitly renamed before a global region can
be created. New ordinary regions cannot take the reserved global name.

Cuboids remain the only physical shape. Polygonal and cylindrical regions are deferred.

## Parent hierarchy

A cuboid or template has at most one parent in the same level and dimension. Physical containment
does not establish a parent relationship and is not required: a template can inherit from a
cuboid, and a distant cuboid can explicitly inherit a parent's rules.

Validation rejects missing parents, self-parenting, cycles, chains longer than 32 regions and
parents with higher priority than their children. A global region cannot have a parent or act
as one; it is a positional fallback, not an inherited template.

Ownership and membership include the current region and its ancestors. Each region retains its
primary UUID owner and its explicit member set. A parent's owner can perform membership-protected
actions in the child, but region membership still does not grant command permissions or bypass.
A child's member does not become a member of a sibling or gain access elsewhere in the parent.

Renaming a parent updates its direct children's links in the same committed snapshot. Deleting
a region with children is rejected until the links are removed explicitly. Priority changes
validate both parent and child constraints before storage or live state changes.

## Flag resolution

All existing listeners continue to use the shared manager and policy. There are still 26 event
flags: this phase adds no new event hooks or broader world-mutation guarantees.

1. Find cuboids at the target position and its dimension's optional global region.
2. Within each matching region's lineage, use the nearest explicit state for the requested flag.
   `inherit` means no local state. Resolve the nearest explicit group independently; its default
   is `all`. This lets a child scope an inherited state without duplicating it.
3. Evaluate the group against that region's effective ownership and membership. If it excludes
   the subject, continue above that candidate to the next explicit ancestor rule and its group.
   A members-only exception on a child must not erase an ancestor's denial for outsiders.
4. A matching descendant represents its ancestor's inherited policy. Do not count the ancestor
   again as an unrelated overlapping region, even when the descendant's group excludes the
   subject. Unrelated overlapping regions still contribute their own rules.
5. Apply descending priority to those effective rules. Deny wins conflicts at the first tier
   with an applicable explicit value. The global region is the final tier.
6. If there is no explicit decision, resolve the registered granular fallback. With no decision
   in that chain, use the flag's existing default.

For membership defaults, passthrough regions are skipped before selecting the highest protecting
priority and resolving parent relationships. A child marked passthrough cannot remove its
parent's implicit protection. Without passthrough, membership in a child also satisfies that
ancestor's overlapping protection, but not an unrelated same-priority region.

Passthrough controls implicit membership protection for `build`, `interact`, `container-access`
and their fallback chains. It does not clear explicit denials, grant bypass, change movement
rules or enable actor automation. Its nearest explicit ancestor value is inherited. Unset
cuboids/templates protect by membership; an unset global region passes through. Adding global
members does not implicitly change passthrough. Set it to deny explicitly to protect wilderness.

Player bypass remains separate from administration and membership. Environmental decisions,
including player immunity through `invincible`, remain independent of player bypass.

The global region has one build exception: its explicit `build allow` has no effect, and its
`build deny` does not override an ordinary region's implicit membership protection. It applies
where there is no non-passthrough cuboid. Use global passthrough deny for membership-protected
wilderness. Other global flags retain ordinary explicit fallback behavior. This exception does
not change lower-priority flags on legacy cuboids, even one named `__global__`.

## Groups

| Group | Applicable players |
| --- | --- |
| `all` | Everyone. |
| `members` | Primary owners, members and inherited owners/members. |
| `owners` | Primary owner and inherited owners. |
| `nonmembers` | Everyone outside effective membership. |
| `nonowners` | Everyone outside effective ownership. |

These are relationship tests, not permission-system groups. Each flag has one state and one
optional group per region. A group may be stored without a local state to scope an inherited
flag. An explicit `all` overrides an inherited group; removing the group resumes inheritance.
Clearing a state with `inherit` does not also erase its independent group override.

A child group override scopes the nearest effective state. If that candidate excludes the
subject, fallback resumes above its source with the remaining ancestors' group rules. For example,
a child's `pvp allow` for members overrides a parent's general denial only for those members;
outsiders still receive the parent's denial.

WG-1 supports non-`all` groups only for player and transition flags. Environmental flags retain
`all` because their current public event routes do not always expose a trustworthy player.
Unsupported group combinations are rejected before saving; they never become ignored settings.
Existing entry/exit flags keep their `all` default, so old denials still affect owners unless an
explicit group changes the rule. This intentionally differs from WorldGuard's nonmember default.

Movement continues to evaluate only regions actually entered or left. Parent rules can now be
inherited by a crossed child. A global entry/exit rule concerns entering/leaving its dimension,
not every step within it. Endstone's existing movement-event limitations still apply.

## Audit of the existing flags

The following is the compatibility contract for old snapshots; an absent group remains `all`.
Every row uses inherited state, descending priority and deny-on-tie when a hierarchy is added.

| Flags | Default/fallback retained | Subject path |
| --- | --- | --- |
| `build`, `interact`, `container-access` | Effective membership at the highest protecting priority. | Player. |
| `block-break`, `block-place` | `build`. | Player. |
| `use`, `sleep` | `interact`. | Player. |
| `use-anvil` | `use`, then `interact`. | Player. |
| `pvp`, `item-drop`, `send-chat` | Allow. | Player. |
| `entry`, `exit` | Allow; unchanged `all` group default. | Player crossing. |
| `explosions`, `fluid-flow`, `block-form`, `leaf-decay`, `actor-griefing`, `mob-spawning`, `mob-damage` | Allow. | Environmental. |
| `item-pickup`, `fall-damage`, `firework-damage` | Allow. | Mixed player/actor routes; groups restricted to `all`. |
| `water-flow`, `lava-flow` | `fluid-flow`. | Environmental. |
| `invincible` | Deny, meaning no added immunity. | Environmental decision at player victim. |

DimenGuard retains its existing narrower build scope, granular-before-base fallback ordering,
independent event-stage checks and explicit environmental flags. WorldGuard-style non-player
association, source-specific TNT flags, block-list explosion filtering, typed non-state values
and new inventory transaction coverage belong to later phases. `explosions` still cancels the
entire reported event if any currently checked location denies it.

## Storage and recovery

Schema version 2 stores region kind, optional parent name, passthrough and independent flag groups.
Version 1 snapshots load as cuboids without parents, groups or explicit passthrough. Migration
validates the old snapshot, creates a consistent SQLite backup and upgrades in a transaction.
The complete migrated hierarchy is validated before the transaction commits.

Backups use a newly reserved sibling directory named
`<database>.v1-backup-<timestamp>-<sequence>`, containing the original database filename. Existing
backups are never overwritten. The SQLite backup includes committed content even when the
original uses a write-ahead log. Initialization fails if a required backup cannot be made.
Invalid old content or a migration failure leaves the original schema and data unchanged.

Reload and save check the schema version and validate the complete graph. Failed saves preserve
both live protection and the prior stored snapshot; failed reloads preserve the prior live state.
Database access remains confined to initialization and administration, never protection queries.

Older binaries do not support schema 2. To downgrade, stop the server, preserve the current
database and any sidecar files as a separate backup, then restore the pre-migration database
and the matching older binary. Changes saved after migration are not in that backup. Do not
lower `user_version` manually or copy an active database over another database.

## Internal service operations and verification

`RegionService` owns `createGlobal`, `createTemplate`, `setParent`, `setPassthrough` and
`setFlagGroup`, alongside the existing mutations. They validate and index a candidate, persist
it, then replace live state only after commit. No native memory hooks or public API are added.

Offline tests cover legacy rules, boundaries, negative coordinates, independent levels and
dimensions, parent overrides, groups, passthrough, global minimum priority, templates,
copy/move safety, failed hierarchy changes, migration backup/rollback and round trips.
Building the plugin verifies adapter integration. Bedrock gameplay acceptance and controls
for the new model remain subsequent work; WG-1 does not deploy or update a public release.

The independent design uses WorldGuard's official documentation as behavioral reference:
[priorities and inheritance](https://worldguard.enginehub.org/en/latest/regions/priorities/),
[global region](https://worldguard.enginehub.org/en/latest/regions/global-region/) and
[flag groups and defaults](https://worldguard.enginehub.org/en/latest/regions/flags/).
