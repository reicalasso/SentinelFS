#pragma once

#include <sqlite3.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include "Logger.h"

namespace SentinelFS {

/**
 * @brief RAII Transaction class for automatic rollback on exception
 */
class Transaction {
public:
    explicit Transaction(sqlite3* db);
    ~Transaction();
    
    void commit();
    void rollback();
    
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    
private:
    sqlite3* db_;
    bool committed_;
    std::string rollbackSavepoint_;
};

/**
 * @brief Prepared statement wrapper for type-safe parameter binding
 */
class PreparedStatement {
public:
    PreparedStatement(sqlite3* db, const std::string& sql);
    ~PreparedStatement();
    
    PreparedStatement& bind(int index, int value);
    PreparedStatement& bind(int index, int64_t value);
    PreparedStatement& bind(int index, double value);
    PreparedStatement& bind(int index, const std::string& value);
    PreparedStatement& bind(int index, const std::vector<uint8_t>& value);
    PreparedStatement& bindNull(int index);
    
    bool step();
    void reset();
    void clearBindings();
    
    int getColumnInt(int index) const;
    int64_t getColumnInt64(int index) const;
    double getColumnDouble(int index) const;
    std::string getColumnString(int index) const;
    std::vector<uint8_t> getColumnBlob(int index) const;
    
    std::string getColumnName(int index) const;
    
    bool hasData() const;
    int getColumnCount() const;
    
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;
    
private:
    sqlite3_stmt* stmt_;
    sqlite3* db_;
};

/**
 * @brief Database connection manager with prepared statement cache
 * 
 * Features:
 * - Thread-safe single connection with mutex
 * - Prepared statement cache for performance
 * - WAL mode for better concurrency
 * - Migration system
 * - RAII transactions
 */
class DatabaseManager {
public:
    /**
     * @brief Migration definition
     */
    struct Migration {
        int version;
        std::string description;
        std::string upSql;   // SQL to upgrade
        std::string downSql; // SQL to downgrade
    };
    
    explicit DatabaseManager(const std::string& dbPath);
    ~DatabaseManager();
    
    // Prevent copying
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    /**
     * @brief Initialize database with WAL mode and run migrations
     * @param migrations List of migrations to apply
     * @return true on success
     */
    bool initialize(const std::vector<Migration>& migrations = {});
    
    /**
     * @brief Get or create prepared statement from cache
     * @param sql SQL query string
     * @return Prepared statement pointer (caller must hold lock or use within transaction)
     */
    std::shared_ptr<PreparedStatement> prepare(const std::string& sql);
    
    /**
     * @brief Execute prepared statement with automatic locking
     * @param sql SQL query string
     * @param binder Function to bind parameters
     * @return true if step() returns at least one row
     */
    bool executeQuery(const std::string& sql, std::function<void(PreparedStatement&)> binder = nullptr);
    
    /**
     * @brief Execute query and return single row result
     * @param sql SQL query string
     * @param binder Function to bind parameters
     * @return Row data as map of column names to values
     */
    std::unordered_map<std::string, std::string> queryOne(const std::string& sql, std::function<void(PreparedStatement&)> binder = nullptr);
    
    /**
     * @brief Execute query and return all rows
     * @param sql SQL query string
     * @param binder Function to bind parameters
     * @return Vector of rows, each as map of column names to values
     */
    std::vector<std::unordered_map<std::string, std::string>> query(const std::string& sql, std::function<void(PreparedStatement&)> binder = nullptr);
    
    /**
     * @brief Execute SQL directly (for DDL/one-off queries)
     * @param sql SQL to execute
     * @return true on success
     */
    bool execute(const std::string& sql);
    
    /**
     * @brief Begin transaction
     * @return Transaction object
     */
    std::unique_ptr<Transaction> beginTransaction();
    
    /**
     * @brief Get last insert row ID
     */
    int64_t lastInsertRowId() const;
    
    /**
     * @brief Get number of rows affected by last statement
     */
    int changes() const;
    
    /**
     * @brief Get last error message
     */
    std::string getLastError() const;
    
    /**
     * @brief Enable/disable foreign key constraints
     */
    void setForeignKeysEnabled(bool enabled);
    
    /**
     * @brief Set busy timeout (ms)
     */
    void setBusyTimeout(int timeoutMs);
    
    // Internal methods (no locking - used during initialization)
    void setForeignKeysEnabledInternal(bool enabled);
    void setBusyTimeoutInternal(int timeoutMs);
    bool executeInternal(const std::string& sql);
    int getCurrentVersionInternal();
    bool setVersionInternal(int version);
    std::unique_ptr<Transaction> beginTransactionInternal();
    
    /**
     * @brief Vacuum database
     */
    bool vacuum();
    
    /**
     * @brief Clear prepared statement cache
     */
    void clearCache();
    
    /**
     * @brief Get database connection stats
     */
    struct Stats {
        int cacheHits;
        int cacheMisses;
        int totalQueries;
        int activeTransactions;
    };
    Stats getStats() const;
    
    // Common query helpers (semantic names for frequent operations)
    std::shared_ptr<PreparedStatement> getInsertFileStmt();
    std::shared_ptr<PreparedStatement> getSelectFileStmt();
    std::shared_ptr<PreparedStatement> getInsertOperationStmt();
    std::shared_ptr<PreparedStatement> getSelectPendingOpsStmt();
    
private:
    sqlite3* db_;
    std::string dbPath_;
    mutable std::recursive_mutex mutex_;
    
    // Prepared statement cache
    std::unordered_map<std::string, std::shared_ptr<PreparedStatement>> statementCache_;
    
    // Common prepared statements
    std::shared_ptr<PreparedStatement> insertFileStmt_;
    std::shared_ptr<PreparedStatement> selectFileStmt_;
    std::shared_ptr<PreparedStatement> insertOperationStmt_;
    std::shared_ptr<PreparedStatement> selectPendingOpsStmt_;
    
    // Statistics
    mutable Stats stats_;
    
    bool enableWALMode();
    bool runMigrations(const std::vector<Migration>& migrations);
    int getCurrentVersion();
    bool setVersion(int version);
    void initializeCommonStatements();
};

} // namespace SentinelFS
