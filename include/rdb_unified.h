#pragma once

#include "rdb.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>

/**
 * @file rdb_unified.h
 * @brief Backend-independent database interfaces for SQLite, PostgreSQL, and MySQL.
 */

#if defined(RDB_ENABLE_POSTGRESQL)
#include <libpq-fe.h>
#endif
#if defined(RDB_ENABLE_MYSQL)
#include <mysql.h>
#endif

namespace rdb {

/**
 * @brief Identifies which database backend a unified interface is using.
 */
enum class BackendType { SQLite, PostgreSQL, MySQL };

/**
 * @brief Exception raised by the backend-independent API.
 */
class UnifiedException : public std::runtime_error {
public:
    explicit UnifiedException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief Common interface for a prepared statement across database backends.
 */
class IStatement {
public:
    virtual ~IStatement() {}
    virtual void bind(int index, int value) = 0;
    virtual void bind(int index, double value) = 0;
    virtual void bind(int index, const std::string& value) = 0;
    virtual void bindNull(int index) = 0;
    virtual bool step() = 0;
    virtual void reset() = 0;
    virtual int getInt(int column) const = 0;
    virtual double getDouble(int column) const = 0;
    virtual std::string getText(int column) const = 0;

    /**
     * @brief Iterates through every row in a result set.
     * @param callback Function invoked for each row.
     */
    void forEachRow(const std::function<void(IStatement&)>& callback) {
        while (step()) callback(*this);
        reset();
    }

    /**
     * @brief Maps each row to a typed result value.
     * @tparam T Destination value type.
     * @param mapper Conversion function for each row.
     * @return Vector containing each mapped row.
     */
    template<typename T>
    std::vector<T> mapRows(const std::function<T(IStatement&)>& mapper) {
        std::vector<T> rows;
        forEachRow([&](IStatement& statement) { rows.push_back(mapper(statement)); });
        return rows;
    }
};

/**
 * @brief Common interface for a database connection independent of the backend.
 */
class IDatabase {
public:
    /**
     * @brief RAII guard for transaction handling across backends.
     */
    class Transaction {
        IDatabase& database_;
        bool active_;
    public:
        explicit Transaction(IDatabase& database) : database_(database), active_(true) { database_.execute("BEGIN"); }
        ~Transaction() { if (active_) { try { database_.execute("ROLLBACK"); } catch (...) {} } }
        void commit() { database_.execute("COMMIT"); active_ = false; }
        void rollback() { database_.execute("ROLLBACK"); active_ = false; }
    };

