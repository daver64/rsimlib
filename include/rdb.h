#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <memory>
#include <functional>
#include <iostream>

/**
 * @file rdb.h
 * @brief Main SQLite wrapper interface for the project.
 *
 * This header provides both a PHP-like compatibility layer and a modern
 * RAII-based C++ API for working with SQLite databases.
 */

namespace rdb {

/**
 * @brief Exception raised when SQLite reports an error.
 */
class SQLiteException : public std::runtime_error {
public:
    SQLiteException(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @brief Represents a SQLite database connection.
 *
 * The object owns the underlying sqlite3 handle and provides RAII-based
 * lifecycle management for queries and transactions.
 */
class Database {
    sqlite3* db_ = nullptr;

public:
    /**
     * @brief Opens or creates a SQLite database file.
     * @param filename Path to the SQLite database file.
     * @throws SQLiteException if the database cannot be opened.
     */
    Database(const std::string& filename) {
        if (sqlite3_open(filename.c_str(), &db_) != SQLITE_OK) {
            throw SQLiteException(sqlite3_errmsg(db_));
        }
    }

    /**
     * @brief Closes the database connection.
     */
    ~Database() { if (db_) sqlite3_close(db_); }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }
    Database& operator=(Database&& other) noexcept {
        if (db_) sqlite3_close(db_);
        db_ = other.db_;
        other.db_ = nullptr;
        return *this;
    }

    /**
     * @brief Returns the underlying sqlite3 handle.
     * @return Pointer to the native SQLite connection object.
     */
    sqlite3* get() { return db_; }

    /**
     * @brief Prepares a SQL statement for execution.
     * @param sql SQL text to prepare.
     * @return Prepared statement instance.
     */
    std::unique_ptr<class Statement> prepare(const std::string& sql);

    /**
     * @brief Executes a SQL statement that does not return rows.
     * @param sql SQL command to execute.
     * @throws SQLiteException if the query fails.
     */
    void execute(const std::string& sql) {
        char* errmsg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "Unknown error";
            sqlite3_free(errmsg);
            throw SQLiteException(msg);
        }
    }

    /**
     * @brief RAII transaction guard that automatically rolls back on destruction.
     */
    class Transaction {
        Database& db_;
        bool active_ = true;
    public:
        /**
         * @brief Starts a transaction.
         * @param db Database to wrap in a transaction.
         */
        Transaction(Database& db) : db_(db) { db_.execute("BEGIN;"); }
        /**
         * @brief Rolls back if the transaction is still active.
         */
        ~Transaction() { if(active_) db_.execute("ROLLBACK;"); }
        /**
         * @brief Commits the transaction.
         */
        void commit() { db_.execute("COMMIT;"); active_ = false; }
        /**
         * @brief Rolls back the transaction explicitly.
         */
        void rollback() { db_.execute("ROLLBACK;"); active_ = false; }
    };
};

/**
 * @brief Represents a prepared SQLite statement.
 */
class Statement {
    friend class DBConnect;
    sqlite3_stmt* stmt_ = nullptr;

public:
    /**
     * @brief Creates a prepared statement from SQL text.
     * @param db SQLite connection handle.
     * @param sql SQL text to prepare.
     * @throws SQLiteException if statement preparation fails.
     */
    Statement(sqlite3* db, const std::string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw SQLiteException(sqlite3_errmsg(db));
        }
    }

