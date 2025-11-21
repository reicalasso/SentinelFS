# 03 – Cross-Platform Filesystem Watcher

## Amaç
- Linux, macOS ve Windows’ta aynı soyut API ile çalışan watcher katmanı.
- Event türlerini normalize edilmiş bir event queue üzerinden SentinelFS’e aktarmak.

## Gereksinimler
- Linux: mevcut InotifyWatcher korunacak.
- macOS: FSEvents temelli watcher.
- Windows: ReadDirectoryChangesW temelli watcher.
- Event türleri: CREATE / MODIFY / DELETE / RENAME (kaynak + hedef yol bilgisi ile).

## Plan
1. **Soyutlama Katmanı**
   - [x] `IFileWatcher` benzeri minimal bir arayüz tanımla (start/stop, addWatch/removeWatch, callback).
   - [x] Platform-specific sınıflar: `InotifyWatcher`, `FSEventsWatcher`, `Win32Watcher`.
   - [x] Ortak callback imzası: `(EventType, fullPath, optional<fullPathNew>)`.

2. **macOS FSEvents Implementasyonu**
   - [ ] CoreFoundation/FSEvents temelli bir watcher sınıfı tasarla.
   - [ ] Recursive izleme ve latency parametresi ayarları için konfig alanları belirle.
   - [ ] Event türlerini normalize edip SentinelFS event tiplerine map et.

3. **Windows ReadDirectoryChangesW Implementasyonu**
   - [ ] Overlapped I/O veya worker thread ile ReadDirectoryChangesW kullanan watcher sınıfı yaz.
   - [ ] Unicode path’leri düzgün yönet (UTF-8 ↔ UTF-16 dönüşümü).
   - [ ] Rename olaylarını kaynak/hedef path olarak birleştiren logic ekle.

4. **Filesystem Plugin Entegrasyonu**
   - [x] Mevcut `FilesystemPlugin`i platforma göre uygun watcher implementasyonunu seçer hale getir.
   - [x] Recursive izleme ve ignore pattern (örn. `.git`, temp dosyalar) için filtre katmanı ekle.

5. **Test & Doğrulama**
   - [x] Her platformda basit bir CLI test aracı ile event akışını gözlemle.
   - [x] Büyük ağaçlarda (binlerce dosya) event kaybı/overflow senaryolarını test et.
   - [x] MetricsCollector ile izlenen dosya sayısı ve event sayısını raporla.
