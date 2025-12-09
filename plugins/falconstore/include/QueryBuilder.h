/**
 * @file QueryBuilder.h
 * @brief Type-safe SQL query builder for FalconStore
 */

#pragma once

#include "FalconStore.h"
#include <sstream>
#include <sqlite3.h>

namespace SentinelFS {
namespace Falcon {

/**
 * @brief SQLite Row implementation
 */
class SQLiteRow : public Row {
public:
    explicit SQLiteRow(sqlite3_stmt* stmt) : stmt_(stmt) {}
    
    bool isNull(const std::string& column) const override {
        int idx = getColumnIndex(column);
        return idx >= 0 && sqlite3_column_type(stmt_, idx) == SQLITE_NULL;
    }
    
    int getInt(const std::string& column) const override {
        int idx = getColumnIndex(column);
        return idx >= 0 ? sqlite3_column_int(stmt_, idx) : 0;
    }
    
    int64_t getInt64(const std::string& column) const override {
        int idx = getColumnIndex(column);
        return idx >= 0 ? sqlite3_column_int64(stmt_, idx) : 0;
    }
    
    double getDouble(const std::string& column) const override {
        int idx = getColumnIndex(column);
        return idx >= 0 ? sqlite3_column_double(stmt_, idx) : 0.0;
    }
    
    std::string getString(const std::string& column) const override {
        int idx = getColumnIndex(column);
        if (idx < 0) return "";
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, idx));
        return text ? text : "";
    }
    
    std::vector<uint8_t> getBlob(const std::string& column) const override {
        int idx = getColumnIndex(column);
        if (idx < 0) return {};
        const void* data = sqlite3_column_blob(stmt_, idx);
        int size = sqlite3_column_bytes(stmt_, idx);
        if (!data || size <= 0) return {};
        return std::vector<uint8_t>(
            static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + size
        );
    }
    
    void buildColumnMap() {
        columnMap_.clear();
        int count = sqlite3_column_count(stmt_);
        for (int i = 0; i < count; ++i) {
            const char* name = sqlite3_column_name(stmt_, i);
            if (name) {
                columnMap_[name] = i;
            }
        }
    }

private:
    sqlite3_stmt* stmt_;
    mutable std::unordered_map<std::string, int> columnMap_;
    
    int getColumnIndex(const std::string& column) const {
        auto it = columnMap_.find(column);
        if (it != columnMap_.end()) {
            return it->second;
        }
        // Fallback: search by name
        int count = sqlite3_column_count(stmt_);
        for (int i = 0; i < count; ++i) {
            const char* name = sqlite3_column_name(stmt_, i);
            if (name && column == name) {
                columnMap_[column] = i;
                return i;
            }
        }
        return -1;
    }
};

/**
 * @brief SQLite ResultSet implementation
 */
class SQLiteResultSet : public ResultSet {
public:
    SQLiteResultSet(sqlite3_stmt* stmt, sqlite3* db) 
        : stmt_(stmt), db_(db), row_(stmt), rowCount_(0), hasMore_(false) {
        row_.buildColumnMap();
    }
    
    ~SQLiteResultSet() override {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }
    
    bool next() override {
        if (!stmt_) return false;
        int rc = sqlite3_step(stmt_);
        hasMore_ = (rc == SQLITE_ROW);
        if (hasMore_) rowCount_++;
        return hasMore_;
    }
    
    const Row& current() const override {
        return row_;
    }
    
    size_t rowCount() const override {
        return rowCount_;
    }
    
    bool empty() const override {
        return rowCount_ == 0 && !hasMore_;
    }
    
    void reset() override {
        if (stmt_) {
            sqlite3_reset(stmt_);
            rowCount_ = 0;
            hasMore_ = false;
        }
    }

private:
    sqlite3_stmt* stmt_;
    sqlite3* db_;
    SQLiteRow row_;
    size_t rowCount_;
    bool hasMore_;
};

/**
 * @brief SQL Query Builder implementation
 */
