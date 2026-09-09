#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace dimenguard::storage {

inline constexpr std::size_t max_regions = 10'000;

class SnapshotLimitError : public std::runtime_error {
public:
    SnapshotLimitError()
        : std::runtime_error("The region snapshot exceeds the supported limit of " + std::to_string(max_regions) +
                             " regions.")
    {
    }
};

}
