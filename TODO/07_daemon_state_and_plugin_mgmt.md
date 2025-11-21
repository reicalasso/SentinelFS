# 07 – Daemon State & Plugin Yönetimi

## Amaç
- Daemon’un çalışma durumunu ve sync state’ini dışarıdan tutarlı şekilde raporlamak.
- Plugin yaşam döngüsünü daha yönetilebilir hale getirmek (enable/disable, hata toleransı).

## Gereksinimler
- IPC `STATUS` çıktısının gerçek `syncEnabled_` ve plugin durumunu yansıtması.
- Plugin manifest’inin gelecekte runtime yönetimine genişleyebilecek şekilde yapılandırılması.

## Plan
1. **Sync State Takibi**
   - [x] DaemonCore içindeki `syncEnabled_` ile EventHandlers’in sync state’i arasında tek kaynaklı bir model kur.
   - [x] IPCHandler’a bu state’i okuyacak bir arayüz ekle.
   - [x] `STATUS` komutundaki sabit "Sync Status: ENABLED" satırını gerçek duruma bağla.

2. **Plugin Durum Raporlama**
   - [x] PluginManager’da yüklenmiş her plugin için basit bir durum kaydı tut (loaded, failed, optional vs.).
   - [x] IPC `STATUS` veya ayrı bir `PLUGINS` komutuyla bu bilgiyi kullanıcıya göster.

3. **Hata Toleransı & Restart Senaryoları**
   - [x] Kritik plugin (storage, network, filesystem) yükleme hatalarında kullanıcıya öneri veren net mesajlar tanımla.
   - [x] ML plugin gibi opsiyonel plugin’ler için degrades gracefully davranışını koru.

4. **Geleceğe Dönük Runtime Yönetimi (Planlama)**
   - [x] Orta vadede plugin enable/disable/hot-reload için ihtiyaç duyulacak minimal API yüzeyini taslakla.
   - [x] Konfig dosyası / manifest üzerinden plugin öncelik ve bağımlılıklarını daha net ifade et.

5. **Test**
   - [x] STATUS/PLUGINS komutunun çeşitli hata senaryolarındaki çıktısını test et.
   - [x] Plugin yükleme sırasındaki log ve IPC çıktılarının tutarlılığını kontrol et.
