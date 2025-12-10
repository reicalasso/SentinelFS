# SentinelFS Core Modülleri

Core klasörü, tüm uygulamanın paylaştığı C++ kütüphaneyi (`sentinel_core`) içerir. Alt dizinler gerçek modülleri temsil eder.

## Modül Haritası

- `include/`
  - Public API arayüzleri: `IPlugin`, `INetworkAPI`, `IStorageAPI`, `IFileAPI`
  - Dış dünya (app, plugins, tests) core'a bu header'lar üzerinden bakar.

- `network/`
  - Ağ ile ilgili düşük seviye bileşenler:
    - `DeltaEngine` (Adler32 + SHA-256 tabanlı delta algoritması)
    - `DeltaSync` (delta protokol yardımcıları)
    - `NetworkManager` (connection pool + transport abstraksiyonu)
    - `BandwidthLimiter` (bant genişliği kontrolü için çekirdek sınıf)

- `security/`
  - Güvenlik ve oturum yönetimi:
    - `Crypto` (AES, HMAC, key derivation)
    - `SessionCode` (oturum kodu üretimi/normalize/validasyon)

- `sync/`
  - Senkronizasyon orkestrasyonu:
    - `EventHandlers` (EventBus abonelikleri, FILE_MODIFIED / DATA_RECEIVED vb.)
    - `FileSyncHandler` (FILE_MODIFIED → UPDATE_AVAILABLE yayını)
    - `DeltaSyncProtocolHandler` (REQUEST_DELTA / DELTA_DATA mesaj akışı)
    - `ConflictResolver` (çakışma çözüm stratejileri)
    - `DeltaSerialization` (signature/delta serileştirme)

- `utils/`
  - Çapraz modül yardımcılar:
    - `Logger`, `MetricsCollector`
    - `Config` (ini-style konfig okuma)
    - `EventBus` (pub/sub sistemi)
    - `PluginLoader`, `PluginManager` (dinamik .so yükleme ve bağımlılık takibi)

- `storage/`
  - Şu an için boş; ileride çekirdek seviye storage/DB soyutlamaları için ayrılmıştır.

Bu yapı sayesinde:
- Uygulama giriş noktaları (`app/cli`, `app/daemon`) `sentinel_core`'u doğrudan kullanır.
- Plugin'ler (`plugins/*`) sadece ihtiyaç duydukları modüllerin header'larını dahil eder.
- Yeni gelen biri, sadece bu dosyaya bakarak core içindeki sorumluluk dağılımını görebilir.
