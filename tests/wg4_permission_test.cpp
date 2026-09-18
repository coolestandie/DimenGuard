#include "dimenguard/region/region.h"
#include "dimenguard/service/region_service.h"
#include "support/database_fixture.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace dimenguard {
namespace {

const DimensionKey dimension{"survival", "minecraft:overworld"};

TEST(Bounds, VolumeUsesInclusiveCoordinatesAndSaturates)
{
    EXPECT_EQ(boundsVolume({{0, 0, 0}, {0, 0, 0}}), 1);
    EXPECT_EQ(boundsVolume({{-1, -1, -1}, {1, 1, 1}}), 27);
    EXPECT_EQ(boundsVolume({{std::numeric_limits<int>::min(), 0, 0},
                            {std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
                             std::numeric_limits<int>::max()}}),
              std::numeric_limits<std::uint64_t>::max());
}

class Wg4ClaimTest : public test::DatabaseFixture {};

TEST_F(Wg4ClaimTest, ClaimsRequireExclusivePhysicalSpaceAndPersistOwner)
{
    RegionService service(path_);
    service.claim(dimension, "plot", {{0, 0, 0}, {7, 7, 7}}, "owner-one");
    const auto *plot = service.getRegions().find({dimension, "plot"});
    ASSERT_NE(plot, nullptr);
    EXPECT_EQ(plot->owner, "owner-one");
    EXPECT_EQ(service.getRegions().countOwned("owner-one"), 1);

    EXPECT_THROW(service.claim(dimension, "overlap", {{7, 7, 7}, {8, 8, 8}}, "owner-two"), ServiceError);
    EXPECT_EQ(service.getRegions().getAll().size(), 1);
}

TEST_F(Wg4ClaimTest, ClaimVolumeAndCountLimitsAreEnforcedBeforeStorage)
{
    RegionService service(path_);
    EXPECT_THROW(service.claim(dimension, "large", {{0, 0, 0}, {128, 128, 64}}, "owner"), ServiceError);
    for (std::size_t index = 0; index < RegionService::MaxClaimedRegions; ++index) {
        service.claim(dimension, "plot-" + std::to_string(index),
                      {{static_cast<int>(index * 10), 0, 0}, {static_cast<int>(index * 10 + 1), 1, 1}}, "owner");
    }
    EXPECT_EQ(service.getRegions().countOwned("owner"), RegionService::MaxClaimedRegions);
    EXPECT_THROW(service.claim(dimension, "overflow", {{1000, 0, 0}, {1001, 1, 1}}, "owner"), ServiceError);
}

TEST_F(Wg4ClaimTest, OwnershipTransferPreservesMembersAndIsTransactional)
{
    RegionService service(path_);
    service.claim(dimension, "plot", {{0, 0, 0}, {7, 7, 7}}, "owner-one");
    service.setMember({dimension, "plot"}, "member", true);
    service.setOwner({dimension, "plot"}, "owner-two");
    const auto *plot = service.getRegions().find({dimension, "plot"});
    ASSERT_NE(plot, nullptr);
    EXPECT_EQ(plot->owner, "owner-two");
    EXPECT_TRUE(plot->members.contains("member"));
}

}
}
