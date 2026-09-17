# Release notes

## Unreleased: WG-3 region administration commands

- Expand region administration with `create`/`define`, `claim`, `delete`/`remove`, `rename`,
  `redefine`, `move`, `list`, `info`, `flags`, priority, parent, passthrough, typed flag and
  selection commands.
- Add `/dg inspect`, stored-cuboid selection loading, inclusive overlap diagnostics, paginated
  listings and explicit confirmation before cascading child-region deletion.
- Support current-dimension player commands and validated console `<level> <dimension>` targets.
  Numeric region names and balanced quoted values use the shared bounded parser.
- Generate native usage metadata, `/dg help` rows and protocol-2169 autocomplete from one catalog,
  including the `define`, `remove` and `set-priority` aliases.

Read [the WG-3 command administration contract](wg3-commands.md). This work is not deployed or
released; client/gameplay acceptance and WG-4 least-privilege permissions remain pending.

## Unreleased: WG-2 unified flag semantics

- Unify typed parsing, values, inheritance, groups, defaults, persistence and command
  presentation. Keep existing player policies; add seven functional flags (33 total).
- Filter explosion terrain per affected block, distinguish TNT/creeper/other sources,
  apply TNT regional association/domains and keep victim damage independent.
- Add deny-spawn actor sets and literal entry/exit denial messages. Generate state
  suggestions from the registry without restricting free-text flag values.
- Upgrade schema 1/2 directly to schema 3 with a consistent original-version backup
  and transactional validation. Older binaries require their pre-upgrade backup.
- Piston boundaries, TNT ignition-origin tracking and additional numeric/location/
  session-effect consumers remain unsupported. No native hooks are introduced.

Read [the compatibility, commands and recovery contract](flag-semantics.md). This work
is not deployed or released; actual BDS/client acceptance is still required.

## Unreleased: WG-1 region model

- Add explicit cuboid, global and template kinds, bounded same-dimension parent hierarchies,
  inherited ownership/membership, group-aware state rules and passthrough membership policy.
- Retain the 26 existing flags, legacy defaults and event coverage. New model administration
  controls are reserved for the command phase; existing game commands still create cuboids.
- Keep parent links consistent during rename and reject deletion or priority changes that would
  invalidate children. Mutations become live only after their snapshot commits.
- Upgrade schema 1 to schema 2 with a consistent, non-overwriting SQLite backup, transactional
  migration and graph validation. Old databases, including cuboids named `__global__`, keep their
  original policy. Downgrading requires restoring the old binary and pre-migration backup.

See [the model and migration contract](region-model.md) before testing this unreleased work.
The published `0.2.0-rc1` package is unchanged; new model gameplay acceptance remains pending.

## 0.2.0 preview candidate

This is the first broad DimenGuard public-preview candidate. It is not a declaration of
stable protection or complete WorldGuard compatibility. Gameplay and client acceptance are
pending; use a backed-up test world first. Public licensing and distribution must follow the
[release gate](releasing.md).

## Included

- 26 registered state flags with a single typed definition table, deterministic priority/tie
  handling and explicit granular fallback chains. Only implemented event paths are advertised.
- Dimension-scoped cuboid regions, owner/member access, administrative priority and membership
  commands, explicit bypass and transactional SQLite persistence.
- Live existing-region suggestions for protocol 2169, filtered to the current dimension and
  administrators. Successful region catalog changes and dimension changes trigger refresh.
- Corrected native subcommand routing, numeric region names and quoted exact online player
  names. The handler remains authoritative even when client hints are unavailable.
- `/dg flags [page]`, six flags per page, with English/Spanish descriptions and defaults.
  Public discovery does not grant administration or disclose other dimensions' region names.
- Focused listeners, command handlers, rules, presentation, policy, indexing and storage modules.
- Offline regression tests, an opt-in benchmark, separate SDK build directories, minimal CI
  definitions and local candidate packaging with checksums and third-party notices.

## Upgrade from 0.1.0

Stop the server normally and back up the complete plugin data folder and previous DLL. Replace
only the DLL built for that runtime's exact SDK/ABI. Do not overwrite `regions.sqlite3` with a
new database. Schema version remains 1 and existing saved names, UUID ownership and flags are
preserved. Granular flags without explicit values follow the original aggregate rules.

The new environmental, item, chat and movement action flags default to allow. `invincible`
is the exception: it defaults to deny (no added immunity), and allow enables interception of
player damage. Explicit granular values can override aggregate values; read the
[resolution rules](../README.md#flags-and-overlapping-regions) before changing defaults.

After saving a newly introduced flag, the old 0.1.0 plugin may reject that unknown flag on load.
Rollback therefore requires restoring the matching old DLL **and its pre-upgrade data backup**,
not simply swapping back to the previous DLL.

The generic upstream DLL is not interchangeable with a custom chunk-development fork DLL,
even when both show API 0.12. The public target is Windows x64, SDK
`46eff9f125f52eac76472d84339ead8fbf51fcd2`, BDS 1.26.45.1, protocol 2169. Linux plugin
binaries and other protocols are not verified by this preview.

## Important boundaries

Protection is limited to the public events that Endstone actually reports. Inventory transactions,
exact piston movement, several growth/fire paths and complete portal/respawn entry prevention
are not provided. Sleep permission still requires allowing the earlier bed click. Immunity
does not prevent direct health changes or every secondary damage effect.

Typed text/number/set/location flags, greetings, timed effects, region groups, parent/global
regions, external plugin API and custom flags are future work, not hidden functionality.
See the [flag matrix](flag-roadmap.md) and [event coverage](event-coverage.md).

## Acceptance

On 2026-09-08, Windows x64 builds with Clang 19.1.5, CMake 3.31.6 and RelWithDebInfo
passed all 223 offline tests for both the pinned upstream SDK and the separate local fork SDK.
Strict project warnings and formatting checks passed. This does not establish Linux plugin,
remote CI, client completion or Bedrock gameplay acceptance.

Offline build/test results do not establish in-game correctness. Record exact runtime and DLL
revisions when performing [testing.md](testing.md), including owners/members/outsiders, permissions,
overlaps, boundaries, both dimensions, real inventory/world state, reload/restart recovery and
client suggestions. No server deployment or external publication is performed by the build,
test, CI preparation or packaging steps.