    virtual ~IDatabase() {}
    virtual BackendType backend() const = 0;
    virtual void execute(const std::string& sql) = 0;
    virtual std::unique_ptr<IStatement> prepare(const std::string& sql) = 0;
};

/**
 * @brief SQLite-backed implementation of the unified statement interface.
 */
class SQLiteUnifiedStatement : public IStatement {
    sqlite3_stmt* statement_;
public:
    SQLiteUnifiedStatement(sqlite3* database, const std::string& sql) : statement_(nullptr) {
        if (sqlite3_prepare_v2(database, sql.c_str(), -1, &statement_, nullptr) != SQLITE_OK)
            throw UnifiedException(sqlite3_errmsg(database));
    }
    ~SQLiteUnifiedStatement() { if (statement_) sqlite3_finalize(statement_); }
    void bind(int index, int value) override { check(sqlite3_bind_int(statement_, index, value)); }
    void bind(int index, double value) override { check(sqlite3_bind_double(statement_, index, value)); }
    void bind(int index, const std::string& value) override { check(sqlite3_bind_text(statement_, index, value.c_str(), -1, SQLITE_TRANSIENT)); }
    void bindNull(int index) override { check(sqlite3_bind_null(statement_, index)); }
    bool step() override {
        const int result = sqlite3_step(statement_);
        if (result == SQLITE_ROW) return true;
        if (result == SQLITE_DONE) return false;
        throw UnifiedException(sqlite3_errmsg(sqlite3_db_handle(statement_)));
    }
    void reset() override { sqlite3_reset(statement_); sqlite3_clear_bindings(statement_); }
    int getInt(int column) const override { return sqlite3_column_int(statement_, column); }
    double getDouble(int column) const override { return sqlite3_column_double(statement_, column); }
    std::string getText(int column) const override {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(statement_, column));
        return text ? text : "";
    }
private:
    static void check(int result) { if (result != SQLITE_OK) throw UnifiedException("SQLite bind failed"); }
};

/**
 * @brief SQLite-backed implementation of the unified database interface.
 */
class SQLiteUnifiedDatabase : public IDatabase {
    sqlite3* database_;
public:
    explicit SQLiteUnifiedDatabase(const std::string& filename) : database_(nullptr) {
        if (sqlite3_open(filename.c_str(), &database_) != SQLITE_OK)
            throw UnifiedException(database_ ? sqlite3_errmsg(database_) : "SQLite open failed");
    }
    ~SQLiteUnifiedDatabase() { if (database_) sqlite3_close(database_); }
    BackendType backend() const override { return BackendType::SQLite; }
    void execute(const std::string& sql) override {
        char* error = nullptr;
        if (sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error ? error : "SQLite execution failed";
            sqlite3_free(error);
            throw UnifiedException(message);
        }
    }
    std::unique_ptr<IStatement> prepare(const std::string& sql) override {
        return std::unique_ptr<IStatement>(new SQLiteUnifiedStatement(database_, sql));
    }
};

#if defined(RDB_ENABLE_POSTGRESQL)
/**
 * @brief PostgreSQL-backed prepared statement for the unified interface.
 */
class PostgreSQLUnifiedStatement : public IStatement {
    PGconn* connection_;
    std::string name_;
    std::vector<std::string> values_;
    std::vector<const char*> parameters_;
    PGresult* result_;
    int row_;
public:
    PostgreSQLUnifiedStatement(PGconn* connection, const std::string& sql, const std::string& name)
        : connection_(connection), name_(name), result_(nullptr), row_(-1) {
        const std::string postgres_sql = translatePlaceholders(sql);
        PGresult* prepared = PQprepare(connection_, name_.c_str(), postgres_sql.c_str(), 0, nullptr);
        if (!prepared || PQresultStatus(prepared) != PGRES_COMMAND_OK) {
            const std::string error = prepared ? PQresultErrorMessage(prepared) : PQerrorMessage(connection_);
            if (prepared) PQclear(prepared);
            throw UnifiedException(error);
        }
        PQclear(prepared);
    }
    ~PostgreSQLUnifiedStatement() { if (result_) PQclear(result_); }
    void bind(int index, int value) override { set(index, std::to_string(value)); }
    void bind(int index, double value) override { set(index, std::to_string(value)); }
    void bind(int index, const std::string& value) override { set(index, value); }
    void bindNull(int index) override { ensure(index); values_[index - 1].clear(); parameters_[index - 1] = nullptr; }
    bool step() override {
        if (!result_) execute();
        if (row_ + 1 < PQntuples(result_)) { ++row_; return true; }
        return false;
    }
    void reset() override { if (result_) { PQclear(result_); result_ = nullptr; } row_ = -1; }
    int getInt(int column) const override { return std::atoi(getText(column).c_str()); }
    double getDouble(int column) const override { return std::atof(getText(column).c_str()); }
    std::string getText(int column) const override {
        if (!result_ || PQgetisnull(result_, row_, column)) return "";
        return PQgetvalue(result_, row_, column);
    }
private:
    static std::string translatePlaceholders(const std::string& sql) {
        std::string translated;
        int parameter = 0;
        bool quoted = false;
        for (size_t i = 0; i < sql.size(); ++i) {
            if (sql[i] == '\'' && (i == 0 || sql[i - 1] != '\\')) quoted = !quoted;
            if (sql[i] == '?' && !quoted) translated += "$" + std::to_string(++parameter);
            else translated += sql[i];
        }
        return translated;
    }
    void ensure(int index) { if (index < 1) throw UnifiedException("Parameter indexes start at 1"); if (static_cast<int>(values_.size()) < index) values_.resize(index); if (static_cast<int>(parameters_.size()) < index) parameters_.resize(index, nullptr); }
    void set(int index, const std::string& value) { ensure(index); values_[index - 1] = value; parameters_[index - 1] = values_[index - 1].c_str(); }
    void execute() {
        result_ = PQexecPrepared(connection_, name_.c_str(), static_cast<int>(parameters_.size()), parameters_.empty() ? nullptr : &parameters_[0], nullptr, nullptr, 0);
        if (!result_ || (PQresultStatus(result_) != PGRES_TUPLES_OK && PQresultStatus(result_) != PGRES_COMMAND_OK)) {
            const std::string error = result_ ? PQresultErrorMessage(result_) : PQerrorMessage(connection_);
            if (result_) { PQclear(result_); result_ = nullptr; }
            throw UnifiedException(error);
        }
    }
};

/**
 * @brief PostgreSQL-backed implementation of the unified database interface.
 */
class PostgreSQLUnifiedDatabase : public IDatabase {
    PGconn* connection_;
    unsigned long statement_id_;
public:
    explicit PostgreSQLUnifiedDatabase(const std::string& conninfo) : connection_(PQconnectdb(conninfo.c_str())), statement_id_(0) {
        if (!connection_ || PQstatus(connection_) != CONNECTION_OK) {
            const std::string error = connection_ ? PQerrorMessage(connection_) : "PostgreSQL open failed";
            if (connection_) PQfinish(connection_);
            connection_ = nullptr;
            throw UnifiedException(error);
        }
    }
    ~PostgreSQLUnifiedDatabase() { if (connection_) PQfinish(connection_); }
    BackendType backend() const override { return BackendType::PostgreSQL; }
    void execute(const std::string& sql) override { run(sql); }
    std::unique_ptr<IStatement> prepare(const std::string& sql) override {
        return std::unique_ptr<IStatement>(new PostgreSQLUnifiedStatement(connection_, sql, "rdb_stmt_" + std::to_string(++statement_id_)));
    }
private:
    void run(const std::string& sql) {
        PGresult* result = PQexec(connection_, sql.c_str());
        if (!result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_TUPLES_OK)) {
            const std::string error = result ? PQresultErrorMessage(result) : PQerrorMessage(connection_);
            if (result) PQclear(result);
            throw UnifiedException(error);
        }
        PQclear(result);
    }
};
#endif

