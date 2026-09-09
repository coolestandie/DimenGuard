# DimenGuard

DimenGuard is a C++20 region-protection plugin for **Endstone API 0.12**. Regions belong to a
specific level and dimension, with shared rules for player actions, world events and reported movement.

Version **0.2.0 is a public-preview candidate**, with 26 supported state flags. It is not a
gameplay-verified stable release: the Bedrock checks in [testing.md](docs/testing.md) remain
pending. It does not provide complete WorldGuard coverage. Review [event coverage](docs/event-coverage.md)
before relying on a particular action and see the [release notes](docs/release-notes.md).

The candidate targets Windows x64, Endstone API 0.12 at SDK commit
`46eff9f125f52eac76472d84339ead8fbf51fcd2`, and BDS 1.26.45.1 (protocol 2169).
An API version match alone does not guarantee ABI compatibility; custom forks need their own build.

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
Use separate build directories for upstream and custom-fork SDKs. The public candidate build is:

```powershell
./scripts/build.ps1 -BuildDirectory build/public
```

Follow [release preparation and upgrade instructions](docs/releasing.md) for approved licensing,
third-party notices, checksums, backups and rollback. A local build does not publish or deploy anything.

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
| `/dg flags [page]` | Browse all supported flags, six per page, with descriptions and defaults. |
| `/dg flag` | Open the first flag catalog page with administrative usage. |
| `/dg flag <region> [flag]` | Read the stored flag states without modifying the region. |
| `/dg flag <region> <flag> <allow\|deny\|inherit>` | Set or clear an explicit flag. |
| `/dg trust <region> <player>` | Add an online player as a trusted member. |
| `/dg untrust <region> <player>` | Remove an online player from the trusted members. |
| `/dg reload` | Reload stored regions, or retry initialization after a storage failure. |
| `/dg language <en\|es>` | Choose the message language for the current player session. |

`dimenguard.use` defaults to everyone and permits help, flag discovery and language commands.
`dimenguard.command` defaults to operators and grants administration. Region ownership alone
does not grant that permission. `dimenguard.bypass` defaults to **false, including for operators**;
grant it explicitly through your permission system if an account should bypass region decisions.
An operator with command access still follows protection rules unless explicitly granted bypass.
The console can use help, reload and the flag catalog; commands needing a dimension require an in-game player.

On protocol 2169, the public outgoing-packet API supplies exact client command paths, flags,
states, languages, online players and existing region names in the current dimension. Suggestions
refresh after successful create, rename, delete, reload and dimension changes. Running `/dg flags`
opens the paginated flag guide. Live region names are sent only to administrators.
New names remain free text, including numeric-only names.
Unsupported protocols and unrecognized packets pass through untouched; commands still work,
but exact client hints and live region completion are unavailable. This adapter does not modify
Endstone internals or replace server-side validation.

