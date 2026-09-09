#include "dimenguard/command/selection.h"

#include <gtest/gtest.h>

namespace dimenguard {
namespace {

TEST(Selection, RequiresBothCornersAndNormalizesBounds)
{
    SelectionManager selections;
    const DimensionKey dimension{"world", "overworld"};
    EXPECT_FALSE(selections.get("alice", dimension));
    selections.set("alice", dimension, {3, 4, 5}, SelectionCorner::First);
    EXPECT_FALSE(selections.get("alice", dimension));
    selections.set("alice", dimension, {-3, -4, -5}, SelectionCorner::Second);
    ASSERT_TRUE(selections.get("alice", dimension));
    EXPECT_EQ(selections.get("alice", dimension)->min, (BlockPosition{-3, -4, -5}));
    EXPECT_EQ(selections.get("alice", dimension)->max, (BlockPosition{3, 4, 5}));
}

TEST(Selection, ChangingDimensionResetsBothCorners)
{
    SelectionManager selections;
    const DimensionKey first{"world", "overworld"};
    const DimensionKey second{"world", "nether"};
    selections.set("alice", first, {}, SelectionCorner::First);
    selections.set("alice", first, {}, SelectionCorner::Second);
    selections.set("alice", second, {}, SelectionCorner::Second);
    EXPECT_FALSE(selections.get("alice", first));
    EXPECT_FALSE(selections.get("alice", second));
    selections.set("alice", second, {}, SelectionCorner::First);
    EXPECT_TRUE(selections.get("alice", second));
}

TEST(Selection, IsolatesPlayersAndClearsSessionState)
{
    SelectionManager selections;
    const DimensionKey dimension{"world", "overworld"};
    selections.set("alice", dimension, {}, SelectionCorner::First);
    selections.set("alice", dimension, {}, SelectionCorner::Second);
    selections.set("bob", dimension, {}, SelectionCorner::First);
    EXPECT_FALSE(selections.get("bob", dimension));
    EXPECT_TRUE(selections.get("alice", dimension));
    selections.forget("alice");
    EXPECT_FALSE(selections.get("alice", dimension));
}

}
}
