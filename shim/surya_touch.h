#pragma once
/* surya touch SPI probe (shim/surya_touch.c), built only by build-surya.sh */
void surya_touch_probe(void);
#include <stdint.h>
void surya_touch_spi_test(uintptr_t dtb);
