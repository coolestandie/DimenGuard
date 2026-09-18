#include "dimenguard/protocol/command_suggestions.h"

#include "dimenguard/command/catalog.h"
#include "dimenguard/protocol/binary_stream.h"
#include "dimenguard/region/region.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

using protocol::BinaryError;
using protocol::BinaryReader;
using protocol::BinaryWriter;

constexpr std::size_t max_payload = 8 * 1024 * 1024;
constexpr std::size_t max_entries = 65536;
constexpr std::size_t max_string = 65536;
constexpr std::uint32_t valid_symbol = 0x00100000;
constexpr std::uint32_t enum_symbol = valid_symbol | 0x00200000;
constexpr std::uint32_t soft_enum_symbol = valid_symbol | 0x04000000;
constexpr std::string_view region_enum_name = "DimenGuardRegion";
constexpr std::string_view free_name_enum_name = "DimenGuardRegionName";

struct HardEnum {
    std::string name;
    std::vector<std::uint32_t> values;
};

struct CommandData {
    std::string_view name;
    std::string_view prefix;
    std::string_view encoded;
};

struct SoftEnum {
    std::string_view name;
    std::string_view encoded;
};

struct CommandPacket {
    std::vector<std::string> values;
    std::string_view chained_values;
    std::string_view postfixes;
    std::vector<HardEnum> enums;
    std::string_view chained_commands;
    std::vector<CommandData> commands;
    std::vector<SoftEnum> soft_enums;
    std::string_view constraints;
};

template <typename Function>
std::string_view readSection(BinaryReader &reader, std::string_view payload, Function read)
{
    const auto start = reader.position();
    read();
    return payload.substr(start, reader.position() - start);
}

std::size_t skipStrings(BinaryReader &reader)
{
    const auto count = reader.readCount(max_entries);
    for (std::size_t i = 0; i < count; ++i) {
        static_cast<void>(reader.readString(max_string));
    }
    return count;
}

void checkIndex(std::uint32_t index, std::size_t size)
{
    if (index >= size) {
        throw BinaryError{};
    }
}

void readOverloads(BinaryReader &reader)
{
    const auto overload_count = reader.readCount(250, 2);
    for (std::size_t i = 0; i < overload_count; ++i) {
        static_cast<void>(reader.readBoolean());
        const auto parameter_count = reader.readCount(max_entries, 7);
        for (std::size_t j = 0; j < parameter_count; ++j) {
            static_cast<void>(reader.readString(512));
            static_cast<void>(reader.readUint32());
            static_cast<void>(reader.readBoolean());
            static_cast<void>(reader.readByte());
        }
    }
}

