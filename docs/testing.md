# Testing DimenGuard

Offline tests and Bedrock runtime acceptance are separate checks. The gameplay/client checklist
below is **pending**. Successful startup alone does not establish runtime protection coverage.
Read [event-coverage.md](event-coverage.md) for the source audit and known API limitations.

## WG-2 acceptance (not run)

Use the [WG-2 compatibility contract](flag-semantics.md), an exact-ABI DLL and a disposable
world. None of the following checks has been marked as passed by the offline suite:

- Migrate a copied schema 1 and schema 2 database; verify the untouched original-version
  backup, groups/parents, restart, and reload. Never test migration on the only live copy.
- Place TNT outside an inclusive cuboid boundary: outside terrain may break, protected target
  blocks remain. Repeat inside the region and with block-break deny. Check actual server blocks.
- Test negative coordinates, overlapping parents, shared/different domains and another dimension.
  A domain on one overlapping region must not grant membership in an unrelated protected region.
- Move primed TNT across the boundary before detonation: record the known current-origin limit.
  This update does not claim ignition tracking or protection against that cannon path by default.
- Compare TNT, TNT minecarts, creepers and unidentified beds/anchors with subtype/aggregate
  conflicts. Check terrain and victim damage separately; confirm allowed terrain is retained.
- Cancel or alter an explosion from another plugin; confirm prior cancellation and guarded errors.
- Use deny-spawn with vanilla/custom actor IDs, empty sets, and mob-spawning deny. Verify removal
  on reported additions, player exclusion and unchanged mobs outside the region.
- Set English/Spanish denial text, quoted empty text and literal inherit. Check one-second
  message throttling, inherited/grouped messages, exit-before-entry and unchanged-region silence.
- Check `/dg flag` queries, per-flag state hints, free text/sets, quoted spaces, --unset, numeric
  region names and both client languages. Invalid values must not alter saved or live rules.
- Confirm unsupported piston/automation and unreported portal/spawn/damage routes are not
  mistaken for coverage. The public upstream DLL is not the custom chunk-fork server build.

## WG-3 acceptance (not run)

Use the [WG-3 command administration contract](wg3-commands.md) on a disposable world and a
DLL built for the exact server ABI. The offline suite validates parsing and service snapshots,
but does not prove Bedrock command routing, client rendering or loaded-dimension identifiers.

- [ ] Execute every player action: `/dg inspect`, create/claim, list with pages, info, flags,
  priority/set-priority, parent clearing, passthrough, typed set/unset, select, redefine, move,
  rename and delete/remove. Check success messages in English and Spanish.
- [ ] Select negative and positive corners, inspect an overlapping location, load a stored cuboid
  with `region select`, redefine it and move it by positive, negative and zero offsets. Verify the
  final bounds in `/dg region info` and that overlap warnings name the final conflicts.
- [ ] Create a parent/child/grandchild chain. Confirm an unconfirmed parent delete changes nothing;
  `delete <parent> confirm` removes the complete tree and refreshes region-name suggestions.
- [ ] Repeat commands with numeric region names, balanced quoted flag text and malformed quotes;
  invalid values, extra arguments and move overflow must be rejected without a snapshot change.
- [ ] From the console run `region list <level> <dimension> [page]`, info, flags, priority, parent,
  passthrough, typed flag, rename, move and confirmed delete. Verify unknown levels/dimensions are
  rejected before storage access; selection and membership commands must remain player-only.
- [ ] Check canonical and alias paths in `/dg`, `/dg region` and protocol-2169 client suggestions.
  Existing names are dimension-filtered, new names remain free text and suggestions refresh after
  create, rename, delete, reload and dimension changes.
- [ ] Verify `/dg help` uses the same action list and hides administrative rows for a sender without
  `dimenguard.command`. Confirm `claim` requires `dimenguard.region.claim` and is no longer an
  implicit alias for administrator create.

## Offline checks

Run `./scripts/build.ps1` from the repository root to build the plugin and run CTest. Use
`-EndstoneSource 'C:\path\to\endstone'` for an existing SDK checkout or `-CoreOnly` to omit
the Endstone adapter. The script stops when configuration, compilation or tests fail.

Offline checks exercise inclusive cuboids, negative coordinates, dimension separation, overlaps,
flag precedence, invalid selections and arguments, SQLite roundtrips and rollback, service state
preservation, granular aggregate fallback chains, inverse immunity defaults, bounded pagination,
locale fallback and English/Spanish message parity. They do not reproduce BDS
event timing, client inventories, projectile attribution or third-party plugin ordering.

