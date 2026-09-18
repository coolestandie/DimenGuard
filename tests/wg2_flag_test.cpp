#include "dimenguard/region/flag.h"
#include "dimenguard/region/protection_policy.h"
#include "dimenguard/region/region_manager.h"

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dimenguard {
namespace {

const DimensionKey dimension{"world", "minecraft:overworld"};
const DimensionKey other_dimension{"world", "minecraft:nether"};
constexpr BlockPosition inside{0, 64, 0};
constexpr BlockPosition outside{100, 64, 0};

Region region(std::string name = "plot", int offset = 0)
{
    Region result;
    result.key = {dimension, std::move(name)};
    result.bounds = {{offset - 10, 0, -10}, {offset + 10, 128, 10}};
    result.owner = "owner-" + result.key.name;
    return result;
}

FlagValue domains(std::initializer_list<std::string> entries)
{
    return FlagValue(FlagSet(entries));
}

TEST(FlagValueCodec, RoundTripsEverySupportedType)
{
    const std::array values{FlagValue(FlagState::Deny),
                            FlagValue("Keep this literal"),
                            FlagValue(std::numeric_limits<std::int64_t>::min()),
                            FlagValue(-0.125),
                            FlagValue(FlagLocation{"My world", "minecraft:overworld", -1.5, 64, 2, 45, -30}),
                            FlagValue(false),
                            domains({"alpha", "beta"})};
    for (const auto &value : values) {
        const auto parsed = parseFlagValue(value.type(), formatFlagValue(value));
        ASSERT_TRUE(parsed) << valueTypeName(value.type());
        EXPECT_EQ(*parsed, value);
        EXPECT_EQ(parseFlagType(valueTypeName(value.type())), value.type());
    }
}

TEST(FlagValueCodec, RejectsUnknownTypesAndTypeMismatches)
{
    EXPECT_FALSE(parseFlagType("String"));
    EXPECT_FALSE(parseFlagType("uuid"));
    EXPECT_FALSE(parseFlagValue(static_cast<FlagType>(99), "x"));
    EXPECT_THROW(static_cast<void>(valueTypeName(static_cast<FlagType>(99))), std::invalid_argument);
    EXPECT_THROW(validateFlagValue(FlagType::String, FlagValue(true)), std::invalid_argument);
    EXPECT_THROW(validateFlagValue(FlagType::State, FlagValue(static_cast<FlagState>(99))), std::invalid_argument);
}

TEST(FlagValueCodec, KeepsStateAndBooleanParsingStrict)
{
    for (const auto text : {"ALLOW", " deny", "deny ", "1", "true", ""}) {
        EXPECT_FALSE(parseFlagValue(FlagType::State, text)) << text;
    }
    for (const auto text : {"True", "FALSE", "1", "0", "yes", "false "}) {
        EXPECT_FALSE(parseFlagValue(FlagType::Boolean, text)) << text;
    }
    EXPECT_EQ(parseFlagValue(FlagType::State, "inherit"), FlagValue(FlagState::Inherit));
    EXPECT_EQ(parseFlagValue(FlagType::Boolean, "true"), FlagValue(true));
}

TEST(FlagValueCodec, ChecksIntegerOverflowAndTrailingCharacters)
{
    EXPECT_EQ(parseFlagValue(FlagType::Integer, "9223372036854775807"),
              FlagValue(std::numeric_limits<std::int64_t>::max()));
    EXPECT_EQ(parseFlagValue(FlagType::Integer, "-9223372036854775808"),
              FlagValue(std::numeric_limits<std::int64_t>::min()));
    for (const auto text : {"9223372036854775808", "-9223372036854775809", "1.0", "1x", " 1", "+1", ""}) {
        EXPECT_FALSE(parseFlagValue(FlagType::Integer, text)) << text;
    }
}

TEST(FlagValueCodec, AcceptsFiniteDoublesAndRejectsNonFiniteOrPartialNumbers)
{
    for (const auto number : {0.0, -0.0, 0.1, std::numeric_limits<double>::min(), std::numeric_limits<double>::max(),
                              std::numeric_limits<double>::denorm_min()}) {
        const FlagValue value(number);
        EXPECT_EQ(parseFlagValue(FlagType::Double, formatFlagValue(value)), value);
    }
    for (const auto text : {"nan", "inf", "-inf", "1e999", "1.5x", "1 ", ""}) {
        EXPECT_FALSE(parseFlagValue(FlagType::Double, text)) << text;
    }
    EXPECT_THROW(validateFlagValue(FlagType::Double, FlagValue(std::numeric_limits<double>::quiet_NaN())),
                 std::invalid_argument);
}

TEST(FlagValueCodec, PreservesEmptyAndReservedStringLiterals)
{
    for (const auto text : {"", "inherit", "--unset", "[]", "two  spaces", "Espa\xc3\xb1ol"}) {
        const auto value = parseFlagValue(FlagType::String, text);
        ASSERT_TRUE(value);
        EXPECT_EQ(formatFlagValue(*value), text);
    }
    EXPECT_TRUE(parseFlagValue(FlagType::String, std::string(maximum_flag_text_bytes, 'a')));
    EXPECT_FALSE(parseFlagValue(FlagType::String, std::string(maximum_flag_text_bytes + 1, 'a')));
}

TEST(FlagValueCodec, RejectsMalformedUtf8AndMultiLineText)
{
    const std::array invalid{std::string{"bad\nline"},        std::string{"bad\rline"},   std::string{"bad\tline"},
                             std::string{"a\0b", 3},          std::string{"\xc0\xaf"},    std::string{"\xed\xa0\x80"},
                             std::string{"\xf4\x90\x80\x80"}, std::string{"\xe2\x82"},    std::string{"\xc2\x85"},
                             std::string{"\xe2\x80\xa8"},     std::string{"\xe2\x80\xa9"}};
    for (const auto &text : invalid) {
        EXPECT_FALSE(parseFlagValue(FlagType::String, text));
    }
    EXPECT_TRUE(parseFlagValue(FlagType::String, "\xf0\x9f\x92\x8e"));
}

TEST(FlagValueCodec, CanonicalizesSetsAndDistinguishesEmptyFromMissing)
{
    const auto value = parseFlagValue(FlagType::Set, " beta,alpha, beta ");
    ASSERT_TRUE(value);
    EXPECT_EQ(formatFlagValue(*value), "alpha,beta");
    EXPECT_EQ(parseFlagValue(FlagType::Set, "[]"), domains({}));
    for (const auto text : {"", ",", "a,", ",a", "a,,b", "[a]", "[a,b]"}) {
        EXPECT_FALSE(parseFlagValue(FlagType::Set, text)) << text;
    }
}

TEST(FlagValueCodec, BoundsSetEntriesAndEncodedInputs)
{
    FlagSet entries;
    for (std::size_t index = 0; index < maximum_flag_set_entries; ++index) {
        entries.insert("entry" + std::to_string(index));
    }
    EXPECT_NO_THROW(validateFlagValue(FlagType::Set, FlagValue(entries)));
    entries.insert("overflow");
    EXPECT_THROW(validateFlagValue(FlagType::Set, FlagValue(entries)), std::invalid_argument);
    EXPECT_FALSE(parseFlagValue(FlagType::Set, std::string(maximum_flag_set_entry_bytes + 1, 'x')));
    EXPECT_FALSE(parseFlagValue(FlagType::String, std::string(maximum_encoded_flag_bytes + 1, 'x')));
    FlagSet large;
    for (std::size_t index = 0; index < maximum_flag_set_entries; ++index) {
        auto entry = std::to_string(index);
        entry.append(maximum_flag_set_entry_bytes - entry.size(), 'x');
        large.insert(std::move(entry));
    }
    EXPECT_THROW(validateFlagValue(FlagType::Set, FlagValue(large)), std::invalid_argument);
}

TEST(FlagValueCodec, ValidatesExplicitLocationIdentityAndCoordinates)
{
    const auto value = parseFlagValue(FlagType::Location, "world,minecraft:nether,-1.5,64,0");
    ASSERT_TRUE(value);
    EXPECT_EQ(formatFlagValue(*value), "world,minecraft:nether,-1.5,64,0,0,0");
    for (const auto text :
         {"world,nether,1,2", "world,nether,1,2,3,4", "world,nether,1,2,3,4,5,6", ",nether,1,2,3", "world,,1,2,3",
          "world,nether,inf,2,3", "world,nether,1,2,3,0,91", "world,nether,1,2,3,0,-91"}) {
        EXPECT_FALSE(parseFlagValue(FlagType::Location, text)) << text;
    }
}

TEST(FlagRegistry, ValidatesRealFlagConstraintsAndDoesNotAdvertisePlaceholders)
{
    EXPECT_TRUE(parseFlagValue(Flag::DenySpawn, "minecraft:zombie,custom:mob_type"));
    for (const auto text : {"zombie", "minecraft:", ":zombie", "minecraft:zombie:x", "Minecraft:zombie",
                            "minecraft:zombie thing", "bad/namespace:zombie"}) {
        EXPECT_FALSE(parseFlagValue(Flag::DenySpawn, text)) << text;
    }
    EXPECT_TRUE(parseFlagValue(Flag::NonPlayerProtectionDomains, "castle,farm-2"));
    for (const auto text : {"Castle", "space here", "minecraft:castle", "../castle"}) {
        EXPECT_FALSE(parseFlagValue(Flag::NonPlayerProtectionDomains, text)) << text;
    }
    EXPECT_FALSE(parseFlagValue(Flag::EntryDenyMessage, "\xc2\xa7"
                                                        "aGreen"));
    for (const auto &definition : flagDefinitions()) {
        EXPECT_EQ(parseFlag(definition.name), definition.flag);
        EXPECT_TRUE(definition.type == FlagType::State || definition.type == FlagType::String ||
                    definition.type == FlagType::Set);
        if (definition.aggregate) {
            EXPECT_EQ(flagType(*definition.aggregate), definition.type);
        }
    }
}

TEST(TypedFlagPolicy, UsesRegisteredDefaultsAndRejectsNonStateAllowQueries)
{
    RegionManager manager;
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::DenySpawn), domains({}));
    EXPECT_FALSE(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "visitor"));
    EXPECT_THROW(static_cast<void>(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(manager.getFlagValue(dimension, inside, Flag::Build)), std::invalid_argument);
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::Tnt), FlagValue(FlagState::Allow));
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::Invincible), FlagValue(FlagState::Deny));
    EXPECT_THROW(static_cast<void>(manager.isEnvironmentAllowed(dimension, inside, Flag::DenySpawn)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(manager.isAllowed(dimension, inside, Flag::EntryDenyMessage, "owner")),
                 std::invalid_argument);
}