CommandPacket readPacket(std::string_view payload)
{
    // Protocol 2169 uses fixed little-endian enum indices and a string permission level.
    // Schema: EndstoneMC/protocol-docs, r26_u4, packets/AvailableCommandsPacket.json and its types.
    BinaryReader reader(payload, max_payload);
    CommandPacket packet;
    const auto value_count = reader.readCount(max_entries);
    for (std::size_t i = 0; i < value_count; ++i) {
        packet.values.emplace_back(reader.readString(max_string));
    }
    std::size_t chained_value_count = 0;
    packet.chained_values = readSection(reader, payload, [&] { chained_value_count = skipStrings(reader); });
    packet.postfixes = readSection(reader, payload, [&] { skipStrings(reader); });

    const auto enum_count = reader.readCount(max_entries, 2);
    for (std::size_t i = 0; i < enum_count; ++i) {
        HardEnum entry{std::string(reader.readString(1000)), {}};
        const auto count = reader.readCount(max_entries, 4);
        for (std::size_t j = 0; j < count; ++j) {
            const auto value = reader.readUint32();
            checkIndex(value, packet.values.size());
            entry.values.push_back(value);
        }
        packet.enums.push_back(std::move(entry));
    }

    std::size_t chained_count = 0;
    packet.chained_commands = readSection(reader, payload, [&] {
        chained_count = reader.readCount(16, 2);
        for (std::size_t i = 0; i < chained_count; ++i) {
            static_cast<void>(reader.readString(1000));
            const auto count = reader.readCount(max_entries, 2);
            for (std::size_t j = 0; j < count; ++j) {
                checkIndex(reader.readVarUint32(), chained_value_count);
                static_cast<void>(reader.readVarUint32());
            }
        }
    });

    constexpr std::array permissions{"any", "gamedirectors", "admin", "host", "owner", "internal"};
    const auto command_count = reader.readCount(max_entries, 11);
    for (std::size_t i = 0; i < command_count; ++i) {
        CommandData command;
        command.encoded = readSection(reader, payload, [&] {
            command.prefix = readSection(reader, payload, [&] {
                command.name = reader.readString(512);
                static_cast<void>(reader.readString(1000));
                static_cast<void>(reader.readUint16());
                const auto permission = reader.readString(32);
                if (std::ranges::find(permissions, permission) == permissions.end()) {
                    throw BinaryError{};
                }
                const auto alias = reader.readUint32();
                if (alias != 0xffffffff) {
                    checkIndex(alias, packet.enums.size());
                }
                const auto count = reader.readCount(250, 4);
                for (std::size_t j = 0; j < count; ++j) {
                    checkIndex(reader.readUint32(), chained_count);
                }
            });
            readOverloads(reader);
        });
        packet.commands.push_back(command);
    }

    const auto soft_count = reader.readCount(max_entries, 2);
    for (std::size_t i = 0; i < soft_count; ++i) {
        SoftEnum entry;
        entry.encoded = readSection(reader, payload, [&] {
            entry.name = reader.readString(1000);
            skipStrings(reader);
        });
        packet.soft_enums.push_back(entry);
    }
    packet.constraints = readSection(reader, payload, [&] {
        const auto count = reader.readCount(max_entries, 9);
        for (std::size_t i = 0; i < count; ++i) {
            checkIndex(reader.readUint32(), packet.values.size());
            checkIndex(reader.readUint32(), packet.enums.size());
            const auto size = reader.readCount(max_entries);
            static_cast<void>(reader.readBytes(size));
        }
    });
    if (reader.remaining() != 0) {
        throw BinaryError{};
    }
    return packet;
}

class ClientGrammar {
public:
    explicit ClientGrammar(CommandPacket &packet) : packet_(packet)
    {
        for (const auto &entry : packet_.enums) {
            names_.insert(entry.name);
        }
    }

    std::string build()
    {
        BinaryWriter writer(max_payload);
        const auto catalog = clientCommandCatalog();
        writer.writeCount(catalog.size(), 250);
        for (std::size_t i = 0; i < catalog.size(); ++i) {
            const auto &command = catalog[i];
            auto path = command.path;
            const auto literal_count = static_cast<std::size_t>(std::ranges::count(path, ' ')) + 1;
            writer.writeBoolean(false);
            writer.writeCount(literal_count + command.parameters.size(), max_entries);
            while (!path.empty()) {
                const auto space = path.find(' ');
                const auto literal = path.substr(0, space);
                writeParameter(writer, literal, literalSymbol(literal), command.optional_path, 1);
                path = space == path.npos ? std::string_view{} : path.substr(space + 1);
            }
            for (const auto &parameter : command.parameters) {
                const auto symbol = parameterSymbol(parameter, i);
                writeParameter(writer, parameter.name, symbol, parameter.optional,
                               parameter.kind == ParameterKind::Choice ? 1 : 0);
            }
        }
        return std::move(writer).finish();
    }

private:
    static void writeParameter(BinaryWriter &writer, std::string_view name, std::uint32_t symbol, bool optional,
                               std::uint8_t options)
    {
        writer.writeString(name);
        writer.writeUint32(symbol);
        writer.writeBoolean(optional);
        writer.writeByte(options);
    }

    std::uint32_t addEnum(std::string name, std::span<const std::string_view> choices)
    {
        const auto base = name;
        for (std::size_t suffix = 1; !names_.insert(name).second; ++suffix) {
            name = base + "_" + std::to_string(suffix);
        }
        HardEnum entry{std::move(name), {}};
        for (const auto choice : choices) {
            auto found = std::ranges::find(packet_.values, choice);
            if (found == packet_.values.end()) {
                if (packet_.values.size() >= max_entries) {
                    throw BinaryError{};
                }
                packet_.values.emplace_back(choice);
                found = std::prev(packet_.values.end());
            }
            entry.values.push_back(static_cast<std::uint32_t>(found - packet_.values.begin()));
        }
        if (packet_.enums.size() >= max_entries) {
            throw BinaryError{};
        }
        const auto symbol = enum_symbol | static_cast<std::uint32_t>(packet_.enums.size());
        packet_.enums.push_back(std::move(entry));
        return symbol;
    }