    /**
     * @brief Finalizes the statement and releases SQLite resources.
     */
    ~Statement() { if(stmt_) sqlite3_finalize(stmt_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept : stmt_(other.stmt_) { other.stmt_ = nullptr; }
    Statement& operator=(Statement&& other) noexcept {
        if(stmt_) sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
        return *this;
    }

    /**
     * @brief Binds an integer parameter by positional index.
     * @param index 1-based parameter index.
     * @param val Value to bind.
     */
    void bind(int index, int val) { sqlite3_bind_int(stmt_, index, val); }
    /**
     * @brief Binds a floating-point parameter by positional index.
     */
    void bind(int index, double val) { sqlite3_bind_double(stmt_, index, val); }
    /**
     * @brief Binds a text parameter by positional index.
     */
    void bind(int index, const std::string& val) {
        sqlite3_bind_text(stmt_, index, val.c_str(), -1, SQLITE_TRANSIENT);
    }

    /**
     * @brief Binds an integer parameter by named placeholder.
     * @param name Placeholder name such as ":name".
     */
    void bind(const std::string& name, int val) {
        int idx = sqlite3_bind_parameter_index(stmt_, name.c_str());
        if(idx) sqlite3_bind_int(stmt_, idx, val);
    }
    /**
     * @brief Binds a double parameter by named placeholder.
     */
    void bind(const std::string& name, double val) {
        int idx = sqlite3_bind_parameter_index(stmt_, name.c_str());
        if(idx) sqlite3_bind_double(stmt_, idx, val);
    }
    /**
     * @brief Binds a string parameter by named placeholder.
     */
    void bind(const std::string& name, const std::string& val) {
        int idx = sqlite3_bind_parameter_index(stmt_, name.c_str());
        if(idx) sqlite3_bind_text(stmt_, idx, val.c_str(), -1, SQLITE_TRANSIENT);
    }

    /**
     * @brief Advances the statement to the next row.
     * @return true while a row is available; false when the result set is exhausted.
     * @throws SQLiteException if SQLite reports an execution error.
     */
    bool step() {
        int rc = sqlite3_step(stmt_);
        if(rc == SQLITE_ROW) return true;
        if(rc == SQLITE_DONE) return false;
        throw SQLiteException(sqlite3_errmsg(sqlite3_db_handle(stmt_)));
    }

    /**
     * @brief Resets the statement so it can be executed again.
     */
    void reset() { sqlite3_reset(stmt_); }

    /**
     * @brief Reads an integer column from the current row.
     */
    int getInt(int col) { return sqlite3_column_int(stmt_, col); }
    /**
     * @brief Reads a double column from the current row.
     */
    double getDouble(int col) { return sqlite3_column_double(stmt_, col); }
    /**
     * @brief Reads a text column from the current row.
     */
    std::string getText(int col) {
        const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
        return txt ? txt : "";
    }

    /**
     * @brief Iterates all rows and invokes a callback for each row.
     * @param fn Callback applied to the current statement.
     */
    void forEachRow(const std::function<void(Statement&)>& fn) {
        while(step()) fn(*this);
        reset();
    }

    /**
     * @brief Maps each row into a value of type T.
     * @param mapper Function converting one row into a result value.
     * @return Vector of mapped values.
     */
    template<typename T>
    std::vector<T> mapRows(std::function<T(Statement&)> mapper) {
        std::vector<T> results;
        forEachRow([&](Statement& row){ results.push_back(mapper(row)); });
        return results;
    }

    /**
     * @brief Extracts a single column from all rows as a vector.
     * @tparam T Column value type.
     * @param colIndex Column index to read.
     */
    template<typename T>
    std::vector<T> column(int colIndex);
};

/**
 * @brief Prepares a SQL statement against the database.
 * @param sql SQL statement text.
 * @return Prepared statement container.
 */
inline std::unique_ptr<Statement> Database::prepare(const std::string& sql) {
    return std::unique_ptr<Statement>(new Statement(db_, sql));
}

template<>
inline std::vector<int> Statement::column<int>(int colIndex) {
    std::vector<int> res;
    while(step()) res.push_back(getInt(colIndex));
    reset();
    return res;
}

template<>
inline std::vector<double> Statement::column<double>(int colIndex) {
    std::vector<double> res;
    while(step()) res.push_back(getDouble(colIndex));
    reset();
    return res;
}

template<>
inline std::vector<std::string> Statement::column<std::string>(int colIndex) {
    std::vector<std::string> res;
    while(step()) res.push_back(getText(colIndex));
    reset();
    return res;
}

/**
 * @brief Alias for a row represented as a column-name to string map.
 */
using SQLRow = std::unordered_map<std::string, std::string>;

/**
 * @brief Stores the result set returned by a PHP-like query.
 */
class SQLResults {
public:
    SQLResults() : num_fields(0), num_rows(0), num_tuples(0) {}

    /**
     * @brief Clears all row data and resets counters.
     */
    void clear() {
        results.clear();
        num_fields = 0;
        num_rows = 0;
        num_tuples = 0;
        error_message.clear();
        row_iterator = results.begin();
    }

    /**
     * @brief Returns the number of rows stored.
     * @return Number of result rows.
     */
    size_t size() const { return results.size(); }

    std::vector<SQLRow> results;
    std::vector<SQLRow>::iterator row_iterator;
    size_t num_fields;
    size_t num_rows;
    size_t num_tuples;
    std::string error_message;
};

/**
 * @brief PHP-like database wrapper around the RAII SQLite interface.
 */
class DBConnect {
private:
    std::unique_ptr<Database> db_;

    /**
     * @brief Executes a query and populates a SQLResults object.
     * @param results Result container to fill.
     * @param sql SQL text to execute.
     */
    void executeQuery(SQLResults* results, const std::string& sql) {
        try {
            results->clear();
            auto stmt = db_->prepare(sql);

            int column_count = sqlite3_column_count(stmt->stmt_);

            while (stmt->step()) {
                SQLRow row;
                for (int i = 0; i < column_count; i++) {
                    const char* col_name = sqlite3_column_name(stmt->stmt_, i);
                    const char* col_text = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt->stmt_, i)
                    );
                    row[col_name ? col_name : ""] = col_text ? col_text : "";
                }
                results->results.push_back(row);
            }

            results->num_rows = results->results.size();
            results->num_fields = column_count;
            results->num_tuples = results->num_rows;
            results->row_iterator = results->results.begin();

        } catch (const SQLiteException& e) {
            results->error_message = e.what();
            results->num_rows = 0;
            results->num_fields = 0;
        }
    }

public:
    /**
     * @brief Creates an empty database wrapper.
     */
    DBConnect() {}
    /**
     * @brief Opens a SQLite database file.
     * @param filename File path of the database.
     */
    explicit DBConnect(const std::string& filename) {
        db_.reset(new Database(filename));
    }

