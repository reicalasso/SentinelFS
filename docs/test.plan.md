## 🧪 **4\. Test Planı ve Değerlendirme**

### 4.1 Test Amaçları

-   Peer discovery ve auto-remesh’in doğruluğunu ve hızını ölçmek
    
-   Dosya delta senkronizasyonunun bütünlük ve verimliliğini değerlendirmek
    
-   Ağ gecikmesi ve senkronizasyon süresini ölçmek
    
-   Metadata veritabanının tutarlılığını kontrol etmek
    
-   Basit güvenlik kontrollerinin etkinliğini doğrulamak
    

---

### 4.2 Test Ortamı

**Önerilen Test Setup:**

-   3–5 adet cihaz veya VM (peer)
    
-   Farklı network koşulları simüle edilecek:
    
    -   Normal LAN (ping < 5 ms)
        
    -   Wi-Fi / yüksek gecikmeli bağlantılar (ping 20–50 ms)
        
    -   Ağ kopmaları ve gecikme artışı
        
-   SentinelFS-Lite her cihazda çalıştırılacak ve aynı **session code** kullanılacak
    

---

### 4.3 Test Senaryoları

#### 4.3.1 Peer Discovery Testi

-   **Amaç:** Sistem aynı session code’a sahip cihazları doğru şekilde keşfediyor mu?
    
-   **Yöntem:**
    
    -   3 cihaz başlatılır, UDP broadcast gönderilir
        
    -   Peer listesi kaydedilir ve beklenen peer sayısı ile karşılaştırılır
        
-   **Başarı Kriteri:**
    
    -   Tüm peer’ler listede yer almalı
        
    -   Discovery süresi < 5 saniye
        

---

#### 4.3.2 Auto-Remesh Testi

-   **Amaç:** Ağdaki gecikmeye göre en iyi bağlantılar seçiliyor mu?
    
-   **Yöntem:**
    
    -   Ping değerleri farklı cihazlar arasında ölçülür
        
    -   Bağlantılar otomatik olarak optimize edilir
        
    -   Yüksek gecikmeli peer’ler pasif, düşük gecikmeli peer’ler aktif olmalı
        
-   **Başarı Kriteri:**
    
    -   3 saniye içinde aktif bağlantılar güncellenir
        
    -   Her zaman en düşük ping’e sahip peer’ler aktif
        

---

#### 4.3.3 Delta Sync Testi

-   **Amaç:** Dosya değişiklikleri doğru ve hızlı senkronize ediliyor mu?
    
-   **Yöntem:**
    
    -   Dosya üzerinde küçük bir değişiklik yapılır
        
    -   Delta Engine farkı hesaplar ve peer’lere gönderir
        
    -   Hedef cihazda hash kontrolü yapılır
        
-   **Başarı Kriteri:**
    
    -   Dosya bütünlüğü sağlanmalı (SHA256 veya CRC32 kontrolü)
        
    -   Transfer edilen veri boyutu tam dosya yerine delta olmalı
        

---

#### 4.3.4 Metadata Tutarlılığı

-   **Amaç:** Tüm peer’lerde metadata doğru mu?
    
-   **Yöntem:**
    
    -   Peer’ler arasında dosya değişiklikleri yapılır
        
    -   Her cihazın DB’si kontrol edilir
        
-   **Başarı Kriteri:**
    
    -   `files` ve `peers` tablolarındaki veriler tüm cihazlarda uyumlu olmalı
        

---

#### 4.3.5 Ağ Kesintisi ve Hata Toleransı

-   **Amaç:** Sistem ağ kesintilerine dayanıklı mı?
    
-   **Yöntem:**
    
    -   Bir peer network’ten çıkarılır ve tekrar eklenir
        
    -   Auto-remesh algoritmasının tepki süresi ölçülür
        
-   **Başarı Kriteri:**
    
    -   Sistem kesinti sonrası 5 saniye içinde yeniden mesh’e katılır
        
    -   Dosya senkronizasyonu devam eder
        

---

### 4.4 Ölçüm Metrikleri

| Metrik | Hedef / Beklenti |
| --- | --- |
| Peer discovery süresi | < 5 saniye |
| Auto-remesh tepki süresi | < 3 saniye |
| Dosya senkronizasyon gecikmesi | < 500 ms (10 MB dosya için) |
| Delta boyutu | < %20 tam dosya boyutu |
| Metadata tutarlılığı | 100% |
| Transfer hatası | 0 |

---

### 4.5 Test Araçları

-   Basit C++ test scriptleri (`tests/peer_discovery.cpp`, `tests/delta_sync.cpp`)
    
-   Ping, iperf3, veya benzeri ağ test araçları
    
-   SQLite veri doğrulama scriptleri
    
-   Logging ve CSV/JSON çıktılarıyla ölçüm kaydı
    

---

### 4.6 Sonuçların Değerlendirilmesi

-   Her test senaryosu için **başarı / başarısızlık** durumu kaydedilir
    
-   Delta sync ve remesh performansı grafiklerle gösterilebilir (ör. gecikme vs. peer sayısı)
    
-   Sorunlu durumlarda loglar incelenir, algoritma optimize edilir
    

---