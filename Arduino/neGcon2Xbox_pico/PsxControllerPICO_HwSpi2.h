#ifndef PSX_CONTROLLER_PICO_HW_SPI2_H
#define PSX_CONTROLLER_PICO_HW_SPI2_H

#include "PsxNewLib.h"

/** \brief Attention Delay
 *
 * Time between attention being issued to the controller and the first clock
 * edge (us).
 */
const byte ATTN_DELAY = 50;

template <unsigned int PIN_ATT, unsigned int PIN_CLK = 2, unsigned int PIN_DAT = 4, unsigned int PIN_CMD = 3>
class PsxControllerPICO_HwSpi: public PsxController {
protected:
	virtual void attention () override {
		gpio_put(PIN_ATT, 0);
		delayMicroseconds (ATTN_DELAY);
	}
	
	virtual void noAttention () override {
		gpio_put(PIN_ATT, 1);
		delayMicroseconds (ATTN_DELAY);
	}
	
	virtual byte shiftInOut (const byte out) override {
		byte in = 0;
		for (uint8_t i = 0; i < 8; i++) {
			// Set command bit (LSB first)
			gpio_put(PIN_CMD, (out & (1 << i)) ? 1 : 0);
			
			// Clock goes LOW
			gpio_put(PIN_CLK, 0);
			delayMicroseconds (4); // ~125kHz clock
			
			// Read data bit
			if (gpio_get(PIN_DAT)) {
				in |= (1 << i);
			}
			
			// Clock goes HIGH
			gpio_put(PIN_CLK, 1);
			delayMicroseconds (4);
		}
		return in;
	}

public:
	virtual boolean begin () override {
		gpio_init(PIN_ATT);
		gpio_set_dir(PIN_ATT, GPIO_OUT);
		gpio_put(PIN_ATT, 1);

		gpio_init(PIN_CLK);
		gpio_set_dir(PIN_CLK, GPIO_OUT);
		gpio_put(PIN_CLK, 1);

		gpio_init(PIN_CMD);
		gpio_set_dir(PIN_CMD, GPIO_OUT);
		gpio_put(PIN_CMD, 1);

		gpio_init(PIN_DAT);
		gpio_set_dir(PIN_DAT, GPIO_IN);
		gpio_pull_up(PIN_DAT);

		return PsxController::begin ();
	}

	// Jogcon 用拡張メソッドのローカル実装 (標準 of PsxNewLib に不足している分を補完)
	boolean enableJogconRumble () {
		return enableRumble (true);
	}

	boolean getJogConAnalog (short& x) {
		if (protocol == PSPROTO_JOGCON) {
			x = (short)(((int16_t)inputBuffer[6] << 8) | inputBuffer[5]);
			return true;
		}
		return false;
	}

	void setJogCommand (byte cmd) {
		motor1Level = cmd;
		motor2Level = 0x00;
	}
};

#endif // PSX_CONTROLLER_PICO_HW_SPI2_H