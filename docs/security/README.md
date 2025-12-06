# SentinelFS Güvenlik Dokümantasyonu

**Versiyon:** 1.0.0  
**Son Güncelleme:** Aralık 2025  
**Güvenlik İletişim:** security@sentinelfs.dev

---

## İçindekiler

1. [Güvenlik Mimarisi](#1-güvenlik-mimarisi)
2. [Şifreleme](#2-şifreleme)
3. [Kimlik Doğrulama](#3-kimlik-doğrulama)
4. [Ağ Güvenliği](#4-ağ-güvenliği)
5. [Anomali Tespiti](#5-anomali-tespiti)
6. [Güvenlik Profilleri](#6-güvenlik-profilleri)
7. [Denetim ve Loglama](#7-denetim-ve-loglama)
8. [En İyi Uygulamalar](#8-en-iyi-uygulamalar)
9. [Güvenlik Açığı Bildirimi](#9-güvenlik-açığı-bildirimi)

---

## 1. Güvenlik Mimarisi

SentinelFS, çok katmanlı bir güvenlik mimarisi kullanır:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Güvenlik Katmanları                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │              Katman 5: ML Anomali Tespiti               │   │
│   │         ONNX Runtime ile davranış analizi               │   │
│   └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │              Katman 4: Denetim & İzleme                 │   │
│   │        Prometheus metrikleri, yapısal loglar            │   │
│   └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │              Katman 3: Erişim Kontrolü                  │   │
│   │           Session code kimlik doğrulama                 │   │
│   └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │              Katman 2: Veri Bütünlüğü                   │   │
│   │              HMAC-SHA256 doğrulama                      │   │
│   └─────────────────────────────────────────────────────────┘   │
│                              │                                  │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │              Katman 1: Şifreleme                        │   │
│   │           AES-256-CBC veri şifreleme                    │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Güvenlik İlkeleri

| İlke | Uygulama |
|:-----|:---------|
| **Defense in Depth** | Çoklu güvenlik katmanları |
| **Zero Trust** | Her iletişim doğrulanır |
| **Least Privilege** | Minimum gerekli yetki |
| **Encryption at Rest** | Yerel veri şifreleme |
| **Encryption in Transit** | Tüm ağ trafiği şifreli |

---

## 2. Şifreleme

### 2.1 Veri Şifreleme (AES-256-CBC)

SentinelFS, tüm dosya içeriklerini ve meta verileri AES-256-CBC ile şifreler:

```
┌─────────────────────────────────────────────────────────────┐
│                 AES-256-CBC Şifreleme                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Düz Metin     ───┬───> AES-256 ───┬───> Şifreli Metin    │
│                    │                │                       │
│   256-bit Key  ────┘                │                       │
│   Random IV    ─────────────────────┘                       │
│                                                             │
│   Özellikler:                                               │
│   • 256-bit anahtar uzunluğu                                │
│   • CBC (Cipher Block Chaining) modu                        │
│   • PKCS#7 padding                                          │
│   • Her mesaj için benzersiz IV                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Bütünlük Doğrulama (HMAC-SHA256)

Her şifreli paket, HMAC-SHA256 ile imzalanır:

```
┌─────────────────────────────────────────────────────────────┐
│                    HMAC-SHA256 Doğrulama                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Şifreli Veri  ───┬───> HMAC-SHA256 ───> MAC Tag (32 B)   │
│                    │                                        │
│   512-bit Key  ────┘                                        │
│                                                             │
│   Paket Yapısı:                                             │
│   ┌──────┬────────────────┬──────────────┬─────────────┐    │
│   │ IV   │  Encrypted     │   HMAC Tag   │  Timestamp  │    │
│   │ 16B  │  Payload       │   32 bytes   │   8 bytes   │    │
│   └──────┴────────────────┴──────────────┴─────────────┘    │
│                                                             │
│   Doğrulama Sırası:                                         │
│   1. HMAC doğrula (Encrypt-then-MAC)                        │
│   2. Timestamp kontrolü (replay koruması)                   │
│   3. Şifre çöz                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Anahtar Yönetimi

Detaylı anahtar yönetimi için: [KEY_MANAGEMENT.md](KEY_MANAGEMENT.md)

| Anahtar | Uzunluk | Kullanım |
|:--------|:--------|:---------|
| Encryption Key | 256-bit | AES-256-CBC şifreleme |
| HMAC Key | 512-bit | Bütünlük doğrulama |
| Session Key | Dinamik | Oturum bazlı türetme |

```bash
# Güvenli anahtar oluşturma
openssl rand -hex 32 > encryption.key  # AES-256
openssl rand -hex 64 > hmac.key        # HMAC-SHA256
```

---

## 3. Kimlik Doğrulama

### 3.1 Session Code Sistemi

SentinelFS, peer kimlik doğrulaması için session code kullanır:

```
┌─────────────────────────────────────────────────────────────┐
│                Session Code Kimlik Doğrulama                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Peer A                              Peer B                │
│     │                                   │                   │
│     │──────── HELLO + Nonce ──────────>│                   │
│     │                                   │                   │
│     │<─────── CHALLENGE + Nonce ───────│                   │
│     │                                   │                   │
│     │    Response = HMAC(SessionCode,  │                   │
│     │               NonceA || NonceB)   │                   │
│     │                                   │                   │
│     │──────── AUTH_RESPONSE ──────────>│                   │
│     │                                   │                   │
│     │<─────── AUTH_SUCCESS ────────────│                   │
│     │                                   │ (Session Key      │
│     │        Güvenli İletişim          │  türetilir)       │
│     │<═══════════════════════════════=>│                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 Session Code Gereksinimleri

| Özellik | Gereksinim |
|:--------|:-----------|
| Minimum uzunluk | 8 karakter |
| Önerilen uzunluk | 16+ karakter |
| Karakter seti | A-Z, a-z, 0-9, özel karakterler |
| Entropi | Minimum 64-bit |

### 3.3 Güvenli Session Code Oluşturma

```bash
# Yöntem 1: OpenSSL
openssl rand -base64 24 | tr -dc 'a-zA-Z0-9' | head -c 16

# Yöntem 2: /dev/urandom
head -c 24 /dev/urandom | base64 | tr -dc 'a-zA-Z0-9' | head -c 16

# Yöntem 3: pwgen
pwgen -s 16 1
```

---

## 4. Ağ Güvenliği

### 4.1 Port Kullanımı

| Port | Protokol | Kullanım | Güvenlik |
|:-----|:---------|:---------|:---------|
| 8082 | TCP | Veri transferi | Şifreli |
| 8083 | UDP | Peer keşfi | İmzalı |
| 9100 | TCP | Metrics | Internal only |

### 4.2 Firewall Kuralları

```bash
# UFW (Ubuntu)
sudo ufw allow from 192.168.0.0/16 to any port 8082 proto tcp
sudo ufw allow from 192.168.0.0/16 to any port 8083 proto udp
sudo ufw deny from any to any port 9100

# iptables
iptables -A INPUT -p tcp --dport 8082 -s 192.168.0.0/16 -j ACCEPT
iptables -A INPUT -p udp --dport 8083 -s 192.168.0.0/16 -j ACCEPT
iptables -A INPUT -p tcp --dport 9100 -j DROP
```

### 4.3 Ağ Trafik Akışı

```
┌─────────────────────────────────────────────────────────────┐
│                    Güvenli Veri Akışı                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Dosya Değişikliği                                         │
│         │                                                   │
│         ▼                                                   │
│   ┌─────────────┐                                           │
│   │ Delta       │  Sadece değişen bloklar                  │
│   │ Hesaplama   │                                           │
│   └──────┬──────┘                                           │
│          │                                                  │
│          ▼                                                  │
│   ┌─────────────┐                                           │
│   │ AES-256-CBC │  Blok şifreleme                          │
│   │ Şifreleme   │                                           │
│   └──────┬──────┘                                           │
│          │                                                  │
│          ▼                                                  │
│   ┌─────────────┐                                           │
│   │ HMAC-SHA256 │  Bütünlük imzası                         │
│   │ İmzalama    │                                           │
│   └──────┬──────┘                                           │
│          │                                                  │
│          ▼                                                  │
│   ┌─────────────┐                                           │
│   │ TCP/8082    │  Şifreli iletim                          │
│   │ Transfer    │                                           │
│   └─────────────┘                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. Anomali Tespiti

### 5.1 ML Tabanlı Analiz

SentinelFS, ONNX Runtime ile çalışan ML modeli kullanarak anormal davranışları tespit eder:

```
┌─────────────────────────────────────────────────────────────┐
│                  ML Anomali Tespiti                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Özellik Vektörü:                                          │
│   ┌────────────────────────────────────────────────────┐    │
│   │ • Dosya operasyonu sayısı/dakika                   │    │
│   │ • Ortalama dosya boyutu                            │    │
│   │ • Silme/oluşturma oranı                            │    │
│   │ • Ağ trafiği paterni                               │    │
│   │ • Zaman damgası anomalisi                          │    │
│   │ • Peer davranış puanı                              │    │
│   └────────────────────────────────────────────────────┘    │
│                       │                                     │
│                       ▼                                     │
│   ┌────────────────────────────────────────────────────┐    │
│   │            ONNX Autoencoder Model                  │    │
│   │   Input → Encoder → Latent → Decoder → Output      │    │
│   └────────────────────────────────────────────────────┘    │
│                       │                                     │
│                       ▼                                     │
│   ┌────────────────────────────────────────────────────┐    │
│   │         Reconstruction Error > Threshold           │    │
│   │                                                    │    │
│   │   Normal: Error < 0.7                              │    │
│   │   Şüpheli: 0.7 ≤ Error < 0.9                       │    │
│   │   Anomali: Error ≥ 0.9                             │    │
│   └────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 Tespit Edilen Tehditler

| Tehdit | Tespit Yöntemi | Eylem |
|:-------|:---------------|:------|
| Ransomware | Yüksek şifreleme oranı | Sync durdur |
| Data exfiltration | Anormal upload | Uyarı |
| Brute force | Çoklu auth hatası | IP engelle |
| Replay attack | Timestamp kontrolü | Reddet |

### 5.3 Otomatik Yanıt

```yaml
# Anomali tetiklendiğinde:
anomaly_response:
  low:
    - log_warning
    - increment_metric
  medium:
    - alert_admin
    - throttle_peer
  high:
    - pause_sync
    - quarantine_files
    - notify_all_peers
```

---

## 6. Güvenlik Profilleri

### 6.1 Profil Seçenekleri

| Profil | Kullanım | Güvenlik Seviyesi |
|:-------|:---------|:------------------|
| **default** | Genel kullanım | Orta |
| **paranoid** | Yüksek güvenlik | Yüksek |
| **performance** | Düşük gecikme | Düşük |
| **lan-only** | Sadece yerel ağ | Orta |

### 6.2 Profil Yapılandırması

```ini
# sentinel.conf

[Security]
profile = default

# Veya özelleştirilmiş:
encryption_enabled = true
integrity_check = true
session_auth = true
anomaly_detection = true
replay_protection = true
timestamp_tolerance = 300  # saniye
```

### 6.3 Paranoid Profil

```ini
[Security]
profile = paranoid

# Ek ayarlar
key_rotation_interval = 3600    # 1 saat
max_auth_failures = 3
auth_lockout_duration = 600     # 10 dakika
require_mutual_auth = true
min_session_code_length = 24
anomaly_threshold = 0.5         # Daha hassas
```

---

## 7. Denetim ve Loglama

### 7.1 Güvenlik Olayları

| Olay | Seviye | Log |
|:-----|:-------|:----|
| Auth success | INFO | ✓ |
| Auth failure | WARNING | ✓ |
| Encryption error | ERROR | ✓ |
| HMAC mismatch | ERROR | ✓ |
| Anomaly detected | CRITICAL | ✓ |
| Sync paused (security) | CRITICAL | ✓ |

### 7.2 Log Formatı

```json
{
  "timestamp": "2025-12-05T14:32:18.456Z",
  "level": "WARNING",
  "event": "AUTH_FAILURE",
  "peer_id": "PEER_82844",
  "peer_ip": "192.168.1.105",
  "reason": "invalid_session_code",
  "attempt": 2,
  "metadata": {
    "user_agent": "SentinelFS/1.0.0"
  }
}
```

### 7.3 Metrik İzleme

```bash
# Prometheus sorguları

# Auth hataları
rate(sentinelfs_auth_failures_total[5m])

# Şifreleme hataları
rate(sentinelfs_encryption_errors_total[5m])

# Anomali tespitleri
increase(sentinelfs_anomalies_detected_total[1h])
```

---

## 8. En İyi Uygulamalar

### 8.1 Güvenlik Kontrol Listesi

```
□ Güçlü session code kullanın (min. 16 karakter)
□ Şifreleme anahtarlarını güvenli saklayın
□ Firewall kurallarını yapılandırın
□ Metrics port'unu dışarıya kapatın
□ Log rotation ayarlayın
□ Düzenli yedekleme yapın
□ Güncellemeleri takip edin
□ Anomali uyarılarını izleyin
```

### 8.2 Yapılmaması Gerekenler

```
✗ Session code'u plaintext saklamayın
✗ Şifreleme anahtarlarını paylaşmayın
✗ Metrics port'unu internete açmayın
✗ Root olarak çalıştırmayın
✗ Eski versiyonları kullanmayın
✗ Logları silmeyin
```

### 8.3 Periyodik Kontroller

| Görev | Sıklık |
|:------|:-------|
| Log inceleme | Günlük |
| Güvenlik güncellemesi | Haftalık |
| Anahtar rotasyonu | Aylık |
| Güvenlik denetimi | Üç aylık |

---

## 9. Güvenlik Açığı Bildirimi

### 9.1 Responsible Disclosure

Güvenlik açığı keşfettiyseniz:

1. **E-posta:** security@sentinelfs.dev
2. **PGP Key:** [pubkey.asc](https://sentinelfs.dev/pubkey.asc)
3. **Response Time:** 48 saat içinde yanıt

### 9.2 Bildirim Formatı

```
Konu: [SECURITY] Kısa açıklama

1. Açık türü
2. Etkilenen bileşen
3. Yeniden üretme adımları
4. Olası etki
5. Önerilen düzeltme (varsa)
```

### 9.3 Bug Bounty

Kritik güvenlik açıkları için ödül programımız mevcuttur. Detaylar için: https://sentinelfs.dev/security/bounty

---

## İlgili Dokümanlar

- [KEY_MANAGEMENT.md](KEY_MANAGEMENT.md) - Anahtar yönetimi detayları
- [../PRODUCTION.md](../PRODUCTION.md) - Production güvenlik ayarları
- [../MONITORING.md](../MONITORING.md) - Güvenlik izleme

---

**Güvenlik Dokümantasyonu Sonu**

*SentinelFS Security Team - Aralık 2025*
