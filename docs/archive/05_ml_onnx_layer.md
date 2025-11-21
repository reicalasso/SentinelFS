# 05 – ML / ONNX Anomali Tespit Katmanı

## Amaç
- Heuristik AnomalyDetector yerine/yanına gerçek bir ONNX tabanlı model (IsolationForest vb.).
- Inference’i daemon’ı bloklamadan, async worker üzerinden çalıştırmak.

## Gereksinimler
- ONNX Runtime entegrasyonu (opsiyonel, build flag ile aç/kapat).
- Skorların 0–1 arası normalize edilmesi.
- Yüksek risk skorunda EventBus üzerinden `ANOMALY_DETECTED` event’i üretmek.

## Plan
1. **Build & Opsiyonellik**
   - [ ] CMake tarafında `SFS_ENABLE_ML` benzeri bir opsiyon ekle.
   - [ ] Bu flag kapalıyken ML plugin’i derlenmesin veya boş bir stub ile değiştirilsin.

2. **Model Entegrasyonu**
   - [ ] ONNX Runtime için minimal bir wrapper sınıfı yaz (model yükleme, input hazırlama, inference çağrısı).
   - [ ] Girdi özelliklerini belirle: zaman başına değişen dosya sayısı, pattern’ler, deletion oranı vb.
   - [ ] Çıktı skorunu [0,1] aralığına normalize eden yardımcı fonksiyon ekle.

3. **Async Inference Worker**
   - [ ] MLPlugin içinde bir worker thread ve iş kuyruğu tasarla.
   - [ ] File event’leri doğrudan model’e değil, kuyruğa basılsın.
   - [ ] Worker kuyruğu tüketip batched veya tek tek inference çalıştırsın.

4. **Karar Mantığı & Threshold’lar**
   - [ ] Konfigüre edilebilir threshold’lar (örn. low/medium/high risk) ekle.
   - [ ] Eşik aşıldığında EventBus’a `ANOMALY_DETECTED` event’i yayınla.
   - [ ] Mevcut EventHandlers içindeki anomaly handling ile entegrasyonu koru.

5. **Test & Gözlemlenebilirlik**
   - [ ] Unit test: model wrapper’ı sahte/dummy bir ONNX modeliyle test et.
   - [ ] Load test: yüksek frekanslı file event akışında worker kuyruğunun davranışını ölç.
   - [ ] Metrics: inference sayısı, ortalama latency, yüksek risk alarm sayısı gibi metrikler ekle.
