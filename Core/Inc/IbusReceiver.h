#include "stm32f4xx_hal.h"
#include "IbusTypes.h"

class IbusReceiver {
public:
	IbusReceiver(UART_HandleTypeDef* uart, uint32_t timeoutMs = 500);
	void begin();	//dmayi baslat
	void poll();	//yeni baytlari isle, failsafe kontrolu
	const RcInput& input() const {return input_;}

    IbusReceiver(const IbusReceiver&) = delete;
    IbusReceiver& operator=(const IbusReceiver&) = delete;
    uint32_t lastUartError() const { return uart_->ErrorCode; }

private:
    UART_HandleTypeDef* uart_;
    uint32_t timeoutMs_;
    uint8_t frameLen_ = 0;
	uint8_t dmaBuf_[128];
	uint16_t readPos_ = 0;
	uint8_t frame_[32];
	uint32_t lastFrameMs_ = 0;
	RcInput input_{};
	bool parseFrame();
	void restartDma();
	uint16_t lastWritePos_ = 0;        // DMA'nin en son yazma pozisyonu
	uint32_t lastWriteChangeMs_ = 0;   // o pozisyonun en son degistigi an
    uint32_t lastRestartMs_ = 0;       // son yeniden baslatma zamani
};
