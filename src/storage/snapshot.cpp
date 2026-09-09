#include "dimenguard/storage/snapshot.h"

#include "dimenguard/storage/limits.h"
#include "dimenguard/storage/sqlite.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace dimenguard::storage {
namespace {

// A new registry entry must never reinterpret data written by an older schema.
constexpr auto legacy_flags = std::to_array<std::string_view>({"build",
                                                               "interact",
                                                               "container-access",
                                                               "pvp",
                                                               "explosions",
                                                               "fluid-flow",
                                                               "block-form",
                                                               "leaf-decay",
                                                               "actor-griefing",
                                                               "mob-spawning",
                                                               "mob-damage",
                                                               "entry",
                                                               "exit",
                                                               "block-break",
                                                               "block-place",
                                                               "use",
                                                               "use-anvil",
                                                               "sleep",
                                                               "item-drop",
                                                               "item-pickup",
                                                               "send-chat",
                                                               "water-flow",
                                                               "lava-flow",
                                                               "fall-damage",
                                                               "firework-damage",
                                                               "invincible"});

std::optional<Flag> parseStoredFlag(std::string_view name, int schema_version)
{
    if (schema_version < 3 && std::ranges::find(legacy_flags, name) == legacy_flags.end()) {
        return std::nullopt;
    }
    return parseFlag(name);
}

struct LoadedSnapshot {
    std::vector<Region> regions;
    std::map<std::int64_t, std::size_t> row_ids;

    Region &find(std::int64_t id)
    {
        const auto found = row_ids.find(id);
        if (found == row_ids.end()) {
            throw std::runtime_error("The database contains a member or flag referencing a missing region.");
        }
        return regions.at(found->second);
    }
};

std::size_t countRegions(const sqlite::Connection &connection)
{
    sqlite::Statement query(connection, "SELECT COUNT(*) FROM regions");
    if (!query.next()) {
        throw std::runtime_error("Could not count stored regions.");
    }
    const auto count = query.integer(0);
    if (count < 0) {
        throw std::runtime_error("The stored region count is invalid.");
    }
    if (static_cast<std::uint64_t>(count) > max_regions) {
        throw SnapshotLimitError{};
    }
    return static_cast<std::size_t>(count);
}

LoadedSnapshot readRegions(const sqlite::Connection &connection, int schema_version)
{
    LoadedSnapshot snapshot;
    snapshot.regions.reserve(countRegions(connection));
    const auto *sql = schema_version == 1
                        ? "SELECT id, level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, "
                          "priority, owner FROM regions ORDER BY level, dimension, name"
                        : "SELECT id, level, dimension, name, min_x, min_y, min_z, max_x, max_y, max_z, "
                          "priority, owner, kind, parent, passthrough FROM regions ORDER BY level, dimension, name";
    sqlite::Statement query(connection, sql);
    while (query.next()) {
        Region region;
        region.key = {{query.text(1), query.text(2)}, query.text(3)};
        region.bounds = {{query.integer32(4), query.integer32(5), query.integer32(6)},
                         {query.integer32(7), query.integer32(8), query.integer32(9)}};
        region.priority = query.integer32(10);
        region.owner = query.text(11);
        if (schema_version >= 2) {
            const auto kind_name = query.text(12);
            const auto kind = parseRegionKind(kind_name);
            const auto passthrough_name = query.text(14);
            const auto passthrough = parseState(passthrough_name);
            if (!kind || !passthrough) {
                throw std::runtime_error("Unsupported stored region kind '" + kind_name + "' or passthrough '" +
                                         passthrough_name + "'.");
            }
            region.kind = *kind;
            region.parent = query.optionalText(13);
            region.passthrough = *passthrough;
        }
        if (!snapshot.row_ids.emplace(query.integer(0), snapshot.regions.size()).second) {
            throw std::runtime_error("The database contains duplicate region identifiers.");
        }
        snapshot.regions.push_back(std::move(region));
    }
    return snapshot;
}

void readMembers(const sqlite::Connection &connection, LoadedSnapshot &snapshot)
{
    sqlite::Statement query(connection, "SELECT region_id, identity FROM members");
    while (query.next()) {
        if (!snapshot.find(query.integer(0)).members.insert(query.text(1)).second) {
            throw std::runtime_error("The database contains a duplicate region member.");
        }
    }
}

void readFlags(const sqlite::Connection &connection, LoadedSnapshot &snapshot, int schema_version)
{
    const auto *sql = schema_version < 3 ? "SELECT region_id, name, state FROM flags"
                                         : "SELECT region_id, name, value, type FROM flags";
    sqlite::Statement query(connection, sql);
    while (query.next()) {
        const auto name = query.text(1, 64);
        const auto flag = parseStoredFlag(name, schema_version);
        if (!flag) {
            throw std::runtime_error("Unsupported stored flag '" + name + "'.");
        }
        const auto type = schema_version < 3 ? std::optional{FlagType::State} : parseFlagType(query.text(3, 16));
        if (!type || *type != flagType(*flag)) {
            throw std::runtime_error("Invalid stored type for flag '" + name + "'.");
        }
        const auto value = parseFlagValue(*flag, query.text(2, maximum_encoded_flag_bytes));
        if (!value) {
            throw std::runtime_error("Invalid stored value for flag '" + name + "'.");
        }
        if (!snapshot.find(query.integer(0)).flags.emplace(*flag, *value).second) {
            throw std::runtime_error("The database contains a duplicate region flag.");
        }
    }
}

void readGroups(const sqlite::Connection &connection, LoadedSnapshot &snapshot, int schema_version)
{
    sqlite::Statement query(connection, "SELECT region_id, name, group_name FROM flag_groups");
    while (query.next()) {
        const auto name = query.text(1, 64);
        const auto group_name = query.text(2, 16);
        const auto flag = parseStoredFlag(name, schema_version);
        const auto group = parseRegionGroup(group_name);
        if (!flag || !group) {
            throw std::runtime_error("Unsupported stored group '" + group_name + "' or flag '" + name + "'.");
        }
        auto &region = snapshot.find(query.integer(0));
        if (!region.flag_groups.emplace(*flag, *group).second) {
            throw std::runtime_error("The database contains a duplicate flag group.");
        }
    }
}

}

