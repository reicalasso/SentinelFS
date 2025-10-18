## 🎓 **5\. Kullanım Senaryoları ve Akademik Bağlam**

SentinelFS-Lite, modüler ve multi-disipliner yapısı sayesinde üç temel ders alanına uygulanabilir:

### 5.1 Veritabanı Dersi (Database)

-   **Kavramlar:** Metadata yönetimi, veri bütünlüğü, transaction, cache kullanımı
    
-   **Uygulama:**
    
    -   Her cihazın kendi SQLite veya mini KV store’u vardır.
        
    -   Dosya hash’leri ve zaman damgaları tutularak **veri bütünlüğü** sağlanır.
        
    -   Öğrenciler metadata tabloları üzerinden **CRUD operasyonlarını** ve transaction yönetimini öğrenebilir.
        
-   **Örnek Aktivite:**
    
    -   Bir dosya değiştirildiğinde tüm peer’lerde metadata güncellenir.
        
    -   Farklı senaryolarda **conflict resolution** gösterilebilir.
        

---

### 5.2 Ağ Dersi (Networking)

-   **Kavramlar:** Peer discovery, UDP/TCP iletişimi, mesh topolojisi, ağ gecikmesi, auto-remesh
    
-   **Uygulama:**
    
    -   Peer cihazlar, UDP broadcast ile keşfedilir, TCP üzerinden veri transferi yapılır.
        
    -   Auto-remesh algoritması, en düşük gecikmeli bağlantıları belirler ve ağı optimize eder.
        
    -   Öğrenciler gerçek zamanlı ağ davranışlarını gözlemleyebilir ve latency optimizasyonu yapabilir.
        
-   **Örnek Aktivite:**
    
    -   Peer sayısı arttıkça discovery ve remesh sürelerini ölçmek
        
    -   Ağ kopması veya yüksek gecikme senaryoları oluşturup sistemin adaptasyonunu gözlemlemek
        

---

### 5.3 İşletim Sistemleri Dersi (OS)

-   **Kavramlar:** Dosya sistemi izleme, delta sync, thread/process yönetimi, concurrency
    
-   **Uygulama:**
    
    -   File Watcher, dosya değişikliklerini thread bazlı izler ve delta farklarını hesaplar.
        
    -   Her katman ayrı thread/process olarak çalışır, thread senkronizasyonu önemlidir.
        
    -   Öğrenciler dosya sistemi olaylarını takip ederek **I/O yönetimi ve concurrency** kavramlarını öğrenebilir.
        
-   **Örnek Aktivite:**
    
    -   Aynı anda birden fazla dosya değişikliği yapılır ve delta sync’in tepki süresi ölçülür
        
    -   Thread-safe logging ve mutex kullanımı ile concurrency sorunları gösterilebilir
        

---

### 5.4 Akademik ve Endüstriyel Uygulamalar

-   Bu proje, **akademik olarak multi-disipliner derslerde uygulamalı bir örnek** sunar.
    
-   Aynı zamanda, gerçek dünyada **P2P dosya senkronizasyonu, network adaptasyonu ve veri bütünlüğü** gerektiren sistemler için prototip olarak kullanılabilir.
    
-   Öğrenciler, projenin modüler yapısı sayesinde **yeni modüller ekleyebilir**, örneğin basit bir güvenlik katmanı veya performans ölçüm aracı.
    

---

### 5.5 Ders Bazlı Parçalar Özet Tablosu

| Ders | Kavram | SentinelFS-Lite Parçası | Aktivite Örneği |
| --- | --- | --- | --- |
| Veritabanı | Transaction, Metadata | Storage Layer, DB module | Dosya değişikliklerinin metadata’ya yansıtılması, conflict resolution |
| Ağ | Mesh, Peer Discovery, Latency | Network Layer | UDP discovery, auto-remesh algoritması |
| İşletim Sistemi | File I/O, Concurrency | File System Layer | File Watcher, Delta Engine, Thread yönetimi |

---