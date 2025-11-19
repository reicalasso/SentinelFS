#include "IPlugin.h"
#include <iostream>
#include <sqlite3.h>
#include <string>

namespace SentinelFS {

    class StoragePlugin : public IPlugin {
    public:
        bool initialize() override {
            std::cout << "StoragePlugin initialized" << std::endl;
            if (sqlite3_open("sentinel.db", &db_) != SQLITE_OK) {
                std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
                return false;
            }
            return createTables();
        }

        void shutdown() override {
            std::cout << "StoragePlugin shutdown" << std::endl;
            if (db_) {
                sqlite3_close(db_);
            }
        }

        std::string getName() const override {
            return "StoragePlugin";
        }

        std::string getVersion() const override {
            return "1.0.0";
        }

    private:
        sqlite3* db_ = nullptr;

        bool createTables() {
            const char* sql = 
                "CREATE TABLE IF NOT EXISTS files ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "path TEXT UNIQUE NOT NULL,"
                "hash TEXT,"
                "timestamp INTEGER,"
                "size INTEGER);"
                
                "CREATE TABLE IF NOT EXISTS peers ("
                "id TEXT PRIMARY KEY,"
                "address TEXT,"
                "port INTEGER,"
                "last_seen INTEGER,"
                "latency INTEGER);"
                
                "CREATE TABLE IF NOT EXISTS config ("
                "key TEXT PRIMARY KEY,"
                "value TEXT);";

            char* errMsg = nullptr;
            if (sqlite3_exec(db_, sql, 0, 0, &errMsg) != SQLITE_OK) {
                std::cerr << "SQL error: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                return false;
            }
            return true;
        }

    public:
        // CRUD Operations for Files
        bool addFile(const std::string& path, const std::string& hash, long long timestamp, long long size) {
            std::string sql = "INSERT OR REPLACE INTO files (path, hash, timestamp, size) VALUES ('" + 
                              path + "', '" + hash + "', " + std::to_string(timestamp) + ", " + std::to_string(size) + ");";
            
            char* errMsg = nullptr;
            if (sqlite3_exec(db_, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK) {
                std::cerr << "Error adding file: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                return false;
            }
            return true;
        }

        bool getFile(const std::string& path) {
            std::string sql = "SELECT * FROM files WHERE path = '" + path + "';";
            // TODO: Implement callback to retrieve data
            return true; 
        }

    };

    extern "C" {
        IPlugin* create_plugin() {
            return new StoragePlugin();
        }

        void destroy_plugin(IPlugin* plugin) {
            delete plugin;
        }
    }

}