std::vector<Region> readSnapshot(const sqlite::Connection &connection, int schema_version)
{
    if (schema_version != 1 && schema_version != 2 && schema_version != 3) {
        throw std::runtime_error("The snapshot format is not supported.");
    }
    auto snapshot = readRegions(connection, schema_version);
    readMembers(connection, snapshot);
    readFlags(connection, snapshot, schema_version);
    if (schema_version >= 2) {
        readGroups(connection, snapshot, schema_version);
    }
    validateRegions(snapshot.regions);
    return std::move(snapshot.regions);
}

void writeSnapshot(const sqlite::Connection &connection, const std::vector<Region> &regions)
{
    sqlite::Statement insert_region(connection,
                                    "INSERT INTO regions (level, dimension, name, min_x, min_y, min_z, max_x, "
                                    "max_y, max_z, priority, owner, kind, parent, passthrough) "
                                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    sqlite::Statement insert_member(connection, "INSERT INTO members (region_id, identity) VALUES (?, ?)");
    sqlite::Statement insert_flag(connection, "INSERT INTO flags (region_id, name, type, value) VALUES (?, ?, ?, ?)");
    sqlite::Statement insert_group(connection,
                                   "INSERT INTO flag_groups (region_id, name, group_name) VALUES (?, ?, ?)");
    for (const auto &region : regions) {
        insert_region.bind(1, region.key.dimension.level);
        insert_region.bind(2, region.key.dimension.dimension);
        insert_region.bind(3, region.key.name);
        insert_region.bind(4, region.bounds.min.x);
        insert_region.bind(5, region.bounds.min.y);
        insert_region.bind(6, region.bounds.min.z);
        insert_region.bind(7, region.bounds.max.x);
        insert_region.bind(8, region.bounds.max.y);
        insert_region.bind(9, region.bounds.max.z);
        insert_region.bind(10, region.priority);
        insert_region.bind(11, region.owner);
        insert_region.bind(12, regionKindName(region.kind));
        if (region.parent) {
            insert_region.bind(13, *region.parent);
        }
        else {
            insert_region.bindNull(13);
        }
        insert_region.bind(14, stateName(region.passthrough));
        insert_region.run();
        const auto id = connection.lastInsertId();
        for (const auto &member : region.members) {
            insert_member.bind(1, id);
            insert_member.bind(2, member);
            insert_member.run();
        }
        for (const auto &[flag, value] : region.flags) {
            insert_flag.bind(1, id);
            insert_flag.bind(2, flagName(flag));
            insert_flag.bind(3, valueTypeName(flagType(flag)));
            insert_flag.bind(4, formatFlagValue(value));
            insert_flag.run();
        }
        for (const auto &[flag, group] : region.flag_groups) {
            insert_group.bind(1, id);
            insert_group.bind(2, flagName(flag));
            insert_group.bind(3, regionGroupName(group));
            insert_group.run();
        }
    }
}

}
