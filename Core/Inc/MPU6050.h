#pragma once
#include "stm32f4xx_hal.h"
#include <cstdint>
#include "ImuTypes.h"

enum class GyroRange : uint8_t {
    Dps250 = 0x00, Dps500 = 0x08, Dps1000 = 0x10, Dps2000 = 0x18,
};

enum class AccelRange : uint8_t {
    G2 = 0x00, G4 = 0x08, G8 = 0x10, G16 = 0x18,
};

// I2C hattinin fiziksel pinleri.
// Bus kurtarma sirasinda pinleri gecici olarak GPIO'ya cevirip elle
// darbe uretmemiz gerekiyor; I2C_HandleTypeDef bu bilgiyi tasimiyor.
// SCL ve SDA farkli portlarda olabilir, o yuzden ikisi de ayri tutuluyor.
struct I2cPins {
    GPIO_TypeDef* sclPort;
    uint16_t      sclPin;
    GPIO_TypeDef* sdaPort;
    uint16_t      sdaPin;
};

class MPU6050 {
public:
	MPU6050(I2C_HandleTypeDef* i2c, I2cPins pins, GyroRange gyroRange = GyroRange::Dps2000, AccelRange accelRange = AccelRange::G8,
			uint8_t address = 0x68);
	ImuStatus recoverBus();   // disaridan cagrilabilmeli

	ImuStatus init(); //mpu hazırla
	ImuStatus calibrateGyro(uint16_t samples = 2000); // bu kullanım sayesinde imu.calibrateGyro() diye
	//çağırılırsa varsayılan 2000 sample kullanır, (500) gibi kullanılırsa 500 sample kullanır.
	ImuStatus read(ImuSample& out); //veriyi oku

	//nesnenin kopyalanmasını yasakla çünkü kopyalanırsa elimzie geçen veri doğru olmaz
	MPU6050(const MPU6050&) = delete;
	MPU6050& operator=(const MPU6050&) = delete;

	const float* gyroStdDps() const { return gyroStdDps_; }
	const float* accelStdG()  const { return accelStdG_; }

private:
	I2C_HandleTypeDef* i2c_;
	I2cPins pins_;
	uint8_t address_;
	GyroRange gyroRange_;
	AccelRange accelRange_;
	float gyroBias_[3] = {0.0f, 0.0f, 0.0f};
	ImuStatus writeReg(uint8_t reg, uint8_t value);
    ImuStatus readRegs(uint8_t reg, uint8_t* buffer, uint16_t length);
    ImuStatus readRaw(int16_t& ax, int16_t& ay, int16_t& az,
                      int16_t& temp,
                      int16_t& gx, int16_t& gy, int16_t& gz, bool& gyroSat, bool& accelSat);
    float gyroStdDps_[3] = {0.0f, 0.0f, 0.0f};
    float accelStdG_[3]  = {0.0f, 0.0f, 0.0f};
};
