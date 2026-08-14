#include "app_main.h"
#include "main.h"
#include "MPU6050.h"
#include "usbd_cdc_if.h"
#include "AttitudeEstimator.h"
#include "IbusReceiver.h"
#include "IbusTypes.h"
#include <cstdio>

extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;
extern I2C_HandleTypeDef hi2c1;

static volatile bool controlTick = false;
static volatile uint32_t missedDeadlines = 0;

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (htim->Instance == TIM3) {
        if (controlTick) missedDeadlines++;
        controlTick = true;
    }
}

void app_main(void)
{
	// USB CDC portunun isletim sistemi tarafindan taninmasi icin bekleme
	HAL_Delay(3000);

	char tx_buffer[256];
	int len = 0;

	len = snprintf(tx_buffer, sizeof(tx_buffer), "\r\n--- FLIGHT CONTROLLER BOOTING ---\r\n");
	CDC_Transmit_FS((uint8_t*)tx_buffer, len);
	HAL_Delay(200);

	MPU6050 imu(&hi2c1, I2cPins{GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7});
	AttitudeEstimator est(0.004f, 1.0f, 0.1f);
	ImuSample sample{};
	IbusReceiver rc(&huart2); //varsayılan olarak ms 500 yapıyor onu kullanacaz zaten

	imu.recoverBus();

	if (imu.init() != ImuStatus::Ok) {
		len = snprintf(tx_buffer, sizeof(tx_buffer), "[CRITICAL] MPU6050 INIT FAILED! Check Wires.\r\n");
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		while (1) {
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			HAL_Delay(100);   // Hizli flas: donanim hatasi
		}
	}

	// --- JIROSKOP KALIBRASYONU VE HAREKET DENETIMI ---
	constexpr int MAX_CALIB_TRIES = 5;
	ImuStatus calibStatus = ImuStatus::MotionDetected;

	for (int tryNo = 1; tryNo <= MAX_CALIB_TRIES; tryNo++) {
		len = snprintf(tx_buffer, sizeof(tx_buffer),
		               "KALIBRASYON %d/%d - LUTFEN KARTI SABIT TUTUN...\r\n", tryNo, MAX_CALIB_TRIES);
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		HAL_Delay(200);

		calibStatus = imu.calibrateGyro(2000);

		// Olculen gurultu seviyesi. Hareket algilansa da basiliyor, cunku
		// esikleri ancak iki durumun sayilarini karsilastirarak ayarlayabiliriz.
		len = snprintf(tx_buffer, sizeof(tx_buffer),
		               "STD gyro: %.2f %.2f %.2f dps | accel: %.4f %.4f %.4f g\r\n",
		               imu.gyroStdDps()[0], imu.gyroStdDps()[1], imu.gyroStdDps()[2],
		               imu.accelStdG()[0],  imu.accelStdG()[1],  imu.accelStdG()[2]);
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		HAL_Delay(200);

		if (calibStatus != ImuStatus::MotionDetected) break;

		len = snprintf(tx_buffer, sizeof(tx_buffer), "[UYARI] Hareket algilandi, yeniden deneniyor...\r\n");
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		HAL_Delay(1000);
	}

	// Kalibrasyon sonucunu isletmek sart - yoksa koruma yazilmis ama
	// calistirilmamis olur ve bozuk bias'la ucusa devam ederiz.
	if (calibStatus == ImuStatus::MotionDetected) {
		len = snprintf(tx_buffer, sizeof(tx_buffer),
		               "[CRITICAL] CALIBRATION FAILED: DRONE IS MOVING! PLACE ON FLAT SURFACE.\r\n");
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		while (1) {
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			HAL_Delay(50);    // Panik flasi: hareket
		}
	}
	else if (calibStatus != ImuStatus::Ok) {
		len = snprintf(tx_buffer, sizeof(tx_buffer),
		               "[CRITICAL] CALIBRATION FAILED: HARDWARE FAULT.\r\n");
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		while (1) {
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			HAL_Delay(500);   // Yavas flas: donanim
		}
	}

	// --- Filtre ilkleme ---
	bool estReady = false;
	for (int i = 0; i < 5 && !estReady; i++) {
		if (imu.read(sample) == ImuStatus::Ok) {
			est.init(sample);
			estReady = true;
		} else {
			HAL_Delay(10);
		}
	}
	if (!estReady) {
		len = snprintf(tx_buffer, sizeof(tx_buffer),
		               "[WARNING] Filtre ilk okumadan baslatilamadi, sifirdan basliyor\r\n");
		CDC_Transmit_FS((uint8_t*)tx_buffer, len);
		HAL_Delay(200);
	}

	len = snprintf(tx_buffer, sizeof(tx_buffer),
	               "MPU6050 SYSTEM READY. INITIATING CONTINUOUS RX DIAGNOSTICS...\r\n\r\n");
	CDC_Transmit_FS((uint8_t*)tx_buffer, len);
	HAL_Delay(1000);


	rc.begin();


	HAL_TIM_Base_Start_IT(&htim3);

	Attitude att{};

	// --- ANA DONGU ---
	uint32_t loopCount = 0;
	while (1) {
			while (!controlTick) { }
			controlTick = false;

			rc.poll();

			if (imu.read(sample) == ImuStatus::Ok) {
				att = est.update(sample);
			} else {
				// Kurtarma seyreltilmez - hemen denenmeli
				if (imu.recoverBus() == ImuStatus::Ok) {
					imu.init();
				}
			}

			// Telemetri: 250/12 ≈ 20 Hz
			if (++loopCount % 12 == 0) {
				const RcInput& in = rc.input();
				len = snprintf(tx_buffer, sizeof(tx_buffer),
				               "Roll:%7.2f Pitch:%7.2f | CH: %u %u %u %u %u %u | link:%d cnt:%lu err:%lu missed:%lu\r\n",
				               att.roll, att.pitch,
				               in.channel[0], in.channel[1], in.channel[2],
				               in.channel[3], in.channel[4], in.channel[5],
				               in.linkOk, (unsigned long)in.frameCount,
				               (unsigned long)rc.lastUartError(),
				               (unsigned long)missedDeadlines);

				if (len > 0 && len < (int)sizeof(tx_buffer)) {
					CDC_Transmit_FS((uint8_t*)tx_buffer, len);
				}
			}
		}
}