## Runtime test record

Local command smoke check on 2026-09-08: Windows x64, BDS 1.26.45.1, Endstone 0.12.0
with the matching local fork SDK. The updated DLL loaded one saved region with no registration
errors, and `plugins` listed only DimenGuard. Console `dg`, `dg help` and `help dg` succeeded;
native help listed the typed overloads. A client test then exposed ambiguous repeated `region`
enum values: seeing a usage in help had not proved that the command was executable.

After the correction at 19:02, 24 console parser probes reached the plugin's player-only guard:
all six region actions, list with/without a page, negative and extreme 32-bit priorities, quoted
rename arguments, region/single-flag queries, and all four flags with each of the three states.
The general `dg flag` catalog rendered successfully. Unknown actions, flags and states were
rejected by native enums. These probes prove native routing, not player-side execution: console
guards prevented mutations. The region database matched its pre-update backup byte-for-byte.
Client suggestions and execution of the corrected commands still need in-game acceptance.

The 26-state-flag 0.2.0 preview candidate with live region completion and paged flag discovery has
not been deployed or accepted in-game. The preceding parser observations describe the earlier
four-flag build, not this candidate. Typed/session expansions remain future work.

Use a disposable world or a restorable copy. Before testing, record:

| Field | Value |
| --- | --- |
| Date | Pending |
| OS | Pending |
| BDS version | Pending |
| Endstone version and commit | Pending |
| DimenGuard commit | Pending |
| DLL hash and build SDK commit | Pending |
| Other plugins and permissions provider | Pending |
| Testers / owner / member / outsider UUIDs | Pending |
| World and dimensions | Pending |

For each check, record the expected result, observed result and relevant server log. Reconnect
when checking block and inventory corrections so a client animation is not mistaken for an
authoritative world change. Mark unsupported cases as limitations, not passing protections.

## Setup and administration

- [ ] Load `endstone_dimenguard.dll` on the recorded compatible server. Confirm the plugin
  reports successful region loading and the log contains no unresolved imports or registration errors.
- [ ] Prepare an owner, a trusted member and an outsider. Give one test account operator status
  with `dimenguard.command` but without `dimenguard.bypass`.
- [ ] Create `spawn` with opposite corners at negative and positive coordinates, including a narrow
  vertical range. Check `/dg region info spawn` shows normalized, inclusive bounds and the owner UUID.
- [ ] Select both corners in opposite orders. Attempt creation with one missing corner, with corners
  selected in different dimensions, and after disconnecting. Incomplete selections must be rejected.
- [ ] Create the same region name in another dimension and, where supported by the server setup,
  another level. List, inspect, rename and delete in one dimension without changing the others.
- [ ] Reject invalid/duplicate names, overflowing priorities, invalid pages and unknown flag/state
  values. Verify failed commands leave existing regions unchanged.
- [ ] Trust and untrust one online player by full name, including case variations and quoted names
  containing spaces. Check the response uses their display name and membership stays UUID-based.
  Reject offline names, partial matches, UUID input and selectors such as `@a`.
- [ ] Reconnect after installing the updated DLL. Check suggestions for `/dg`, `/dg region`,
  `/dg flag spawn`, `/dg language` and `/dg trust spawn`. Check existing region-name suggestions
  in the current dimension, create/rename/delete refresh, dimension switches and numeric-only names.
- [ ] Confirm `region` appears only once among the root suggestions. Execute every region action,
  not only `help dg`: `list`, `list 1`, `info spawn`, and create/rename/priority/delete on a disposable
  region. Verify missing/extra arguments and invalid integers are rejected without changing data.
- [ ] Confirm `/dg flags` matches `/dg flags 1`. Across pages 1 through 6, collect all 33 descriptions
  once, with no more than six per page and valid previous/next navigation. Reject pages 0, -1, 7,
  overflow and trailing arguments. Check console and client rendering in both languages.
- [ ] As an administrator, `/dg flag` must show the first administrative catalog page, not all
  33 descriptions in one chat dump. `/dg flag spawn` and
  `/dg flag spawn build` must only read stored states; provide a state to change one. Test
  all 33 flag/state suggestions and reject invalid values. A full stored-state query remains
  unpaged; it must not modify data.
- [ ] Inspect `/dg help` in English and Spanish: framed heading, categorized command rows and
  no administrative rows for a player without `dimenguard.command`. Verify `DimenGuard >` colors.