TEST(TypedFlagPolicy, KeepsScalarPriorityAndCanonicalNameTieBreak)
{
    auto alpha = region("alpha");
    auto beta = region("beta");
    alpha.flags[Flag::EntryDenyMessage] = FlagValue("alpha message");
    beta.flags[Flag::EntryDenyMessage] = FlagValue("beta message");
    RegionManager manager;
    manager.replaceAll({beta, alpha});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "visitor"), FlagValue("alpha message"));
    beta.priority = 1;
    manager.replaceAll({alpha, beta});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "visitor"), FlagValue("beta message"));
    beta.flags.clear();
    manager.replaceAll({alpha, beta});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "visitor"), FlagValue("alpha message"));
}

TEST(TypedFlagPolicy, UnionsOnlyTheHighestExplicitSetTier)
{
    auto alpha = region("alpha");
    auto beta = region("beta");
    auto lower = region("lower");
    alpha.priority = beta.priority = 5;
    alpha.flags[Flag::DenySpawn] = domains({"minecraft:zombie"});
    beta.flags[Flag::DenySpawn] = domains({"minecraft:skeleton"});
    lower.flags[Flag::DenySpawn] = domains({"minecraft:pig"});
    RegionManager manager;
    manager.replaceAll({lower, beta, alpha});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::DenySpawn),
              domains({"minecraft:skeleton", "minecraft:zombie"}));
    alpha.priority = 10;
    alpha.flags[Flag::DenySpawn] = domains({});
    manager.replaceAll({lower, beta, alpha});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::DenySpawn), domains({}));
}

