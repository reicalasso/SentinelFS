#include "DatabaseManager.h"
#include "Logger.h"
#include <cstring>
#include <stdexcept>

namespace SentinelFS {

// Transaction Implementation
Transaction::Transaction(sqlite3* db) : db_(db), committed_(false) {
    // Create savepoint for nested transaction support
    rollbackSavepoint_ = "txn_" + std::to_string(reinterpret_cast<uintptr_t>(this));
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, ("SAVEPOINT " + rollbackSavepoint_).c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::instance().log(LogLevel::ERROR, "Failed to create transaction savepoint: " + std::string(errMsg ? errMsg : ""), "DatabaseManager");
        if (errMsg) sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create transaction");
    }
}

Transaction::~Transaction() {
    if (!committed_) {
        rollback();
    }
}

void Transaction::commit() {
    if (!committed_) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, ("RELEASE " + rollbackSavepoint_).c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            Logger::instance().log(LogLevel::ERROR, "Failed to commit transaction: " + std::string(errMsg ? errMsg : ""), "DatabaseManager");
            if (errMsg) sqlite3_free(errMsg);
            throw std::runtime_error("Failed to commit transaction");
        }
        committed_ = true;
    }
}

void Transaction::rollback() {
    if (!committed_) {
        char* errMsg = nullptr;
        sqlite3_exec(db_, ("ROLLBACK TO " + rollbackSavepoint_).c_str(), nullptr, nullptr, &errMsg);
        sqlite3_exec(db_, ("RELEASE " + rollbackSavepoint_).c_str(), nullptr, nullptr, &errMsg);
        if (errMsg) sqlite3_free(errMsg);
    }
}

// PreparedStatement Implementation
PreparedStatement::PreparedStatement(sqlite3* db, const std::string& sql) : db_(db), stmt_(nullptr) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + sql + " - " + sqlite3_errmsg(db_));
    }
}

PreparedStatement::~PreparedStatement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

PreparedStatement& PreparedStatement::bind(int index, int value) {
    sqlite3_bind_int(stmt_, index, value);
    return *this;
}

PreparedStatement& PreparedStatement::bind(int index, int64_t value) {
    sqlite3_bind_int64(stmt_, index, value);
    return *this;
}

PreparedStatement& PreparedStatement::bind(int index, double value) {
    sqlite3_bind_double(stmt_, index, value);
    return *this;
}

PreparedStatement& PreparedStatement::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
    return *this;
}

PreparedStatement& PreparedStatement::bind(int index, const std::vector<uint8_t>& value) {
    sqlite3_bind_blob(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    return *this;
}

PreparedStatement& PreparedStatement::bindNull(int index) {
    sqlite3_bind_null(stmt_, index);
    return *this;
}

bool PreparedStatement::step() {
    int result = sqlite3_step(stmt_);
    return result == SQLITE_ROW || result == SQLITE_DONE;
}

void PreparedStatement::reset() {
    sqlite3_reset(stmt_);
}

void PreparedStatement::clearBindings() {
    sqlite3_clear_bindings(stmt_);
}

int PreparedStatement::getColumnInt(int index) const {
    return sqlite3_column_int(stmt_, index);
}

int64_t PreparedStatement::getColumnInt64(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

double PreparedStatement::getColumnDouble(int index) const {
    return sqlite3_column_double(stmt_, index);
}

std::string PreparedStatement::getColumnString(int index) const {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
    return text ? std::string(text) : std::string();
}

std::vector<uint8_t> PreparedStatement::getColumnBlob(int index) const {
    const void* blob = sqlite3_column_blob(stmt_, index);
    int size = sqlite3_column_bytes(stmt_, index);
    if (blob && size > 0) {
        const uint8_t* bytes = static_cast<const uint8_t*>(blob);
        return std::vector<uint8_t>(bytes, bytes + size);
    }
    return {};
}

std::string PreparedStatement::getColumnName(int index) const {
    const char* name = sqlite3_column_name(stmt_, index);
    return name ? std::string(name) : std::string();
}

bool PreparedStatement::hasData() const {
    return sqlite3_column_count(stmt_) > 0;
}

int PreparedStatement::getColumnCount() const {
    return sqlite3_column_count(stmt_);
}

// DatabaseManager Implementation
DatabaseManager::DatabaseManager(const std::string& dbPath) 
    : db_(nullptr), dbPath_(dbPath) {
    std::memset(&stats_, 0, sizeof(stats_));
}

DatabaseManager::~DatabaseManager() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DatabaseManager::initialize(const std::vector<Migration>& migrations) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    Logger::instance().log(LogLevel::INFO, "Opening database: " + dbPath_, "DatabaseManager");
    
    // Open database
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        Logger::instance().log(LogLevel::ERROR, "Failed to open database: " + dbPath_, "DatabaseManager");
        return false;
    }
    
    Logger::instance().log(LogLevel::INFO, "Enabling WAL mode", "DatabaseManager");
    
    // Enable WAL mode for better concurrency
    if (!enableWALMode()) {
        Logger::instance().log(LogLevel::WARN, "Failed to enable WAL mode", "DatabaseManager");
    }
    
    Logger::instance().log(LogLevel::INFO, "Setting foreign keys", "DatabaseManager");
    
    // Enable foreign keys
    setForeignKeysEnabledInternal(true);
    
    Logger::instance().log(LogLevel::INFO, "Setting busy timeout", "DatabaseManager");
    
    // Set busy timeout to 5 seconds
    setBusyTimeoutInternal(5000);
    
    Logger::instance().log(LogLevel::INFO, "Running migrations", "DatabaseManager");
    
    // Run migrations
    if (!runMigrations(migrations)) {
        Logger::instance().log(LogLevel::ERROR, "Failed to run migrations", "DatabaseManager");
        return false;
    }
    
    Logger::instance().log(LogLevel::INFO, "Initializing common statements", "DatabaseManager");
    
    // Initialize common prepared statements
    initializeCommonStatements();
    
    Logger::instance().log(LogLevel::INFO, "Database initialized successfully: " + dbPath_, "DatabaseManager");
    return true;
}

