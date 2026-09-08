#include "dimenguard/region/coordinates.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dimenguard {
namespace {

TEST(ChunkCoordinates, FloorsAtNegativeAndPositiveBoundaries)
{
    constexpr std::array cases{
        std::pair{-33, -3}, std::pair{-32, -2}, std::pair{-17, -2}, std::pair{-16, -1}, std::pair{-15, -1},
        std::pair{-1, -1},  std::pair{0, 0},    std::pair{1, 0},    std::pair{15, 0},   std::pair{16, 1},
        std::pair{17, 1},   std::pair{31, 1},   std::pair{32, 2},
    };
    for (const auto &[block, chunk] : cases) {
        EXPECT_EQ(chunkCoordinate(block), chunk) << "Block coordinate: " << block;
    }
}

TEST(ChunkCoordinates, HandlesExtremeBlockCoordinates)
{
    for (const auto block : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        const auto chunk_min = static_cast<std::int64_t>(chunkCoordinate(block)) * 16;
        EXPECT_LE(chunk_min, block);
        EXPECT_GT(chunk_min + 16, block);
    }
}

TEST(BlockOffsets, AddsOffsetsAtIntegerEdges)
{
    constexpr int minimum = std::numeric_limits<int>::min();
    constexpr int maximum = std::numeric_limits<int>::max();
    EXPECT_EQ(checkedOffset(minimum, 0), minimum);
    EXPECT_EQ(checkedOffset(maximum, 0), maximum);
    EXPECT_EQ(checkedOffset(minimum, 1), minimum + 1);
    EXPECT_EQ(checkedOffset(maximum, -1), maximum - 1);
    EXPECT_EQ(checkedOffset(minimum, maximum), -1);
    EXPECT_EQ(checkedOffset(maximum, minimum), -1);
    EXPECT_EQ(checkedOffset(-16, -1), -17);
}

TEST(BlockOffsets, RejectsOverflowBeforeAdding)
{
    constexpr int minimum = std::numeric_limits<int>::min();
    constexpr int maximum = std::numeric_limits<int>::max();
    EXPECT_THROW(static_cast<void>(checkedOffset(minimum, -1)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(checkedOffset(maximum, 1)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(checkedOffset(minimum, minimum)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(checkedOffset(maximum, maximum)), std::out_of_range);
}

TEST(BlockCoordinates, FloorsFractionsInsteadOfTruncating)
{
    constexpr std::array cases{
        std::pair{-17.1, -18}, std::pair{-17.0, -17}, std::pair{-16.1, -17}, std::pair{-16.0, -16},
        std::pair{-0.1, -1},   std::pair{-0.0, 0},    std::pair{0.0, 0},     std::pair{0.9, 0},
        std::pair{15.9, 15},   std::pair{16.0, 16},   std::pair{16.9, 16},
    };
    for (const auto &[coordinate, expected] : cases) {
        EXPECT_EQ(floorBlockCoordinate(coordinate), expected) << "Coordinate: " << coordinate;
    }
}

TEST(BlockCoordinates, RejectsNonFiniteInput)
{
    EXPECT_FALSE(floorBlockCoordinate(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(floorBlockCoordinate(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(floorBlockCoordinate(-std::numeric_limits<double>::infinity()));
}

TEST(BlockCoordinates, ChecksFlooredIntegerRangeBeforeConversion)
{
    constexpr int minimum = std::numeric_limits<int>::min();
    constexpr int maximum = std::numeric_limits<int>::max();
    EXPECT_EQ(floorBlockCoordinate(minimum), minimum);
    EXPECT_EQ(floorBlockCoordinate(maximum), maximum);
    EXPECT_EQ(floorBlockCoordinate(static_cast<double>(maximum) + 0.75), maximum);
    EXPECT_EQ(floorBlockCoordinate(std::nextafter(static_cast<double>(maximum) + 1.0, 0.0)), maximum);
    EXPECT_FALSE(
        floorBlockCoordinate(std::nextafter(static_cast<double>(minimum), -std::numeric_limits<double>::infinity())));
    EXPECT_FALSE(floorBlockCoordinate(static_cast<double>(maximum) + 1.0));
    EXPECT_FALSE(floorBlockCoordinate(static_cast<double>(minimum) - 1.0));
    EXPECT_FALSE(floorBlockCoordinate(std::numeric_limits<double>::max()));
    EXPECT_FALSE(floorBlockCoordinate(-std::numeric_limits<double>::max()));
    // Endstone locations use float: INT_MAX rounds upward and must not be cast back to int.
    EXPECT_FALSE(floorBlockCoordinate(static_cast<float>(maximum)));
    EXPECT_EQ(floorBlockCoordinate(static_cast<float>(minimum)), minimum);
}

}  // namespace
}  // namespace dimenguard
