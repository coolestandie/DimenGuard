#include "dimenguard/storage/sqlite.h"
#include "support/database_fixture.h"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dimenguard {
namespace {

using SqliteTest = test::DatabaseFixture;

TEST_F(SqliteTest, BoundTextPreservesEmptyStringsEmbeddedNullsAndQuotes)
{
    sqlite::Connection connection(path_);
    connection.execute("CREATE TABLE sample (id INTEGER PRIMARY KEY, value TEXT NOT NULL)");
    sqlite::Statement insert(connection, "INSERT INTO sample (value) VALUES (?)");
    const std::string embedded_null("a\0b", 3);
    insert.bind(1, std::string_view{});
    insert.run();
    insert.bind(1, embedded_null);
    insert.run();
    insert.bind(1, "O'Brien; DROP TABLE sample;");
    insert.run();
    sqlite::Statement query(connection, "SELECT value FROM sample ORDER BY id");
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.text(0), "");
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.text(0), embedded_null);
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.text(0), "O'Brien; DROP TABLE sample;");
    EXPECT_FALSE(query.next());
}

TEST_F(SqliteTest, TypedColumnsRejectImplicitConversionAndOverflow)
{
    sqlite::Connection connection(path_);
    sqlite::Statement query(connection, "SELECT '12', 12, 2147483648, NULL");
    ASSERT_TRUE(query.next());
    EXPECT_THROW(static_cast<void>(query.integer(0)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.text(1)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.integer32(2)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.text(3)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.text(4)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(query.integer(-1)), std::out_of_range);
    EXPECT_EQ(query.integer(2), 2147483648LL);
}

TEST_F(SqliteTest, NullableTextKeepsNullDistinctFromEmptyAndRejectsOtherTypes)
{
    sqlite::Connection connection(path_);
    connection.execute("CREATE TABLE sample (value TEXT)");
    sqlite::Statement insert(connection, "INSERT INTO sample (value) VALUES (?)");
    insert.bindNull(1);
    insert.run();
    insert.bind(1, "");
    insert.run();
    sqlite::Statement query(connection, "SELECT value FROM sample ORDER BY rowid");
    ASSERT_TRUE(query.next());
    EXPECT_FALSE(query.optionalText(0));
    ASSERT_TRUE(query.next());
    ASSERT_TRUE(query.optionalText(0));
    EXPECT_EQ(*query.optionalText(0), "");
    EXPECT_THROW(static_cast<void>(query.optionalText(1)), std::out_of_range);
    sqlite::Statement invalid(connection, "SELECT 42");
    ASSERT_TRUE(invalid.next());
    EXPECT_THROW(static_cast<void>(invalid.optionalText(0)), std::runtime_error);
}

TEST_F(SqliteTest, BoundedTextRejectsOversizedValuesWithoutChangingBytesOrTypeRules)
{
    sqlite::Connection connection(path_);
    sqlite::Statement query(connection, "SELECT 'abcd', '', 'a' || char(0) || 'b', 12, NULL");
    ASSERT_TRUE(query.next());
    EXPECT_THROW(static_cast<void>(query.text(0, 3)), std::runtime_error);
    EXPECT_EQ(query.text(0, 4), "abcd");
    EXPECT_EQ(query.text(0), "abcd");
    EXPECT_EQ(query.text(1, 0), "");
    EXPECT_THROW(static_cast<void>(query.text(2, 2)), std::runtime_error);
    EXPECT_EQ(query.text(2, 3), std::string("a\0b", 3));
    EXPECT_THROW(static_cast<void>(query.text(3, 10)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.text(4, 10)), std::runtime_error);
    EXPECT_THROW(static_cast<void>(query.text(-1, 10)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(query.text(5, 10)), std::out_of_range);
}

TEST_F(SqliteTest, FailedCommitRollsBackThenConnectionCanBeReused)
{
    sqlite::Connection connection(path_, 0);
    connection.execute("CREATE TABLE sample (value INTEGER); INSERT INTO sample VALUES (1)");
    test::RawDatabase reader(path_);
    reader.execute("BEGIN; SELECT value FROM sample");
    {
        sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
        connection.execute("INSERT INTO sample VALUES (2)");
        EXPECT_THROW(transaction.commit(), std::runtime_error);
    }
    reader.execute("ROLLBACK");
    {
        sqlite::Statement query(connection, "SELECT value FROM sample");
        ASSERT_TRUE(query.next());
        EXPECT_EQ(query.integer(0), 1);
        EXPECT_FALSE(query.next());
    }
    {
        sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
        connection.execute("INSERT INTO sample VALUES (3)");
        transaction.commit();
    }
    sqlite::Statement query(connection, "SELECT SUM(value) FROM sample");
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.integer(0), 4);
}

TEST_F(SqliteTest, UncommittedTransactionRollsBackDuringExceptionUnwinding)
{
    sqlite::Connection connection(path_);
    connection.execute("CREATE TABLE sample (value INTEGER)");
    EXPECT_THROW(
        {
            sqlite::Transaction transaction(connection, sqlite::TransactionMode::Write);
            connection.execute("INSERT INTO sample VALUES (1)");
            throw std::runtime_error("Abort the operation");
        },
        std::runtime_error);
    sqlite::Statement query(connection, "SELECT COUNT(*) FROM sample");
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.integer(0), 0);
}

}
}
