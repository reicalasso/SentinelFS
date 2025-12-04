# SentinelFS TODO / Geliştirme Planı

Bu dosya, SentinelFS için planlanan geliştirmeleri ve iyileştirmeleri özetler.

## 1. Kullanıcı Deneyimi (UX)

- [x] GUI'de onboarding sihirbazı ("İki cihazı 3 adımda bağla")
- [x] Hata mesajlarını insan-diliyle ve çözüm önerileriyle göster (ör. discovery yok, firewall kapalı mı?)
- [x] GUI log penceresine "Copy" ve "Export as file/zip" butonları ekle
- [x] Basit "support bundle" üretme (config + log + temel sistem bilgisi)

## 2. Conflict Resolution & Versiyonlama

- [x] Basit dosya versiyonlama (N son sürümü sakla)
  - `FileVersionManager` C++ sınıfı oluşturuldu (core/storage/)
  - Configurable: maxVersionsPerFile, maxTotalVersionsBytes
  - Otomatik pruning ve metadata yönetimi
- [x] Conflict durumlarında GUI'de "Conflict Center" görünümü (hangi peer, hangi sürüm)
  - `ConflictCenter.tsx` React komponenti eklendi
  - Dual-view: Conflicts tab + Versions tab
  - Detaylı karşılaştırma: local vs remote, hash, timestamp, peer bilgisi
- [x] Last-write-wins'e ek olarak kullanıcıya seçim opsiyonu sun (keep local / keep remote / keep both)
  - GUI'de 3 seçenek: Keep Local, Keep Remote, Keep Both
  - IPC komutları: RESOLVE <id> LOCAL|REMOTE|BOTH
  - Version restore/preview/delete desteği

## 3. WAN / Çok Segment Ağ Desteği

- [x] Opsiyonel lightweight relay / introducer sunucusu tasarla (global discovery için)
  - Python asyncio tabanlı relay sunucusu: `relay/relay_server.py`
  - Session code tabanlı peer gruplandırma
  - REST API endpoint: `/peers?session=...`
  - Docker/Docker Compose desteği
  - Heartbeat ve otomatik temizlik mekanizması
- [x] NAT arkasındaki peer'lar için temel NAT traversal stratejisi (örn. TCP hole punching veya sadece relay üzerinden)
  - TCPRelay client'a connectToRelay/disconnectFromRelay metodları eklendi
  - Daemon'a RELAY_CONNECT, RELAY_DISCONNECT, RELAY_STATUS, RELAY_PEERS komutları
  - NAT type bilgisi peer metadata'sında
- [x] GUI'de "Remote peer" ekleme akışı (hostname/IP + session code)
  - Peers.tsx'e "Add Remote" butonu ve modal eklendi
  - Session code oluşturma/kopyalama/regenerate
  - Relay sunucusundan peer listesi görüntüleme
  - Bağlantı durumu göstergesi

## 4. Güvenlik Sertleşmesi (Hardening)

- [ ] Handshake aşamasına TLS sertifika doğrulama / pinning ekle
- [ ] IPC socket dosyası için daha sıkı izinler ve konfigüre edilebilir path
- [ ] AppArmor/SELinux profil örnekleri ekle (docs/ altında)
- [ ] Session code yanında uzun-ömürlü key + kısa-ömürlü session ayrımını tasarla

## 5. Monitoring & Operasyonellik

- [ ] Prometheus endpoint şemasını finalize et ve örnek Grafana dashboard JSON ekle
- [ ] Liveness / readiness health-check endpoint'leri (production için)
- [ ] systemd unit ile birlikte log formatı ve log rotation senaryosunu dokümante et

## 6. CLI Geliştirmeleri

- [ ] `sentinel_cli` için gerçek komutlar implemente et:
  - [ ] `status`, `peers`, `files`, `config`
  - [ ] `add-watch`, `remove-watch`
  - [ ] `pause`, `resume`, `set-session-code`
- [ ] CLI çıktıları için JSON mod ekle (otomasyon/script dostu)

## 7. Testler & CI/CD

- [ ] GUI için temel e2e/smoke testi ekle (Playwright/Cypress)
- [ ] GitHub Actions pipeline'ı: build + unit/integration + lint + basic GUI tests
- [ ] Load/performance test senaryolarını otomatikleştirme (ör. büyük dosya, çok peer)

## 8. Platform & Paketleme

- [ ] Windows installer (MSIX/NSIS) ve macOS için DMG/PKG hikayesini tasarla
- [ ] Destek matrisi dokümanı: hangi OS'de hangi feature stable/experimental
- [ ] AppImage dışı dağıtım kanalları (örn. Flatpak, winget, Homebrew) için plan

## 9. Seçmeli Sync ve Filtreler

- [ ] Selective sync (klasör/dosya bazlı) UI ve core mantığı
- [ ] Gelişmiş ignore pattern yönetimi (GUI üzerinden .gitignore benzeri)

## 10. ML / Anomali Algılama (Opsiyonel)

- [ ] ML plugin'ini "experimental" flag'i ile net işaretle (hem code hem GUI)
- [ ] Model güncelleme / versiyonlama stratejisi
- [ ] Gerçek kullanım senaryoları için metrik toplama (false positive/negative oranları)

## 11. Dokümantasyon İyileştirmeleri

- [ ] Kısa ve görsel ağırlıklı "Getting Started" rehberi
- [ ] Production runbook (sorun giderme akışları, sık görülen hatalar)
- [ ] Teknik rapor ile gerçek kod arasındaki sapmaları periyodik gözden geçirme

---

Bu liste, teknik rapordaki "Bilinen Limitasyonlar" ve "Gelecek Geliştirmeler" ile README'de dolaylı olarak ima edilen eksikleri bir araya getirir. Prioritization için ayrı bir sütun/etiketleme daha sonra eklenebilir.