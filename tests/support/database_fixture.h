#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace dimenguard::test {

// Tests use the C API directly so fixture behavior cannot hide a production wrapper failure.
class RawDatabase {
public:
    explicit RawDatabase(const std::filesystem::path &path)
    {
        const auto encoded_path = path.u8string();
        const std::string filename(encoded_path.begin(), encoded_path.end());
        sqlite3 *database = nullptr;
        const auto result = sqlite3_open(filename.c_str(), &database);
        database_.reset(database);
        check(result);
    }

    void execute(std::string_view sql) const
    {
        const std::string statement(sql);
        check(sqlite3_exec(database_.get(), statement.c_str(), nullptr, nullptr, nullptr));
    }

private:
    struct Closer {
        void operator()(sqlite3 *database) const noexcept { sqlite3_close_v2(database); }
    };

    void check(int result) const
    {
        if (result != SQLITE_OK) {
            throw std::runtime_error(database_ ? sqlite3_errmsg(database_.get())
                                               : "Could not create a SQLite test connection.");
        }
    }

    std::unique_ptr<sqlite3, Closer> database_;
};

class DatabaseFixture : public testing::Test {
protected:
    void SetUp() override
    {
        static std::atomic<unsigned int> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
                     ("dimenguard-test-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
        owns_directory_ = std::filesystem::create_directory(directory_);
        ASSERT_TRUE(owns_directory_);
        path_ = directory_ / "regions.sqlite3";
    }

    void TearDown() override
    {
        if (owns_directory_) {
            std::error_code error;
            std::filesystem::remove_all(directory_, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void executeRaw(std::string_view sql) const { RawDatabase(path_).execute(sql); }

    std::filesystem::path directory_;
    std::filesystem::path path_;

private:
    bool owns_directory_ = false;
};

}
