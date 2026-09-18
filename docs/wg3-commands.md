# WG-3 command administration

WG-3 expands DimenGuard's region administration surface while keeping the existing
dimension-scoped model and server-side validation. The command catalog is the single
source for native usage metadata, client suggestions and the localized `/dg help` panel.

## Player commands

An in-game command always targets the level and dimension of the executing player:

```text
/dg pos1
/dg pos2
/dg inspect
/dg region create <name>
/dg region claim <name>
/dg region delete <name> [confirm]
/dg region rename <region> <new-name>
/dg region redefine <region>
/dg region move <region> <x> <y> <z>
/dg region list [page]
/dg region info <region>
/dg region flags <region>
/dg region priority <region> <priority>
/dg region set-parent <region> <parent|none>
/dg region set-passthrough <region> <allow|deny|inherit>
/dg region set-flag <region> <flag> <value>
/dg region unset-flag <region> <flag>
/dg region select <region>
```

`create` and `claim` both create a cuboid from the two-corner selection. `define` is an
alias for `create`; `remove` is an alias for `delete`; and `set-priority` is an alias for
`priority`. Region names are validated by the service, so numeric names are preserved
instead of being rejected by the native `str` parser. Region IDs still use the documented
lowercase letters, digits, underscores and hyphens. Quote a whole value when it contains
spaces, for example:

```text
/dg region rename spawn new-spawn
/dg region set-flag spawn entry-deny-message "This area is private."
```

The current parser accepts balanced quotes without escape sequences and rejects control
characters, embedded quotes and excess arguments. Typed flag values use the same registry
as `/dg flag`; `--unset` and state `inherit` clear an explicit value. The `region flags`
and `region info` forms never mutate a region.

## Selection and inspection

`pos1` and `pos2` record the block under the player's feet in the current dimension.
`/dg inspect` reports every matching region at that block, including kind and priority,
without changing the selection. `/dg region select <region>` loads a cuboid's stored bounds
into the player's selection so it can be reviewed or redefined. Global and template regions
cannot be selected as physical cuboids.

Creating, redefining or moving a region reports overlapping physical regions after the
validated mutation. The warning is diagnostic only; intentional overlaps remain valid and are
resolved by the existing priority and flag policy.

## Safe deletion and hierarchy edits

Deleting a region with direct or indirect children is intentionally two-step:

```text
/dg region delete parent
/dg region delete parent confirm
```

The first form makes no change and explains the required confirmation. The confirmed form
deletes the complete child tree in one validated, transactional snapshot. Deleting a leaf
continues to use the single-argument form. Parent assignment accepts `none` (or `-`) to
clear the link; service validation rejects missing parents, cycles, cross-dimension links,
excessive depth and invalid priority relationships.

`move` translates every bound by checked 32-bit block offsets. If an offset would overflow,
the command fails before the active snapshot changes. `redefine` and `move` report conflicts
using the final bounds, while `rename` updates direct child links atomically.

## Console targeting

Region commands from the server console require an explicit level and dimension immediately
after the action:

```text
/dg region list survival minecraft:overworld 2
/dg region info survival minecraft:overworld spawn
/dg region set-flag survival minecraft:overworld spawn pvp deny
/dg region delete survival minecraft:overworld parent confirm
```

The two target tokens are removed before normal action parsing, so region names, numbers,
confirmation and typed values keep the same validation as player commands. Console commands
do not have a player selection; selection-based `create`, `claim`, `redefine` and `select`
therefore return the localized player-only error. Membership commands still require an in-game
sender because they resolve the target by an online player's exact name.

The console target is resolved against the server's loaded level and dimensions and does not
silently fall back to an operator's location. An unknown level or dimension is rejected before
the region service is touched. Use `/dg region list <level> <dimension>` first when operating
on stored data from the console, and verify the exact custom-dimension identifier used by the
server during runtime acceptance.

## Permissions and limits

WG-3 retains the existing permission boundary: `dimenguard.command` is required for region,
selection, flag mutation and membership commands. `dimenguard.use` remains sufficient for
help, language selection and flag discovery. Least-privilege administration nodes, per-region
ownership checks and player claiming are planned for WG-4; `claim` is therefore currently an
administrative synonym for `create`.

The service enforces the 10,000-region snapshot limit and persists every successful mutation
transactionally. Failed validation or storage writes leave the previous live snapshot intact.
No command performs background work or loads chunks, and no command promises protection for
unavailable Endstone event paths.

## Autocomplete

On protocol 2169, DimenGuard publishes canonical region actions plus `define`, `remove` and
`set-priority` aliases through the public command-suggestion packet. The same packet includes
the registered flag names, state choices, languages, online player names and existing region
names for the player's current dimension. Region names remain a free-text parameter so new
numeric names can be entered. Suggestions refresh after successful region-name mutations,
reloads and dimension changes; unsupported packet versions pass through unchanged.