- [ ] Verify operation permissions independently: selection, create, claim, delete, list, info,
  flags, priority, parent, membership, ownership and recovery. An owner without the matching
  `.own` scope cannot administer a region merely by owning it; a member needs the matching `.member`
  scope. Verify a non-operator without an operation node cannot create, delete or change flags.
- [ ] Grant `dimenguard.region.flags.own`, `dimenguard.region.flag.pvp` and
  `dimenguard.region.flag.pvp.deny` to an owner. Confirm that `pvp allow` and unrelated flags remain
  unavailable until their value-specific nodes are granted. Confirm `dimenguard.region.flag.*` is an
  explicit all-flag grant, not an implicit wildcard.
- [ ] Grant `dimenguard.region.claim` to a non-operator. Claim a non-overlapping selection, reject a
  physical overlap, reject a claim larger than 1,048,576 blocks and reject the 65th claim for one UUID.
  Confirm the owner UUID persists after reload and `/dg region set-owner` preserves members.
- [ ] Verify a player with the default `dimenguard.use` can read help, browse `/dg flags` and
  choose their language without receiving administration rights. Check reload from the console
  and rejection of player-only commands.
- [ ] Without `dimenguard.command`, catalog/help panels must omit administrative mutation syntax
  and examples. Region-name completion must not reveal the administrator's dimension catalog.
  Native fallback syntax may still list administrative subcommands; executing them must be denied.
- [ ] Where a separately compatible runtime is available, verify an unsupported protocol or
  unrecognized command packet retains native commands without live region-name hints. Otherwise
  record the offline passthrough coverage only; do not corrupt live packets or load an incompatible DLL.

## Policy and boundaries

- [ ] With all flags inherited, owners and members can build, interact and access supported
  containers inside a single region; outsiders cannot. PvP is allowed by default.
- [ ] Repeat the outsider actions as the operator without bypass. They must still be denied.
  Explicitly grant bypass, verify access, remove it and verify denial resumes.
- [ ] For ordinary action flags, set an explicit `allow` and verify outsiders gain that action.
  Set `deny` and verify owners and members lose it. Set `inherit` and verify the documented
  fallback returns. Test the inverse `invincible` feature separately below.
- [ ] Create two equal-priority overlapping regions. `deny` must beat `allow`. With both inherited,
  default member access requires membership in both highest-priority regions.
- [ ] Give an overlapping region a higher priority. Its explicit decision must beat lower-priority
  decisions. Set its flag to `inherit`; a lower-priority explicit denial must apply again.
- [ ] For every granular chain, leave the granular flag inherited and check its existing aggregate.
  Then set a granular explicit value, including in a lower-priority overlap with a higher-priority
  aggregate denial. The explicit granular decision is resolved first; `inherit` restores the
  aggregate. Repeat `use-anvil -> use -> interact` and `water-flow/lava-flow -> fluid-flow`.
- [ ] Check each selected corner, minimum and maximum Y, and the block immediately outside each
  face. Repeat across zero and at negative fractional player positions to verify coordinate flooring.
- [ ] Stand outside and target a protected block; then stand inside and target an unprotected one.
  Decisions must follow the affected target, not only where the player stands.
- [ ] For an outsider, allow `build` while the clicked block still denies `interact`, then try placing
  a block. The interaction denial must remain effective. Permit the applicable interaction too
  and repeat: every intercepted event stage must allow the action.

## Player blocks and interactions

- [ ] Break and place blocks in survival and creative. Confirm cancellation leaves the actual world
  unchanged, does not duplicate inventory items, and corrects the client's view after reconnect.
- [ ] With `build deny`, set `block-break allow`: breaking should be permitted by that stage while
  bucket operations retain `build deny`. Test `block-place allow` separately with the clicked
  target's interaction explicitly permitted. `block-place` must not silently permit bucket emptying.
- [ ] Test door and bed placement with secondary blocks crossing the boundary. Record any exposed
  target gap as unsupported; do not assume the whole multi-block operation was protected.
- [ ] Fill and empty water, lava and powder-snow buckets at region edges. Test waterlogging,
  cauldrons, supported creature buckets and milking separately, including inventory accounting.
- [ ] Use doors, buttons, levers, signs, lecterns, item frames and other supported targeted actions.
  Check server-side changes as well as displayed screens and animations.
- [ ] Set `interact deny` and `use allow`; exercise reviewed wood/copper door, trapdoor, gate,
  button and lever identifiers. Other block types and custom-namespace lookalikes must retain
  their documented interaction/container behavior. Pressure plates/trampling are not click tests.
