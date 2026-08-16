# Hata Günlüğü ve Çözüm Notları

Bu dosyada, projeyi geliştirirken karşılaştığım gerçek hataları, kök nedenlerini, çözüm yollarımı ve her birini nasıl doğruladığımı not aldım.

---

## 1. I2C Hattının Kilitlenip Kalması (SDA Low)

- **Sorun:** MPU6050 modülünü breadboard üzerinde oynatırken I2C hattı kilitleniyor, ondan sonraki tüm register okumaları sessizce `0x00` dönüyordu.
- **Neden Oldu?** I2C'de master bir bayt transferinin ortasında hattı kaybederse, slave (MPU6050) SDA hattını LOW'da tutup bir sonraki clock darbesini bekler durumda kalır. Çevre birimini (I2C periferalini) yeniden başlatmak bunu çözmez, çünkü sorun slave tarafındadır.
- **Nasıl Çözdüm?** Pinleri geçici olarak open-drain GPIO çıkışına çevirip SDA serbest kalana kadar en fazla 9 clock darbesi ürettim, ardından bir STOP koşulu göndererek I2C çevre birimini yeniden başlattım.
- **Nasıl Test Ettim?** Veri akarken SDA/SCL kablolarını tekrar tekrar çıkarıp taktım. Log 7 ayrı kesinti dönemi gösterdi (en uzunu ~18 saniye) ve hepsinde veri akışı otomatik olarak toparlandı.

---

## 2. Sensörün Beklenmedik Kimlik (WHO_AM_I) Dönmesi

- **Sorun:** Sensörü başlatırken datasheet'e göre `WHO_AM_I` register'ından `0x68` gelmesi gerekirken `0x72` geliyordu.
- **Neden Oldu?** Kullandığım GY-521/MPU6050 kartı orijinal InvenSense silikonu değil, klon bir varyant; üretici çip kimliğini `0x72` olarak programlamış.
- **Nasıl Çözdüm?** WHO_AM_I kontrolünü hem `0x68` hem `0x72`'yi kabul edecek şekilde genişlettim. Asıl güveni, kimlik baytından değil, ham ivme/gyro verisinin fiziksel olarak anlamlı olup olmadığından aldım — ivmeölçer vektör büyüklüğünün ~1g'ye yakın çıkması, gyronun harekete doğru tepki vermesi gibi.
- **Nasıl Test Ettim?** ESP32 üzerinde basit bir I2C tarama scripti ile adresi ve kimlik değerini doğruladım, sonra STM32 sürücüsünde gerçek ivme/gyro okumalarının fiziksel olarak tutarlı olduğunu gözlemledim.

---

## 3. Jiroskop/İvmeölçerin Doyuma Girmesi (Saturation)

- **Sorun:** Sensörü elle hızlıca çevirince açı hesabı bozuluyor, gyro değerleri sabit bir tavanda kalıyordu.
- **Neden Oldu?** Sensör ±250 dps / ±2g aralığında yapılandırılmıştı. Bu aralıkta ham 16-bit değer, normal bir el hareketiyle bile fiziksel üst sınıra (32767) kolayca dayanıyordu — ölçtüğümde örneklerin yaklaşık yarısı doyuma giriyordu.
- **Nasıl Çözdüm?** Aralığı ±2000 dps / ±8g'ye çıkardım. Ölçek katsayısını sabit bir sayı olarak yazmak yerine, yapılandırılan aralıktan (bir enum üzerinden) türeten bir fonksiyona bağladım — böylece aralık değişince ölçek de otomatik güncelleniyor, ikisinin birbirinden kopup yanlış sonuç üretmesi imkânsız hale geldi. Ayrıca her okumada doyum durumunu ayrı bir bayrakla işaretledim.
- **Nasıl Test Ettim?** Aralığı geçici olarak ±250 dps'e düşürüp elimle çevirdim: ~537 örneğin 277'sinde doyum bayrağı doğru şekilde yandı. ±2000 dps'e dönünce aynı hareket artık doyum üretmiyordu.

---

## 4. Kalibrasyon Sırasında Hareket Edilmesi