    virtual ~DBConnect() {}

    /**
     * @brief Opens or recreates the database connection.
     * @param filename SQLite database file path.
     */
    void open(const std::string& filename) {
        db_.reset(new Database(filename));
    }

    /**
     * @brief Executes a SQL statement and stores all returned rows.
     * @param results Container for the query results.
     * @param sql SQL text to run.
     */
    void query(SQLResults* results, const std::string& sql) {
        executeQuery(results, sql);
    }

    /**
     * @brief Executes a SQL statement and stores all returned rows.
     * @param results Container for the query results.
     * @param sql SQL text to run.
     */
    void query(SQLResults* results, const char* sql) {
        executeQuery(results, std::string(sql));
    }

    /**
     * @brief Executes a non-result SQL statement such as INSERT or UPDATE.
     * @param sql SQL command to run.
     */
    void query(const std::string& sql) {
        try {
            db_->execute(sql);
        } catch (const SQLiteException& e) {
            std::cerr << "Query error: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Executes a non-result SQL statement such as INSERT or UPDATE.
     * @param sql SQL command to run.
     */
    void query(const char* sql) {
        query(std::string(sql));
    }

    /**
     * @brief Fetches the next row from a result set in PHP-style iteration.
     * @param results Result set being iterated.
     * @param row Output row object.
     * @return true when a row was retrieved; false otherwise.
     */
    bool fetch_array(SQLResults* results, SQLRow* row) {
        if (results->num_rows == 0)
            return false;

        if (results->row_iterator != results->results.end()) {
            *row = *results->row_iterator;
            results->row_iterator++;
            return true;
        }
        return false;
    }

    /**
     * @brief Returns the row id of the last insert statement.
     * @return Last inserted row id, or 0 if the database is not open.
     */
    int64_t last_rowid() {
        if (!db_) return 0;
        return sqlite3_last_insert_rowid(db_->get());
    }

    /**
     * @brief Checks whether a table exists in the connected database.
     * @param table_name Name of the table to look for.
     * @return true if the table exists; false otherwise.
     */
    bool does_table_exist(const std::string& table_name) {
        SQLResults results;
        std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + table_name + "';";
        query(&results, sql);
        return results.num_rows > 0;
    }

    /**
     * @brief Returns the underlying modern SQLite database object.
     * @return Pointer to the wrapped Database instance.
     */
    Database* getDatabase() { return db_.get(); }
};

/**
 * @brief Escapes single quote characters for safe use in SQL string literals.
 * @param src Raw input string.
 * @return Escaped string suitable for SQL insertion.
 */
inline std::string sql_escape(const std::string& src) {
    std::string result;
    result.reserve(src.length() * 2);
    for (char c : src) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace rdb
