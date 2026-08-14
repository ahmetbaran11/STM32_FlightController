# Projeyi Derleme ve Çalıştırma Kılavuzu

Bu belgede kodları STM32 kartına nasıl yükleyeceğinizi ve açı hesaplama testlerini bilgisayarda nasıl çalıştıracağınızı anlattım.

---

## 1. Gereksinimler

* **STM32CubeIDE** (v1.14 veya üzeri)
* **ST-LINK V2** Programlayıcı
* **STM32F411CEU6 Black Pill** Geliştirme Kartı
* Bilgisayar testleri için: Herhangi bir **C++ derleyicisi (g++ / clang)**

---

## 2. STM32CubeIDE ile Projeyi Açma ve Derleme

1. STM32CubeIDE'yi açın.
2. **File $\rightarrow$ Import $\rightarrow$ Existing Projects into Workspace** seçeneğine tıklayın.
3. Projenin bulunduğu `F450_FlightController` klasörünü seçip **Finish** deyin.
4. **Project $\rightarrow$ Build Project** (veya `Ctrl + B`) ile projeyi derleyin.
5. Hata almadan derlendiğini ve `Debug/` klasöründe `.elf` dosyasının oluştuğunu görün.

---

## 3. Karta Yükleme (ST-Link Bağlantısı)

Black Pill ile ST-Link arasındaki bağlantı:
* `SWDIO` $\rightarrow$ `PA13`
* `SWCLK` $\rightarrow$ `PA14`
* `GND` $\rightarrow$ `GND`
* `3.3V` $\rightarrow$ `3.3V`

IDE üzerinden **Run $\rightarrow$ Debug** (`F11`) butonuna basarak kodu karta yükleyebilirsiniz.

---

## 4. Bilgisayarda Birim Testleri Çalıştırma (Kart Olmadan)

Açı hesaplama ve filtre mantığını test etmek için STM32 kartına ihtiyacınız yok:

```bash
# Proje ana dizinindeyken:
g++ -std=c++14 Core/Test/test_attitude.cpp Core/Src/AttitudeEstimator.cpp -I Core/Inc -o test_calistir

# Testi yürüt:
./test_calistir
