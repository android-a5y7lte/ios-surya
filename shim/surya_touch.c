/*
 * surya touch bring-up, step 3a: READ-ONLY probe of the touch SPI bus.
 *
 * POCO X3 NFC (surya): Novatek NT36672C in-cell touch on QUP wrapper 0 SE0
 * (spi@0x880000, `qcom,spi-geni`), pins gpio49-52 (qup00), reset gpio8,
 * irq gpio9. See SURYA_NOTES.md "Touch plan".
 *
 * This file only reads registers. Before touching the serial engine it checks
 * the GCC branch clocks: reading a QUP block whose AHB clocks are off can
 * hang the bus, so in that case the SE registers are skipped and reported.
 * Only built by shim/build-surya.sh (-DSURYA_TOUCH_PROBE).
 */
#include <stdint.h>
#include "console.h"
#include "surya_touch.h"

#define GCC_BASE		0x00100000UL
#define GCC_QUP_VOTE		0x5200cUL	/* APCS clock branch ena vote 1 */
#define GCC_QUP0_M_AHB_CBCR	0x17004UL	/* vote bit 6 */
#define GCC_QUP0_S_AHB_CBCR	0x17008UL	/* vote bit 7 */
#define GCC_QUP0_CORE_CBCR	0x1700cUL	/* vote bit 8 */
#define GCC_QUP0_CORE2X_CBCR	0x17014UL	/* vote bit 9 */
#define GCC_QUP0_S0_CBCR	0x17030UL	/* vote bit 10 */
#define CBCR_CLK_OFF		(1U << 31)

#define QUP0_WRAPPER		0x008c0000UL	/* qupv3_0_geni_se */
#define QUP0_SE0		0x00880000UL	/* spi@0x880000 */

/* GENI serial engine registers (Linux include/linux/soc/qcom/geni-se.h) */
#define GENI_STATUS		0x040
#define GENI_SER_M_CLK_CFG	0x048
#define GENI_IF_DISABLE_RO	0x064
#define GENI_FW_REVISION_RO	0x068
#define SE_GENI_CLK_SEL		0x07c
#define SE_SPI_CPHA		0x224
#define SE_SPI_CPOL		0x230
#define SE_SPI_DEMUX_SEL	0x248
#define SE_GENI_DMA_MODE_EN	0x258
#define SE_SPI_WORD_LEN		0x268
#define SE_GENI_M_IRQ_STATUS	0x610
#define SE_GENI_TX_FIFO_STATUS	0x800
#define SE_GENI_RX_FIFO_STATUS	0x804
#define SE_HW_PARAM_0		0xe24
#define SE_HW_PARAM_1		0xe28

static inline uint32_t rd(uintptr_t a) { return *(volatile uint32_t *)a; }

static const char *proto_name(uint32_t p)
{
	switch (p) {
	case 1: return "SPI (expected)";
	case 2: return "UART";
	case 3: return "I2C";
	case 0xff: return "none loaded";
	default: return "unknown";
	}
}

void surya_touch_probe(void)
{
	uint32_t vote = rd(GCC_BASE + GCC_QUP_VOTE);
	uint32_t m_ahb = rd(GCC_BASE + GCC_QUP0_M_AHB_CBCR);
	uint32_t s_ahb = rd(GCC_BASE + GCC_QUP0_S_AHB_CBCR);
	uint32_t core = rd(GCC_BASE + GCC_QUP0_CORE_CBCR);
	uint32_t core2x = rd(GCC_BASE + GCC_QUP0_CORE2X_CBCR);
	uint32_t se0 = rd(GCC_BASE + GCC_QUP0_S0_CBCR);

	con_str("TOUCH", "surya step 3a: SPI SE0 probe (read-only)");
	con_hex2("GCC VOTE", "5200c=", vote, "qup0 bits6-10=", (vote >> 6) & 0x1f);
	con_hex2("GCC AHB", "m=", m_ahb, "s=", s_ahb);
	con_hex2("GCC CORE", "core=", core, "2x=", core2x);
	con_hex("GCC SE0", se0);

	if ((m_ahb | s_ahb) & CBCR_CLK_OFF) {
		con_str("TOUCH", "QUP0 AHB clock off: SE registers skipped");
		return;
	}
	uint32_t hwver = rd(QUP0_WRAPPER + 0x4);
	uint32_t fw = rd(QUP0_SE0 + GENI_FW_REVISION_RO);
	uint32_t proto = (fw >> 8) & 0xff;
	con_hex2("QUP0", "hw_ver=", hwver, "se0_fw=", fw);
	con_str("SE0 PROTO", proto_name(proto));
	con_hex2("SE0", "status=", rd(QUP0_SE0 + GENI_STATUS), "if_dis=", rd(QUP0_SE0 + GENI_IF_DISABLE_RO));
	con_hex2("SE0 CLK", "m_cfg=", rd(QUP0_SE0 + GENI_SER_M_CLK_CFG), "sel=", rd(QUP0_SE0 + SE_GENI_CLK_SEL));
	con_hex2("SE0 SPI", "cpha=", rd(QUP0_SE0 + SE_SPI_CPHA), "cpol=", rd(QUP0_SE0 + SE_SPI_CPOL));
	con_hex2("SE0 SPI", "word=", rd(QUP0_SE0 + SE_SPI_WORD_LEN), "demux=", rd(QUP0_SE0 + SE_SPI_DEMUX_SEL));
	con_hex2("SE0 MODE", "dma_en=", rd(QUP0_SE0 + SE_GENI_DMA_MODE_EN), "m_irq=", rd(QUP0_SE0 + SE_GENI_M_IRQ_STATUS));
	con_hex2("SE0 FIFO", "tx=", rd(QUP0_SE0 + SE_GENI_TX_FIFO_STATUS), "rx=", rd(QUP0_SE0 + SE_GENI_RX_FIFO_STATUS));
	con_hex2("SE0 HW", "p0=", rd(QUP0_SE0 + SE_HW_PARAM_0), "p1=", rd(QUP0_SE0 + SE_HW_PARAM_1));
}
