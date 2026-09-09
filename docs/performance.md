# Offline performance baseline

One local run on 2026-09-08: Windows x64, AMD Ryzen 5 5600G, Clang 19.1.5,
RelWithDebInfo, C++20, release shared CRT. See [benchmark methodology](../benchmarks/README.md).
The benchmark verifies result counts and durable writes; it does not run Bedrock.

| Operation | 100 regions | 1,000 regions | 10,000 regions |
| --- | ---: | ---: | ---: |
| Sparse matching query | 0.178 us | 0.186 us | 0.225 us |
| Query matching every region | 4.755 us | 91.301 us | 1,181.100 us |
| Full dimension name list | 2.279 us | 17.084 us | 290.000 us |
| Full administrative snapshot change | 6.121 ms | 18.967 ms | 195.225 ms |

Sparse lookups avoid scanning all regions, but genuinely overlapping regions must be examined.
Administrative saves are synchronous and currently replace a full snapshot: the measured
10,000-region write would be noticeable on a server thread. The region limit is a safety bound,
not a recommended deployment size or latency guarantee. Optimizing administrative persistence
is separate from the event-query path.

Repeat measurements on deployment hardware and record real server tick timing during final
acceptance. Do not convert these offline numbers into TPS guarantees.

## WG-1 model measurements

A later local run on 2026-09-08 used the upstream-pinned Windows x64 Clang 19.1.5
RelWithDebInfo build with the WG-1 hierarchy. These are single-run observations, including
storage variability, not a controlled before/after benchmark or a server latency guarantee.

| Operation | 100 regions | 1,000 regions | 10,000 regions |
| --- | ---: | ---: | ---: |
| Sparse matching query | 0.510 us | 0.355 us | 0.426 us |
| Query matching every region | 5.783 us | 88.253 us | 1,495.350 us |
| Full dimension name list | 2.935 us | 17.401 us | 207.110 us |
| Full policy: 32-level inherited membership | 4.692 us | 4.780 us | 4.911 us |
| Full policy: 32-level inherited group | 4.037 us | 2.842 us | 3.247 us |
| Full administrative snapshot change | 33.381 ms | 91.791 ms | 685.941 ms |

The deep-chain dataset contains 31 shared templates and the remaining cuboids, up to 10,000
total regions. Parent indices are compiled with the snapshot; queries walk indices without
constructing region keys or searching names for each ancestor. In the initial WG-1 measurement,
before that change, the 10,000-region deep membership/group cases took 98.667/41.045 us.
Both runs validated every result; timing variation means these samples do not establish an
exact speedup ratio.

Full-snapshot administrative saves remain synchronous and expensive at the maximum count.
The new hierarchy does not remove that existing storage limitation. These timings exclude
Bedrock event dispatch and do not measure a live server or deeply overlapping parent chains.

## WG-2 regression sample

On 2026-09-09 the same Windows x64 Clang 19.1.5 upstream-pinned build passed the
benchmark's result checks after typed-value and subject integration. At 10,000 regions,
the sparse query measured 0.438 us, the 32-level inherited membership decision 5.002 us,
and inherited group decision 3.433 us. A full administrative snapshot change took
384.074 ms. These are single-run samples, not a controlled performance comparison.

This benchmark does not measure per-block explosion filtering, actor additions or message
delivery inside BDS. Typed sets and domain queries still need realistic runtime measurements.
The synchronous full-snapshot administrative write remains a documented limitation.
