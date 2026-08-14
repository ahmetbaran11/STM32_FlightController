#include "IbusReceiver.h"
#include <cstdio>

namespace {
    // Bu sureden uzun sure hic bayt gelmediyse DMA takilmis sayiyoruz.
    // i-BUS cerceveleri ~7ms'de bir geldigi icin 100ms bol bir pay.
    constexpr uint32_t STALL_MS = 100;

    // Yeniden baslatmayi bu araliktan sik denemiyoruz - hat kopukken
    // her poll()'da DMAStop/Start yapmanin anlami yok.
    constexpr uint32_t RESTART_MIN_INTERVAL_MS = 100;
}

IbusReceiver::IbusReceiver(UART_HandleTypeDef* uart, uint32_t timeoutMs)
    : uart_(uart), timeoutMs_(timeoutMs) {}

void IbusReceiver::begin() {
	// Circular DMA: cevre birimi surekli halka tampona yazar, hic durmaz.
	// Bir daha yeniden baslatmaya gerek yok.
	HAL_UART_Receive_DMA(uart_, dmaBuf_, sizeof(dmaBuf_));
	readPos_ = 0;
	frameLen_ = 0;
	lastFrameMs_ = HAL_GetTick();
	lastWritePos_ = 0;
	lastWriteChangeMs_ = HAL_GetTick();
	lastRestartMs_ = HAL_GetTick();
}


bool IbusReceiver::parseFrame() {
	// Saglama: 0xFFFF eksi ilk 30 baytin toplami
	uint16_t sum = 0;
	for (int i = 0; i < 30; i++) {
		sum += frame_[i];
	}

	uint16_t expected = 0xFFFF - sum;
	uint16_t received = frame_[30] | (frame_[31] << 8);

	if (expected != received) {
		//bozuk çerçeve, kanallara dokunma eski gecerli değerler kalsın
		return false;
	}

	for (int i = 0; i < IBUS_CHANNELS; i++) {
		input_.channel[i] = frame_[2 + i*2] | (frame_[3 + i*2] << 8);
	}

	lastFrameMs_ = HAL_GetTick();
	input_.frameCount++;
	return true;
}

void IbusReceiver::poll() {

	uint32_t now = HAL_GetTick();

	uint16_t writePos = sizeof(dmaBuf_) - __HAL_DMA_GET_COUNTER(uart_->hdmarx);
	bool stalled = (now - lastWriteChangeMs_) > STALL_MS;


	if ((uart_->ErrorCode != HAL_UART_ERROR_NONE || stalled)
		    && (now - lastRestartMs_) >= RESTART_MIN_INTERVAL_MS) {
			restartDma();
			lastRestartMs_ = now;
			lastWriteChangeMs_ = now;   // yeniden baslat, sayaci sifirla
			return;
		}


	while (readPos_ != writePos) {
	        uint8_t b = dmaBuf_[readPos_];
	        readPos_ = (readPos_ + 1) % sizeof(dmaBuf_);

	        if (frameLen_ == 0 && b != 0x20) continue;
	        if (frameLen_ == 1 && b != 0x40) {
	        	frameLen_ = 0;
	        	continue;
	        }
	        frame_[frameLen_++] = b;

	        if (frameLen_ == 32) {
	        	parseFrame();
	        	frameLen_ = 0;
	        }

	}
	// Son gecerli cerceveden bu yana timeout gectiyse link kopmus demektir
	input_.linkOk = (HAL_GetTick() - lastFrameMs_) < timeoutMs_;
}

void IbusReceiver::restartDma() {
	HAL_UART_DMAStop(uart_);

	//bayrakları temizle
	__HAL_UART_CLEAR_OREFLAG(uart_);
	__HAL_UART_CLEAR_FEFLAG(uart_);
	__HAL_UART_CLEAR_NEFLAG(uart_);
	__HAL_UART_CLEAR_PEFLAG(uart_);

	//error codeu sıfırla
	uart_->ErrorCode = HAL_UART_ERROR_NONE;

	//dmayı yeniden başlat
	HAL_UART_Receive_DMA(uart_, dmaBuf_, sizeof(dmaBuf_));

	readPos_ = 0;
	frameLen_ = 0;
}