TEST(TypedFlagPolicy, InheritsTypedValuesAndSuppressesMatchingAncestors)
{
    auto parent = region("parent");
    auto child = region("child");
    child.parent = parent.key.name;
    parent.flags[Flag::DenySpawn] = domains({"minecraft:pig"});
    child.flags[Flag::DenySpawn] = domains({"minecraft:zombie"});
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::DenySpawn), domains({"minecraft:zombie"}));
    child.flags.clear();
    manager.replaceAll({parent, child});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::DenySpawn), domains({"minecraft:pig"}));
}

TEST(TypedFlagPolicy, ExcludedChildMessageFallsBackToApplicableParentGroup)
{
    auto parent = region("parent");
    auto child = region("child");
    child.parent = parent.key.name;
    parent.flags[Flag::EntryDenyMessage] = FlagValue("parent message");
    child.flags[Flag::EntryDenyMessage] = FlagValue("member message");
    child.flag_groups[Flag::EntryDenyMessage] = RegionGroup::Members;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "outsider"), FlagValue("parent message"));
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, parent.owner),
              FlagValue("member message"));
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, child.owner),
              FlagValue("member message"));
}

TEST(TypedFlagPolicy, GroupOnlyOverrideScopesInheritedMessage)
{
    auto parent = region("parent");
    parent.kind = RegionKind::Template;
    parent.flags[Flag::EntryDenyMessage] = FlagValue("private message");
    auto child = region("child");
    child.parent = parent.key.name;
    child.flag_groups[Flag::EntryDenyMessage] = RegionGroup::Owners;
    RegionManager manager;
    manager.replaceAll({parent, child});
    EXPECT_FALSE(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "outsider"));
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, child.owner),
              FlagValue("private message"));
}

