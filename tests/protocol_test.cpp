#include "dimenguard/command/catalog.h"
#include "dimenguard/protocol/binary_stream.h"
#include "dimenguard/protocol/command_suggestions.h"

#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

using protocol::BinaryError;
using protocol::BinaryReader;
using protocol::BinaryWriter;
constexpr std::size_t test_limit = 8 * 1024 * 1024;

std::string bytes(std::initializer_list<unsigned int> values)
{
    std::string result;
    for (const auto value : values) {
        result += static_cast<char>(value);
    }
    return result;
}

// Independently encoded from the protocol-2169 schema, not produced by BinaryWriter.
// https://github.com/EndstoneMC/protocol-docs/tree/r26_u4
const auto golden = bytes({
    0x01, 0x06, 'r',  'e',  'g',  'i',  'o',  'n',  0x00, 0x00, 0x01, 0x04, 'R',  'o',  'o',  't',
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 'd',  'g',  0x07, 'E',  'x',  'a',  'm',  'p',
    'l',  'e',  0x02, 0x01, 0x03, 'a',  'n',  'y',  0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x00, 0x01,
    0x06, 'r',  'e',  'g',  'i',  'o',  'n',  0x00, 0x00, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00,
});

void writeStrings(BinaryWriter &writer, std::initializer_list<std::string_view> values)
{
    writer.writeCount(values.size(), 65536);
    for (const auto value : values) {
        writer.writeString(value);
    }
}

void writeCommand(BinaryWriter &writer, std::string_view name, bool custom)
{
    writer.writeString(name);
    writer.writeString(custom ? "DimenGuard description" : "Unrelated command description");
    writer.writeUint16(custom ? 0x0102 : 0x0804);
    writer.writeString(custom ? "any" : "admin");
    writer.writeUint32(custom ? 0 : 0xffffffff);
    writer.writeCount(custom ? 0 : 1, 250);
    if (!custom) {
        writer.writeUint32(0);
    }
    writer.writeCount(1, 250);
    writer.writeBoolean(!custom);
    writer.writeCount(1, 65536);
    writer.writeString("existing_parameter");
    writer.writeUint32(0x00100001);
    writer.writeBoolean(true);
    writer.writeByte(2);
}

std::string fixture(std::string_view soft_enum_name = "OtherEnum", bool invalid_index = false)
{
    BinaryWriter writer(test_limit);
    writeStrings(writer, {"dg", "dimenguard", "vanilla", "true"});
    writeStrings(writer, {"as"});
    writeStrings(writer, {"L"});
    writer.writeCount(2, 65536);
    writer.writeString("DgAliases");
    writer.writeCount(2, 65536);
    writer.writeUint32(0);
    writer.writeUint32(1);
    writer.writeString("Boolean");
    writer.writeCount(1, 65536);
    writer.writeUint32(invalid_index ? 999 : 3);
    writer.writeCount(1, 16);
    writer.writeString("execute");
    writer.writeCount(1, 65536);
    writer.writeVarUint32(0);
    writer.writeVarUint32(300);
    writer.writeCount(2, 65536);
    writeCommand(writer, "dg", true);
    writeCommand(writer, "vanilla", false);
    writer.writeCount(1, 65536);
    writer.writeString(soft_enum_name);
    writeStrings(writer, {"keep"});
    writer.writeCount(1, 65536);
    writer.writeUint32(3);
    writer.writeUint32(1);
    writer.writeCount(2, 65536);
    writer.writeByte(0);
    writer.writeByte(2);
    return std::move(writer).finish();
}

struct ParameterView {
    std::string name;
    std::uint32_t symbol;
    bool optional;
    std::uint8_t options;
};

struct CommandView {
    std::string prefix;
    std::string encoded;
    std::vector<std::vector<ParameterView>> overloads;
};

struct EnumView {
    std::string name;
    std::vector<std::uint32_t> values;
    bool operator==(const EnumView &) const = default;
};

struct PacketView {
    std::vector<std::string> values;
    std::string chained_values;
    std::string postfixes;
    std::vector<EnumView> enums;
    std::string chained_commands;
    std::map<std::string, CommandView> commands;
    std::vector<std::pair<std::string, std::vector<std::string>>> soft_enums;
    std::string constraints;
};

std::vector<std::string> readStrings(BinaryReader &reader)
{
    std::vector<std::string> values;
    for (auto count = reader.readCount(65536); count > 0; --count) {
        values.emplace_back(reader.readString(65536));
    }
    return values;
}

