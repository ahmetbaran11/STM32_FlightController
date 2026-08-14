#include "MPU6050.h"
#include <cmath>

namespace {
    constexpr uint8_t REG_WHO_AM_I    = 0x75;
    constexpr uint8_t REG_PWR_MGMT_1  = 0x6B;
    constexpr uint8_t REG_CONFIG      = 0x1A;
    constexpr uint8_t REG_SMPLRT_DIV   = 0x19;  // ornekleme hizi bolucu
    constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;  // gyro olcek araligi
    constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;  // ivmeolcer olcek araligi
    constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;  // 14 baytlik veri blogunun basi
    inline void busDelay() {
            for (volatile uint32_t i = 0; i < 200; i++) { }
    }

    float gyroScale(GyroRange r) {
        switch (r) {
            case GyroRange::Dps250:  return 131.0f;
            case GyroRange::Dps500:  return 65.5f;
            case GyroRange::Dps1000: return 32.8f;
            case GyroRange::Dps2000: return 16.4f;
        }
        return 131.0f;
    }

    float accelScale(AccelRange ar) {
    	switch (ar) {
    		case AccelRange::G2:	return 16384.0f;
    		case AccelRange::G4:	return 8192.0f;
    		case AccelRange::G8:	return 4096.0f;
    		case AccelRange::G16:	return 2048.0f;
    	}
    	return 16384.f;
    }
}

MPU6050::MPU6050(I2C_HandleTypeDef* i2c, I2cPins pins,
                 GyroRange gyroRange, AccelRange accelRange, uint8_t address) : i2c_(i2c), pins_(pins),
                		 address_(address),gyroRange_(gyroRange), accelRange_(accelRange) {}

//*********************************************************


