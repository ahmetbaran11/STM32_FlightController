```markdown
Hata Günlüğü ve Çözüm Notları

Bu dosyada, projeyi geliştirirken karşılaştığım gerçek hataları, nedenlerini ve nasıl çözdüğümü adım adım not aldım.

---

1. I2C Hattının Kilitlenip Kalması (SDA Low)
* Sorun: Sensörden sürekli veri okurken veya kabloyu anlık oynattığımda I2C kilitleniyor ve kod `while` döngüsünde takılı kalıyordu.
* Neden Oldu?: MPU6050 tam veri gönderirken hat parazit kaparsa SDA hattını LOW (0V) seviyesinde tutuyor ve MCU'dan gelecek saat sinyalini bekliyordu. MCU resetlense bile sensör hattı bırakmıyordu.
* Nasıl Çözdüm?: I2C birimini başlatmadan önce SCL ve SDA pinlerini normal GPIO çıkışı yapıp SCL hattına elle 9 kez clock darbesi vuran, ardından STOP sinyali gönderen küçük bir hat kurtarma (bus recovery) fonksiyonu yazdım.
* Nasıl Test Ettim?: Veri akarken SDA kablosunu çıkarıp taktığımda sistemin kilitlenmeden 1.2 ms içinde okumaya devam ettiğini gördüm.

---

2. Sensörün Beklenmedik Kimlik (WHO_AM_I) Dönmesi
* Sorun: Sensörü başlatırken datasheet'e göre `WHO_AM_I` register'ından `0x68` değeri gelmesi gerekirken `0x72` geliyordu ve sürücü hata verip duruyordu.
* Neden Oldu?: Kullandığım MPU6050 kartı klon/farklı bir silikon varyantı olduğu için üretici çip kimliğini `0x72` olarak programlamıştı.
* Nasıl Çözdüm?: Sürücüye hem `0x68` hem de `0x72` değerini geçerli kabul eden bir kontrol ekledim ve seri porttan hangi çipin algılandığını yazdırdım.
* Nasıl Test Ettim?: Mantıksal analizör ile register adresini ve dönen `0x72` cevabını doğruladım.

---

3. Jiroskopun Hızlı Dönüşlerde Taşması (Doyuma Girmesi)
* Sorun: Drone'u elimle hızlıca çevirdiğimde açı hesabı bir anda saçmalıyor ve toparlayamıyordu.
* Neden Oldu?: Jiroskopu başta 250 modunda başlatmıştım. Hızlı hareketlerde açısal hız bu sınırı aştığı için 16-bitlik ham değer maksimum sınıra (32767) takılıp kalıyordu (doyum).
* Nasıl Çözdüm?: Jiroskop skala aralığını 2000 seviyesine çıkardım ve hassasiyet çarpanlarını buna göre güncelledim.
* Nasıl Test Ettim?: Hızlı dönüşlerde ham değerlerin doymadığını ve açının doğru şekilde takip edildiğini gözlemledim.

---

4. Kart Açılırken Hareket Ettirilirse Açının Bozulması
* Sorun: Cihaz açılırken elimde tutarsam veya masaya sert koyarsam, başlangıç açısı 20 - 30 derece hatalı başlıyordu.
* Neden Oldu?: İlk 500 örneğin ortalamasını alıp jiroskop sıfır noktası (ofset) hesaplarken oluşan titreşimler, dinamik hareketi "sıfır noktası" olarak kaydediyordu.
* Nasıl Çözdüm?: Kalibrasyon sırasında varyans denetimi ekledim. Eğer okunan örneklerin varyansı belirli bir eşiğin üzerindeyse (yani kart hareket ediyorsa) kalibrasyon kendini otomatik olarak sıfırlayıp baştan başlıyor.
* Nasıl Test Ettim?: Kartı açılışta salladığımda kalibrasyon tamamlanmıyor; masaya tamamen sabit bırakana kadar bekleyip sonra sıfırlıyor.

---

5. Euler Açılarında Takla Atarken Açının Fırlaması (Gimbal Lock)
* Sorun: Pitch açısı dik konuma (90 derece) yaklaştığında roll ve yaw açıları aniden -180 derece değerlerine fırlıyordu.
* Neden Oldu?: Doğrudan Euler açıları ($atan2 üzerinden integral aldığımda açısal tekillik (Gimbal Lock) ve kadran geçiş hatası oluşuyordu.
* Nasıl Çözdüm?: Açı hesabını 4 elemanlı Kuaterniyon matematiğine taşıdım. Açıyı kuaterniyon olarak güncelleyip en son aşamada anlaşılır olması için Euler açılarına (Roll/Pitch/Yaw) çevirdim.
* Nasıl Test Ettim?: Bilgisayarda yazdığım test kodunda (`test_attitude.cpp`) ardışık 22 tam 360 derece dönüş simüle ettim; hiçbir fırlama olmadan açıların düzgün çalıştığını gördüm.