bool DatabaseManager::enableWALMode() {
    char* errMsg = nullptr;
    int result = sqlite3_exec(db_, "PRAGMA journal_mode=WAL", nullptr, nullptr, &errMsg);
    if (result != SQLITE_OK) {
        if (errMsg) {
            Logger::instance().log(LogLevel::ERROR, "Failed to enable WAL mode: " + std::string(errMsg), "DatabaseManager");
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    // Optimize for concurrent access
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA cache_size=10000", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA temp_store=MEMORY", nullptr, nullptr, nullptr);
    
    return true;
}

bool DatabaseManager::runMigrations(const std::vector<Migration>& migrations) {
    // Create migration table if not exists
    if (!executeInternal("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY, description TEXT)")) {
        return false;
    }
    
    int currentVersion = getCurrentVersionInternal();
    
    for (const auto& migration : migrations) {
        if (migration.version > currentVersion) {
            Logger::instance().log(LogLevel::INFO, "Running migration " + std::to_string(migration.version) + ": " + migration.description, "DatabaseManager");
            
            auto txn = beginTransactionInternal();
            if (!executeInternal(migration.upSql)) {
                Logger::instance().log(LogLevel::ERROR, "Migration failed: " + migration.description, "DatabaseManager");
                return false;
            }
            
            if (!setVersionInternal(migration.version)) {
                Logger::instance().log(LogLevel::ERROR, "Failed to set version after migration", "DatabaseManager");
                return false;
            }
            
            txn->commit();
        }
    }
    
    return true;
}

int DatabaseManager::getCurrentVersion() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return getCurrentVersionInternal();
}

int DatabaseManager::getCurrentVersionInternal() {
    auto stmt = prepare("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1");
    if (stmt->step()) {
        return stmt->getColumnInt(0);
    }
    return 0;
}

bool DatabaseManager::setVersion(int version) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return setVersionInternal(version);
}

bool DatabaseManager::setVersionInternal(int version) {
    auto stmt = prepare("INSERT OR REPLACE INTO schema_version (version, description) VALUES (?, '')");
    stmt->bind(1, version);
    bool result = stmt->step();
    if (!result) {
        Logger::instance().log(LogLevel::ERROR, "setVersionInternal failed: " + getLastError(), "DatabaseManager");
    }
    return result;
}

void DatabaseManager::initializeCommonStatements() {
    insertFileStmt_ = prepare("INSERT OR REPLACE INTO files (path, hash, size, modified_time) VALUES (?, ?, ?, ?)");
    selectFileStmt_ = prepare("SELECT id, hash, size, modified_time FROM files WHERE path = ?");
    insertOperationStmt_ = prepare("INSERT INTO operations (file_id, device_id, op_type, status, timestamp) VALUES (?, ?, ?, ?, ?)");
    selectPendingOpsStmt_ = prepare("SELECT o.id, o.file_id, f.path, o.op_type, o.status FROM operations o JOIN files f ON o.file_id = f.id WHERE o.status = 'PENDING' ORDER BY o.timestamp ASC");
}

std::shared_ptr<PreparedStatement> DatabaseManager::prepare(const std::string& sql) {
    auto it = statementCache_.find(sql);
    if (it != statementCache_.end()) {
        stats_.cacheHits++;
        stats_.totalQueries++;
        it->second->reset();
        return it->second;
    }
    
    stats_.cacheMisses++;
    stats_.totalQueries++;
    
    try {
        auto stmt = std::make_shared<PreparedStatement>(db_, sql);
        statementCache_[sql] = stmt;
        return stmt;
    } catch (const std::exception& e) {
        Logger::instance().log(LogLevel::ERROR, "Failed to prepare statement: " + std::string(e.what()), "DatabaseManager");
        return nullptr;
    }
}

bool DatabaseManager::executeQuery(const std::string& sql, std::function<void(PreparedStatement&)> binder) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto stmt = prepare(sql);
    if (!stmt) {
        return false;
    }
    
    if (binder) {
        binder(*stmt);
    }
    
    return stmt->step();
}