    std::uint32_t literalSymbol(std::string_view literal)
    {
        const auto [entry, inserted] = literals_.try_emplace(std::string(literal), 0);
        if (inserted) {
            const std::array choices{literal};
            entry->second = addEnum("DimenGuardClientLiteral_" + std::string(literal), choices);
        }
        return entry->second;
    }

    std::uint32_t parameterSymbol(const CommandParameter &parameter, std::size_t command_index)
    {
        switch (parameter.kind) {
        case ParameterKind::Word:
            return soft_enum_symbol |
                   static_cast<std::uint32_t>(packet_.soft_enums.size() + (parameter.name == "region" ? 0 : 1));
        case ParameterKind::Integer:
            return valid_symbol | 1;
        case ParameterKind::Player:
            return valid_symbol | 8;
        case ParameterKind::Message:
            return valid_symbol | 0x44;
        case ParameterKind::Choice:
            return addEnum("DimenGuardClientChoice_" + std::to_string(command_index) + "_" +
                               std::string(parameter.name),
                           parameter.choices);
        }
        throw BinaryError{};
    }

    CommandPacket &packet_;
    std::set<std::string> names_;
    std::map<std::string, std::uint32_t> literals_;
};

void writeStrings(BinaryWriter &writer, std::span<const std::string> values)
{
    writer.writeCount(values.size(), max_entries);
    for (const auto &value : values) {
        writer.writeString(value);
    }
}

std::string rewrite(CommandPacket &packet, std::span<const std::string> regions)
{
    if (regions.size() > 10000 || packet.soft_enums.size() > max_entries - 2 ||
        std::ranges::count(packet.commands, std::string_view{"dg"}, &CommandData::name) != 1) {
        throw BinaryError{};
    }
    for (const auto &entry : packet.soft_enums) {
        if (entry.name == region_enum_name || entry.name == free_name_enum_name) {
            throw BinaryError{};
        }
    }
    std::vector<std::string> names;
    names.reserve(regions.size());
    for (const auto &region : regions) {
        if (!isValidRegionName(region)) {
            throw BinaryError{};
        }
        names.push_back(region);
    }
    std::ranges::sort(names);
    names.erase(std::unique(names.begin(), names.end()), names.end());

    const auto grammar = ClientGrammar(packet).build();
    BinaryWriter writer(max_payload);
    writeStrings(writer, packet.values);
    writer.writeBytes(packet.chained_values);
    writer.writeBytes(packet.postfixes);
    writer.writeCount(packet.enums.size(), max_entries);
    for (const auto &entry : packet.enums) {
        writer.writeString(entry.name);
        writer.writeCount(entry.values.size(), max_entries);
        for (const auto value : entry.values) {
            writer.writeUint32(value);
        }
    }
    writer.writeBytes(packet.chained_commands);
    writer.writeCount(packet.commands.size(), max_entries);
    for (const auto &command : packet.commands) {
        if (command.name == "dg") {
            writer.writeBytes(command.prefix);
            writer.writeBytes(grammar);
        }
        else {
            writer.writeBytes(command.encoded);
        }
    }
    writer.writeCount(packet.soft_enums.size() + 2, max_entries);
    for (const auto &entry : packet.soft_enums) {
        writer.writeBytes(entry.encoded);
    }
    writer.writeString(region_enum_name);
    writeStrings(writer, names);
    writer.writeString(free_name_enum_name);
    writer.writeCount(0, max_entries);
    writer.writeBytes(packet.constraints);
    return std::move(writer).finish();
}

}

std::optional<std::string> rewriteCommandSuggestions(std::string_view payload, std::span<const std::string> regions)
{
    try {
        auto packet = readPacket(payload);
        return rewrite(packet, regions);
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

}
