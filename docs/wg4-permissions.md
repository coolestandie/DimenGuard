# WG-4 permissions and ownership

WG-4 replaces command handlers' single administrative check with explicit permission nodes while
keeping protection bypass independent. The legacy `dimenguard.command` node remains a compatibility
aggregate for existing administrators; new installations can grant only the nodes they need.

## Permission layers

The root `dimenguard.use` permission only exposes help, language selection and public flag discovery.
Region commands use one of these operation nodes:

| Operation | Permission |
| --- | --- |
| Selection | `dimenguard.region.selection` |
| Inspect | `dimenguard.region.inspect` |
| Create | `dimenguard.region.create` |
| Claim | `dimenguard.region.claim` |
| Delete | `dimenguard.region.delete` |
| Rename | `dimenguard.region.rename` |
| Redefine | `dimenguard.region.redefine` |
| Move | `dimenguard.region.move` |
| List | `dimenguard.region.list` |
| Info | `dimenguard.region.info` |
| Flags and flag values | `dimenguard.region.flags` |
| Priority | `dimenguard.region.priority` |
| Parent | `dimenguard.region.parent` |
| Passthrough | `dimenguard.region.passthrough` |
| Membership | `dimenguard.region.membership` |
| Ownership transfer | `dimenguard.region.ownership` |
| Recovery/reload | `dimenguard.recovery` |

Operation nodes default to false. `dimenguard.command` is an operator-default compatibility
aggregate that grants the operation children; it does not grant `dimenguard.bypass`.

## Owner and member scopes

Every operation node also has `.own` and `.member` scopes. They apply only when the executing player
is the region's UUID owner or an explicit member, respectively. A region-specific override can be
attached as `dimenguard.region.<operation>.own.<region>` or
`dimenguard.region.<operation>.member.<region>`. Explicit dynamic permission attachments are checked
only when present, so an operator's default permission cannot accidentally grant an unknown region node.

Owner/member scopes are never protection bypasses. An owner can still be denied by an explicit region
flag, and a member is not automatically allowed to administer membership or flags without the relevant
operation node.

## Flags

Administrators with `dimenguard.region.flags` can set any registered flag. Scoped flag editing requires
the region scope plus the flag node, for example:

```text
dimenguard.region.flags.own
dimenguard.region.flag.pvp
dimenguard.region.flag.pvp.deny
```

State values have `allow`, `deny`, `inherit` and `unset` nodes. Typed non-state values use the generic
`set` node. `dimenguard.region.flag.*` is an explicit all-flag grant and still requires the region
operation scope. Reads use the region flags operation but do not require a value-specific grant.

## Claims and limits

`/dg region claim <name>` is no longer an administrative synonym for create. It requires
`dimenguard.region.claim`, assigns the executing player's UUID as owner and rejects physical overlap
with every existing cuboid in the same level and dimension.

The service enforces these bounds before writing a snapshot:

- 10,000 regions in one database;
- 16,777,216 blocks for an administrator-created cuboid;
- 1,048,576 blocks for a player claim;
- 64 claimed regions per owner;
- 32 parent levels, including inherited templates.

Claimed regions cannot overlap another physical region. Global and template regions do not consume
claim volume and are not physical claim conflicts. Redefinition through an owner-scoped permission
uses the claim volume limit; a global operation permission may use the administrator cuboid limit.

`/dg region set-owner <region> <player>` transfers the stored UUID owner to an online player and
preserves members and flags. The command requires the ownership operation globally or through its
owner/member scope. Recovery remains separate from protection bypass and is intended for explicit
administrative repair.

## Current boundaries

Teleport permission is reserved for the future region-teleport command and is not advertised as an
implemented command yet. Runtime acceptance of player claims, permission attachments and client
completion remains part of the disposable-server checklist; offline tests do not prove a live
permission provider's configuration.