- **Sorun:** Kartı açılışta sabit tutmazsam (elimde tutarken ya da masaya sert koyarken), başlangıç açısı 20-30 derece hatalı çıkıyordu; bir denemede pitch değeri kaçarak 400 dereceyi geçmişti.
- **Neden Oldu?** Kalibrasyon fonksiyonu 2000 örneğin ortalamasını jiroskop sıfır noktası (bias) olarak kaydediyordu. Kart hareket halindeyken alınan örnekler, gerçek dönüşü bias sanıp öyle kaydediyordu.
- **Nasıl Çözdüm?** Kalibrasyon sırasında ortalamanın yanında örneklerin standart sapmasını da hesapladım (taşmayı ve hassasiyet kaybını önlemek için tam sayı aritmetiğiyle). Sapma bir eşiği aşarsa kalibrasyonu "hareket algılandı" diye reddedip en fazla 5 kez otomatik olarak yeniden deniyorum.
- **Nasıl Test Ettim?** Kalibrasyon sırasında bilerek kartı salladım — ölçülen standart sapma 19-38 dps aralığına çıktı ve kalibrasyon reddedildi. Kartı sabit tuttuğumda sapma 0.7-2 dps'e indi ve kalibrasyon geçti.

---

## 5. Euler Açılarının Fiziksel Sınırın Dışına Çıkması

- **Sorun:** Kartı büyük açılara çevirdiğimde pitch değeri ±90 derece sınırının dışına çıkıyordu (görülen en uç değer: -182.6°), açı hesabı bir daha toparlanamıyordu.
- **Neden Oldu?** Roll/pitch'i doğrudan gyro entegrasyonu + `atan2` tabanlı düzeltmeyle hesaplayan basit tamamlayıcı filtre, küçük açı varsayımıyla çalışıyordu. Roll'un ±180°'de sarmalanması (wraparound) doğru ele alınmayınca filtre kararsızlaşıyordu.
- **Nasıl Çözdüm?** Açı hesabını dört elemanlı kuaterniyona taşıdım (Mahony filtresi): gyro ile kuaterniyonu ilerletip, ivmeölçerden gelen düzeltmeyi çapraz çarpımla hesaplayarak oransal+integral geri besleme uyguluyorum. Euler açıları artık sadece çıktı formatı — en son adımda kuaterniyondan türetiliyor, ve `asinf` sayesinde pitch matematiksel olarak ±90'ı hiç aşamıyor.
- **Nasıl Test Ettim?** Bilgisayarda (donanımdan bağımsız, `g++` ile derlenen) test kodunda gyroya sabit 200 dps dönüş verip 10.000 adım (40 saniye, ~22 tam tur) simüle ettim. Pitch tek bir kez bile ±90'ı aşmadı; kümülatif açı hatası sadece 0.13° çıktı.

---

## 6. UART/DMA Hattının Kablo Kopmasından Sonra Toparlanamaması

- **Sorun:** i-BUS sinyal kablosunu çıkarıp taktığımda alıcı bağlantısı (link) bir daha geri gelmiyordu.
- **Neden Oldu?** Kablo koptuğunda UART bir framing error üretiyor, ve HAL bu durumda circular DMA alımını sessizce durduruyor. Kablo geri gelse de DMA çalışmadığı için hiçbir bayt işlenmiyordu.
- **Nasıl Çözdüm?** `poll()` içinde hem UART hata kodunu hem de DMA yazma pozisyonunun uzun süredir değişip değişmediğini (takılma durumu) kontrol eden bir mekanizma yazdım. Hata ya da takılma tespit edilince DMA'yı durdurup hata bayraklarını temizleyip yeniden başlatıyorum (çok sık tekrarı önlemek için hız sınırlı).
- **Nasıl Test Ettim?** Kabloyu tekrar tekrar çıkarıp taktım. Log, iki tam `link:1 → link:0 → link:1` döngüsü ve her seferinde çerçeve sayacının kaldığı yerden devam ettiğini gösterdi.

---

## 7. Alıcının, Verici Kapansa Bile Veri Göndermeye Devam Etmesi

- **Sorun:** Sadece çerçeve zaman aşımına (timeout) dayanan bir link kontrolü, kumandayı kapattığımda hiç tetiklenmiyordu.
- **Neden Oldu?** FS-iA10B alıcısı, verici sinyali kaybolsa bile i-BUS hattına geçerli formatta çerçeve göndermeye devam ediyor. Yani "çerçeve geliyor" ile "kumandadan gerçek bir komut geliyor" aynı şey değilmiş.
- **Nasıl Çözdüm?** Zaman aşımı kontrolünü koruyup üstüne, kumandada arm switch kanalının failsafe değerini disarm konumuna ayarladım. Link koptuğunda alıcı otomatik olarak disarm değeri göndermeye başlıyor ve motor güvenlik mantığı buna göre devreye giriyor.
- **Nasıl Test Ettim?** Kumandayı açık tutup çalışırken kapattım; ilgili kanalın beklenen disarm değerine (1000) düştüğünü gördüm.
