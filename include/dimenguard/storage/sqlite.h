#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace dimenguard::sqlite {

class Connection {
public:
    explicit Connection(const std::filesystem::path &path, int busy_timeout_ms = 2000);
    ~Connection();

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    [[nodiscard]] const std::string &getPath() const;
    [[nodiscard]] std::int64_t lastInsertId() const;
    void execute(const char *sql) const;
    // Call before schema writes while this connection holds a reserved write transaction.
    [[nodiscard]] std::filesystem::path backupBeforeMigration(int schema_version) const;

private:
    friend class Statement;
    friend class Transaction;

    struct DatabaseCloser {
        void operator()(sqlite3 *database) const noexcept;
    };

    void check(int result) const;

    std::string path_;
    std::unique_ptr<sqlite3, DatabaseCloser> database_;
};

// The owning connection must outlive its statements and transactions.
class Statement {
public:
    Statement(const Connection &connection, const char *sql);
    ~Statement();

    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;

    void bind(int index, std::string_view value);
    void bind(int index, std::int64_t value);
    void bindNull(int index);
    [[nodiscard]] bool next();
    void run();
    [[nodiscard]] std::int64_t integer(int column) const;
    [[nodiscard]] int integer32(int column) const;
    [[nodiscard]] std::string text(int column) const;
    [[nodiscard]] std::string text(int column, std::size_t maximum_length) const;
    [[nodiscard]] std::optional<std::string> optionalText(int column) const;

private:
    void requireType(int column, int expected) const;
    [[nodiscard]] std::string_view columnName(int column) const;

    const Connection &connection_;
    sqlite3_stmt *statement_ = nullptr;
};

enum class TransactionMode {
    Read,
    Write,
};

class Transaction {
public:
    explicit Transaction(const Connection &connection, TransactionMode mode);
    ~Transaction();

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    void commit();

private:
    const Connection &connection_;
    bool committed_ = false;
};

}