PacketView inspect(std::string_view input)
{
    BinaryReader reader(input, test_limit);
    const auto section = [&](auto read) {
        const auto start = reader.position();
        read();
        return std::string(input.substr(start, reader.position() - start));
    };
    PacketView packet;
    packet.values = readStrings(reader);
    packet.chained_values = section([&] { readStrings(reader); });
    packet.postfixes = section([&] { readStrings(reader); });
    for (auto count = reader.readCount(65536); count > 0; --count) {
        EnumView entry{std::string(reader.readString(1000)), {}};
        for (auto value_count = reader.readCount(65536); value_count > 0; --value_count) {
            entry.values.push_back(reader.readUint32());
        }
        packet.enums.push_back(std::move(entry));
    }
    packet.chained_commands = section([&] {
        for (auto count = reader.readCount(16); count > 0; --count) {
            static_cast<void>(reader.readString(1000));
            for (auto values = reader.readCount(65536); values > 0; --values) {
                static_cast<void>(reader.readVarUint32());
                static_cast<void>(reader.readVarUint32());
            }
        }
    });
    for (auto count = reader.readCount(65536); count > 0; --count) {
        CommandView command;
        std::string name;
        command.encoded = section([&] {
            command.prefix = section([&] {
                name = reader.readString(512);
                static_cast<void>(reader.readString(1000));
                static_cast<void>(reader.readUint16());
                static_cast<void>(reader.readString(32));
                static_cast<void>(reader.readUint32());
                for (auto indices = reader.readCount(250); indices > 0; --indices) {
                    static_cast<void>(reader.readUint32());
                }
            });
            for (auto overload_count = reader.readCount(250); overload_count > 0; --overload_count) {
                static_cast<void>(reader.readBoolean());
                std::vector<ParameterView> parameters;
                for (auto parameter_count = reader.readCount(65536); parameter_count > 0; --parameter_count) {
                    parameters.push_back({std::string(reader.readString(512)), reader.readUint32(),
                                          reader.readBoolean(), reader.readByte()});
                }
                command.overloads.push_back(std::move(parameters));
            }
        });
        packet.commands.emplace(std::move(name), std::move(command));
    }
    for (auto count = reader.readCount(65536); count > 0; --count) {
        const auto name = std::string(reader.readString(1000));
        packet.soft_enums.emplace_back(name, readStrings(reader));
    }
    packet.constraints = section([&] {
        for (auto count = reader.readCount(65536); count > 0; --count) {
            static_cast<void>(reader.readUint32());
            static_cast<void>(reader.readUint32());
            static_cast<void>(reader.readBytes(reader.readCount(65536)));
        }
    });
    EXPECT_EQ(reader.remaining(), 0);
    return packet;
}

std::vector<std::string> enumValues(const PacketView &packet, std::uint32_t symbol)
{
    EXPECT_EQ(symbol & 0xfff00000, 0x00300000);
    std::vector<std::string> values;
    for (const auto index : packet.enums.at(symbol & 0x000fffff).values) {
        values.push_back(packet.values.at(index));
    }
    return values;
}

TEST(BinaryStreams, EncodesFixedLittleEndianAndCanonicalVariableIntegers)
{
    BinaryWriter writer(64);
    writer.writeUint16(0x0102);
    writer.writeUint32(0x01020304);
    writer.writeVarUint32(300);
    writer.writeVarUint32(0xffffffff);
    writer.writeString("any");
    const auto encoded = std::move(writer).finish();
    EXPECT_EQ(encoded, bytes({2, 1, 4, 3, 2, 1, 0xac, 2, 0xff, 0xff, 0xff, 0xff, 0x0f, 3, 'a', 'n', 'y'}));
    BinaryReader reader(encoded, 64);
    EXPECT_EQ(reader.readUint16(), 0x0102);
    EXPECT_EQ(reader.readUint32(), 0x01020304);
    EXPECT_EQ(reader.readVarUint32(), 300);
    EXPECT_EQ(reader.readVarUint32(), 0xffffffff);
    EXPECT_EQ(reader.readString(3), "any");
    EXPECT_EQ(reader.remaining(), 0);
}

