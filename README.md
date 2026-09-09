# DimenGuard

DimenGuard is a C++20 region-protection plugin for **Endstone API 0.12**. Regions belong to a
specific level and dimension, with shared rules for building, interaction, container access and PvP.

This is an **alpha**. Offline tests cover the domain and supporting services; the Bedrock runtime
checks in [testing.md](docs/testing.md) remain pending. It does not provide complete WorldGuard
coverage. Review [event coverage](docs/event-coverage.md) before relying on a particular action.

## Build

On Windows, install Git, Visual Studio 2022 C++ Build Tools, the C++ Clang tools component,
and the Visual Studio CMake/Ninja tools. The build requires CMake 3.29 or newer and downloads
pinned dependencies on its first run.

From the repository directory in PowerShell:

```powershell
./scripts/build.ps1
```

To build against an existing Endstone checkout:

```powershell
./scripts/build.ps1 -EndstoneSource 'C:\path\to\endstone'
```

The script configures the project, compiles it and runs the offline tests. The plugin output is
`build/endstone_dimenguard.dll`. Copy it into a compatible Endstone server's `plugins` directory
while the server is stopped. Building against a different SDK does not establish compatibility
with an arbitrary Bedrock server version; verify the exact Endstone and BDS combination separately.

For domain and service development without building the plugin adapter:

```powershell
./scripts/build.ps1 -CoreOnly
```

## Create a protected region

Use an account with `dimenguard.command`, stand at the first corner, and run `/dg pos1`.
Move to the opposite corner and run `/dg pos2`, then:

```text
/dg region create spawn
/dg region info spawn
/dg flag spawn pvp deny
/dg trust spawn <exact-online-player-name>
```

Selections include both corner blocks and every block between them, including the selected
vertical range. They are cuboids, not automatically full-height columns. Both corners must be
in the same level and dimension. Setting a corner in another dimension starts a new selection;
disconnecting clears the selection.

The creating player's UUID becomes the owner. Owners and trusted members can build, interact
and access supported containers by default. Explicit flags can change that behavior.

## Commands and permissions

Command keywords, region names, flags and state values use English in either display language.
Region names accept 1–64 lowercase letters, digits, underscores or hyphens. Region commands
operate in the executing player's **current level and dimension**; the same name can exist in
other dimensions. Trust and untrust accept one online player's full name, case-insensitively.
Use quotes for names containing spaces. UUID input and selector expressions such as `@a` are
not supported; UUIDs are still stored internally, so existing ownership and membership are preserved.

| Command | Purpose |
| --- | --- |
| `/dg help` | Show available commands. |
| `/dg pos1`, `/dg pos2` | Select the block at the player's current position. |
| `/dg region create <name>` | Create a region from the current selection. |
| `/dg region delete <name>` | Delete a region and its stored rules. |
| `/dg region rename <name> <new-name>` | Rename a region in the current dimension. |
| `/dg region list [page]` | List regions in the current dimension. |
| `/dg region info <name>` | Show bounds, priority, owner, membership count and flags. |
| `/dg region priority <name> <integer>` | Set a signed 32-bit priority; larger numbers take precedence. |
| `/dg flag` | List supported flags, descriptions and usage. |
| `/dg flag <region> [flag]` | Read the stored flag states without modifying the region. |
| `/dg flag <region> <flag> <allow\|deny\|inherit>` | Set or clear an explicit flag. |
| `/dg trust <region> <player>` | Add an online player as a trusted member. |
| `/dg untrust <region> <player>` | Remove an online player from the trusted members. |
| `/dg reload` | Reload stored regions, or retry initialization after a storage failure. |
| `/dg language <en\|es>` | Choose the message language for the current player session. |

`dimenguard.use` defaults to everyone and permits help and language commands.
`dimenguard.command` defaults to operators and grants administration. Region ownership alone
does not grant that permission. `dimenguard.bypass` defaults to **false, including for operators**;
grant it explicitly through your permission system if an account should bypass region decisions.
An operator with command access still follows protection rules unless explicitly granted bypass.
The console can use help, reload and the flag catalog; commands needing a dimension require an in-game player.

Native command parameters suggest subcommands, flags, states, languages and online player names.
After `/dg region `, choose an action; after `/dg flag <region> `, choose a flag. Running `/dg flag`
without arguments lists the available flags. Region names remain free text: the current public
Endstone API cannot update their suggestions dynamically.

