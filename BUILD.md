# Projeyi Derleme ve Çalıştırma Kılavuzu

Bu belgede kodu STM32 kartına nasıl yükleyeceğinizi ve açı hesaplama testlerini bilgisayarda (kart olmadan) nasıl çalıştıracağınızı anlattım.

---

## 1. Gereksinimler

- **STM32CubeIDE** (CubeMX'i de içeriyor, ayrıca kurmaya gerek yok)
- **STM32CubeProgrammer** (karta yükleme için — USB DFU üzerinden çalışıyoruz, ST-Link kullanmıyoruz)
- USB veri kablosu (sadece şarj kablosu değil)
- Bilgisayarda birim testleri çalıştırmak için: **MSYS2** (UCRT64 ortamı) + `g++`, ya da elinizdeki herhangi bir C++ derleyicisi

---

## 2. STM32CubeIDE ile Projeyi Açma ve Derleme

1. STM32CubeIDE'yi açın.
2. **File → Open Projects from File System...** ile proje klasörünü (`F450_FlightController`) seçin.
3. Project Explorer'da **`Core/Test`** klasörünün build dışında tutulduğunu doğrulayın: klasöre sağ tık → Properties → C/C++ Build → **"Exclude resource from build"** işaretli olmalı. Bu klasörde ayrı bir `main()` içeren PC test dosyası var; build'e dahil edilirse `main.c` ile çakışıp "multiple definition of main" hatası verir.
4. **Project → Build Project** (`Ctrl+B`) ile derleyin.
5. `Debug/` klasöründe `.bin` ve `.elf` dosyalarının oluştuğunu doğrulayın.

---

## 3. Karta Yükleme (USB DFU ile)

Bu proje **ST-Link/SWD değil, kartın kendi USB'sinden DFU (Device Firmware Upgrade)** ile flaşlanıyor — ekstra bir programlayıcıya gerek yok.

1. Kartı bilgisayara USB ile bağlayın.
2. **BOOT0** pinini **3.3V**'a çekin, **RESET**'e basıp bırakın. Kart artık DFU (bootloader) modunda.
3. **STM32CubeProgrammer**'ı açın, sol üstten **USB** seçip **Connect**'e basın. Bağlantı kurulunca sağ panelde çip bilgileri (STM32F411xC/E, Device ID 0x431) görünmeli.
4. **Erase & Program** sekmesine geçin, **Browse** ile `Debug/` klasöründeki `.bin` dosyasını seçin.
5. Start Address'in `0x08000000` olduğunu doğrulayıp **Start Programming**'e basın.
6. İşlem bitince kartı DFU modundan çıkarın: **BOOT0**'ı **GND**'ye alın, kartı resetleyin ya da USB'yi çıkarıp tekrar takın. Kart artık yüklediğiniz firmware ile normal modda çalışacak.

**Not:** Kart DFU modunda kaldığı sürece kendi firmware'iniz çalışmaz — flaşlama sonrası BOOT0/reset adımını atlamayın.

---

## 4. Bilgisayarda Birim Testleri Çalıştırma (Kart Olmadan)

Açı hesaplama (`AttitudeEstimator`) donanımdan (HAL) tamamen bağımsız yazıldığı için, mantığını test etmek için karta hiç ihtiyacınız yok.

1. **MSYS2 UCRT64** terminalini açın (normal kullanıcı olarak, yönetici değil).
2. Derleyici kurulu değilse: `pacman -S mingw-w64-ucrt-x86_64-gcc`
3. Proje klasöründeki `Core` dizinine geçin:
   ```
   cd "/c/Users/<pc kullanıcı adınız>/.../F450_FlightController/Core"
   ```
4. Derleyip çalıştırın:
   ```
   g++ -I Inc Src/AttitudeEstimator.cpp Test/test_attitude.cpp -o ~/test.exe && ~/test.exe
   ```

**Not:** Çıktı dosyasını (`-o`) proje klasörünün **içine değil**, ev dizininize (`~`) yazdırıyoruz. Proje klasörü OneDrive gibi bir bulut senkronizasyon aracının altındaysa, oraya doğrudan `.exe` yazmak "cannot open output file" hatası verebiliyor — ev dizini bu sorunu ortadan kaldırıyor.

Testler geçtiğinde şuna benzer bir çıktı görürsünüz:
```
[PASS] ihlal sayisi                    beklenen     0.00, gelen     0.00
[PASS] duz duruyor - roll              beklenen     0.00, gelen     0.00
[PASS] 22 tur sonrasi pitch            beklenen    80.00, gelen    79.87

0 test basarisiz
```
