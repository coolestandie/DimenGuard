#include "dimenguard/region/region_manager.h"
#include "dimenguard/service/region_service.h"
#include "dimenguard/storage/sqlite_store.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace dimenguard::benchmark {
namespace {

using Clock = std::chrono::steady_clock;

const DimensionKey dimension{"benchmark-level", "minecraft:overworld"};
constexpr std::array<std::size_t, 3> region_counts{100, 1000, 10000};
constexpr std::size_t sparse_query_iterations = 4000;
constexpr std::size_t returned_region_budget = 100000;

enum class Layout {
    Sparse,
    Overlapping,
};

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto parent = std::filesystem::temp_directory_path();
        for (unsigned int attempt = 0; attempt < 16; ++attempt) {
            const auto timestamp = Clock::now().time_since_epoch().count();
            const auto candidate =
                parent / ("dimenguard-benchmark-" + std::to_string(timestamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                path_ = candidate;
                return;
            }
        }
        throw std::runtime_error("Could not create a unique benchmark temporary directory.");
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        if (error) {
            std::cerr << "Could not remove benchmark temporary directory " << path_ << ": " << error.message() << '\n';
        }
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    [[nodiscard]] const std::filesystem::path &getPath() const { return path_; }

private:
    std::filesystem::path path_;
};

BlockPosition plotOrigin(std::size_t index)
{
    return {static_cast<int>(index % 100) * 32 - 1600, -64, static_cast<int>(index / 100) * 32 - 1600};
}

std::vector<Region> makeRegions(std::size_t count, Layout layout)
{
    std::vector<Region> regions;
    regions.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        Region region;
        region.key = {dimension, "plot-" + std::to_string(index)};
        const auto origin = plotOrigin(index);
        region.bounds = layout == Layout::Sparse ? Bounds{origin, {origin.x + 15, 319, origin.z + 15}}
                                                 : Bounds{{-1600, -64, -1600}, {1600, 319, 1600}};
        region.priority = static_cast<int>(index % 13);
        region.owner = "00000000-0000-4000-8000-000000000001";
        region.members = {"00000000-0000-4000-8000-000000000002", "00000000-0000-4000-8000-000000000003"};
        region.flags = {{Flag::Build, FlagState::Deny}, {Flag::Pvp, FlagState::Deny}};
        regions.push_back(std::move(region));
    }
    return regions;
}

std::uint64_t checksum(const std::vector<const Region *> &regions, std::size_t expected_count)
{
    if (regions.size() != expected_count) {
        throw std::runtime_error("A benchmark query returned an unexpected region count.");
    }
    if (regions.empty()) {
        return 0;
    }
    return static_cast<std::uint64_t>(regions.size()) + regions.front()->key.name.size() +
           regions.back()->key.name.size() + static_cast<std::uint64_t>(regions.front()->priority);
}

template <typename Operation>
void measure(std::string_view scenario, std::size_t count, std::size_t iterations, Operation &&operation)
{
    std::uint64_t result = 0;
    const auto start = Clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        result += operation(iteration);
    }
    const auto elapsed = Clock::now() - start;
    const auto total_ms = std::chrono::duration<double, std::milli>(elapsed).count();
    const auto microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::cout << scenario << ',' << count << ',' << iterations << ',' << std::fixed << std::setprecision(3) << total_ms
              << ',' << microseconds / static_cast<double>(iterations) << ',' << result << '\n';
}

void measureIndex(std::size_t count, const std::vector<Region> &sparse)
{
    RegionManager sparse_manager;
    measure("index_build_sparse", count, 3, [&](std::size_t) {
        sparse_manager.replaceAll(sparse);
        return sparse_manager.getAll().size();
    });

    measure("sparse_query_hit", count, sparse_query_iterations, [&](std::size_t iteration) {
        const auto origin = plotOrigin(iteration % count);
        return checksum(sparse_manager.query(dimension, {origin.x + 8, 64, origin.z + 8}), 1);
    });
    measure("sparse_query_miss", count, sparse_query_iterations, [&](std::size_t iteration) {
        const auto origin = plotOrigin(iteration % count);
        return checksum(sparse_manager.query(dimension, {origin.x + 20, 64, origin.z + 20}), 0);
    });

    const auto list_iterations = std::max(std::size_t{10}, returned_region_budget / count);
    measure("dimension_name_list", count, list_iterations,
            [&](std::size_t) { return checksum(sparse_manager.inDimension(dimension), count); });

    const auto overlapping = makeRegions(count, Layout::Overlapping);
    RegionManager dense_manager;
    measure("index_build_overlapping", count, 3, [&](std::size_t) {
        dense_manager.replaceAll(overlapping);
        return dense_manager.getAll().size();
    });
    measure("overlapping_query", count, list_iterations,
            [&](std::size_t) { return checksum(dense_manager.query(dimension, {0, 64, 0}), count); });
}

void measureAdministrativeSave(std::size_t count, const std::vector<Region> &regions,
                               const std::filesystem::path &directory)
{
    const auto path = directory / ("regions-" + std::to_string(count) + ".sqlite3");
    {
        SqliteStore store(path);
        store.save(regions);
    }
    const auto &key = regions.front().key;
    constexpr int changed_priority = 1001;
    {
        RegionService service(path);
        measure("full_snapshot_priority_change", count, 1, [&](std::size_t) {
            service.setPriority(key, changed_priority);
            const auto *updated = service.getRegions().find(key);
            if (!updated || updated->priority != changed_priority) {
                throw std::runtime_error("The administrative mutation was not published to the live snapshot.");
            }
            return static_cast<std::uint64_t>(updated->priority) + service.getRegions().getAll().size();
        });
    }
    RegionService reopened(path);
    const auto *persisted = reopened.getRegions().find(key);
    if (!persisted || persisted->priority != changed_priority || reopened.getRegions().getAll().size() != count) {
        throw std::runtime_error("The administrative mutation was not preserved when reopening storage.");
    }
}

}

void run()
{
    const TemporaryDirectory temporary;
    std::cout << "scenario,regions,iterations,total_ms,us_per_operation,checksum\n";
    for (const auto count : region_counts) {
        const auto sparse = makeRegions(count, Layout::Sparse);
        measureIndex(count, sparse);
        measureAdministrativeSave(count, sparse, temporary.getPath());
    }
}

}

int main()
{
    try {
        dimenguard::benchmark::run();
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << "DimenGuard benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