class SQLiteQueryBuilder : public QueryBuilder {
public:
    explicit SQLiteQueryBuilder(sqlite3* db) : db_(db) {}
    
    // SELECT
    QueryBuilder& select(const std::vector<std::string>& columns = {"*"}) override {
        type_ = QueryType::SELECT;
        columns_ = columns;
        return *this;
    }
    
    QueryBuilder& selectDistinct(const std::vector<std::string>& columns) override {
        type_ = QueryType::SELECT;
        distinct_ = true;
        columns_ = columns;
        return *this;
    }
    
    QueryBuilder& from(const std::string& table) override {
        table_ = table;
        return *this;
    }
    
    // JOIN
    QueryBuilder& join(const JoinSpec& spec) override {
        joins_.push_back(spec);
        return *this;
    }
    
    QueryBuilder& innerJoin(const std::string& table, const std::string& onLeft, const std::string& onRight) override {
        joins_.push_back({JoinType::INNER, table, onLeft, onRight});
        return *this;
    }
    
    QueryBuilder& leftJoin(const std::string& table, const std::string& onLeft, const std::string& onRight) override {
        joins_.push_back({JoinType::LEFT, table, onLeft, onRight});
        return *this;
    }
    
    // WHERE
    QueryBuilder& where(const std::string& column, const std::string& op, const QueryValue& value) override {
        conditions_.push_back({column, op, value, conditions_.empty() ? "" : "AND"});
        return *this;
    }
    
    QueryBuilder& whereNull(const std::string& column) override {
        conditions_.push_back({column, "IS", nullptr, conditions_.empty() ? "" : "AND"});
        return *this;
    }
    
    QueryBuilder& whereNotNull(const std::string& column) override {
        conditions_.push_back({column, "IS NOT", nullptr, conditions_.empty() ? "" : "AND"});
        return *this;
    }
    
    QueryBuilder& whereIn(const std::string& column, const std::vector<QueryValue>& values) override {
        // Store as special IN condition
        inConditions_[column] = values;
        return *this;
    }
    
    QueryBuilder& whereBetween(const std::string& column, const QueryValue& low, const QueryValue& high) override {
        betweenConditions_.push_back({column, low, high});
        return *this;
    }
    
    QueryBuilder& orWhere(const std::string& column, const std::string& op, const QueryValue& value) override {
        conditions_.push_back({column, op, value, "OR"});
        return *this;
    }
    
    // ORDER, GROUP, LIMIT
    QueryBuilder& orderBy(const std::string& column, OrderDirection dir = OrderDirection::ASC) override {
        orders_.push_back({column, dir});
        return *this;
    }
    
    QueryBuilder& groupBy(const std::vector<std::string>& columns) override {
        groupBy_ = columns;
        return *this;
    }
    
    QueryBuilder& having(const std::string& condition) override {
        having_ = condition;
        return *this;
    }
    
    QueryBuilder& limit(int count) override {
        limit_ = count;
        return *this;
    }
    
    QueryBuilder& offset(int count) override {
        offset_ = count;
        return *this;
    }
    
    // Execute
    std::unique_ptr<ResultSet> execute() override {
        std::string sql = toSql();
        sqlite3_stmt* stmt = nullptr;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return nullptr;
        }
        
        // Bind parameters
        int paramIdx = 1;
        for (const auto& cond : conditions_) {
            bindValue(stmt, paramIdx++, cond.value);
        }
        
        for (const auto& [col, values] : inConditions_) {
            for (const auto& val : values) {
                bindValue(stmt, paramIdx++, val);
            }
        }
        
        for (const auto& [col, low, high] : betweenConditions_) {
            bindValue(stmt, paramIdx++, low);
            bindValue(stmt, paramIdx++, high);
        }
        
