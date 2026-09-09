#include "dimenguard/storage/sqlite.h"

#include <atomic>
#include <chrono>
#include <limits>
#include <sqlite3.h>
#include <stdexcept>

namespace dimenguard::sqlite {
namespace {

std::string pathText(const std::filesystem::path &path)
{
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

}

void Connection::DatabaseCloser::operator()(sqlite3 *database) const noexcept
{
    sqlite3_close_v2(database);
}

Connection::Connection(const std::filesystem::path &path, int busy_timeout_ms) : path_(pathText(path))
{
    try {
        if (path.empty()) {
            throw std::invalid_argument("The database path must not be empty.");
        }
        if (busy_timeout_ms < 0) {
            throw std::invalid_argument("The SQLite busy timeout must not be negative.");
        }
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        sqlite3 *database = nullptr;
        const auto result = sqlite3_open_v2(
            path_.c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        database_.reset(database);
        if (result != SQLITE_OK) {
            throw std::runtime_error(database ? sqlite3_errmsg(database) : "SQLite could not allocate a connection.");
        }
        check(sqlite3_busy_timeout(database, busy_timeout_ms));
        execute("PRAGMA foreign_keys = ON");
    }
    catch (const std::exception &error) {
        throw std::runtime_error("Cannot open SQLite database '" + path_ + "': " + error.what());
    }
}

Connection::~Connection() = default;

const std::string &Connection::getPath() const
{
    return path_;
}

std::int64_t Connection::lastInsertId() const
{
    return sqlite3_last_insert_rowid(database_.get());
}

void Connection::execute(const char *sql) const
{
    check(sqlite3_exec(database_.get(), sql, nullptr, nullptr, nullptr));
}

std::filesystem::path Connection::backupBeforeMigration(int schema_version) const
{
    const auto *filename = sqlite3_db_filename(database_.get(), "main");
    if (filename == nullptr || *filename == '\0') {
        throw std::runtime_error("A schema migration requires a file-backed database for its backup.");
    }
    const std::string_view encoded_source(filename);
    const std::filesystem::path source_path(std::u8string(encoded_source.begin(), encoded_source.end()));
    static std::atomic<unsigned int> sequence{0};
    const auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::filesystem::path backup_directory;
    do {
        backup_directory = source_path;
        backup_directory += ".v" + std::to_string(schema_version) + "-backup-" + std::to_string(timestamp) + "-" +
                            std::to_string(sequence++);
    } while (!std::filesystem::create_directory(backup_directory));
    const auto backup_path = backup_directory / source_path.filename();

    // SQLite cannot back up a connection with an active write transaction. A separate reader sees
    // the same committed state because the caller holds BEGIN IMMEDIATE and has not changed it.
    sqlite3 *source_database = nullptr;
    const auto result =
        sqlite3_open_v2(filename, &source_database, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    const std::unique_ptr<sqlite3, DatabaseCloser> source(source_database);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Could not open the migration backup source: " +
                                 std::string(source ? sqlite3_errmsg(source.get()) : "SQLite allocation failed."));
    }
    Connection destination(backup_path);
    auto *backup = sqlite3_backup_init(destination.database_.get(), "main", source.get(), "main");
    if (backup == nullptr) {
        throw std::runtime_error("Could not initialize the migration backup: " +
                                 std::string(sqlite3_errmsg(destination.database_.get())));
    }
    const auto step_result = sqlite3_backup_step(backup, -1);
    const auto finish_result = sqlite3_backup_finish(backup);
    if (step_result != SQLITE_DONE || finish_result != SQLITE_OK) {
        throw std::runtime_error("Could not complete the migration backup: " +
                                 std::string(sqlite3_errmsg(destination.database_.get())));
    }
    return backup_path;
}

void Connection::check(int result) const
{
    if (result != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(database_.get()));
    }
}

Statement::Statement(const Connection &connection, const char *sql) : connection_(connection)
{
    if (sqlite3_prepare_v2(connection_.database_.get(), sql, -1, &statement_, nullptr) != SQLITE_OK) {
        const std::string message = sqlite3_errmsg(connection_.database_.get());
        sqlite3_finalize(statement_);
        throw std::runtime_error(message);
    }
}

Statement::~Statement()
{
    sqlite3_finalize(statement_);
}

void Statement::bind(int index, std::string_view value)
{
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("A text field exceeds SQLite's supported length.");
    }
    // A default-constructed string_view has a null data pointer, which SQLite treats as SQL NULL.
    const auto *data = value.empty() ? "" : value.data();
    connection_.check(sqlite3_bind_text(statement_, index, data, static_cast<int>(value.size()), SQLITE_TRANSIENT));
}

