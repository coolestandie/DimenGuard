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