TEST(TypedFlagPolicy, GlobalIsLowerThanOrdinaryMinimumPriorityRegion)
{
    auto global = region("__global__");
    global.kind = RegionKind::Global;
    global.priority = std::numeric_limits<int>::min();
    global.flags[Flag::EntryDenyMessage] = FlagValue("global");
    auto plot = region();
    plot.priority = std::numeric_limits<int>::min();
    plot.flags[Flag::EntryDenyMessage] = FlagValue("plot");
    RegionManager manager;
    manager.replaceAll({global, plot});
    EXPECT_EQ(manager.getFlagValue(dimension, inside, Flag::EntryDenyMessage, "visitor"), FlagValue("plot"));
    EXPECT_EQ(manager.getFlagValue(dimension, outside, Flag::EntryDenyMessage, "visitor"), FlagValue("global"));
    EXPECT_FALSE(manager.getFlagValue(other_dimension, inside, Flag::EntryDenyMessage, "visitor"));
}

TEST(TypedFlagPolicy, EnvironmentalSetGroupsRemainUnambiguous)
{
    auto plot = region();
    for (const auto flag : {Flag::DenySpawn, Flag::NonPlayerProtectionDomains}) {
        plot.flag_groups = {{flag, RegionGroup::Members}};
        EXPECT_THROW(validateRegion(plot), std::invalid_argument);
        plot.flag_groups[flag] = RegionGroup::All;
        EXPECT_NO_THROW(validateRegion(plot));
    }
    plot.flags[Flag::DenySpawn] = FlagState::Allow;
    EXPECT_THROW(validateRegion(plot), std::invalid_argument);
}

TEST(NonPlayerSubjects, ProtectsBoundaryAndAllowsRealSourceRegionMembership)
{
    RegionManager manager;
    manager.replaceAll({region()});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, BlockPosition{-10, 64, 0}, dimension, inside));
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, BlockPosition{-11, 64, 0}, dimension, inside));
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, inside, dimension, outside));
}

TEST(NonPlayerSubjects, UnknownAndCrossDimensionSourcesHaveNoMembership)
{
    auto plot = region();
    auto nether = plot;
    nether.key.dimension = other_dimension;
    plot.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    nether.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    RegionManager manager;
    manager.replaceAll({plot, nether});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, std::nullopt, dimension, inside));
    EXPECT_FALSE(manager.isNonPlayerAllowed(other_dimension, inside, dimension, inside));
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, std::nullopt, dimension, outside));
    EXPECT_THROW(static_cast<void>(manager.isNonPlayerAllowed(dimension, inside, dimension, inside, Flag::Tnt)),
                 std::invalid_argument);
}