        return std::make_unique<SQLiteResultSet>(stmt, db_);
    }
    
    std::future<std::unique_ptr<ResultSet>> executeAsync() override {
        return std::async(std::launch::async, [this]() {
            return execute();
        });
    }
    
    // Get generated SQL
    std::string toSql() const override {
        std::stringstream ss;
        
        // SELECT
        ss << "SELECT ";
        if (distinct_) ss << "DISTINCT ";
        
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << columns_[i];
        }
        
        // FROM
        ss << " FROM " << table_;
        
        // JOINs
        for (const auto& join : joins_) {
            switch (join.type) {
                case JoinType::INNER: ss << " INNER JOIN "; break;
                case JoinType::LEFT: ss << " LEFT JOIN "; break;
                case JoinType::RIGHT: ss << " RIGHT JOIN "; break;
                case JoinType::FULL: ss << " FULL OUTER JOIN "; break;
            }
            ss << join.table << " ON " << join.onLeft << " = " << join.onRight;
        }
        
        // WHERE
        if (!conditions_.empty() || !inConditions_.empty() || !betweenConditions_.empty()) {
            ss << " WHERE ";
            bool first = true;
            
            for (const auto& cond : conditions_) {
                if (!first) ss << " " << cond.logic << " ";
                first = false;
                
                ss << cond.column << " " << cond.op;
                if (!std::holds_alternative<std::nullptr_t>(cond.value)) {
                    ss << " ?";
                } else {
                    ss << " NULL";
                }
            }
            
            for (const auto& [col, values] : inConditions_) {
                if (!first) ss << " AND ";
                first = false;
                ss << col << " IN (";
                for (size_t i = 0; i < values.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << "?";
                }
                ss << ")";
            }
            
            for (const auto& [col, low, high] : betweenConditions_) {
                if (!first) ss << " AND ";
                first = false;
                ss << col << " BETWEEN ? AND ?";
            }
        }
        
        // GROUP BY
        if (!groupBy_.empty()) {
            ss << " GROUP BY ";
            for (size_t i = 0; i < groupBy_.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << groupBy_[i];
            }
            
            if (!having_.empty()) {
                ss << " HAVING " << having_;
            }
        }
        
        // ORDER BY
        if (!orders_.empty()) {
            ss << " ORDER BY ";
            for (size_t i = 0; i < orders_.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << orders_[i].column;
                ss << (orders_[i].direction == OrderDirection::ASC ? " ASC" : " DESC");
            }
        }
        
        // LIMIT & OFFSET
        if (limit_ > 0) {
            ss << " LIMIT " << limit_;
        }
        if (offset_ > 0) {
            ss << " OFFSET " << offset_;
        }
        
        return ss.str();
    }

private:
    enum class QueryType { SELECT, INSERT, UPDATE, DELETE };
    
    sqlite3* db_;
    QueryType type_{QueryType::SELECT};
    bool distinct_{false};
    std::vector<std::string> columns_{"*"};
    std::string table_;
    std::vector<JoinSpec> joins_;
    std::vector<Condition> conditions_;
    std::unordered_map<std::string, std::vector<QueryValue>> inConditions_;
    std::vector<std::tuple<std::string, QueryValue, QueryValue>> betweenConditions_;
    std::vector<OrderSpec> orders_;
    std::vector<std::string> groupBy_;
    std::string having_;
    int limit_{0};
    int offset_{0};
    
    void bindValue(sqlite3_stmt* stmt, int idx, const QueryValue& value) {
        std::visit([stmt, idx](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                sqlite3_bind_null(stmt, idx);
            } else if constexpr (std::is_same_v<T, bool>) {
                sqlite3_bind_int(stmt, idx, arg ? 1 : 0);
            } else if constexpr (std::is_same_v<T, int>) {
                sqlite3_bind_int(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                sqlite3_bind_int64(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, idx, arg.c_str(), -1, SQLITE_TRANSIENT);
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                sqlite3_bind_blob(stmt, idx, arg.data(), arg.size(), SQLITE_TRANSIENT);
            }
        }, value);
    }
};

} // namespace Falcon
} // namespace SentinelFS
