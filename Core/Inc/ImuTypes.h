#pragma once
#include <cstdint>

//ImuStatus eklemedim çünkü zaten read() fonksiyonuyla döndüreceğiz onı
struct ImuSample {
	float ax, ay, az;
	float gx, gy, gz;
	int16_t temperature;
	bool gyroSaturated;
	bool accelSaturated;
	int16_t rawGx;
};

enum class ImuStatus : uint8_t {
	Ok           = 0x00,
	NoResponse   = 0x01,   // cip ACK vermiyor
	BusFault     = 0x02,   // I2C hatti kilitlenmis
	InvalidData  = 0x03,   // veri geldi ama güvenilmez
	MotionDetected = 0x04
};

//şimdilik yaw eklemedim
struct Attitude {
    float roll;
    float pitch;
};