- [ ] Set `use deny`, then `use-anvil allow`; test each supported anvil variant and verify its
  opening route. Where a target is exposed as a `Container`, `container-access deny` must still
  deny access. Do not treat anvil UI success as evidence of inventory transaction coverage.
- [ ] In a valid Overworld sleep attempt, `interact allow` plus `sleep deny` must prevent sleeping.
  `interact deny` plus `sleep allow` must still deny the preceding bed click; allowing both permits
  the reported stages. Test owner/member/outsider and check actual sleeping/spawn state separately.
- [ ] In a disposable Nether/End setup, keep `interact deny` and set `sleep allow`. A bed click
  must remain denied, without using the sleep exception to trigger a bed explosion. With interaction
  permitted, test explosive consequences under `explosions` separately; `sleep` is not that guard.
- [ ] Record left-click item-frame and dye-related behavior separately. The block interaction
  listener ignores `LeftClickBlock`; these paths are unverified, not covered by a general `interact` claim.
- [ ] Interact with mobs and storage actors. Test armor-stand equipment manipulation separately;
  it has a distinct event registration.
- [ ] Confirm environmental and automated changes are not mistakenly reported as protected by
  `build`: explosions, pistons, subsequent fluid flow, growth and other plugins' edits are separate paths.

## Containers

- [ ] Open recognized containers from both sides of a region boundary. Verify `container-access`
  applies to the clicked container rather than to the player's current position.
- [ ] Open both halves of normal and trapped double chests crossing the boundary, in every
  orientation and across a chunk edge. A denied candidate must prevent the opening interaction.
- [ ] Place a chest next to a protected same-type chest. Check pairing behavior and record any
  secondary-block limitation exposed by the public placement event.
- [ ] Put an unpaired same-type chest horizontally beside a protected chest. Confirm and record
  the documented conservative denial; precise pairing is not exposed by the current adapter.
- [ ] With a controlled setup, leave a neighboring chest candidate's chunk unloaded. The action
  must be denied without loading that chunk; repeat once the neighbor is naturally loaded.
- [ ] Change trust or flags while a container is already open. Exercise transfers, dragging and
  shift transfers. These ongoing transactions are a known coverage gap, not an expected full protection.
- [ ] Test remote/plugin-opened inventories, custom/actor containers and hopper/dropper transfers.
  Document unsupported paths without attributing protection to the targeted opening listener.

## Items and chat

- [ ] Set `item-drop deny` while alive; try single-item and stack drops on either side of a region
  boundary. Verify actual inventory counts and world item actors after reconnect. Death drops,
  XP and dispenser output remain separate unsupported paths for this flag.
- [ ] Set `item-pickup deny`; test loose items, arrows and thrown tridents with only the player
  inside, only the item inside, both inside and both outside. Both endpoints must allow pickup.
  Explicit player bypass may permit pickup; operator status alone must not.
- [ ] Test reported non-player pickup with suitable mobs and item locations on opposite sides of
  the boundary. Denial checks both endpoints without adopting a nearby player's identity/bypass.
  Hopper and inventory transfers are not this event.
- [ ] Set `send-chat deny`, send chat inside, outside and after changing dimensions, then restore
  inherit. Verify denial at the sender, message throttling and no accidental blocking of plugin
  administration commands. Receive filtering, proxy chat and plugin broadcasts are not promised.

## PvP and event cooperation

- [ ] Set `pvp deny`; test melee with only the attacker inside, only the victim inside, and both
  inside. Repeat with both outside as a control. Check attacker and victim dimensions separately.
- [ ] Repeat with arrows and other supported projectile damage. Record attributed shooter,
  damage and knockback as separate observations.
- [ ] Disconnect the shooter before impact. Record any missing attribution; this preview candidate cannot
  infer a historical player position from an unavailable shooter.
- [ ] Check splash/potion effects, fire and other delayed environmental consequences separately.
  Damage cancellation alone must not be reported as protection of all secondary effects.
- [ ] Set `fall-damage deny`; test reported player and mob fall damage. Falling blocks, flying
  into walls and stalactites must not be mistaken for the `fall` cause. Repeat with allow/inherit.
- [ ] Set `firework-damage deny`; verify the actual reported firework cause on players and mobs,
  distinguishing it from ordinary projectile and explosion damage. Record secondary effects separately.
- [ ] With `invincible` absent or inherited, verify no added player immunity inside or outside
  regions. Set `invincible allow` and test multiple reported damage causes at a player victim;
  a non-player mob at the same position must not gain this immunity.
