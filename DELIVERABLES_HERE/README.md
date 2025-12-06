# 🛡️ SentinelFS - Proje Teslim Dosyaları

> **Peer-to-Peer Dosya Senkronizasyon Sistemi**  
> Versiyon 1.0.0 | Aralık 2025

---

## 📖 Genel Bakış

Bu klasör, SentinelFS projesinin akademik ve profesyonel teslim materyallerini içermektedir. Her bir bölüm, sistemin farklı yönlerini detaylı şekilde dokümante etmektedir.

---

## 📁 Klasör Yapısı

```
DELIVERABLES_HERE/
│
├── 📄 README.md                        # Bu dosya
│
├── 📂 config/                          # Yapılandırma Dosyaları
│   ├── sentinel.conf.template          # Ana daemon yapılandırma şablonu
│   ├── peer1.conf                      # Test ortamı: Peer 1 yapılandırması
│   └── peer2.conf                      # Test ortamı: Peer 2 yapılandırması
│
├── 📂 docs/                            # Teknik Dokümantasyon
│   └── TECHNICAL_REPORT.md             # Kapsamlı teknik analiz raporu
│
├── 📂 source/                          # Kaynak Kod Referansı
│   └── SOURCE_INDEX.md                 # Modül bazlı kod indeksi
│
├── 📂 tests/                           # Test Dokümantasyonu
│   └── TEST_RESULTS.md                 # Test senaryoları ve sonuçları
│
├── 📂 performance/                     # Performans Analizi
│   └── PERFORMANCE_REPORT.md           # Benchmark ve metrik raporları
│
└── 📂 presentation/                    # Sunum Materyalleri
    └── PRESENTATION.md                 # Proje sunumu (slaytlar)
```

---

## 📋 Doküman Özeti

| Kategori | Dosya | İçerik |
|:---------|:------|:-------|
| **Teknik Rapor** | `docs/TECHNICAL_REPORT.md` | Sistem mimarisi, protokol tasarımı, algoritma detayları |
| **Kaynak İndeksi** | `source/SOURCE_INDEX.md` | Modül bazlı kod haritası ve satır istatistikleri |
| **Test Sonuçları** | `tests/TEST_RESULTS.md` | Unit, integration ve manuel test sonuçları |
| **Performans** | `performance/PERFORMANCE_REPORT.md` | Throughput, latency ve kaynak kullanımı analizi |
| **Sunum** | `presentation/PRESENTATION.md` | Proje tanıtım slaytları |
| **Yapılandırma** | `config/*.conf` | Daemon yapılandırma dosyaları |

---

## 🔢 Proje İstatistikleri

| Metrik | Değer |
|:-------|:------|
| **Toplam Kod Satırı** | ~19,600+ |
| **C++ (Core/Daemon)** | ~16,600 satır |
| **TypeScript (GUI)** | ~3,000 satır |
| **Kaynak Dosya Sayısı** | ~150 dosya |
| **Test Coverage** | %86.6 (ortalama) |
| **Unit Test Sayısı** | 50 test |
| **Integration Test Sayısı** | 12 test |

---

## 🏗️ Proje Bilgileri

| Alan | Değer |
|:-----|:------|
| **Proje Adı** | SentinelFS |
| **Versiyon** | 1.0.0 |
| **Geliştirme Tarihi** | 2025 |
| **Programlama Dilleri** | C++17/20, TypeScript |
| **Çerçeveler** | Electron, React 18 |
| **Veritabanı** | SQLite3 (WAL mode) |
| **Şifreleme** | AES-256-CBC + HMAC-SHA256 |
| **Lisans** | MIT |

---

## 🚀 Hızlı Erişim

### Temel Dokümanlar

- 📘 [Teknik Rapor](docs/TECHNICAL_REPORT.md) - Mimari ve protokol detayları
- 📊 [Performans Raporu](performance/PERFORMANCE_REPORT.md) - Benchmark sonuçları
- ✅ [Test Sonuçları](tests/TEST_RESULTS.md) - Test coverage ve sonuçları
- 📑 [Sunum](presentation/PRESENTATION.md) - Proje sunumu

### Referans Dokümanlar

- 📂 [Kaynak Kod İndeksi](source/SOURCE_INDEX.md) - Modül haritası
- ⚙️ [Yapılandırma Şablonu](config/sentinel.conf.template) - Daemon ayarları

---

## 📝 Notlar

- Tüm dokümanlar Türkçe olarak hazırlanmıştır
- Teknik terimler için İngilizce karşılıklar parantez içinde verilmiştir
- Kod örnekleri ve diyagramlar ASCII formatında sunulmuştur
- Performans testleri LAN ortamında (Gigabit Ethernet) gerçekleştirilmiştir

---

**SentinelFS Development Team**  
📧 İletişim: team@sentinelfs.dev  
🌐 GitHub: github.com/reicalasso/SentinelFS
