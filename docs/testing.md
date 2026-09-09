# Testing DimenGuard

Offline tests and Bedrock runtime acceptance are separate checks. The gameplay/client checklist
below is **pending**. Successful startup alone does not establish runtime protection coverage.
Read [event-coverage.md](event-coverage.md) for the source audit and known API limitations.

## Offline checks

Run `./scripts/build.ps1` from the repository root to build the plugin and run CTest. Use
`-EndstoneSource 'C:\path\to\endstone'` for an existing SDK checkout or `-CoreOnly` to omit
the Endstone adapter. The script stops when configuration, compilation or tests fail.

Offline checks exercise inclusive cuboids, negative coordinates, dimension separation, overlaps,
flag precedence, invalid selections and arguments, SQLite roundtrips and rollback, service state
preservation, locale fallback and English/Spanish message parity. They do not reproduce BDS
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

Use a disposable world or a restorable copy. Before testing, record:

| Field | Value |
| --- | --- |
| Date | Pending |
| OS | Pending |
| BDS version | Pending |
| Endstone version and commit | Pending |
| DimenGuard commit | Pending |
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
  `/dg flag spawn`, `/dg language` and `/dg trust spawn`. Region names are entered manually.
- [ ] Confirm `region` appears only once among the root suggestions. Execute every region action,
  not only `help dg`: `list`, `list 1`, `info spawn`, and create/rename/priority/delete on a disposable
  region. Verify missing/extra arguments and invalid integers are rejected without changing data.
- [ ] Run `/dg flag` to see all supported flags and descriptions. `/dg flag spawn` and
  `/dg flag spawn build` must only read stored states; provide a state to change one. Test
  all flag/state suggestions and reject invalid values. The catalog is also available from console.
- [ ] Inspect `/dg help` in English and Spanish: framed heading, categorized command rows and
  no administrative rows for a player without `dimenguard.command`. Verify `DimenGuard >` colors.
- [ ] Verify an owner without `dimenguard.command` cannot administer regions merely by owning one.
  Verify a non-operator without the command permission cannot create, delete or change flags.
- [ ] Verify a player with the default `dimenguard.use` can read help and choose their language
  without receiving administration rights. Check reload from the console and rejection of player-only commands.

## Policy and boundaries

- [ ] With all flags inherited, owners and members can build, interact and access supported
  containers inside a single region; outsiders cannot. PvP is allowed by default.
- [ ] Repeat the outsider actions as the operator without bypass. They must still be denied.
  Explicitly grant bypass, verify access, remove it and verify denial resumes.
- [ ] Set an explicit `allow` and verify outsiders gain that action. Set `deny` and verify owners and
  members lose it. Set `inherit` and verify the documented fallback returns.
- [ ] Create two equal-priority overlapping regions. `deny` must beat `allow`. With both inherited,
  default member access requires membership in both highest-priority regions.
- [ ] Give an overlapping region a higher priority. Its explicit decision must beat lower-priority
  decisions. Set its flag to `inherit`; a lower-priority explicit denial must apply again.
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
- [ ] Test door and bed placement with secondary blocks crossing the boundary. Record any exposed
  target gap as unsupported; do not assume the whole multi-block operation was protected.
- [ ] Fill and empty water, lava and powder-snow buckets at region edges. Test waterlogging,
  cauldrons, supported creature buckets and milking separately, including inventory accounting.
- [ ] Use doors, buttons, levers, signs, lecterns, item frames and other supported targeted actions.
  Check server-side changes as well as displayed screens and animations.
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

## PvP and event cooperation

- [ ] Set `pvp deny`; test melee with only the attacker inside, only the victim inside, and both
  inside. Repeat with both outside as a control. Check attacker and victim dimensions separately.
- [ ] Repeat with arrows and other supported projectile damage. Record attributed shooter,
  damage and knockback as separate observations.
- [ ] Disconnect the shooter before impact. Record any missing attribution; this alpha cannot
  infer a historical player position from an unavailable shooter.
- [ ] Check splash/potion effects, fire and other delayed environmental consequences separately.
  Damage cancellation alone must not be reported as protection of all secondary effects.
- [ ] With a small test plugin cancelling an event earlier, confirm DimenGuard never uncancels it,
  even for allowed or bypassed actions. DimenGuard uses `High` priority and skips cancelled
  events. Repeat break/place, interaction and damage paths.
- [ ] If another plugin deliberately uncancels a later event, document the interference. Do not
  treat event priority as a guarantee against an incompatible plugin.

## Persistence and recovery

- [ ] Create multiple regions with different dimensions, priorities, members and flags. Stop and
  restart the server normally. Confirm every field and protection decision is restored.
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

Complete a runtime record before calling this alpha ready for a server's intended protection
requirements. Advanced world hooks and a third-party plugin API require separate implementation
and verification phases.
