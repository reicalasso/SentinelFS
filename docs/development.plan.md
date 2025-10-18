## 🗺️ **6\. Geliştirme Planı ve Yol Haritası**

### 6.1 Geliştirme Felsefesi

-   **Modülerlik:** Her katman bağımsız, test edilebilir ve ders parçalarıyla eşleştirilebilir
    
-   **Iteratif Yaklaşım:** Önce temel işlevler, sonra gelişmiş optimizasyon ve güvenlik
    
-   **Ölçeklenebilirlik:** Peer sayısı arttıkça performans ve veri bütünlüğü korunacak
    

---

### 6.2 Aşamalar

#### **Aşama 1: Temel Dosya ve Metadata Yönetimi (Weeks 1–2)**

-   Dosya sistemi izleyici (File Watcher) oluşturulacak
    
-   SQLite veya basit KV store ile metadata yönetimi
    
-   Delta Engine prototipi: küçük dosya değişikliklerini hesaplama
    
-   **Hedef:** Tek cihazda dosya değişikliklerini kaydetmek ve delta hesaplamak
    

---

#### **Aşama 2: Peer Discovery ve Network Layer (Weeks 3–4)**

-   UDP broadcast tabanlı peer discovery implementasyonu
    
-   TCP üzerinden basit veri aktarımı
    
-   Peer listesi ve temel ping ölçümü
    
-   **Hedef:** En az 3 cihazın birbirini keşfetmesi ve veri transferi
    

---

#### **Aşama 3: Auto-Remesh ve Latency Optimizasyonu (Weeks 5–6)**

-   Ping testleri ile en iyi peer bağlantılarını belirleme
    
-   Bağlantıları dinamik olarak güncelleme
    
-   Ağ koşullarına göre aktif/pasif peer yönetimi
    
-   **Hedef:** Ağ değişikliklerine adaptif mesh ağı
    

---

#### **Aşama 4: Delta Sync ve Multi-Peer Senkronizasyon (Weeks 7–8)**

-   Delta Engine’in multi-peer desteği
    
-   Metadata veritabanıyla senkronizasyon
    
-   Ağ üzerinden veri farklarının uygulanması
    
-   **Hedef:** Dosya değişikliklerinin tüm peer’lere doğru ve hızlı şekilde iletilmesi
    

---

#### **Aşama 5: Basit Güvenlik ve Veri Bütünlüğü (Weeks 9–10)**

-   Session code tabanlı peer doğrulama
    
-   CRC32 veya SHA256 hash ile dosya bütünlüğü kontrolü
    
-   Replay saldırılarını önlemek için nonce tabanlı handshake
    
-   **Hedef:** Temel güvenlik ve veri bütünlüğü sağlanmış P2P sistem
    

---

#### **Aşama 6: Test ve Değerlendirme (Weeks 11–12)**

-   Peer discovery, remesh, delta sync testleri
    
-   Metadata tutarlılığı ve concurrency senaryoları
    
-   Latency ve transfer hızlarının ölçülmesi
    
-   **Hedef:** Tüm modüllerin stabil çalıştığını doğrulamak
    

---

#### **Aşama 7: Demo ve Ders Entegrasyonu (Weeks 13–14)**

-   3–5 node demo ortamı
    
-   Örnek network senaryoları ve dosya değişiklikleri gösterimi
    
-   Ders bazlı parçaların sunumu (DB, OS, Networking)
    
-   **Hedef:** Projenin akademik ve pratik kullanım için hazır olması
    

---

### 6.3 Geliştirme Tablosu

| Hafta | Modül / Katman | Görev | Çıktı |
| --- | --- | --- | --- |
| 1–2 | File System / DB | Metadata, File Watcher, Delta Engine | Tek cihazda değişiklikler kaydediliyor |
| 3–4 | Network Layer | Peer Discovery, TCP transfer | Multi-peer bağlantı sağlanıyor |
| 5–6 | Network Layer | Auto-Remesh | Ağ adaptasyonu sağlanıyor |
| 7–8 | Delta Sync | Multi-peer senkronizasyon | Dosya farkları tüm peer’lere gönderiliyor |
| 9–10 | Security Layer | Session auth, CRC hash, handshake | Temel güvenlik ve bütünlük sağlanıyor |
| 11–12 | Test / QA | Senaryolar ve ölçümler | Stabil ve ölçülebilir sistem |
| 13–14 | Demo / Eğitim | Multi-node demo, ders entegrasyonu | Akademik sunuma hazır prototip |