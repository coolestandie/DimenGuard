# Offline region benchmark

`region_benchmarks.cpp` is a standalone executable linked to `dimenguard_services`. It uses
only standard C++ and existing project dependencies. It does not start Endstone or access
server data. Each run creates its own temporary directory and removes it on completion.

The executable emits CSV to standard output:

```text
scenario,regions,iterations,total_ms,us_per_operation,checksum
```

It evaluates 100, 1,000 and 10,000 regions. Sparse regions are disjoint 16-by-16 plots with
gaps between them. The overlapping dataset gives every region the same bounds and varies
their priorities. Each region has two members and two explicit flags.

WG-1 also measures a 32-region inheritance chain: 31 shared templates plus the remaining
disjoint cuboids, with the same total region count. The root template owns the queried player
and has `pvp deny` scoped to nonmembers. Cuboids have no local flags. These scenarios time the
complete policy path, including spatial matching and inherited ownership or group evaluation.

| Scenario | Measured work |
| --- | --- |
| `index_build_sparse` | Copy, validate and index a sparse snapshot through `RegionManager::replaceAll()`. |
| `sparse_query_hit` | Resolve one plot and return its matching region. |
| `sparse_query_miss` | Query a gap outside all plots. |
| `dimension_name_list` | Return all regions in one dimension, ordered by name. |
| `index_build_overlapping` | Copy, validate and index a fully overlapping snapshot. |
| `overlapping_query` | Return all overlapping regions in priority/name order. |
| `deep_inherited_membership` | Allow building through an owner inherited from the root of a 32-region chain. |
| `deep_inherited_group` | Exclude that inherited owner from the root's nonmember PvP denial. |
| `full_snapshot_priority_change` | Change a priority through `RegionService`, including candidate construction, SQLite transaction and live-state publication. |

Build measurements repeat three times. Sparse queries repeat 4,000 times; dense queries and
dimension listings return about 100,000 region references in total per dataset size. The
administrative write runs once per size. Storage initialization and final reopen verification
are outside the timed write. The process fails if a query count or persisted update is wrong.
Printed checksums consume the query results and updates.

Each hierarchy scenario performs 4,000 queries and verifies that every inherited decision allows
the ancestor owner. A wrong decision fails the executable rather than producing a timing result.

Measurements have no pass/fail latency threshold. Record the build configuration, compiler,
machine and storage alongside the CSV, and repeat runs when comparing a change. These
numbers exclude Bedrock event dispatch, other plugins, network traffic and real player activity;
they do not establish server tick or production performance guarantees.

## Optional build integration

The intended target name is `dimenguard_benchmarks`. It should be opt-in and excluded from
CTest's correctness suite:

```cmake
option(DIMENGUARD_BUILD_BENCHMARKS "Build offline performance benchmarks" OFF)
if (DIMENGUARD_BUILD_BENCHMARKS)
    add_executable(dimenguard_benchmarks benchmarks/region_benchmarks.cpp)
    target_link_libraries(dimenguard_benchmarks PRIVATE dimenguard_services)
    dimenguard_warnings(dimenguard_benchmarks)
endif ()
```

Configure with `-DDIMENGUARD_BUILD_BENCHMARKS=ON`, build the target in `RelWithDebInfo` or
`Release`, and run `build/dimenguard_benchmarks.exe` on Windows. A build-script switch can
forward that CMake option; benchmark execution should remain an explicit step.
