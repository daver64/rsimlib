#include "rdb.h"

#include <cassert>
#include <filesystem>
#include <string>

int main()
{
    const std::filesystem::path database_path =
        std::filesystem::temp_directory_path() / "simlib_rdb_test.sqlite";
    std::error_code cleanup_error;
    std::filesystem::remove(database_path, cleanup_error);

    {
        rdb::Database database(database_path.string());
        database.execute("CREATE TABLE records (name TEXT NOT NULL, score INTEGER NOT NULL);");
        {
            rdb::Database::Transaction transaction(database);
            auto insert = database.prepare("INSERT INTO records (name, score) VALUES (?, ?);");
            insert->bind(1, std::string("alpha"));
            insert->bind(2, 10);
            assert(!insert->step());
            insert->reset();
            insert->bind(1, std::string("beta"));
            insert->bind(2, 20);
            assert(!insert->step());
            transaction.commit();
        }

        auto select = database.prepare("SELECT name, score FROM records ORDER BY score;");
        int rows = 0;
        int total = 0;
        select->forEachRow([&](rdb::Statement &row)
        {
            ++rows;
            total += row.getInt(1);
            assert(!row.getText(0).empty());
        });
        assert(rows == 2);
        assert(total == 30);
    }

    assert(std::filesystem::remove(database_path, cleanup_error));
    return 0;
}