- [ ] Set `invincible deny` and confirm it removes only that feature, not other denials: with
  `pvp deny` or `fall-damage deny`, the corresponding damage must still be blocked. Test allow/deny
  ties, higher priorities, victim/attacker bypass and regions in another dimension. No bypass should
  create immunity outside a region or pierce a victim's configured immunity.
- [ ] Record direct health changes, removal, knockback and potion effects as separate coverage
  questions. Do not claim `invincible` makes every health/death path impossible.
- [ ] With a small test plugin cancelling an event earlier, confirm DimenGuard never uncancels it,
  even for allowed or bypassed actions. DimenGuard uses `High` priority and skips cancelled
  events. Repeat break/place, interaction and damage paths.
- [ ] If another plugin deliberately uncancels a later event, document the interference. Do not
  treat event priority as a guarantee against an incompatible plugin.

## Persistence and recovery

- [ ] Create multiple regions with different dimensions, priorities, members and flags. Stop and
  restart the server normally. Confirm every field and protection decision is restored.
- [ ] Upgrade a disposable copy of an older four-/13-flag database; inherited new granular flags
  must retain old aggregate decisions and `invincible` must add no immunity. Save values from all
  33 flags and reopen them. Do not assume an older plugin can read newer flag names after downgrade;
  keep the pre-upgrade backup.
- [ ] Rename, delete and modify regions, then restart again. Confirm removed flags and members do
  not return. Untrusting an owner does not remove ownership; verify the distinction.
- [ ] On a disposable copy, induce an administrative save failure, for example with a controlled
  database lock. Verify the command reports failure and the old live and stored snapshot survives.
- [ ] Make a disposable database unreadable or invalid before startup. Confirm a clear log error
  and denial of the plugin's basic intercepted actions while storage is unavailable.
- [ ] Repair access and run `/dg reload`, or stop the server, restore a valid backup and restart.
  Confirm normal region decisions resume only after a successful load.
- [ ] Induce a failed reload after a valid snapshot was loaded. Existing protections must remain
  active. Restore the database and reload successfully; verify failed data never replaces live state.
- [ ] On a copy only, try a future schema version, unknown flag or malformed stored region. Confirm
  an actionable failure instead of silent deletion or an empty successful load. Preserve the original
  database backup; do not conduct corruption tests on the production file.

## Messages and load observations

- [ ] Verify English and Spanish client locale detection, unknown-locale English fallback,
  `/dg language en`, `/dg language es`, and reset of that override after disconnect.
- [ ] Check amethyst, white, muted purple-gray and dark-gray rendering on the actual Bedrock client.
  Exercise errors, success messages, region lists and multi-line information in both languages.
- [ ] Repeat denied actions rapidly. Messages should be throttled while every action still receives
  its protection decision; disconnecting must clear transient notification and selection state.
- [ ] Measure command duration and server tick timing with a recorded region count and hardware.
  Compare sparse regions with many overlapping regions. Snapshot saves are synchronous; avoid
  inferring latency guarantees from the 10,000-region limit or from offline test speed.

## Advanced world and movement acceptance

- [ ] Set `explosions deny`; test origins inside/outside and explosions whose affected blocks
  cross the boundary. Verify whole-event cancellation and victim damage separately.
- [ ] Test `fluid-flow` at both source and destination, `block-form` for lava solidification,
  and `leaf-decay` before/after setting inherit. Record instant-ticking gaps separately.
- [ ] With `fluid-flow deny`, set `water-flow allow` and leave `lava-flow` inherited. Verify water
  and lava separately, in still/flowing variants, across both endpoints and region priorities.
  Swap the configured subtype. Custom/unknown types must remain on the aggregate rule.
- [ ] Test reported actor griefing, non-player mob spawning and attributed mob-to-player damage.
  Players/items/projectiles must not be mistakenly removed by `mob-spawning`.
- [ ] Verify independent environmental action rules default to allow, granular liquid rules use
  `fluid-flow`, and `invincible` defaults to deny. Autonomous world/damage decisions must not use
  player membership or operator bypass; player pickup has its separately documented bypass path.
- [ ] Verify `entry`/`exit` on reported moves and same-dimension teleports. Moving within the
  same region must remain possible. Test owners, bypass and overlapping changed region sets.
- [ ] Record portals, respawn, cross-dimension teleports and tiny movement as known incomplete
  entry/exit paths, not successful exclusion tests. There is no automatic corrective teleport.

Complete a runtime record before calling this preview candidate ready for a server's intended protection
requirements. Additional WorldGuard-inspired flags remain planned in
[flag-roadmap.md](flag-roadmap.md), not implemented merely by appearing in that document.
