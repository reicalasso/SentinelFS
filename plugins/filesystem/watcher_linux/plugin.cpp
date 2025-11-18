#include "core/plugin_api.h"
#include "watcher_linux.h"

/**
 * @brief watcher.linux plugin
 * 
 * Linux filesystem watcher using inotify API.
 * Provides real-time file system monitoring.
 */

extern "C" {

SFS_PluginInfo plugin_info() {
    SFS_PluginInfo info;
    info.name = "watcher.linux";
    info.version = "1.0.0";
    info.author = "SentinelFS Team";
    info.description = "Linux filesystem watcher using inotify";
    info.type = SFS_PLUGIN_TYPE_FILESYSTEM;
    info.api_version = SFS_PLUGIN_API_VERSION;
    
    return info;
}

void* plugin_create() {
    return new sfs::plugins::WatcherLinux();
}

void plugin_destroy(void* instance) {
    delete static_cast<sfs::plugins::WatcherLinux*>(instance);
}

} // extern "C"