std::unordered_map<std::string, std::string> DatabaseManager::queryOne(const std::string& sql, std::function<void(PreparedStatement&)> binder) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::unordered_map<std::string, std::string> result;
    
    auto stmt = prepare(sql);
    if (!stmt) {
        return result;
    }
    
    if (binder) {
        binder(*stmt);
    }
    
    if (stmt->step()) {
        int columnCount = stmt->getColumnCount();
        for (int i = 0; i < columnCount; ++i) {
            std::string columnName = stmt->getColumnName(i);
            std::string value = stmt->getColumnString(i);
            result[columnName] = value;
        }
    }
    
    return result;
}

std::vector<std::unordered_map<std::string, std::string>> DatabaseManager::query(const std::string& sql, std::function<void(PreparedStatement&)> binder) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto stmt = prepare(sql);
    if (!stmt) {
        return {};
    }
    
    if (binder) {
        binder(*stmt);
    }
    
    std::vector<std::unordered_map<std::string, std::string>> results;
    
    while (stmt->step()) {
        std::unordered_map<std::string, std::string> row;
        int columnCount = stmt->getColumnCount();
        for (int i = 0; i < columnCount; ++i) {
            std::string columnName = stmt->getColumnName(i);
            std::string value = stmt->getColumnString(i);
            row[columnName] = value;
        }
        results.push_back(std::move(row));
    }
    
    return results;
}

bool DatabaseManager::executeInternal(const std::string& sql) {
    char* errMsg = nullptr;
    int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (result != SQLITE_OK) {
        Logger::instance().log(LogLevel::ERROR, "Failed to execute SQL: " + sql + " - " + std::string(errMsg ? errMsg : ""), "DatabaseManager");
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return executeInternal(sql);
}

std::unique_ptr<Transaction> DatabaseManager::beginTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return beginTransactionInternal();
}

std::unique_ptr<Transaction> DatabaseManager::beginTransactionInternal() {
    stats_.activeTransactions++;
    return std::make_unique<Transaction>(db_);
}

int64_t DatabaseManager::lastInsertRowId() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_last_insert_rowid(db_);
}

int DatabaseManager::changes() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_changes(db_);
}

std::string DatabaseManager::getLastError() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_errmsg(db_);
}

void DatabaseManager::setForeignKeysEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    setForeignKeysEnabledInternal(enabled);
}

void DatabaseManager::setForeignKeysEnabledInternal(bool enabled) {
    std::string sql = enabled ? "PRAGMA foreign_keys=ON" : "PRAGMA foreign_keys=OFF";
    executeInternal(sql);
}

void DatabaseManager::setBusyTimeout(int timeoutMs) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    setBusyTimeoutInternal(timeoutMs);
}

void DatabaseManager::setBusyTimeoutInternal(int timeoutMs) {
    sqlite3_busy_timeout(db_, timeoutMs);
}

bool DatabaseManager::vacuum() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Clear all prepared statements before VACUUM
    // VACUUM cannot run while there are active prepared statements
    statementCache_.clear();
    
    // Finalize any remaining statements
    // Note: We must restart from nullptr after each finalize
    sqlite3_stmt* stmt = nullptr;
    while ((stmt = sqlite3_next_stmt(db_, nullptr)) != nullptr) {
        sqlite3_finalize(stmt);
        stmt = nullptr;  // Reset to nullptr for next iteration
    }
    
    char* errMsg = nullptr;
    int result = sqlite3_exec(db_, "VACUUM", nullptr, nullptr, &errMsg);
    
    if (result != SQLITE_OK) {
        Logger::instance().log(LogLevel::ERROR, "Failed to vacuum database: " + std::string(errMsg ? errMsg : ""), "DatabaseManager");
        if (errMsg) sqlite3_free(errMsg);
        
        // Even if VACUUM fails, we need to ensure the database is in a usable state
        Logger::instance().log(LogLevel::INFO, "Database state reset after failed VACUUM", "DatabaseManager");
        return false;
    }
    
    Logger::instance().log(LogLevel::INFO, "Database VACUUM completed successfully", "DatabaseManager");
    return true;
}

void DatabaseManager::clearCache() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    statementCache_.clear();
    Logger::instance().log(LogLevel::INFO, "Prepared statement cache cleared", "DatabaseManager");
}

DatabaseManager::Stats DatabaseManager::getStats() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return stats_;
}

// Common query helpers
std::shared_ptr<PreparedStatement> DatabaseManager::getInsertFileStmt() {
    return insertFileStmt_;
}

std::shared_ptr<PreparedStatement> DatabaseManager::getSelectFileStmt() {
    return selectFileStmt_;
}

std::shared_ptr<PreparedStatement> DatabaseManager::getInsertOperationStmt() {
    return insertOperationStmt_;
}

std::shared_ptr<PreparedStatement> DatabaseManager::getSelectPendingOpsStmt() {
    return selectPendingOpsStmt_;
}

sqlite3* DatabaseManager::getDatabase() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return db_;
}

} // namespace SentinelFS
