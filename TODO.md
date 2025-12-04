# SentinelFS TODO / Geliştirme Planı

Bu dosya, SentinelFS için planlanan geliştirmeleri ve iyileştirmeleri özetler.

## 1. Kullanıcı Deneyimi (UX)

- [x] GUI'de onboarding sihirbazı ("İki cihazı 3 adımda bağla")
- [x] Hata mesajlarını insan-diliyle ve çözüm önerileriyle göster (ör. discovery yok, firewall kapalı mı?)
- [x] GUI log penceresine "Copy" ve "Export as file/zip" butonları ekle
- [x] Basit "support bundle" üretme (config + log + temel sistem bilgisi)

## 2. Conflict Resolution & Versiyonlama

- [ ] Basit dosya versiyonlama (N son sürümü sakla)
- [ ] Conflict durumlarında GUI'de "Conflict Center" görünümü (hangi peer, hangi sürüm)
- [ ] Last-write-wins'e ek olarak kullanıcıya seçim opsiyonu sun (keep local / keep remote / keep both)

## 3. WAN / Çok Segment Ağ Desteği

- [ ] Opsiyonel lightweight relay / introducer sunucusu tasarla (global discovery için)
- [ ] NAT arkasındaki peer'lar için temel NAT traversal stratejisi (örn. TCP hole punching veya sadece relay üzerinden)
- [ ] GUI'de "Remote peer" ekleme akışı (hostname/IP + session code)

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