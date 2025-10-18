

## 📘 **1\. Fikir Oluşturma ve Konsept Tanımı**

### 1.1 Projenin Temel Amacı

**SentinelFS-Lite**, birden fazla cihaz arasında **eşler arası (peer-to-peer) dosya senkronizasyonu** sağlayan, **otomatik bağlantı optimizasyonu (auto-remesh)** özellikli, **hafif ve güvenli bir dağıtık dosya sistemi** projesidir.  
Klasik istemci-sunucu mimarisinden farklı olarak, bu sistem **her cihazı hem istemci hem sunucu olarak** çalıştırır. Böylece merkezi bir “datacenter” olmadan bile cihazlar birbirleriyle **veri bütünlüğünü koruyarak** dosya paylaşabilir.

Amaç, veri senkronizasyonunu yalnızca “kopyalama” düzeyinde değil, aynı zamanda **ağ verimliliği, sistem kaynak yönetimi ve güvenlik bilinci** düzeyinde optimize etmektir.

---

### 1.2 Proje Fikri Nasıl Ortaya Çıktı?

Modern sistemlerde dosya paylaşımı genellikle **bulut temelli çözümler** (Google Drive, Dropbox, OneDrive vb.) ile sağlanır.  
Ancak bu tür çözümler:

-   **Merkezi sunucuya bağımlıdır**,
    
-   **Tek hata noktasına (single point of failure)** sahiptir,
    
-   **Veri gizliliğini garanti etmez**,
    
-   **Düşük ağ kalitesi olan bölgelerde yavaş** çalışır.
    

Bu eksiklikler, özellikle **yerel ağlarda (LAN/WLAN)** veya **offline-first** senaryolarda çalışan kullanıcılar için verimsizdir.  
Buradan hareketle SentinelFS-Lite’ın fikri doğmuştur:

> “Veriyi merkeze taşımak yerine, cihazları merkeze dönüştürmek.”

Her cihaz, aynı erişim kodu (örneğin bir “session code” veya “shared key”) ile sisteme dahil olur ve ağ üzerindeki diğer cihazları otomatik olarak bulur. Sistem, **gecikme sürelerini (latency)** ve **bağlantı kalitesini** analiz ederek, **en uygun mesh topolojisini** kendisi oluşturur ve gerektiğinde yeniden yapılandırır (auto-remesh).

---

### 1.3 Projenin Hedefleri

| Hedef | Açıklama |
| --- | --- |
| **Dağıtık Senkronizasyon** | Birden fazla cihaz arasında, merkezi sunucu olmadan dosya bütünlüğü sağlamak. |
| **Otomatik Ağ Optimizasyonu** | Ağ kalitesine göre peer bağlantılarını dinamik olarak yeniden düzenlemek. |
| **Verimlilik ve Güvenlik** | Gereksiz veri transferini engellemek, sadece değişen blokları senkronize etmek ve bağlantıları güvenli hale getirmek. |
| **Küçük Ölçekli Laboratuvar Uygulamaları için Uygunluk** | Üniversite veya Ar-Ge ortamlarında, hem ağ hem işletim sistemi hem de veritabanı konularını içeren entegre bir sistem sunmak. |

---

### 1.4 Proje Neden “Lite”?

Orijinal SentinelFS konsepti, LSTM tabanlı tehdit analizi, YARA ile dosya içi tarama, ve ileri düzey güvenlik mekanizmaları içeriyordu.  
SentinelFS-Lite, bu karmaşıklığı azaltarak **temel fonksiyonları korur** ancak **öğrenilebilir ve uygulanabilir** bir yapı sunar:

-   Kod C++ ile yazılacak (yüksek performans + sistem erişimi).
    
-   Makine öğrenmesi veya ileri tarama modülleri olmadan sadeleştirilmiş.
    
-   Peer discovery, remesh ve delta-sync mekanizmaları temel özellikler olacak.
    

---

### 1.5 Üç Dersle Bağlantısı

| Ders | Projeye Katkı | Kullanım Alanı |
| --- | --- | --- |
| **Veritabanı Yönetimi** | Metadata saklama (dosya hash, cihaz bilgileri, zaman damgaları) | SQLite veya C++ mini KV store |
| **Bilgisayar Ağları** | Peer discovery, bağlantı yönetimi, auto-remesh | TCP/UDP socket + ping latency ölçümü |
| **İşletim Sistemleri** | Dosya değişiklik takibi, thread senkronizasyonu, buffer yönetimi | `inotify` benzeri watcher, mutex, I/O cache |

---

### 1.6 Özgünlük

Bu proje, sıradan dosya senkronizasyon yazılımlarından farklı olarak:

-   **Ağın kendisini akıllıca yeniden şekillendiren** (auto-remesh) bir yapıya sahiptir.
    
-   **Dağıtık sistemlerin basitleştirilmiş bir eğitim modelidir.**
    
-   **Üç farklı dersin birleşiminden doğan çok katmanlı bir mühendislik projesi** olarak tasarlanmıştır.
    

---

### 1.7 Sonuç – Fikirin Özeti

SentinelFS-Lite, “**merkeziyetsiz senkronizasyon**” fikrini basit, anlaşılır ve uygulanabilir bir mühendislik projesine dönüştürür.  
Her cihaz bir **mini datacenter** gibi çalışır, ağ koşulları değiştikçe **kendini optimize eder**, dosya değişimlerini **delta farklarıyla aktarır** ve tüm süreci küçük bir **metadata veritabanı** üzerinden yönetir.  
Amaç, **dağıtık sistem kavramını somutlaştıran**, **akademik temeli güçlü**, **gerçek dünyada da işe yarar** bir sistem geliştirmektir.

---