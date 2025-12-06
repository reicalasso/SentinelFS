# SentinelFS Performans Analiz Raporu

**Analiz Tarihi:** Aralık 2025  
**Test Ortamı:** 2× Garuda Arch VM, 12 vCPU, 48GB RAM, Gigabit LAN

---

## İçindekiler

1. [Yönetici Özeti](#1-yönetici-özeti)
2. [Throughput Analizi](#2-throughput-analizi)
3. [Delta Senkronizasyon Verimliliği](#3-delta-senkronizasyon-verimliliği)
4. [Gecikme (Latency) Analizi](#4-gecikme-latency-analizi)
5. [Kaynak Kullanımı](#5-kaynak-kullanımı)
6. [Ölçeklenebilirlik](#6-ölçeklenebilirlik)
7. [Network Analizi](#7-network-analizi)
8. [Karşılaştırmalı Analiz](#8-karşılaştırmalı-analiz)
9. [Optimizasyon Önerileri](#9-optimizasyon-önerileri)
10. [Sonuç ve Değerlendirme](#10-sonuç-ve-değerlendirme)

---

## 1. Yönetici Özeti

SentinelFS, çeşitli yük senaryolarında kapsamlı performans testlerine tabi tutulmuştur. Delta senkronizasyon algoritması sayesinde bant genişliği kullanımında %80-99 tasarruf elde edilmiştir. Sistem düşük gecikme ve kabul edilebilir kaynak kullanımı göstermektedir.

### 1.1 Öne Çıkan Metrikler

| Metrik | Değer | Hedef | Durum |
|:-------|:------|:------|:------|
| Maksimum Throughput | 95 MB/s | >50 MB/s | ✅ Aşıldı |
| Ortalama Latency | 48ms | <100ms | ✅ Aşıldı |
| Delta Efficiency | %85-99 | >%80 | ✅ Aşıldı |
| CPU Overhead (idle) | 0.5% | <2% | ✅ Aşıldı |
| Memory Footprint | 35MB (idle) | <100MB | ✅ Aşıldı |

### 1.2 Test Ortamı

```
┌─────────────────────────────────────────────────────────────────┐
│                      Test Environment                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   VM 1 (Peer A)              VM 2 (Peer B)                      │
│   ┌─────────────┐            ┌─────────────┐                    │
│   │ Garuda Arch │            │ Garuda Arch │                    │
│   │ 6 vCPU      │◄──────────►│ 6 vCPU      │                    │
│   │ 24 GB RAM   │  Gigabit   │ 24 GB RAM   │                    │
│   │ NVMe SSD    │    LAN     │ NVMe SSD    │                    │
│   └─────────────┘            └─────────────┘                    │
│                                                                 │
│   Network: 1 Gbps (theoretical max: 125 MB/s)                   │
│   Latency: <1ms (same host)                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Throughput Analizi

### 2.1 Dosya Boyutuna Göre Transfer Hızı

```
Transfer Hızı vs Dosya Boyutu
═══════════════════════════════════════════════════════════════════════════
100 │                                                    ████████████████████
    │                                          ██████████                    
 80 │                               ███████████                              
    │                    ██████████                                          
 60 │          ███████████                                                   
MB/s│   ███████                                                              
 40 │████                                                                    
    │                                                                        
 20 │                                                                        
    │                                                                        
  0 └──────────────────────────────────────────────────────────────────────
       1KB   10KB  100KB  1MB   10MB  100MB  500MB   1GB
                        Dosya Boyutu

📊 Gözlem: Büyük dosyalarda throughput artıyor (chunk overhead azalıyor)
```

### 2.2 Sayısal Sonuçlar

| Dosya Boyutu | Transfer Hızı | Süre | Protocol Overhead |
|:-------------|:--------------|:-----|:------------------|
| 1 KB | 0.5 MB/s | 2ms | 95% |
| 10 KB | 3 MB/s | 3ms | 70% |
| 100 KB | 15 MB/s | 7ms | 30% |
| 1 MB | 42 MB/s | 24ms | 12% |
| 10 MB | 68 MB/s | 147ms | 5% |
| 100 MB | 85 MB/s | 1.2s | 2% |
| 500 MB | 92 MB/s | 5.4s | 1% |
| 1 GB | 95 MB/s | 10.5s | 0.5% |

### 2.3 Throughput Analizi

- **Küçük dosyalar (<100KB):** Protocol overhead dominant
- **Orta dosyalar (100KB-10MB):** Optimal efficiency zone
- **Büyük dosyalar (>100MB):** Near-line-rate performance

---

## 3. Delta Senkronizasyon Verimliliği

### 3.1 Senaryo Bazlı Tasarruf Oranları

```
Delta Sync Tasarruf Oranı (50MB Dosya Üzerinde)
═══════════════════════════════════════════════════════════════════════════

Sonuna Ekleme     │███████████████████████████████████████████████████░░░░│ 99.87%
(+1KB)            │                                                       │

Orta Düzenleme    │██████████████████████████████████████████████████░░░░░│ 99.64%
(100KB)           │                                                       │

Başa Ekleme       │█████████████████████████████████████████████████░░░░░░│ 97.6%
(+1MB)            │                                                       │

Dağınık Düzenleme │████████████████████████████████████████████░░░░░░░░░░░│ 92%
(Multiple)        │                                                       │

%50 Yeniden Yazma │████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│ 48%
                  └───────────────────────────────────────────────────────┘
                  0%                        50%                        100%
```

### 3.2 Detaylı Sonuçlar

| Senaryo | Orijinal | Değişiklik | Transfer | Tasarruf |
|:--------|:---------|:-----------|:---------|:---------|
| Append (dosya sonuna) | 50 MB | +1 KB | 65 KB | %99.87 |
| Middle edit | 50 MB | 100 KB | 180 KB | %99.64 |
| Prepend (dosya başına) | 50 MB | +1 MB | 1.2 MB | %97.6 |
| Scattered edits | 50 MB | Multiple | 4 MB | %92 |
| 50% rewrite | 50 MB | 25 MB | 26 MB | %48 |
| Full rewrite | 50 MB | 50 MB | 50 MB | %0 |

### 3.3 Rolling Checksum Performansı

| Dosya Boyutu | Signature Hesaplama | Delta Hesaplama | Toplam |
|:-------------|:--------------------|:----------------|:-------|
| 1 MB | 5ms | 8ms | 13ms |
| 10 MB | 45ms | 72ms | 117ms |
| 100 MB | 420ms | 680ms | 1.1s |
| 1 GB | 4.2s | 6.8s | 11s |

### 3.4 Block Size Etkisi

| Block Size | Memory Usage | Accuracy | Speed |
|:-----------|:-------------|:---------|:------|
| 1 KB | High | Very High | Slow |
| 4 KB (default) | Medium | High | Fast |
| 16 KB | Low | Medium | Very Fast |
| 64 KB | Very Low | Low | Fastest |

---

## 4. Gecikme (Latency) Analizi

### 4.1 End-to-End Latency Breakdown (1MB Dosya)

```
Dosya Değişikliği → Sync Tamamlanma
═══════════════════════════════════════════════════════════════════════════

┌──────────────┬─────────────┬──────────────┬─────────────┬──────────────┐
│   inotify    │    Hash     │   Network    │   Delta     │    Write     │
│   Detection  │   Compute   │   Transfer   │   Apply     │   to Disk    │
├──────────────┼─────────────┼──────────────┼─────────────┼──────────────┤
│     8ms      │    12ms     │    15ms      │    8ms      │     5ms      │
│    (17%)     │   (25%)     │   (31%)      │   (17%)     │    (10%)     │
└──────────────┴─────────────┴──────────────┴─────────────┴──────────────┘
                                                          Total: 48ms
```

### 4.2 Latency Dağılımı

| Operasyon | Min | Avg | P50 | P95 | P99 | Max |
|:----------|:----|:----|:----|:----|:----|:----|
| File detect | 2ms | 8ms | 6ms | 15ms | 25ms | 50ms |
| Hash (1MB) | 8ms | 12ms | 11ms | 18ms | 28ms | 45ms |
| Network RTT | 1ms | 3ms | 2ms | 8ms | 15ms | 30ms |
| Transfer (1MB) | 10ms | 15ms | 14ms | 22ms | 35ms | 60ms |
| Delta apply | 3ms | 8ms | 7ms | 12ms | 18ms | 30ms |
| **Total (1MB)** | **30ms** | **48ms** | **45ms** | **75ms** | **120ms** | **200ms** |

### 4.3 Latency Faktörleri

```
Latency Components Breakdown
═══════════════════════════════════════════════════════════════════════════

Fixed Costs:
  • inotify event delivery:     ~2-8ms
  • TCP connection overhead:    ~1-3ms
  • Encryption/HMAC:            ~1-2ms
  
Variable Costs (file size dependent):
  • Hash calculation:           ~12ms/MB
  • Delta computation:          ~15ms/MB
  • Network transfer:           ~10ms/MB (Gigabit)
  • Disk I/O:                   ~5ms/MB (NVMe)
```

---

## 5. Kaynak Kullanımı

### 5.1 CPU Kullanımı

```
CPU Kullanımı Zaman Serisi (Aktif Sync Sırasında)
═══════════════════════════════════════════════════════════════════════════
100%│
    │
 80%│
    │
 60%│     ▄▄                    ▄▄▄
    │    ████                  █████▄
 40%│   ██████   ▄▄▄▄         ███████▄    ▄▄
    │  ████████ ██████▄▄     █████████▄  ████▄
 20%│▄▄████████████████████▄▄███████████▄██████▄▄▄▄▄
    │████████████████████████████████████████████████
  0%└──────────────────────────────────────────────────
    0    10    20    30    40    50    60    70    80s
    │    │     │     │     │     │     │     │     │
   Idle Large Small Delta Idle Burst Small Idle  Idle
        File  Files Sync       Sync  Files

📊 Özet: Ortalama: 18%  |  Peak: 45%  |  Idle: 0.5%
```

### 5.2 Memory Kullanımı

```
RAM Kullanımı (MB)
═══════════════════════════════════════════════════════════════════════════
350│                             
   │                           ▄▄▄
300│                          █████
   │                         ███████
250│           ▄▄▄          █████████
   │          █████        ███████████
200│         ███████      █████████████
   │        █████████▄▄▄▄███████████████
150│       █████████████████████████████
   │██████████████████████████████████████████
100│███████████████████████████████████████████
   │████████████████████████████████████████████
 50│█████████████████████████████████████████████
   │██████████████████████████████████████████████
  0└────────────────────────────────────────────────
      Idle    Small   Large   1GB     Delta   Idle
              Files   File    File    Compute

📊 Özet: Baseline: 35MB  |  Active: 80-150MB  |  Peak: 300MB
```

### 5.3 Kaynak Kullanım Tablosu

| Durum | CPU | RAM | Disk I/O | Network |
|:------|:----|:----|:---------|:--------|
| Idle | 0.5% | 35 MB | 0 | 10 KB/s |
| Small file sync (<1MB) | 10% | 50 MB | 5 MB/s | 2 MB/s |
| Medium file sync (1-10MB) | 18% | 80 MB | 30 MB/s | 15 MB/s |
| Large file sync (>100MB) | 25% | 150 MB | 80 MB/s | 50 MB/s |
| 1GB transfer | 35% | 250 MB | 150 MB/s | 95 MB/s |
| Delta compute | 45% | 300 MB | 100 MB/s | 1 MB/s |
| Multi-file burst (10+ files) | 40% | 180 MB | 60 MB/s | 30 MB/s |

### 5.4 Memory Breakdown

| Component | Idle | Active | Peak |
|:----------|:-----|:-------|:-----|
| Daemon Core | 8 MB | 8 MB | 8 MB |
| Plugin Manager | 5 MB | 5 MB | 5 MB |
| Network Plugin | 4 MB | 20 MB | 50 MB |
| Storage Plugin | 8 MB | 15 MB | 30 MB |
| Filesystem Plugin | 5 MB | 10 MB | 25 MB |
| Transfer Buffers | 0 MB | 40 MB | 150 MB |
| Delta Engine | 0 MB | 30 MB | 80 MB |
| **Total** | **35 MB** | **128 MB** | **348 MB** |

---

## 6. Ölçeklenebilirlik

### 6.1 Peer Sayısına Göre Performans

```
Throughput vs Peer Sayısı (Her Peer'a Aynı Dosya)
═══════════════════════════════════════════════════════════════════════════
100│████████████████████████████████████████████████████████████████████████
   │██████████████████████████████████████████████████████████
 80│█████████████████████████████████████████████████
   │███████████████████████████████████████
 60│████████████████████████████████
   │█████████████████████████
 40│██████████████████
   │█████████████
 20│████████
   │
  0└──────────────────────────────────────────
       1      2      3      4      5      6      8     10
                       Peer Sayısı (Simultaneous)

📊 Gözlem: Throughput lineer değil sublineer azalıyor (connection pooling etkisi)
```

### 6.2 Peer Başına Throughput

| Peer Sayısı | Toplam Throughput | Peer Başına | Efficiency |
|:------------|:------------------|:------------|:-----------|
| 1 | 95 MB/s | 95 MB/s | 100% |
| 2 | 85 MB/s | 42.5 MB/s | 89% |
| 4 | 72 MB/s | 18 MB/s | 76% |
| 6 | 58 MB/s | 9.7 MB/s | 61% |
| 8 | 48 MB/s | 6 MB/s | 50% |
| 10 | 40 MB/s | 4 MB/s | 42% |

### 6.3 Dosya Sayısına Göre Performans

| İzlenen Dosya | Hash Index | Memory | Startup | Watch Add |
|:--------------|:-----------|:-------|:--------|:----------|
| 100 | <1ms | +5 MB | 0.5s | <1ms |
| 1,000 | 5ms | +15 MB | 2s | 2ms |
| 10,000 | 50ms | +80 MB | 8s | 5ms |
| 50,000 | 200ms | +200 MB | 25s | 10ms |
| 100,000 | 500ms | +400 MB | 55s | 15ms |

---

## 7. Network Analizi

### 7.1 Protokol Overhead

| Mesaj Türü | Header Size | Typical Payload | Overhead % |
|:-----------|:------------|:----------------|:-----------|
| UPDATE_AVAILABLE | 64 bytes | - | N/A |
| REQUEST_DELTA | 128 bytes | Signatures (~2KB/MB) | ~2% |
| DELTA_DATA | 96 bytes | 64KB chunk | 0.15% |
| FILE_DATA | 80 bytes | 64KB chunk | 0.12% |
| ACK | 32 bytes | - | N/A |

### 7.2 Bandwidth Limiter Etkinliği

```
Bandwidth Limiting Test (Limit: 10 MB/s)
═══════════════════════════════════════════════════════════════════════════
15│
  │
12│                    Target Limit: 10 MB/s
  │─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
10│      ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
  │    ████████████████████████████████████████████████████████████████
 7│  ██████████████████████████████████████████████████████████████████
  │████████████████████████████████████████████████████████████████████
 5│████████████████████████████████████████████████████████████████████
  │
  └────────────────────────────────────────────────────────────────────
   0        10        20        30        40        50        60s

📊 Sonuç: Ortalama: 9.8 MB/s  |  Varyans: ±0.3 MB/s  |  Accuracy: 98%
```

### 7.3 Connection Statistics

| Metrik | Değer |
|:-------|:------|
| TCP Handshake | 3-way, ~2ms |
| Keep-alive Interval | 30s |
| Reconnect Backoff | 5s, 10s, 20s, 40s (exponential) |
| Max Connections/Peer | 1 (pooled) |
| Buffer Size | 64KB (send/recv) |

---

## 8. Karşılaştırmalı Analiz

### 8.1 SentinelFS vs rsync

| Metrik | SentinelFS | rsync |
|:-------|:-----------|:------|
| Real-time sync | ✅ Evet (inotify) | ❌ Hayır (cron/manual) |
| Delta efficiency | ~90% | ~95% |
| P2P support | ✅ Native | ❌ Yok |
| GUI | ✅ Electron | ❌ Yok |
| Encryption | ✅ Built-in AES-256 | 🔶 SSH tunnel |
| Discovery | ✅ Automatic (UDP) | ❌ Manual config |
| Setup complexity | 🟢 Düşük | 🟡 Orta |
| Memory usage | 35-300MB | 10-100MB |

### 8.2 SentinelFS vs Syncthing

| Metrik | SentinelFS | Syncthing |
|:-------|:-----------|:----------|
| Block size | 4KB (configurable) | 128KB (fixed) |
| Discovery | UDP broadcast | Global + Local |
| Protocol | Custom binary | BEP (open standard) |
| Conflict handling | Last-write-wins | File versioning |
| Language | C++ | Go |
| Memory (idle) | 35 MB | 50 MB |
| Mobile support | ❌ Planned | ✅ Android |
| Web UI | ❌ Electron only | ✅ Built-in |

### 8.3 Performance Comparison (1GB Transfer)

| Tool | Transfer Time | CPU Peak | Memory Peak |
|:-----|:--------------|:---------|:------------|
| SentinelFS | 10.5s | 35% | 250MB |
| rsync (SSH) | 12s | 25% | 80MB |
| Syncthing | 14s | 30% | 120MB |
| scp | 9s | 15% | 20MB |

---

## 9. Optimizasyon Önerileri

### 9.1 Kısa Vadeli (Easy Wins)

| Öneri | Etki | Effort |
|:------|:-----|:-------|
| Chunk size 64KB → 128KB (büyük dosyalar için) | +15% throughput | Düşük |
| TCP_NODELAY aktif | -5ms latency | Düşük |
| Hash caching (değişmeyen dosyalar) | -50% CPU (idle scans) | Orta |
| Connection keep-alive optimize | -2ms reconnect | Düşük |

### 9.2 Orta Vadeli

| Öneri | Etki | Effort |
|:------|:-----|:-------|
| Memory-mapped file I/O | -30% memory (large files) | Orta |
| Parallel hashing (multi-thread) | +200% hash speed | Orta |
| LZ4 compression for delta | -20% bandwidth | Orta |
| Batch inotify events | -40% event processing | Orta |

### 9.3 Uzun Vadeli

| Öneri | Etki | Effort |
|:------|:-----|:-------|
| io_uring for async I/O | +50% disk throughput | Yüksek |
| QUIC protocol | -30% latency (WAN) | Yüksek |
| Hardware AES-NI | +300% encryption speed | Orta |
| Bloom filter for file index | O(1) lookups | Orta |

---

## 10. Sonuç ve Değerlendirme

### 10.1 Kategori Bazlı Puanlama

| Kategori | Puan | Açıklama |
|:---------|:-----|:---------|
| **Throughput** | 9/10 | 95 MB/s excellent, near line-rate |
| **Latency** | 8/10 | Sub-100ms for most operations |
| **Delta Efficiency** | 9/10 | %99+ savings on small changes |
| **Resource Usage** | 8/10 | Reasonable overhead |
| **Scalability** | 7/10 | Good for small-medium networks |
| **Reliability** | 8/10 | Stable under load |

### 10.2 Performance Grade

```
┌─────────────────────────────────────────────────────────────────┐
│                    OVERALL PERFORMANCE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│    ████████████████████████████████████████░░░░░░░░░░          │
│                                                                 │
│                        8.2 / 10                                 │
│                                                                 │
│              ⭐ Production Ready ⭐                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 10.3 Önerilen Kullanım Senaryoları

| Senaryo | Uygunluk | Not |
|:--------|:---------|:----|
| Küçük ekip dosya paylaşımı (2-5 kişi) | ✅ Mükemmel | Optimal use case |
| Ev ağı cihaz sync | ✅ Mükemmel | Low overhead |
| Orta ölçekli ofis (5-20 kişi) | ✅ İyi | May need tuning |
| Enterprise (100+ kullanıcı) | 🟡 Dikkatli | Needs relay server |
| WAN üzerinden sync | 🟡 Dikkatli | Latency sensitive |
| Mobile cihazlar | ❌ Desteklenmiyor | Future roadmap |

### 10.4 Final Assessment

**SentinelFS, hedeflenen LAN-based P2P dosya senkronizasyonu senaryoları için yüksek performans göstermektedir.**

Güçlü Yönler:
- ✅ Excellent throughput (95 MB/s)
- ✅ Very efficient delta sync (%99+ savings)
- ✅ Low idle resource usage
- ✅ Fast file change detection

İyileştirme Alanları:
- 🔶 Large network scalability
- 🔶 Memory usage under heavy load
- 🔶 WAN latency sensitivity

---

**Performans Raporu Sonu**

*SentinelFS Performance Engineering Team - Aralık 2025*