void Statement::bind(int index, std::int64_t value)
{
    connection_.check(sqlite3_bind_int64(statement_, index, value));
}

void Statement::bindNull(int index)
{
    connection_.check(sqlite3_bind_null(statement_, index));
}

bool Statement::next()
{
    const auto result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    throw std::runtime_error(sqlite3_errmsg(connection_.database_.get()));
}

void Statement::run()
{
    if (next()) {
        throw std::runtime_error("A SQLite write statement returned an unexpected row.");
    }
    connection_.check(sqlite3_reset(statement_));
    connection_.check(sqlite3_clear_bindings(statement_));
}

std::int64_t Statement::integer(int column) const
{
    requireType(column, SQLITE_INTEGER);
    return sqlite3_column_int64(statement_, column);
}

int Statement::integer32(int column) const
{
    const auto value = integer(column);
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        throw std::runtime_error("Stored column '" + std::string(columnName(column)) +
                                 "' is outside the supported 32-bit integer range.");
    }
    return static_cast<int>(value);
}

std::string Statement::text(int column) const
{
    return text(column, std::numeric_limits<std::size_t>::max());
}

std::string Statement::text(int column, std::size_t maximum_length) const
{
    requireType(column, SQLITE_TEXT);
    const auto *value = sqlite3_column_text(statement_, column);
    if (value == nullptr) {
        throw std::runtime_error("Could not read a text field: SQLite ran out of memory.");
    }
    const auto length = static_cast<std::size_t>(sqlite3_column_bytes(statement_, column));
    if (length > maximum_length) {
        throw std::runtime_error("Stored column '" + std::string(columnName(column)) +
                                 "' exceeds its supported length.");
    }
    return {reinterpret_cast<const char *>(value), length};
}

std::optional<std::string> Statement::optionalText(int column) const
{
    if (column < 0 || column >= sqlite3_column_count(statement_)) {
        throw std::out_of_range("The SQLite column index is outside the result set.");
    }
    if (sqlite3_column_type(statement_, column) == SQLITE_NULL) {
        return std::nullopt;
    }
    return text(column);
}

void Statement::requireType(int column, int expected) const
{
    if (column < 0 || column >= sqlite3_column_count(statement_)) {
        throw std::out_of_range("The SQLite column index is outside the result set.");
    }
    if (sqlite3_column_type(statement_, column) != expected) {
        throw std::runtime_error("Stored column '" + std::string(columnName(column)) + "' has an invalid SQL type.");
    }
}

std::string_view Statement::columnName(int column) const
{
    const auto *name = sqlite3_column_name(statement_, column);
    return name ? name : "<unknown>";
}

Transaction::Transaction(const Connection &connection, TransactionMode mode) : connection_(connection)
{
    connection_.execute(mode == TransactionMode::Write ? "BEGIN IMMEDIATE" : "BEGIN");
}

Transaction::~Transaction()
{
    if (!committed_) {
        sqlite3_exec(connection_.database_.get(), "ROLLBACK", nullptr, nullptr, nullptr);
    }
}

void Transaction::commit()
{
    connection_.execute("COMMIT");
    committed_ = true;
}

}