TEST(BinaryStreams, RejectsOversizedReadsWritesAndLengthPrefixesBeforeAllocation)
{
    EXPECT_THROW(BinaryReader("123", 2), BinaryError);
    BinaryReader empty("", 0);
    EXPECT_THROW(static_cast<void>(empty.readByte()), BinaryError);
    const auto impossible = bytes({0xff, 0xff, 0xff, 0xff, 0x0f});
    BinaryReader length(impossible, 5);
    EXPECT_THROW(static_cast<void>(length.readString(65536)), BinaryError);
    BinaryReader count(impossible, 5);
    EXPECT_THROW(static_cast<void>(count.readCount(65536)), BinaryError);
    const auto invalid_boolean = bytes({2});
    BinaryReader boolean(invalid_boolean, 1);
    EXPECT_THROW(static_cast<void>(boolean.readBoolean()), BinaryError);
    BinaryWriter writer(2);
    writer.writeBytes("ab");
    EXPECT_THROW(writer.writeByte(0), BinaryError);
    EXPECT_EQ(std::move(writer).finish(), "ab");
}

TEST(BinaryStreams, RejectsNonCanonicalTruncatedAndOverflowingVariableIntegers)
{
    for (const auto &input : {bytes({0x80}), bytes({0x80, 0}), bytes({0x81, 0}), bytes({0xff, 0xff, 0xff, 0xff, 0x10}),
                              bytes({0xff, 0xff, 0xff, 0xff, 0xff, 0x01})}) {
        BinaryReader reader(input, 6);
        EXPECT_THROW(static_cast<void>(reader.readVarUint32()), BinaryError);
    }
}

TEST(CommandSuggestions, AcceptsIndependentProtocol2169Golden)
{
    const auto original = inspect(golden);
    ASSERT_EQ(original.values, (std::vector<std::string>{"region"}));
    ASSERT_EQ(original.enums.front().values, (std::vector<std::uint32_t>{0}));
    const auto output = rewriteCommandSuggestions(golden, {});
    ASSERT_TRUE(output);
    const auto rewritten = inspect(*output);
    EXPECT_EQ(rewritten.commands.at("dg").prefix, original.commands.at("dg").prefix);
    EXPECT_EQ(rewritten.commands.at("dg").overloads.size(), commandCatalog().size());
}

TEST(CommandSuggestions, PreservesUnrelatedCommandsAliasesAndIndexedData)
{
    const auto input = fixture();
    const auto before = inspect(input);
    const std::vector<std::string> regions{"test"};
    const auto output = rewriteCommandSuggestions(input, regions);
    ASSERT_TRUE(output);
    const auto after = inspect(*output);
    ASSERT_EQ(after.commands.size(), before.commands.size());
    EXPECT_EQ(after.commands.at("vanilla").encoded, before.commands.at("vanilla").encoded);
    EXPECT_EQ(after.commands.at("dg").prefix, before.commands.at("dg").prefix);
    EXPECT_EQ(after.chained_values, before.chained_values);
    EXPECT_EQ(after.postfixes, before.postfixes);
    EXPECT_EQ(after.chained_commands, before.chained_commands);
    EXPECT_EQ(after.constraints, before.constraints);
    ASSERT_GE(after.values.size(), before.values.size());
    EXPECT_TRUE(std::equal(before.values.begin(), before.values.end(), after.values.begin()));
    ASSERT_GE(after.enums.size(), before.enums.size());
    EXPECT_TRUE(std::equal(before.enums.begin(), before.enums.end(), after.enums.begin()));
    EXPECT_EQ(after.soft_enums.front(), before.soft_enums.front());
}

TEST(CommandSuggestions, ProvidesSortedUniqueExistingNamesAndUnrestrictedNewNames)
{
    const std::vector<std::string> regions{"z", "123", "-1", "a", "123"};
    const auto output = rewriteCommandSuggestions(fixture(), regions);
    ASSERT_TRUE(output);
    const auto packet = inspect(*output);
    ASSERT_EQ(packet.soft_enums.size(), 3);
    EXPECT_EQ(packet.soft_enums[1].first, "DimenGuardRegion");
    EXPECT_EQ(packet.soft_enums[1].second, (std::vector<std::string>{"-1", "123", "a", "z"}));
    EXPECT_EQ(packet.soft_enums[2].first, "DimenGuardRegionName");
    EXPECT_TRUE(packet.soft_enums[2].second.empty());
}

