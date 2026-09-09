#pragma once

namespace dimenguard::sqlite {
class Connection;
}

namespace dimenguard::storage {

void initializeSchema(const sqlite::Connection &connection);
// Check after beginning a transaction so schema and data belong to the same snapshot.
void requireSchemaVersion(const sqlite::Connection &connection);

}
