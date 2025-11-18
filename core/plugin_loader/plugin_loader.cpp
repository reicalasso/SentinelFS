#include "plugin_loader.h"
#include <iostream>
#include <cstring>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace sfs {
namespace core {

PluginLoader::PluginLoader() {
}

PluginLoader::~PluginLoader() {
    unload_all();
}

bool PluginLoader::load_plugin(const std::string& path) {
    // Load the dynamic library
    void* lib_handle = load_library(path);
    if (!lib_handle) {
        std::cerr << "Failed to load library: " << path << " - " << get_error() << std::endl;
        return false;
    }
    
    // Get required symbols
    auto info_func = reinterpret_cast<SFS_PluginInfoFunc>(
        get_symbol(lib_handle, "plugin_info"));
    auto create_func = reinterpret_cast<SFS_PluginCreateFunc>(
        get_symbol(lib_handle, "plugin_create"));
    auto destroy_func = reinterpret_cast<SFS_PluginDestroyFunc>(
        get_symbol(lib_handle, "plugin_destroy"));
    
    if (!info_func || !create_func || !destroy_func) {
        std::cerr << "Plugin missing required symbols: " << path << std::endl;
        unload_library(lib_handle);
        return false;
    }
    
    // Get plugin info
    SFS_PluginInfo info = info_func();
    
    // Validate API version
    if (info.api_version != SFS_PLUGIN_API_VERSION) {
        std::cerr << "Plugin API version mismatch: " << info.name 
                  << " (expected " << SFS_PLUGIN_API_VERSION 
                  << ", got " << info.api_version << ")" << std::endl;
        unload_library(lib_handle);
        return false;
    }
    
    // Check if already loaded
    if (is_loaded(info.name)) {
        std::cerr << "Plugin already loaded: " << info.name << std::endl;
        unload_library(lib_handle);
        return false;
    }
    
    // Create plugin instance
    void* instance = create_func();
    if (!instance) {
        std::cerr << "Failed to create plugin instance: " << info.name << std::endl;
        unload_library(lib_handle);
        return false;
    }
    
    // Store plugin handle
    auto handle = std::make_unique<PluginHandle>();
    handle->path = path;
    handle->library_handle = lib_handle;
    handle->info = info;
    handle->instance = instance;
    handle->info_func = info_func;
    handle->create_func = create_func;
    handle->destroy_func = destroy_func;
    
    plugins_[info.name] = std::move(handle);
    
    std::cout << "Loaded plugin: " << info.name << " v" << info.version << std::endl;
    return true;
}

bool PluginLoader::unload_plugin(const std::string& plugin_name) {
    auto it = plugins_.find(plugin_name);
    if (it == plugins_.end()) {
        return false;
    }
    
    auto& handle = it->second;
    
    // Destroy instance
    if (handle->instance && handle->destroy_func) {
        handle->destroy_func(handle->instance);
        handle->instance = nullptr;
    }
    
    // Unload library
    if (handle->library_handle) {
        unload_library(handle->library_handle);
        handle->library_handle = nullptr;
    }
    
    plugins_.erase(it);
    std::cout << "Unloaded plugin: " << plugin_name << std::endl;
    return true;
}

void PluginLoader::unload_all() {
    std::vector<std::string> names;
    for (const auto& [name, _] : plugins_) {
        names.push_back(name);
    }
    
    for (const auto& name : names) {
        unload_plugin(name);
    }
}

void* PluginLoader::get_plugin_instance(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    if (it != plugins_.end()) {
        return it->second->instance;
    }
    return nullptr;
}

const SFS_PluginInfo* PluginLoader::get_plugin_info(const std::string& plugin_name) const {
    auto it = plugins_.find(plugin_name);
    if (it != plugins_.end()) {
        return &it->second->info;
    }
    return nullptr;
}

std::vector<std::string> PluginLoader::get_loaded_plugins() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : plugins_) {
        names.push_back(name);
    }
    return names;
}

bool PluginLoader::is_loaded(const std::string& plugin_name) const {
    return plugins_.find(plugin_name) != plugins_.end();
}

// Platform-specific implementations

#ifdef _WIN32

void* PluginLoader::load_library(const std::string& path) {
    return LoadLibraryA(path.c_str());
}

void PluginLoader::unload_library(void* handle) {
    if (handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
    }
}

void* PluginLoader::get_symbol(void* handle, const char* symbol_name) {
    return reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle), symbol_name)
    );
}

std::string PluginLoader::get_error() const {
    DWORD error = GetLastError();
    char buffer[256];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, buffer, sizeof(buffer), nullptr
    );
    return std::string(buffer);
}

#else // Unix/Linux/macOS

void* PluginLoader::load_library(const std::string& path) {
    return dlopen(path.c_str(), RTLD_LAZY);
}

void PluginLoader::unload_library(void* handle) {
    if (handle) {
        dlclose(handle);
    }
}

void* PluginLoader::get_symbol(void* handle, const char* symbol_name) {
    return dlsym(handle, symbol_name);
}

std::string PluginLoader::get_error() const {
    const char* error = dlerror();
    return error ? std::string(error) : "";
}

#endif

} // namespace core
} // namespace sfs