// I2C bus kilitlenmesinden kurtarma.
//
// Problem: Master, slave tam bir bayt gonderirken islemi yarida keserse
// (kablo temassizligi, reset, gurultu), slave SDA hattini asagida tutmaya
// devam eder ve bir sonraki clock darbesini bekler. Bu durumda hat olu
// kalir; her okuma 0x00 doner. Cevre birimini yeniden baslatmak yetmez,
// cunku sorun slave'in icinde.
//
// Cozum: Pinleri gecici olarak GPIO'ya alip, slave kalan bitleri bosaltana
// kadar elle clock darbesi uretmek, sonra bir STOP kosulu ile islemi
// resmen sonlandirmak.
ImuStatus MPU6050::recoverBus() {

    // --- 1) I2C cevre birimini birak ---
    // Pinler su an alternate-function modunda ve I2C donanimi tarafindan
    // suruluyor. Elle mudahale edebilmek icin once cevre birimini kapatiyoruz.
    HAL_I2C_DeInit(i2c_);

    // --- 2) SCL ve SDA'yi open-drain GPIO cikisi yap ---
    // Open-drain sart: I2C'de hicbir cihaz hatti aktif olarak yukari cekmez,
    // sadece asagi ceker; yukari cekme isini pull-up dirençleri yapar.
    // Push-pull kullanirsak, slave SDA'yi asagida tutarken biz yukari
    // zorlariz ve fiilen kisa devre olusur.
    GPIO_InitTypeDef gpio = {};
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = pins_.sclPin;
    HAL_GPIO_Init(pins_.sclPort, &gpio);

    gpio.Pin = pins_.sdaPin;
    HAL_GPIO_Init(pins_.sdaPort, &gpio);

    // Open-drain'de HIGH yazmak "hatti birak" demektir; hat pull-up
    // sayesinde yukari cikar (slave asagida tutmuyorsa).
    HAL_GPIO_WritePin(pins_.sclPort, pins_.sclPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(pins_.sdaPort, pins_.sdaPin, GPIO_PIN_SET);
    busDelay();

    // --- 3) SDA serbest kalana kadar en fazla 9 clock darbesi ---
    // 9 sayisi keyfi degil: takili kalmis bir slave en fazla 8 veri biti +
    // 1 ACK biti bekliyor olabilir. Dokuz darbeden sonra hala birakmiyorsa
    // sorun kilitlenme degil, baska bir sey demektir.
    for (int i = 0; i < 9; i++) {
        if (HAL_GPIO_ReadPin(pins_.sdaPort, pins_.sdaPin) == GPIO_PIN_SET) {
            break;   // SDA serbest, devam etmeye gerek yok
        }
        HAL_GPIO_WritePin(pins_.sclPort, pins_.sclPin, GPIO_PIN_RESET);
        busDelay();
        HAL_GPIO_WritePin(pins_.sclPort, pins_.sclPin, GPIO_PIN_SET);
        busDelay();
    }

    // Kurtarmanin gercekten ise yarayip yaramadigini burada tespit ediyoruz.
    const bool sdaFreed =
        (HAL_GPIO_ReadPin(pins_.sdaPort, pins_.sdaPin) == GPIO_PIN_SET);

    // --- 4) STOP kosulu uret ---
    // I2C'de STOP = SCL yukaridayken SDA'nin asagidan yukariya gecmesi.
    // Bu, hattaki tum cihazlara "islem bitti, bastan basliyoruz" der.
    HAL_GPIO_WritePin(pins_.sdaPort, pins_.sdaPin, GPIO_PIN_RESET);
    busDelay();
    HAL_GPIO_WritePin(pins_.sclPort, pins_.sclPin, GPIO_PIN_SET);
    busDelay();
    HAL_GPIO_WritePin(pins_.sdaPort, pins_.sdaPin, GPIO_PIN_SET);
    busDelay();

    // --- 5) I2C cevre birimini geri kur ---
    // HAL_I2C_Init icinde HAL_I2C_MspInit cagriliyor; o da CubeMX'in
    // urettigi pin yapilandirmasini (alternate function modu) geri yukluyor.
    // Yani pinleri elle I2C moduna dondurmemize gerek yok.
    if (HAL_I2C_Init(i2c_) != HAL_OK) {
        return ImuStatus::BusFault;
    }

    return sdaFreed ? ImuStatus::Ok : ImuStatus::BusFault;
}





//*********************************************************

ImuStatus MPU6050::writeReg(uint8_t reg, uint8_t value) {
    HAL_StatusTypeDef hal = HAL_I2C_Mem_Write(
    		i2c_,
			address_ << 1,
			reg,
			I2C_MEMADD_SIZE_8BIT,
			&value,
			1,
			100
    );

    if (hal == HAL_OK) return ImuStatus::Ok;
    if (hal == HAL_TIMEOUT) return ImuStatus::BusFault;
    return ImuStatus::NoResponse;
}

ImuStatus MPU6050::readRegs(uint8_t reg, uint8_t* buffer, uint16_t length) {
	HAL_StatusTypeDef hal = HAL_I2C_Mem_Read(i2c_,
			address_ << 1,
			reg,
			I2C_MEMADD_SIZE_8BIT,
			buffer,
			length,
			100
	);

	if (hal == HAL_OK) return ImuStatus::Ok;
	if (hal == HAL_TIMEOUT) return ImuStatus::BusFault;
	return ImuStatus::NoResponse;
}


ImuStatus MPU6050::init() {
    // cihaz hatta cevap veriyor mu?
    if (HAL_I2C_IsDeviceReady(i2c_, address_ << 1, 3, 100) != HAL_OK) {
        return ImuStatus::NoResponse;
    }

    // 2) WHO_AM_I oku
    uint8_t id = 0;
    ImuStatus st = readRegs(REG_WHO_AM_I, &id, 1);
    if (st != ImuStatus::Ok) return st;
    //burada farklı bi parça kullanıldı mı diye kontrol etmek için bildiğimiz adresleri kontrol ediyoruz
    if (id != 0x68 && id != 0x72) return ImuStatus::NoResponse;

    // 3) Uyandir
    st = writeReg(REG_PWR_MGMT_1, 0x00);
    if (st != ImuStatus::Ok) return st;

    st = writeReg(REG_CONFIG, 0x03);
    if (st != ImuStatus::Ok) return st;

    st = writeReg(REG_SMPLRT_DIV, 0x00);
    if (st != ImuStatus::Ok) return st;

    st = writeReg(REG_GYRO_CONFIG, static_cast<uint8_t>(gyroRange_));
    if (st != ImuStatus::Ok) return st;

    st = writeReg(REG_ACCEL_CONFIG, static_cast<uint8_t>(accelRange_));
    if (st != ImuStatus::Ok) return st;


    return ImuStatus::Ok;
}

ImuStatus MPU6050::readRaw (int16_t& ax, int16_t& ay, int16_t& az,
        int16_t& temp,
        int16_t& gx, int16_t& gy, int16_t& gz, bool& gyroSat, bool& accelSat) {

	uint8_t buffer[14];

	ImuStatus st = readRegs(REG_ACCEL_XOUT_H, buffer, 14);
	if (st != ImuStatus::Ok) return st;

	ax = static_cast<int16_t>(buffer[0] << 8 | buffer[1]);
	ay = static_cast<int16_t>(buffer[2] << 8 | buffer[3]);
	az = static_cast<int16_t>(buffer[4] << 8 | buffer[5]);
	temp = static_cast<int16_t>(buffer[6] << 8 | buffer[7]);
	gx = static_cast<int16_t>(buffer[8] << 8 | buffer[9]);
	gy = static_cast<int16_t>(buffer[10] << 8 | buffer[11]);
	gz = static_cast<int16_t>(buffer[12] << 8 | buffer[13]);

	//bus kilitlendiyse veri okuma başarılı gözüküyordu ama tüm bytelar 0 kalıyrdu onu kontrol et
	if (ax == 0 && ay == 0 && az == 0 && gx == 0 && gy == 0 && gz == 0) {
	    return ImuStatus::InvalidData;
	}

	if (ax == -1 && ay == -1 && az == -1 && gx == -1 && gy == -1 && gz == -1) {
		return ImuStatus::InvalidData;
	}

	// Bir eksen sinira dayandiysa o grup doyumdadir
	auto atLimit = [](int16_t v) {
	    return v == INT16_MAX || v == INT16_MIN;
	};

	gyroSat  = atLimit(gx) || atLimit(gy) || atLimit(gz);
	accelSat = atLimit(ax) || atLimit(ay) || atLimit(az);

	return ImuStatus::Ok;
}


//YAPAY ZEKAYA YAPTIRDIM BU FONKSİYONU
//YAPAY ZEKAYA YAPTIRDIM BU FONKSİYONU
ImuStatus MPU6050::calibrateGyro(uint16_t samples) {
    int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;
    int32_t sum_ax = 0, sum_ay = 0, sum_az = 0;

    int64_t sq_gx = 0, sq_gy = 0, sq_gz = 0;
    int64_t sq_ax = 0, sq_ay = 0, sq_az = 0;

    int16_t ax, ay, az, temp, gx, gy, gz;
    bool gyroSat = false, accelSat = false;

    uint16_t collected = 0;
    ImuStatus lastError = ImuStatus::Ok;

    const uint32_t maxAttempts = static_cast<uint32_t>(samples) * 2;

    for (uint32_t attempt = 0; attempt < maxAttempts && collected < samples; attempt++) {
        ImuStatus st = readRaw(ax, ay, az, temp, gx, gy, gz, gyroSat, accelSat);

        if (st == ImuStatus::Ok) {
            sum_gx += gx; sum_gy += gy; sum_gz += gz;
            sum_ax += ax; sum_ay += ay; sum_az += az;

            sq_gx += static_cast<int64_t>(gx) * gx;
            sq_gy += static_cast<int64_t>(gy) * gy;
            sq_gz += static_cast<int64_t>(gz) * gz;
            sq_ax += static_cast<int64_t>(ax) * ax;
            sq_ay += static_cast<int64_t>(ay) * ay;
            sq_az += static_cast<int64_t>(az) * az;

            collected++;
        } else {
            lastError = st;
        }
        HAL_Delay(1);
    }

    if (collected < samples) return lastError;

        const int64_t n  = static_cast<int64_t>(collected);
        const int64_t n2 = n * n;

        // Standart sapmayi tam sayida hesapla: var = (N*sq - sum^2) / N^2
        // float ile (E[x^2] - E[x]^2) yapilsaydi ~16 milyon mertebesindeki
        // degerlerde mantis cozunurlugu yetmez, gercek varyans yuvarlama
        // hatasinin icinde kaybolurdu (katastrofik iptal).
        auto stdDev = [n, n2](int64_t sq, int32_t sum) -> float {
            const int64_t s   = static_cast<int64_t>(sum);
            const int64_t num = n * sq - s * s;
            if (num <= 0) return 0.0f;
            return std::sqrt(static_cast<double>(num) / static_cast<double>(n2));
        };

        float mean_gx = static_cast<float>(sum_gx) / collected;
        float mean_gy = static_cast<float>(sum_gy) / collected;
        float mean_gz = static_cast<float>(sum_gz) / collected;

        // Olcek ayardan turetiliyor - sabit yazilirsa aralik degistiginde
        // sessizce yanlis sonuc verir
        const float gScale = gyroScale(gyroRange_);
        const float aScale = accelScale(accelRange_);

        float std_gx_dps = stdDev(sq_gx, sum_gx) / gScale;
        float std_gy_dps = stdDev(sq_gy, sum_gy) / gScale;
        float std_gz_dps = stdDev(sq_gz, sum_gz) / gScale;

        float std_ax_g = stdDev(sq_ax, sum_ax) / aScale;
        float std_ay_g = stdDev(sq_ay, sum_ay) / aScale;
        float std_az_g = stdDev(sq_az, sum_az) / aScale;

        // app_main'in basabilmesi icin sakla
        gyroStdDps_[0] = std_gx_dps;
        gyroStdDps_[1] = std_gy_dps;
        gyroStdDps_[2] = std_gz_dps;
        accelStdG_[0]  = std_ax_g;
        accelStdG_[1]  = std_ay_g;
        accelStdG_[2]  = std_az_g;

        // Olculerek belirlenen esikler: sabit kartta 0.72-0.96 dps / 0.008 g,
        // hareketli kartta 19-38 dps / 0.14 g olculmustu
        constexpr float MAX_GYRO_STD_DPS = 8.0f;
        constexpr float MAX_ACCEL_STD_G  = 0.05f;

        if (std_gx_dps > MAX_GYRO_STD_DPS || std_gy_dps > MAX_GYRO_STD_DPS || std_gz_dps > MAX_GYRO_STD_DPS ||
            std_ax_g > MAX_ACCEL_STD_G    || std_ay_g > MAX_ACCEL_STD_G    || std_az_g > MAX_ACCEL_STD_G) {
            return ImuStatus::MotionDetected;
        }

        gyroBias_[0] = mean_gx;
        gyroBias_[1] = mean_gy;
        gyroBias_[2] = mean_gz;

        return ImuStatus::Ok;

}

ImuStatus MPU6050::read(ImuSample& out) {
	int16_t ax = 0, ay = 0, az = 0, temp = 0, gx = 0, gy = 0, gz = 0;
	bool gyroSat = false, accelSat = false;


	ImuStatus st = readRaw(ax,ay,az,temp,gx,gy,gz,gyroSat,accelSat);
	if (st != ImuStatus::Ok) return st;

	out.ax = ax / accelScale(accelRange_);
	out.ay = ay / accelScale(accelRange_);
	out.az = az / accelScale(accelRange_);
	out.gx = (gx - gyroBias_[0]) / gyroScale(gyroRange_);
	out.gy = (gy - gyroBias_[1]) / gyroScale(gyroRange_);
	out.gz = (gz - gyroBias_[2]) / gyroScale(gyroRange_);
	out.temperature = temp;
	out.gyroSaturated  = gyroSat;
	out.accelSaturated = accelSat;
	out.rawGx = gx;

	return ImuStatus::Ok;
}