TEST(NonPlayerSubjects, SharedDomainsPermitAssociationButNeverOverrideExplicitDeny)
{
    auto source = region("source", 100);
    auto target = region("target");
    source.flags[Flag::NonPlayerProtectionDomains] = domains({"farm"});
    target.flags[Flag::NonPlayerProtectionDomains] = domains({"farm", "castle"});
    RegionManager manager;
    manager.replaceAll({source, target});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target.flags[Flag::BlockBreak] = FlagState::Deny;
    manager.replaceAll({source, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target.flags[Flag::BlockBreak] = FlagState::Allow;
    target.flags.erase(Flag::NonPlayerProtectionDomains);
    manager.replaceAll({source, target});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, std::nullopt, dimension, inside));
}

TEST(NonPlayerSubjects, SourceDomainTierAndEachTargetMembershipRemainIndependent)
{
    auto source = region("source", 100);
    auto source_high = region("source_high", 100);
    auto target = region("target");
    auto target_high = region("target_high");
    source.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    target.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    source_high.priority = target_high.priority = 5;
    source_high.flags[Flag::NonPlayerProtectionDomains] = domains({"other"});
    RegionManager manager;
    manager.replaceAll({source, source_high, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    source_high.flags.clear();
    target_high.flags[Flag::NonPlayerProtectionDomains] = domains({});
    manager.replaceAll({source, source_high, target, target_high});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target_high.flags.clear();
    manager.replaceAll({source, source_high, target, target_high});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target_high.passthrough = FlagState::Allow;
    manager.replaceAll({source, source_high, target, target_high});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
}

TEST(NonPlayerSubjects, SharedDomainsCannotGrantMembershipInAnUnrelatedOverlappingTarget)
{
    auto source = region("source", 100);
    auto target = region("target");
    auto unrelated = region("unrelated");
    source.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    target.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    RegionManager manager;
    manager.replaceAll({source, target, unrelated});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    unrelated.flags[Flag::NonPlayerProtectionDomains] = domains({"different"});
    manager.replaceAll({source, target, unrelated});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    unrelated.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    manager.replaceAll({source, target, unrelated});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
}

TEST(NonPlayerSubjects, DomainTiesUnionAndGlobalDoesNotGrantUnrelatedMembership)
{
    auto source = region("source", 100);
    auto source_peer = region("source_peer", 100);
    auto target = region("target");
    source.flags[Flag::NonPlayerProtectionDomains] = domains({"other"});
    source_peer.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    target.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    RegionManager manager;
    manager.replaceAll({source, source_peer, target});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    auto global = region("__global__");
    global.kind = RegionKind::Global;
    global.priority = std::numeric_limits<int>::min();
    global.flags[Flag::NonPlayerProtectionDomains] = domains({"shared"});
    manager.replaceAll({global, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    source.flags.clear();
    target.flags.clear();
    manager.replaceAll({global, source, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
}

TEST(NonPlayerSubjects, AncestorAssociationUsesHierarchyNotPlayerIdentity)
{
    auto parent = region("parent");
    parent.kind = RegionKind::Template;
    auto source = region("source", 100);
    auto target = region("target");
    source.parent = target.parent = parent.key.name;
    RegionManager manager;
    manager.replaceAll({parent, source, target});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target.flag_groups[Flag::BlockBreak] = RegionGroup::Owners;
    target.flags[Flag::BlockBreak] = FlagState::Deny;
    manager.replaceAll({parent, source, target});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target.flag_groups[Flag::BlockBreak] = RegionGroup::Members;
    manager.replaceAll({parent, source, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    target.flag_groups[Flag::BlockBreak] = RegionGroup::NonOwners;
    manager.replaceAll({parent, source, target});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
}

TEST(NonPlayerSubjects, UnrelatedHighestPriorityRegionsStillRequireAllMemberships)
{
    auto source = region("source", 100);
    auto target = region("target");
    target.parent = source.key.name;
    auto unrelated = region("unrelated");
    RegionManager manager;
    manager.replaceAll({source, target, unrelated});
    EXPECT_FALSE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
    unrelated.passthrough = FlagState::Allow;
    manager.replaceAll({source, target, unrelated});
    EXPECT_TRUE(manager.isNonPlayerAllowed(dimension, outside, dimension, inside));
}

TEST(TransitionMessages, SelectsMessageOnlyFromActuallyEnteredOrExitedRegions)
{
    auto entered = region("entered");
    entered.flags[Flag::Entry] = FlagState::Deny;
    entered.flags[Flag::EntryDenyMessage] = FlagValue("You cannot enter");
    auto unchanged = region("unchanged");
    unchanged.bounds = {{-200, 0, -10}, {200, 128, 10}};
    unchanged.priority = 10;
    unchanged.flags[Flag::EntryDenyMessage] = FlagValue("Wrong unchanged message");
    RegionManager manager;
    manager.replaceAll({unchanged, entered});
    const auto denial = manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor");
    ASSERT_TRUE(denial);
    EXPECT_EQ(denial->flag, Flag::Entry);
    EXPECT_EQ(denial->message, "You cannot enter");
    EXPECT_FALSE(manager.getTransitionDenial(dimension, inside, dimension, BlockPosition{1, 64, 0}, "visitor"));
    EXPECT_FALSE(manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor", true));
}

TEST(TransitionMessages, ExitPrecedesEntryAndUsesOriginMessage)
{
    auto origin = region("origin", 100);
    auto destination = region("destination");
    origin.flags[Flag::Exit] = FlagState::Deny;
    origin.flags[Flag::ExitDenyMessage] = FlagValue("Stay here");
    destination.flags[Flag::Entry] = FlagState::Deny;
    destination.flags[Flag::EntryDenyMessage] = FlagValue("No entry");
    RegionManager manager;
    manager.replaceAll({origin, destination});
    const auto denial = manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor");
    ASSERT_TRUE(denial);
    EXPECT_EQ(denial->flag, Flag::Exit);
    EXPECT_EQ(denial->message, "Stay here");
    EXPECT_FALSE(manager.isTransitionAllowed(dimension, outside, dimension, inside, "visitor"));
}

TEST(TransitionMessages, MissingMessageRemainsDistinctFromEmptyAndRespectsGroups)
{
    auto plot = region();
    plot.flags[Flag::Entry] = FlagState::Deny;
    RegionManager manager;
    manager.replaceAll({plot});
    auto denial = manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor");
    ASSERT_TRUE(denial);
    EXPECT_FALSE(denial->message);
    plot.flags[Flag::EntryDenyMessage] = FlagValue("");
    manager.replaceAll({plot});
    denial = manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor");
    ASSERT_TRUE(denial && denial->message);
    EXPECT_TRUE(denial->message->empty());
    plot.flag_groups[Flag::EntryDenyMessage] = RegionGroup::Owners;
    manager.replaceAll({plot});
    denial = manager.getTransitionDenial(dimension, outside, dimension, inside, "visitor");
    ASSERT_TRUE(denial);
    EXPECT_FALSE(denial->message);
}

TEST(TransitionMessages, GlobalMessageAppliesOnlyWhenCrossingItsDimension)
{
    auto global = region("__global__");
    global.kind = RegionKind::Global;
    global.priority = std::numeric_limits<int>::min();
    global.flags[Flag::Exit] = FlagState::Deny;
    global.flags[Flag::ExitDenyMessage] = FlagValue("Stay in this dimension");
    RegionManager manager;
    manager.replaceAll({global});
    EXPECT_FALSE(manager.getTransitionDenial(dimension, inside, dimension, outside, "visitor"));
    const auto denial = manager.getTransitionDenial(dimension, inside, other_dimension, inside, "visitor");
    ASSERT_TRUE(denial);
    EXPECT_EQ(denial->flag, Flag::Exit);
    EXPECT_EQ(denial->message, "Stay in this dimension");
}

}
}