#if defined(RDB_ENABLE_MYSQL)
/**
 * @brief MySQL-backed prepared statement for the unified interface.
 */
class MySQLUnifiedStatement : public IStatement {
    MYSQL_STMT* statement_;
    std::vector<std::string> values_;
    std::vector<MYSQL_BIND> parameters_;
    MYSQL_RES* metadata_;
    std::vector<MYSQL_BIND> results_;
    std::vector<std::vector<char> > buffers_;
    std::vector<unsigned long> lengths_;
    std::vector<my_bool> nulls_;
    bool executed_;
public:
    MySQLUnifiedStatement(MYSQL* connection, const std::string& sql)
        : statement_(mysql_stmt_init(connection)), metadata_(nullptr), executed_(false) {
        if (!statement_ || mysql_stmt_prepare(statement_, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
            throw UnifiedException(statement_ ? mysql_stmt_error(statement_) : "MySQL statement initialization failed");
    }
    ~MySQLUnifiedStatement() { if (metadata_) mysql_free_result(metadata_); if (statement_) mysql_stmt_close(statement_); }
    void bind(int index, int value) override { set(index, std::to_string(value)); }
    void bind(int index, double value) override { set(index, std::to_string(value)); }
    void bind(int index, const std::string& value) override { set(index, value); }
    void bindNull(int index) override { set(index, std::string(), true); }
    bool step() override {
        if (!executed_) execute();
        const int result = mysql_stmt_fetch(statement_);
        if (result == 0 || result == MYSQL_DATA_TRUNCATED) return true;
        if (result == MYSQL_NO_DATA) return false;
        throw UnifiedException(mysql_stmt_error(statement_));
    }
    void reset() override { mysql_stmt_free_result(statement_); mysql_stmt_reset(statement_); executed_ = false; }
    int getInt(int column) const override { return std::atoi(getText(column).c_str()); }
    double getDouble(int column) const override { return std::atof(getText(column).c_str()); }
    std::string getText(int column) const override { return (column >= 0 && column < static_cast<int>(buffers_.size()) && !nulls_[column]) ? std::string(&buffers_[column][0], lengths_[column]) : ""; }
private:
    void set(int index, const std::string& value, bool is_null = false) {
        if (index < 1) throw UnifiedException("Parameter indexes start at 1");
        if (static_cast<int>(values_.size()) < index) values_.resize(index);
        values_[index - 1] = value;
        if (parameters_.size() < values_.size()) parameters_.resize(values_.size());
        if (nulls_.size() < values_.size()) nulls_.resize(values_.size(), 0);
        std::memset(&parameters_[index - 1], 0, sizeof(MYSQL_BIND));
        parameters_[index - 1].buffer_type = MYSQL_TYPE_STRING;
        parameters_[index - 1].buffer = const_cast<char*>(values_[index - 1].data());
        parameters_[index - 1].buffer_length = static_cast<unsigned long>(values_[index - 1].size());
        parameters_[index - 1].is_null = is_null ? &nulls_[index - 1] : nullptr;
        nulls_[index - 1] = is_null;
    }
    void execute() {
        if (!parameters_.empty() && mysql_stmt_bind_param(statement_, &parameters_[0]) != 0) throw UnifiedException(mysql_stmt_error(statement_));
        if (mysql_stmt_execute(statement_) != 0) throw UnifiedException(mysql_stmt_error(statement_));
        metadata_ = mysql_stmt_result_metadata(statement_);
        if (!metadata_) { executed_ = true; return; }
        const unsigned int count = mysql_num_fields(metadata_);
        results_.resize(count); buffers_.resize(count, std::vector<char>(4096)); lengths_.resize(count); nulls_.resize(count);
        for (unsigned int i = 0; i < count; ++i) { std::memset(&results_[i], 0, sizeof(MYSQL_BIND)); results_[i].buffer_type = MYSQL_TYPE_STRING; results_[i].buffer = &buffers_[i][0]; results_[i].buffer_length = buffers_[i].size(); results_[i].length = &lengths_[i]; results_[i].is_null = &nulls_[i]; }
        if (mysql_stmt_bind_result(statement_, &results_[0]) != 0 || mysql_stmt_store_result(statement_) != 0) throw UnifiedException(mysql_stmt_error(statement_));
        executed_ = true;
    }
};

/**
 * @brief MySQL-backed implementation of the unified database interface.
 */
class MySQLUnifiedDatabase : public IDatabase {
    MYSQL* connection_;
public:
    MySQLUnifiedDatabase(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port = 0) : connection_(mysql_init(nullptr)) {
        if (!connection_ || !mysql_real_connect(connection_, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0)) {
            const std::string error = connection_ ? mysql_error(connection_) : "MySQL open failed";
            if (connection_) mysql_close(connection_);
            connection_ = nullptr;
            throw UnifiedException(error);
        }
    }
    ~MySQLUnifiedDatabase() { if (connection_) mysql_close(connection_); }
    BackendType backend() const override { return BackendType::MySQL; }
    void execute(const std::string& sql) override { if (mysql_query(connection_, sql.c_str()) != 0) throw UnifiedException(mysql_error(connection_)); }
    std::unique_ptr<IStatement> prepare(const std::string& sql) override { return std::unique_ptr<IStatement>(new MySQLUnifiedStatement(connection_, sql)); }
};
#endif

/**
 * @brief Creates a SQLite-backed unified database object.
 * @param filename SQLite database file path.
 * @return Database instance implementing IDatabase.
 */
inline std::unique_ptr<IDatabase> makeSQLiteDatabase(const std::string& filename) { return std::unique_ptr<IDatabase>(new SQLiteUnifiedDatabase(filename)); }
#if defined(RDB_ENABLE_POSTGRESQL)
inline std::unique_ptr<IDatabase> makePostgreSQLDatabase(const std::string& conninfo) { return std::unique_ptr<IDatabase>(new PostgreSQLUnifiedDatabase(conninfo)); }
#endif
#if defined(RDB_ENABLE_MYSQL)
inline std::unique_ptr<IDatabase> makeMySQLDatabase(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port = 0) { return std::unique_ptr<IDatabase>(new MySQLUnifiedDatabase(host, user, password, database, port)); }
inline std::unique_ptr<IDatabase> makeMariaDBDatabase(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port = 0) { return makeMySQLDatabase(host, user, password, database, port); }
#endif

} // namespace rdb