Region actions share one native overload. Endstone registers a distinct enum symbol for each
declaration, so separate overloads with the same `region` prefix conflict in Bedrock's parser.
The native message tail accepts both names and numbers (Bedrock's `str` rejects numeric tokens);
the handler splits at most two arguments and validates each action's required values and integers.
Flag and membership commands use the same bounded parser, with up to three flag arguments,
so numeric region names and quoted player names reach the handler consistently.
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
| `block-break`, `block-place` | Player breaking and placement, independently. | Follow `build`. |
| `interact` | Supported targeted block and actor interactions. | Owner/member access. |
| `use` | Recognized doors, trapdoors, fence gates, buttons and levers. | Follow `interact`. |
| `use-anvil` | Opening recognized anvils. | Follow `use`, then `interact`. |
| `sleep` | Reported bed entry; the earlier bed click must also permit interaction. | Follow `interact`. |
| `container-access` | Opening interactions for recognized block containers. | Owner/member access. |
| `pvp` | Player-attributed damage between players. | Allowed. |
| `item-drop` | Items actively dropped by players, excluding death drops. | Allowed. |
| `item-pickup` | Reported player/mob pickup, including arrows; checks collector and item. | Allowed. |
| `send-chat` | Player chat dispatched by this server. | Allowed. |
| `explosions` | Reported explosion origin/affected blocks and explosion damage at victims. | Allowed. |
| `fluid-flow` | Reported liquid spread, checking both source and destination. | Allowed. |
| `water-flow`, `lava-flow` | Recognized water/lava spread, independently. | Follow `fluid-flow`. |
| `block-form` | Reported lava solidification, not all block formation. | Allowed. |
| `leaf-decay` | Reported leaf removal through decay. | Allowed. |
| `actor-griefing` | Reported actor changes to a block. | Allowed. |
| `mob-spawning` | Non-player mobs added to the level; denial removes the added mob. | Allowed. |
| `mob-damage` | Attributed non-player mob damage to a player. | Allowed. |
| `fall-damage`, `firework-damage` | Reported fall/firework damage to players and mobs. | Allowed. |
| `invincible` | `allow` cancels intercepted player damage; `deny` adds no immunity. | Denied (no immunity). |
| `entry`, `exit` | Region crossings reported by move/same-dimension teleport events. | Allowed. |

Outside every region, actions are allowed. Inside regions, the policy resolves each flag separately:

1. Examine overlapping regions from highest to lowest priority.
2. At the first priority containing an explicit `allow` or `deny`, `deny` wins any tie. Otherwise,
   an explicit `allow` grants the action. Lower priorities do not override that decision.
3. `inherit` clears the region's explicit value, allowing evaluation to continue to lower priorities.
4. If no priority has an explicit value for a granular flag, resolve its base flag using the same
   overlapping regions. The chains are listed above. An explicit granular rule takes precedence
   over its base rule, even when the base is set on a higher-priority region.
5. Without explicit or inherited values, `build`, `interact` and `container-access` require ownership
   or membership in **every overlapping region at the highest priority**. Other action flags allow;
   `invincible` instead defaults to `deny`, so an unset region adds no immunity.

An explicit `deny` therefore also denies owners and members without bypass. A higher-priority
region with `inherit` does not erase a lower-priority explicit denial. PvP checks both attacker
and victim locations when both players can be resolved.

Environmental flags do not invent a responsible player, use membership, or inherit an operator's
bypass. Entry/exit resolve only regions actually entered or left, so moving inside a denied
region is still possible. Both the exited and entered sets must permit a crossing. A region
containing both endpoints does not override a newly entered region's entry decision.
This differs from WorldGuard's region-group defaults: explicit entry/exit denial currently
affects owners too. Existing policy is preserved pending the planned group-aware expansion.

One player action can emit more than one event. Placing a block may require both `block-place`
(falling back to `build`) at the destination and `interact` at the clicked block. Every intercepted
stage must permit the action. In particular, `sleep allow` does not bypass an earlier `interact deny`
on the bed, and `use-anvil allow` does not bypass `container-access` if the runtime also reports
a container. This prevents a sleep exception from opening exploding-bed interactions in other dimensions.

`invincible allow` protects players against the intercepted damage event, regardless of owner,
membership or player bypass. `invincible deny` never forces damage through another denial
such as `pvp deny`; it simply adds no immunity. It does not cover direct health replacement,
entity removal, knockback or every secondary effect. Outside all regions it grants no immunity.
Item pickup uses player bypass only when a player-specific pickup event identifies that player;
autonomous mob pickup has no invented player identity.

## Persistence and failure behavior

Regions are stored in `regions.sqlite3` inside the plugin's data folder. The key is the level name,
complete dimension identifier and region name. Renaming a level changes that mapping; this
preview does not automatically migrate regions to the renamed level.

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
- World changes use their own explicit flags, not `build`. Explosion denial cancels the whole
  reported explosion if its origin or any affected block denies; damage is checked separately.
  Knockback and every secondary effect are not guaranteed by damage cancellation.
- Fluid hooks exclude instant-ticking paths. Block formation currently means lava solidification.
  Actor-griefing events exclude falling blocks and sheep grass consumption.
- Movement checks cannot prevent every portal, respawn, cross-dimension teleport or tiny
  unreported movement. There is no speculative teleport rollback or claim of complete exclusion.
- Exact piston affected-block protection, growth hooks that never fire, commands and other
  plugins' block edits are not covered by these rules.
- Doors, beds and other multi-block operations need boundary verification. One exposed event
  target does not prove every secondary block is protected.
- PvP needs a resolvable responsible player. A disconnected or unloaded projectile shooter can
  leave attribution unavailable. Knockback, potion effects and indirect environmental consequences
  are not implied to be blocked by damage cancellation.
- DimenGuard registers protection at `High` priority and preserves existing event cancellations.
  Other plugins can still interfere by overriding cancellation later; listener priority does not
  isolate plugins from one another.

The exact source audit and missing Endstone APIs are documented in
[event-coverage.md](docs/event-coverage.md). The larger WorldGuard-inspired flag expansion is
planned in [flag-roadmap.md](docs/flag-roadmap.md); planned flags are not shipped functionality.
Custom native hooks and a public API for other plugins remain deferred.

## Structure

| Directory | Responsibility |
| --- | --- |
| `include/dimenguard/region`, `src/region` | Flag definitions, cuboids, spatial index, snapshot manager and separate action/transition policy. |
| `include/dimenguard/service`, `src/service` | Administrative mutations and consistent live/persisted snapshots. |
| `include/dimenguard/storage`, `src/storage` | SQLite ownership, schema versioning, validation and transactional persistence. |
| `include/dimenguard/command`, `src/command` | Shared context/catalog/parser plus focused general, selection, region, flag and membership handlers. |
| `include/dimenguard/adapter` | Conversion between Endstone handles and domain positions/identities. |
| `include/dimenguard/listener`, `src/listener` | Separate block, interaction, actor, world, explosion, mob, movement, item, player-activity, session and command-packet adapters. |
| `include/dimenguard/rules`, `src/rules` | Testable reviewed block, fluid and damage-source classification. |
| `include/dimenguard/protection`, `src/protection` | Shared target conversion, guarded cancellation, failure handling and policy access. |
| `include/dimenguard/i18n`, `src/i18n` | Locale selection and bilingual message catalog only. |
| `include/dimenguard/presentation`, `src/presentation` | Shared theme/panels, help, flag rendering and message delivery. |
| `include/dimenguard/protocol`, `src/protocol` | Bounded protocol-2169 command suggestion codec, independent of Endstone handles. |
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
Prepared [CI checks](docs/ci.md) cover a Windows plugin build and Linux core-only build;
remote jobs have not yet been run. [Offline scale measurements](docs/performance.md) document
query and administrative-save costs without claiming server tick performance.
