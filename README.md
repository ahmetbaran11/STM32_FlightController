# F450 STM32 Özel Uçuş Kontrol Yazılımı
# F450 STM32 Özel Uçuş Kontrol Yazılımı

Bu proje, F450 gövdeye sahip bir quadcopter için **STM32F411 (Black Pill)** kartı üzerinde C ve C++ kullanarak sıfırdan geliştirdiğim uçuş kontrol (flight controller) yazılımıdır. 

Hazır otopilot yazılımlarını (Betaflight, INAV vb.) yüklemek yerine; sensör okuma, açı hesaplama (tutum kestirimi), kumanda protokolü ayrıştırma ve motor güvenlik mantığını kendim kodlayarak gömülü sistemler ve uçuş dinamiklerini derinlemesine öğrenmeyi hedefledim.

---

## Bugüne kadar yaptıklarım:

* **Katmanlı Mimari:** Donanım sürücüleri (I2C/UART) ile matematiksel hesaplamaları (filtreler, güvenlik kontrolleri) birbirinden ayırdım.
* **SIL (Software-in-the-Loop) Test Edilebilirlik:** Sensör ve kumanda verilerini `ImuTypes.h` ve `IbusTypes.h` dosyalarında saf veri yapıları (struct) olarak tanımladım. Böylece açı hesaplama kodlarını STM32 kartı olmadan, doğrudan bilgisayarımda `g++` ile derleyip birim testten geçirebiliyorum.
* **Sensör Füzyonu & Kuaterniyon:** MPU6050'den gelen ivmeölçer ve jiroskop verilerini kuaterniyon tabanlı Tamamlayıcı Filtre (Complementary Filter) ile birleştirerek drone'un anlık eğim açılarını hesapladım.
* **i-BUS Kumanda Protokolü:** FlySky kumanda alıcısından gelen seri paketleri UART DMA ile kesintisiz okuyup 14 kanalı ayrıştırdım.
* **Motor Güvenlik Durum Makinesi:** Gaz kolu sıfırda değilse veya kumanda sinyali koptuğunda motorların aniden dönmesini engelleyen bir güvenlik sistemi (Arming FSM) kurdum.

---

## Ölçtüğüm Bazı Test Sonuçları

Geliştirme sürecinde algoritmaları sadece "çalışıyor" diye bırakmayıp tezgâhta test ettim:

* **Jiroskop Sürüklenmesi (Drift):** 89 saniyelik sabit beklemede ham jiroskop 7.15° kayarken, filtre çıkışında bu kaymayı 0.15° seviyesinde tuttum (yaklaşık 47 kat daha stabil).
* **Sinyal Gürültüsü:** Filtrelenmiş açı verisi, ham ivmeölçere göre yaklaşık 6 kat daha pürüzsüz çıktı veriyor.
* **Döngü Süresi:** 250 Hz (4 ms) olarak hedeflediğim ana kontrol döngüsü, 6.500 çevrim boyunca periyot kaçırmadan stabil çalıştı.
* **Kumanda Sinyali:** 3.470 paket boyunca i-BUS okumasında tek bir kilitlenme veya donma yaşanmadı.
* **Açı Doğrulama:** Bilgisayarda koşturduğum simülasyonda 22 tam turluk ($360^\circ$) takla testinde kümülatif açı hatası **0.13°** oldu.

---

## 📂 Proje Dosya Yapısı

```text
├── Core/
│   ├── Inc/
│   │   ├── ImuTypes.h             # Donanımdan bağımsız IMU veri yapıları
│   │   ├── IbusTypes.h            # Donanımdan bağımsız RC kumanda yapıları
│   │   ├── AttitudeEstimator.h    # Açı hesaplama sınıfı başlığı
│   │   ├── ArmController.h        # Güvenlik durum makinesi başlığı
│   │   ├── MPU6050.h              # I2C sensör sürücüsü
│   │   └── IbusReceiver.h         # i-BUS alıcı sürücüsü
│   ├── Src/
│   │   ├── AttitudeEstimator.cpp  # Filtre ve kuaterniyon matematiği
│   │   ├── ArmController.cpp      # Motor çalıştırma güvenlik kilitleri
│   │   ├── MPU6050.cpp            # MPU6050 register okuma/yazma
│   │   └── IbusReceiver.cpp       # Halka arabellek ve sağlama toplamı
│   └── Test/
│       └── test_attitude.cpp      # Bilgisayarda koşan birim test kodu
├── docs/
│   └── architecture.md            # Yazılım blok şeması ve matematiksel açıklamalar
├── FAULTS.md                      # Karşılaştığım hatalar ve nasıl çözdüğüm
├── BUILD.md                       # Derleme ve yükleme adımları
└── README.md
