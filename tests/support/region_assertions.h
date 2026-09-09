#pragma once

#include "dimenguard/region/region.h"

#include <gtest/gtest.h>

namespace dimenguard::test {

inline void expectRegionEqual(const Region &actual, const Region &expected)
{
    EXPECT_EQ(actual.key, expected.key);
    EXPECT_EQ(actual.bounds, expected.bounds);
    EXPECT_EQ(actual.priority, expected.priority);
    EXPECT_EQ(actual.owner, expected.owner);
    EXPECT_EQ(actual.members, expected.members);
    EXPECT_EQ(actual.flags, expected.flags);
}

}