TEST(CommandSuggestions, ClientOverloadsMatchDetailedCatalogAndShareRegionSymbol)
{
    const auto output = rewriteCommandSuggestions(fixture(), {});
    ASSERT_TRUE(output);
    const auto packet = inspect(*output);
    const auto &overloads = packet.commands.at("dg").overloads;
    const auto catalog = commandCatalog();
    ASSERT_EQ(overloads.size(), 15);
    ASSERT_EQ(overloads.size(), catalog.size());
    std::set<std::uint32_t> region_symbols;
    std::size_t region_overloads = 0;
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const auto &command = catalog[i];
        SCOPED_TRACE(command.path);
        const auto &parameters = overloads[i];
        auto path = command.path;
        std::size_t offset = 0;
        while (!path.empty()) {
            ASSERT_LT(offset, parameters.size());
            const auto space = path.find(' ');
            const auto literal = path.substr(0, space);
            EXPECT_EQ(enumValues(packet, parameters[offset].symbol), (std::vector<std::string>{std::string(literal)}));
            EXPECT_EQ(parameters[offset].optional, command.optional_path);
            if (offset == 0 && literal == "region") {
                region_symbols.insert(parameters[offset].symbol);
                ++region_overloads;
            }
            ++offset;
            path = space == path.npos ? std::string_view{} : path.substr(space + 1);
        }
        ASSERT_EQ(parameters.size(), offset + command.parameters.size());
        for (std::size_t j = 0; j < command.parameters.size(); ++j) {
            const auto &expected = command.parameters[j];
            const auto &actual = parameters[offset + j];
            EXPECT_EQ(actual.name, expected.name);
            EXPECT_EQ(actual.optional, expected.optional);
            switch (expected.kind) {
            case ParameterKind::Word:
                EXPECT_EQ(actual.symbol, expected.name == "region" ? 0x04100001 : 0x04100002);
                break;
            case ParameterKind::Integer:
                EXPECT_EQ(actual.symbol, 0x00100001);
                break;
            case ParameterKind::Player:
                EXPECT_EQ(actual.symbol, 0x00100008);
                break;
            case ParameterKind::Message:
                EXPECT_EQ(actual.symbol, 0x00100044);
                break;
            case ParameterKind::Choice: {
                const auto choices = enumValues(packet, actual.symbol);
                EXPECT_EQ(choices, (std::vector<std::string>(expected.choices.begin(), expected.choices.end())));
                break;
            }
            }
        }
    }
    EXPECT_EQ(region_overloads, 6);
    EXPECT_EQ(region_symbols.size(), 1);
}

TEST(CommandSuggestions, LeavesTheOriginalUntouchedWhenNoCustomCommandIsPresent)
{
    const std::string empty_packet(8, '\0');
    EXPECT_FALSE(rewriteCommandSuggestions(empty_packet, {}));
    auto input = golden;
    const auto position = input.find("dg");
    ASSERT_NE(position, input.npos);
    input.replace(position, 2, "xx");
    EXPECT_FALSE(rewriteCommandSuggestions(input, {}));
}

TEST(CommandSuggestions, RejectsTruncationTrailingBytesOversizeAndInvalidIndices)
{
    const auto input = fixture();
    for (std::size_t size = 0; size < input.size(); ++size) {
        SCOPED_TRACE(size);
        EXPECT_FALSE(rewriteCommandSuggestions(std::string_view(input).substr(0, size), {}));
    }
    EXPECT_FALSE(rewriteCommandSuggestions(input + '\0', {}));
    EXPECT_FALSE(rewriteCommandSuggestions(std::string(test_limit + 1, '\0'), {}));
    EXPECT_FALSE(rewriteCommandSuggestions(fixture("OtherEnum", true), {}));
    EXPECT_FALSE(rewriteCommandSuggestions(bytes({1, 0xff, 0xff, 0xff, 0xff, 0x0f}), {}));
    EXPECT_FALSE(rewriteCommandSuggestions(bytes({0x81, 0x80, 0x04}), {}));
}

TEST(CommandSuggestions, RejectsNameCollisionsInvalidNamesAndExcessiveRegionLists)
{
    EXPECT_FALSE(rewriteCommandSuggestions(fixture("DimenGuardRegion"), {}));
    EXPECT_FALSE(rewriteCommandSuggestions(fixture("DimenGuardRegionName"), {}));
    for (const auto name : {"", "bad name", "UpperCase", "@a"}) {
        const std::vector<std::string> regions{name};
        EXPECT_FALSE(rewriteCommandSuggestions(golden, regions)) << name;
    }
    const std::vector<std::string> too_many(10001, "test");
    EXPECT_FALSE(rewriteCommandSuggestions(golden, too_many));
}

}
}
