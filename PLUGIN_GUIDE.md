# SentinelFS-Neo Plugin Geliştirme Rehberi

## Plugin API Formatı

Her plugin şu üç fonksiyonu export eder:

extern "C" SFS_PluginInfo plugin_info();
extern "C" void* plugin_create();
extern "C" void plugin_destroy(void*);

## Plugin Türleri

- filesystem
- network
- storage
- ml

## Plugin Sorumlulukları

- İş mantığı yalnızca plugin içindedir.
- Plugin bağımsız bir modüldür.
- Core yalnızca bu plugin'i yükler.
