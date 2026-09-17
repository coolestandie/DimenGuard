#include "dimenguard/region/region_manager.h"
#include "dimenguard/service/region_service.h"
#include "support/database_fixture.h"

#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace dimenguard {
namespace {

const DimensionKey dimension{"survival", "minecraft:overworld"};

Region makeRegion(std::string name, Bounds bounds = {{0, 0, 0}, {2, 2, 2}})
{
    Region region;
    region.key = {dimension, std::move(name)};
    region.bounds = bounds;
    region.owner = "00000000-0000-4000-8000-000000000001";
    return region;
}

TEST(Bounds, OverlapIsInclusiveAndThreeDimensional)
{
    const Bounds first{{0, 0, 0}, {2, 2, 2}};
    EXPECT_TRUE(first.overlaps({{2, -4, 2}, {4, 1, 4}}));
    EXPECT_FALSE(first.overlaps({{3, 0, 0}, {4, 2, 2}}));
    EXPECT_FALSE(first.overlaps({{0, 3, 0}, {2, 4, 2}}));
}

TEST(RegionManager, OverlapsReturnsOnlyPhysicalRegionsInNameOrder)
{
    auto far = makeRegion("far", {{20, 0, 0}, {30, 2, 2}});
    auto near = makeRegion("near");
    auto edge = makeRegion("edge", {{2, 2, 2}, {4, 4, 4}});
    auto template_region = makeRegion("template");
    template_region.kind = RegionKind::Template;
    RegionManager manager;
    manager.replaceAll({far, near, edge, template_region});
    const auto result = manager.overlaps(dimension, {{1, 1, 1}, {3, 3, 3}});
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]->key.name, "edge");
    EXPECT_EQ(result[1]->key.name, "near");
    EXPECT_EQ(manager.overlaps(dimension, {{1, 1, 1}, {3, 3, 3}}, "near").size(), 1);
}

class Wg3ServiceTest : public test::DatabaseFixture {};

TEST_F(Wg3ServiceTest, RedefineAndMovePersistValidatedGeometry)
{
    auto region = makeRegion("plot");
    RegionService service(path_);
    service.create(region);
    service.setBounds(region.key, {{10, 11, 12}, {20, 21, 22}});
    service.move(region.key, {-2, 3, -4});
    const auto *updated = service.getRegions().find(region.key);
    ASSERT_NE(updated, nullptr);
    EXPECT_EQ(updated->bounds, (Bounds{{8, 14, 8}, {18, 24, 18}}));
    RegionService reopened(path_);
    ASSERT_NE(reopened.getRegions().find(region.key), nullptr);
    EXPECT_EQ(reopened.getRegions().find(region.key)->bounds, updated->bounds);
}

TEST_F(Wg3ServiceTest, GeometryOverflowPreservesTheCommittedSnapshot)
{
    auto region =
        makeRegion("plot", {{std::numeric_limits<int>::max(), 0, 0}, {std::numeric_limits<int>::max(), 1, 1}});
    RegionService service(path_);
    service.create(region);
    EXPECT_THROW(service.move(region.key, {1, 0, 0}), ServiceError);
    EXPECT_EQ(service.getRegions().find(region.key)->bounds, region.bounds);
}

TEST_F(Wg3ServiceTest, CascadingDeleteRequiresAnExplicitChoiceAndRemovesTheChildTree)
{
    auto parent = makeRegion("parent");
    auto child = makeRegion("child", {{3, 0, 0}, {4, 2, 2}});
    child.parent = parent.key.name;
    auto grandchild = makeRegion("grandchild", {{5, 0, 0}, {6, 2, 2}});
    grandchild.parent = child.key.name;
    RegionService service(path_);
    service.create(parent);
    service.create(child);
    service.create(grandchild);
    EXPECT_TRUE(service.hasChildren(parent.key));
    EXPECT_THROW(service.erase(parent.key), ServiceError);
    service.erase(parent.key, true);
    EXPECT_TRUE(service.getRegions().getAll().empty());
    RegionService reopened(path_);
    EXPECT_TRUE(reopened.getRegions().getAll().empty());
}

}
}
