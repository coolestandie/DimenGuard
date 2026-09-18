#pragma once

#include "dimenguard/region/region.h"

#include <vector>

namespace dimenguard::sqlite {
class Connection;
}

namespace dimenguard::storage {

[[nodiscard]] std::vector<Region> readSnapshot(const sqlite::Connection &connection, int schema_version);
void writeSnapshot(const sqlite::Connection &connection, const std::vector<Region> &regions);

}
