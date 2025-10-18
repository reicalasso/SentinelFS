## 🧩 **2\. Mimari Tasarım**

### 2.1 Genel Bakış

SentinelFS-Lite modüler bir yapıya sahiptir. Sistem, her biri farklı bir mühendislik disipliniyle ilişkili **dört temel katmandan** oluşur:

1.  **App Layer (Uygulama Katmanı)**
    
2.  **File System Layer (Dosya Yönetim Katmanı)**
    
3.  **Network Layer (Ağ Katmanı)**
    
4.  **Storage Layer (Veritabanı Katmanı)**
    

Bu katmanlar arasında **net bir sorumluluk ayrımı** bulunur:

-   **App Layer**, kullanıcı ile sistem arasındaki arabirimi sağlar.
    
-   **File System Layer**, dosya değişikliklerini takip eder ve delta farklarını hesaplar.
    
-   **Network Layer**, peer keşfi, bağlantı ve auto-remesh işlemlerini yönetir.
    
-   **Storage Layer**, tüm metadata ve senkronizasyon durumlarını kaydeder.
    

---

### 2.2 Katmanlar Arası İlişki

```pgsql
+---------------------------------------------------+
|                Application Layer                  |
| (CLI, Config, Shared Session Code, Logger)        |
+----------------------------▲----------------------+
                             |
+----------------------------|----------------------+
|          File System Layer |                      |
| (Watcher, Delta Diff Engine, File Queue)          |
+----------------------------▲----------------------+
                             |
+----------------------------|----------------------+
|          Network Layer     |                      |
| (Peer Discovery, Auto-Remesh, Transfer Engine)    |
+----------------------------▲----------------------+
                             |
+----------------------------|----------------------+
|          Storage Layer     |                      |
| (Metadata DB, Hash Store, Device Cache)           |
+---------------------------------------------------+
```

---

### 2.3 Katmanların Teknik Detayları

#### **2.3.1 Application Layer**

-   CLI veya config dosyası üzerinden sistem başlatılır.
    
-   Kullanıcı bir **session code** (örneğin `SENT-1234-XYZ`) girerek aynı ağa katılır.
    
-   Logger bileşeni, her modülden gelen olayları kayıt altına alır (örneğin `peer connected`, `file synced`, `mesh rebuilt` gibi).
    
-   Uygulama bu katmanda başlatılır ve diğer tüm servisler thread olarak açılır.
    

**Bileşenler:**

-   `main.cpp`
    
-   `cli.cpp / cli.hpp`
    
-   `logger.cpp / logger.hpp`
    
-   `config.json`
    

---

#### **2.3.2 File System Layer**

-   `inotify` benzeri bir **dosya izleme mekanizması** ile değişiklikler algılanır.
    
-   Dosya farkları **hash tabanlı delta algoritması**yla hesaplanır (örneğin `xxhash` veya `SHA256`).
    
-   Elde edilen delta verisi, **send queue** aracılığıyla ağ katmanına iletilir.
    
-   Senkronizasyon sırasında gelen dosya farkları da bu katmanda uygulanır.
    

**Bileşenler:**

-   `watcher.cpp / watcher.hpp` – dosya değişikliklerini algılar.
    
-   `delta_engine.cpp / delta_engine.hpp` – dosya farklarını hesaplar.
    
-   `file_queue.cpp / file_queue.hpp` – senkronizasyon sırasını yönetir.
    

---

#### **2.3.3 Network Layer**

-   Cihazlar, belirli bir **UDP broadcast** veya **multicast** paketi ile birbirlerini keşfeder.
    
-   Ardından **TCP bağlantısı** kurularak kalıcı peer bağlantısı oluşturulur.
    
-   `ping` mesajlarıyla **bağlantı gecikmesi ölçülür** ve bağlantılar buna göre **auto-remesh** edilir.
    
-   Transfer motoru, dosya delta’larını veya metadata güncellemelerini taşır.
    

**Bileşenler:**

-   `discovery.cpp / discovery.hpp` – cihaz keşfi ve peer listesi.
    
-   `remesh.cpp / remesh.hpp` – bağlantı optimizasyonu.
    
-   `transfer.cpp / transfer.hpp` – veri gönderme/alma işlemleri.
    

---

#### **2.3.4 Storage Layer**

-   Sistem metadata’yı tutmak için küçük, gömülü bir veritabanı (örneğin SQLite veya kendi mini KV store) kullanır.
    
-   Saklanan bilgiler:
    
    -   Dosya hash değerleri
        
    -   Zaman damgaları
        
    -   Peer listesi ve bağlantı kalitesi
        
-   Veritabanı, hızlı erişim için memory cache destekli çalışabilir.
    

**Bileşenler:**

-   `db.cpp / db.hpp` – metadata yönetimi
    
-   `models.hpp` – veri şemaları
    
-   `cache.cpp / cache.hpp` – önbellekleme sistemi
    

---

### 2.4 Dosya Organizasyonu

```arduino
SentinelFS-Lite/
│
├── src/
│   ├── app/
│   │   ├── main.cpp
│   │   ├── cli.cpp / cli.hpp
│   │   ├── logger.cpp / logger.hpp
│   │   └── config.json
│   │
│   ├── fs/
│   │   ├── watcher.cpp / watcher.hpp
│   │   ├── delta_engine.cpp / delta_engine.hpp
│   │   └── file_queue.cpp / file_queue.hpp
│   │
│   ├── net/
│   │   ├── discovery.cpp / discovery.hpp
│   │   ├── remesh.cpp / remesh.hpp
│   │   └── transfer.cpp / transfer.hpp
│   │
│   └── db/
│       ├── db.cpp / db.hpp
│       ├── models.hpp
│       └── cache.cpp / cache.hpp
│
├── CMakeLists.txt
└── README.md
```

---

### 2.5 Veri Akışı (Kısaltılmış Süreç)

1.  Kullanıcı SentinelFS-Lite’ı başlatır → Session code girer.
    
2.  Network Layer, aynı kodu kullanan cihazları keşfeder.
    
3.  En düşük ping sürelerine göre mesh topolojisi kurulur.
    
4.  File System Layer dosya değişikliklerini izler.
    
5.  Delta farkları hesaplanır → Network Layer’a aktarılır.
    
6.  Peer’ler delta’yı alır → kendi sistemine uygular.
    
7.  Metadata DB güncellenir.
    
8.  Bağlantı kalitesi düşerse Remesh motoru ağı yeniden yapılandırır.
    

---

### 2.6 Thread & Process Yapısı (Basitleştirilmiş)

```mathematica
Main Thread (CLI)
 ├── Network Thread (Discovery + Transfer)
 ├── File Watcher Thread
 ├── Database Thread
 └── Remesh Timer (Periodic Ping Test)
```

---