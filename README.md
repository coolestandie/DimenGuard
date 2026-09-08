# DimenGuard

A C++20 region-protection plugin for Endstone, organized by level and dimension.

Development is proceeding in tested phases. The first milestone covers cuboid regions,
explicit protection policies, SQLite persistence and English/Spanish commands.
Advanced world protections and a third-party plugin API are separate future milestones.

The project follows Endstone's C++ naming and formatting conventions. Protection decisions
belong to the domain layer; Endstone listeners adapt events to that shared policy.

## Development

Branches use `type/short-description`; commits use short English conventional messages.
Planning notes and agent instructions are local files excluded from version control.
