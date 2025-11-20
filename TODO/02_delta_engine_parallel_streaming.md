# 02 – Delta Engine: Paralel & Streaming

## Amaç
- Büyük dosyalarda CPU ve bellek kullanımını optimize etmek.
- Delta hesaplama ve transfer hattını chunk-based, paralel ve stream-friendly hale getirmek.

## Gereksinimler
- Adler32 + SHA-256 hesaplaması korunacak (uyumluluk).
- CPU-bound iş yükleri için thread pool kullanımı.
- Protokol seviyesinde delta verisini parçalara ayırma (chunk streaming).

## Plan
1. **Thread Pool Altyapısı**
   - [ ] `sfs::core` içinde hafif bir thread pool (sabit worker sayısı) tasarla.
   - [ ] Görev arayüzü: `std::function<void()>` veya küçük bir task struct.
   - [ ] Pool, global değil; DeltaEngine tarafından kompozisyon olarak kullanılmalı.

2. **Signature Hesabının Paralelleştirilmesi**
   - [ ] `calculateSignature`ı refactor et: dosyayı ardışık bloklar halinde oku.
   - [ ] Her blok için Adler32+SHA256 işi, thread pool’a iş olarak verilsin.
   - [ ] Çıktı sırası blok index’ine göre stabil olacak şekilde yeniden sıralansın.

3. **Delta Hesabının Paralelleştirilmesi**
   - [ ] Eski imzaları memory’de tutarken map yapısını koru.
   - [ ] Yeni dosyayı streaming + sliding window ile işle; window’ları paralel görevler halinde dağıt.
   - [ ] Bellek kullanımını sınırlamak için maksimum aktif window boyutu tanımla.

4. **Chunk-based Transfer Protokolü**
   - [ ] Delta protokolüne `DELTA_CHUNK` konsepti ekle (örn. `DELTA_DATA|filename|chunkId/total|...`).
   - [ ] `DeltaSerialization`ı chunk’lara bölünebilir tasarla (iterator/tabanlı API).
   - [ ] Network tarafında büyük delta’ları bir anda değil, ardışık chunk’lar halinde gönder.

5. **Error Handling & Retry**
   - [ ] Chunk kaybı durumunda (timeout, hata) yeniden gönderim mantığını tanımla.
   - [ ] Yarım kalmış transferler için basit bir yeniden deneme politikası (örn. max 3 deneme).

6. **Benchmark & Test**
   - [ ] Büyük dosya (1–10 GB) senaryoları için baseline benchmark al.
   - [ ] Yeni tasarım ile CPU kullanımını ve süreyi ölç; iyileşme oranını dokümante et.
   - [ ] Unit test: küçük dosya ve delta senaryolarında eski sonuçlarla bit-eşdeğer doğrulama yap.