Region actions share one native overload. Endstone registers a distinct enum symbol for each
declaration, so separate overloads with the same `region` prefix conflict in Bedrock's parser.
The native message tail accepts both names and numbers (Bedrock's `str` rejects numeric tokens);
the handler splits at most two arguments and validates each action's required values and integers.
Detailed help syntax and suggested actions
come from the same command catalog. Metadata registration alone is not a runtime parsing test.

Messages use English or Spanish from the player's client locale, with English as the fallback.
`/dg language` temporarily overrides that choice until disconnect. The shared theme uses
an amethyst `Dimen`, light-gray `Guard`, dark-gray `>` and white message, without brackets.
Help is a framed command guide with localized categories, descriptions and permission filtering.
Denial notifications are
limited to one per player per second; this does not delay protection decisions.

## Flags and overlapping regions

| Flag | Basic intercepted action | Default when all applicable flags inherit |
| --- | --- | --- |
| `build` | Player block breaking, placement and supported bucket operations. | Owner/member access. |
| `interact` | Supported targeted block and actor interactions. | Owner/member access. |
| `container-access` | Opening interactions for recognized block containers. | Owner/member access. |
| `pvp` | Player-attributed damage between players. | Allowed. |

Outside every region, actions are allowed. Inside regions, the policy resolves each flag separately:

1. Examine overlapping regions from highest to lowest priority.
2. At the first priority containing an explicit `allow` or `deny`, `deny` wins any tie. Otherwise,
   an explicit `allow` grants the action. Lower priorities do not override that decision.
3. `inherit` clears the region's explicit value, allowing evaluation to continue to lower priorities.
4. If no priority has an explicit value, `build`, `interact` and `container-access` require ownership
   or membership in **every overlapping region at the highest priority**. PvP remains allowed.

An explicit `deny` therefore also denies owners and members without bypass. A higher-priority
region with `inherit` does not erase a lower-priority explicit denial. PvP checks both attacker
and victim locations when both players can be resolved.

One player action can emit more than one event. Placing a block may require both `build` at
the destination and `interact` at the clicked block; setting `build allow` alone does not override
an interaction denial. Every intercepted stage of that action must permit it.

## Persistence and failure behavior

Regions are stored in `regions.sqlite3` inside the plugin's data folder. The key is the level name,
complete dimension identifier and region name. Renaming a level changes that mapping; this
alpha does not automatically migrate regions to the renamed level.

Administrative mutations prepare and validate a complete candidate snapshot, then save it in
one SQLite transaction. Only a committed save becomes the active in-memory state. Saves are
synchronous on the owning server thread and replace the full snapshot; very large databases
or slow storage can make commands expensive. The current limit is **10,000 regions in total**,
not a performance guarantee. Protection events perform no database or filesystem I/O.

An ordinary failed save or reload preserves the previously active regions. Unknown schema
versions, malformed metadata and invalid region content produce errors rather than silently
discarding records. Check the server log for the database path and underlying error.

If storage initialization fails at startup, there is no valid region snapshot. The plugin enters a
protective unavailable state: its basic intercepted actions are denied globally until storage is
repaired and `/dg reload` succeeds. This is limited to the intercepted event paths, not every
possible world mutation. It must not be treated as complete lockdown. Keep backups of the
database; stop the server before replacing a damaged file, then verify a successful load.

## Coverage limits

- Container protection checks targeted opening interactions. It cannot enforce transactions
  in an inventory already open, remotely opened inventories, every actor/custom container,
  or hopper/dropper transfers.
- Normal and trapped chests are checked conservatively with horizontal same-type neighbors.
  A neighboring protected chest can deny access even when the two chests are not paired.
  If a neighboring chunk is unloaded, the action is denied because the neighbor cannot be
  verified; the listener does not load chunks to complete that check.
- Block interaction handling covers right-click targets. It deliberately ignores `LeftClickBlock`
  so breaking follows `build`; left-click item-frame and dye-related paths are unverified and
  must not be treated as protected by `interact`.
- Explosions, pistons, flowing fluids, mob block changes, growth, commands and other plugins'
  block edits are separate paths. They are not covered by the basic player-action flags.
- Doors, beds and other multi-block operations need boundary verification. One exposed event
  target does not prove every secondary block is protected.
- PvP needs a resolvable responsible player. A disconnected or unloaded projectile shooter can
  leave attribution unavailable. Knockback, potion effects and indirect environmental consequences
  are not implied to be blocked by damage cancellation.
- DimenGuard registers protection at `High` priority and preserves existing event cancellations.
  Other plugins can still interfere by overriding cancellation later; listener priority does not
  isolate plugins from one another.

The exact source audit and missing Endstone APIs are documented in
[event-coverage.md](docs/event-coverage.md). Advanced world protections, custom native hooks
and a public API for other plugins are deferred.

## Structure

| Directory | Responsibility |
| --- | --- |
| `include/dimenguard/region`, `src/region` | Typed cuboids, per-dimension spatial trees and shared protection policy. |
| `include/dimenguard/service`, `src/service` | Administrative mutations and consistent live/persisted snapshots. |
| `include/dimenguard/storage`, `src/storage` | SQLite ownership, schema versioning, validation and transactional persistence. |
| `include/dimenguard/command`, `src/command` | Checked arguments, player selections and command dispatch. |
| `include/dimenguard/adapter` | Conversion between Endstone handles and domain positions/identities. |
| `include/dimenguard/listener`, `src/listener` | Concrete public-event adapters and guarded protection decisions. |
| `include/dimenguard/i18n`, `src/i18n` | Bilingual message catalog, shared palette and delivery. |
| `tests` | Offline domain, persistence, service and presentation checks. |
| `docs` | Runtime coverage audit and manual acceptance checklist. |

Concrete protection listeners adapt Endstone events to shared domain decisions. Persistence
belongs to the service layer; listeners do not duplicate region policy or save data. Internal
headers are implementation details, not a supported third-party ABI. The plugin lifecycle owns
disconnect cleanup for selections, temporary language choices and notification state.

## Development

The project follows Endstone's C++ naming and formatting conventions, with focused types,
explicit includes, RAII and shared helpers. Format C++ with the checked-in `.clang-format`.
Branches use `type/short-description`; commits use short English conventional messages.
Planning notes and agent instructions are local files excluded from version control.
