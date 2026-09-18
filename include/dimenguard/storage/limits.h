#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace dimenguard::storage {

inline constexpr std::size_t max_regions = 10'000;
inline constexpr std::uint64_t max_cuboid_volume = 16'777'216;
inline constexpr std::uint64_t max_claim_volume = 1'048'576;
inline constexpr std::size_t max_claimed_regions = 64;

class SnapshotLimitError : public std::runtime_error {
public:
    SnapshotLimitError()
        : std::runtime_error("The region snapshot exceeds the supported limit of " + std::to_string(max_regions) +
                             " regions.")
    {
    }
};

}
