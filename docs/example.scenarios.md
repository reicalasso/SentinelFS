## 📝 **8\. Örnek Senaryolar ve Ders Uygulamaları**

### 8.1 Veritabanı Dersi (Database)

**Senaryo:** Dosya metadata’sının yönetimi ve conflict resolution

-   **Amaç:** Öğrenciler transaction yönetimi ve veri bütünlüğü kavramlarını öğrenir
    
-   **Aktivite:**
    
    1.  Bir dosya farklı peer’lerde aynı anda değiştiriliyor
        
    2.  MetadataDB, SHA256 hash ve timestamp ile değişiklikleri takip ediyor
        
    3.  Öğrenciler, conflict resolution algoritmasını uygulayarak hangi değişikliğin geçerli olacağını belirliyor
        
-   **Öğrenilen Kavramlar:** Transaction, metadata, veri bütünlüğü, hashing, conflict resolution
    

---

### 8.2 Ağ Dersi (Networking)

**Senaryo:** Peer discovery ve auto-remesh

-   **Amaç:** Öğrenciler P2P ağ topolojisi ve adaptif routing kavramlarını öğrenir
    
-   **Aktivite:**
    
    1.  3–5 cihazdan oluşan bir P2P ağı kuruluyor
        
    2.  UDP broadcast ile peer discovery sağlanıyor
        
    3.  Ağ gecikmesi artırılıyor ve auto-remesh algoritması en iyi bağlantıları belirliyor
        
    4.  Öğrenciler, peer listesi ve ping değerlerini gözlemleyerek ağın adaptasyonunu analiz ediyor
        
-   **Öğrenilen Kavramlar:** UDP/TCP iletişimi, peer discovery, mesh topolojisi, latency optimizasyonu, adaptif routing
    

---

### 8.3 İşletim Sistemleri Dersi (OS)

**Senaryo:** File watcher ve delta sync

-   **Amaç:** Öğrenciler concurrency ve I/O yönetimi kavramlarını öğrenir
    
-   **Aktivite:**
    
    1.  FileWatcher, belirli bir dizini izliyor
        
    2.  Aynı anda birden fazla dosya değişikliği yapılıyor
        
    3.  DeltaEngine, değişiklikleri blok bazında hesaplıyor ve peer’lere gönderiyor
        
    4.  Öğrenciler thread-safe logging ve mutex kullanarak concurrency problemlerini gözlemliyor
        
-   **Öğrenilen Kavramlar:** File I/O, concurrency, thread/process yönetimi, delta computation
    

---

### 8.4 Akademik Sunum Senaryosu

-   **Amaç:** Projenin ders ve araştırma ortamında kullanılabilirliğini göstermek
    
-   **Aktivite:**
    
    1.  3-node demo ortamı başlatılır
        
    2.  Dosya değişiklikleri yapılır ve peer’ler arasında senkronizasyon izlenir
        
    3.  Auto-remesh ve metadata güncellemeleri gösterilir
        
    4.  Öğrenciler, loglar ve metriklerle sistem performansını değerlendirir
        
-   **Öğrenilen Kavramlar:** Sistem bütünlüğü, veri senkronizasyonu, network adaptasyonu, delta transfer
    

---

### 8.5 Özet Tablosu

| Ders | Senaryo | Aktivite | Öğrenilen Kavramlar |
| --- | --- | --- | --- |
| Veritabanı | Metadata conflict | Dosya değişikliği ve metadata güncelleme | Transaction, hash, conflict resolution |
| Ağ | Peer discovery & remesh | Ağ gecikmesi artırma, peer listesi gözlemi | UDP/TCP, mesh topoloji, latency optimizasyonu |
| İşletim Sistemi | FileWatcher & Delta | Çoklu dosya değişikliği ve delta sync | File I/O, concurrency, thread yönetimi |
| Akademik Sunum | 3-node demo | Delta transfer ve auto-remesh gösterimi | Sistem bütünlüğü, network adaptasyonu, performans ölçümü |

---