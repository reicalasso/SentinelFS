# 07 – Daemon State & Plugin Yönetimi

## Amaç
- Daemon’un çalışma durumunu ve sync state’ini dışarıdan tutarlı şekilde raporlamak.
- Plugin yaşam döngüsünü daha yönetilebilir hale getirmek (enable/disable, hata toleransı).

## Gereksinimler
- IPC `STATUS` çıktısının gerçek `syncEnabled_` ve plugin durumunu yansıtması.
- Plugin manifest’inin gelecekte runtime yönetimine genişleyebilecek şekilde yapılandırılması.

## Plan
1. **Sync State Takibi**
   - [ ] DaemonCore içindeki `syncEnabled_` ile EventHandlers’in sync state’i arasında tek kaynaklı bir model kur.
   - [ ] IPCHandler’a bu state’i okuyacak bir arayüz ekle.
   - [ ] `STATUS` komutundaki sabit "Sync Status: ENABLED" satırını gerçek duruma bağla.

2. **Plugin Durum Raporlama**
   - [ ] PluginManager’da yüklenmiş her plugin için basit bir durum kaydı tut (loaded, failed, optional vs.).
   - [ ] IPC `STATUS` veya ayrı bir `PLUGINS` komutuyla bu bilgiyi kullanıcıya göster.

3. **Hata Toleransı & Restart Senaryoları**
   - [ ] Kritik plugin (storage, network, filesystem) yükleme hatalarında kullanıcıya öneri veren net mesajlar tanımla.
   - [ ] ML plugin gibi opsiyonel plugin’ler için degrades gracefully davranışını koru.

4. **Geleceğe Dönük Runtime Yönetimi (Planlama)**
   - [ ] Orta vadede plugin enable/disable/hot-reload için ihtiyaç duyulacak minimal API yüzeyini taslakla.
   - [ ] Konfig dosyası / manifest üzerinden plugin öncelik ve bağımlılıklarını daha net ifade et.

5. **Test**
   - [ ] STATUS/PLUGINS komutunun çeşitli hata senaryolarındaki çıktısını test et.
   - [ ] Plugin yükleme sırasındaki log ve IPC çıktılarının tutarlılığını kontrol et.
