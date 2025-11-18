# SentinelFS-Neo — Copilot Davranış Kuralları

Copilot, bu repo için kod üretirken aşağıdaki kurallara uymalıdır:

1. Tüm modüller plugin tabanlıdır.
2. Plugin interface aşağıdaki gibidir:
   - plugin_info
   - plugin_create
   - plugin_destroy
3. Core katmanına yeni logic eklenmez.
4. İş mantığı plugin katmanlarına yazılır.
5. Tüm sınıflar interface üzerinden soyutlanmalıdır.
6. OS bağımlı kod → filesystem plugin’leri içinde olmalıdır.
7. Network logic → network plugin içinde olmalıdır.
8. Database logic → storage plugin’inde olmalıdır.
9. ML kodu → ml plugin’inde olmalıdır.
10. Kod önerileri C++17 + CMake üzerine olmalıdır.
