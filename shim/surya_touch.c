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

#if defined(SURYA_TOUCH_STEP) && SURYA_TOUCH_STEP >= 2
/*
 * Step 3b: bring up SE0 as an SPI master and READ the Novatek chip ID.
 *
 * Writes, all reversible by a reset: GCC clock votes for QUP wrapper 0 + SE0
 * (OR-ed in, like sweet's touch_clocks), the SE0 RCG set to XO/1 = 19.2 MHz
 * unless DFS owns it, TLMM gpio49-52 muxed to qup00, SE0 SPI registers.
 * On the touch IC: only Novatek "page select" frames (ff XX YY) and read
 * commands. No bootloader reset, no firmware, no IC register writes.
 * Protocol from sm7150-mainline nt36xxx_spi.c / nt36xxx_core.c.
 */
#define GCC_QUP0_S0_RCG		0x17034UL	/* cmd_rcgr; +4 cfg, +0x14 dfsr */
#define TLMM_WEST		0x03500000UL	/* gpio49-52 */
#define TLMM_NORTH		0x03900000UL	/* gpio8 (reset), gpio9 (irq) */
#define TLMM_CFG(base, n)	((base) + (uintptr_t)(n) * 0x1000)

#define GENI_FORCE_DEFAULT	0x020
#define GENI_OUTPUT_CTRL	0x024
#define SE_SPI_LOOPBACK		0x22c
#define SE_SPI_DEMUX_OUT_INV	0x24c
#define SE_SPI_TRANS_CFG	0x25c
#define SE_TX_PACKING_CFG0	0x260
#define SE_TX_PACKING_CFG1	0x264
#define SE_SPI_TX_TRANS_LEN	0x26c
#define SE_SPI_RX_TRANS_LEN	0x270
#define SE_SPI_PRE_POST_DLY	0x274
#define SE_SPI_DELAY_COUNTERS	0x278
#define SE_RX_PACKING_CFG0	0x284
#define SE_RX_PACKING_CFG1	0x288
#define SE_GENI_M_CMD0		0x600
#define SE_GENI_M_CMD_CTRL	0x604
#define SE_GENI_M_IRQ_EN	0x614
#define SE_GENI_M_IRQ_CLEAR	0x618
#define SE_GENI_TX_FIFO		0x700
#define SE_GENI_RX_FIFO		0x780
#define SE_GENI_TX_WATERMARK	0x80c
#define SE_IRQ_EN		0xe1c

#define SPI_FULL_DUPLEX		3u
#define M_CMD_DONE		(1u << 0)
#define M_ERRORS		(0x3eu)		/* overrun, illegal, failure, cancel, abort */
#define SPI_CLK_DIV		4u		/* 19.2 MHz / 4 = 4.8 MHz (chip max 8 MHz) */

static inline void wr(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }

static uint64_t ticks(void)
{
	uint64_t t;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(t));
	return t;
}

static void sleep_us(unsigned us)
{
	uint64_t end = ticks() + (uint64_t)us * 192 / 10;	/* 19.2 MHz */
	while (ticks() < end)
		;
}

static int wait_clear(uintptr_t a, uint32_t mask, unsigned us)
{
	uint64_t end = ticks() + (uint64_t)us * 192 / 10;
	while (rd(a) & mask)
		if (ticks() > end)
			return 0;
	return 1;
}

static int clocks_on(void)
{
	wr(GCC_BASE + GCC_QUP_VOTE, rd(GCC_BASE + GCC_QUP_VOTE) | (0x1fu << 6));
	uint32_t dfs = rd(GCC_BASE + GCC_QUP0_S0_RCG + 0x14);
	uint32_t cfg = rd(GCC_BASE + GCC_QUP0_S0_RCG + 4);
	con_hex2("SPI RCG", "cfg=", cfg, "dfsr=", dfs);
	if (dfs & 1u) {
		uint32_t l0 = rd(GCC_BASE + GCC_QUP0_S0_RCG + 0x1c);
		con_hex("SPI DFS L0", l0);
		if ((l0 & 0x371fu) != 1u) {
			con_str("SPI", "DFS level 0 is not XO/1: stop");
			return 0;
		}
	} else {
		/* XO parent (0), HID divider 1, no MND: 19.2 MHz, no PLL change. */
		wr(GCC_BASE + GCC_QUP0_S0_RCG + 4, (cfg & ~(0x371fu | (1u << 20))) | 1u);
		wr(GCC_BASE + GCC_QUP0_S0_RCG, rd(GCC_BASE + GCC_QUP0_S0_RCG) | 1u);
		if (!wait_clear(GCC_BASE + GCC_QUP0_S0_RCG, 1u, 10000)) {
			con_str("SPI", "RCG update timeout: stop");
			return 0;
		}
	}
	static const uint32_t cbcr[] = {
		GCC_QUP0_M_AHB_CBCR, GCC_QUP0_S_AHB_CBCR, GCC_QUP0_CORE_CBCR,
		GCC_QUP0_CORE2X_CBCR, GCC_QUP0_S0_CBCR,
	};
	for (unsigned i = 0; i < sizeof(cbcr) / sizeof(cbcr[0]); i++) {
		if (!wait_clear(GCC_BASE + cbcr[i], CBCR_CLK_OFF, 10000)) {
			con_hex2("SPI CBCR off", "reg=", cbcr[i], "val=", rd(GCC_BASE + cbcr[i]));
			return 0;
		}
	}
	con_hex2("SPI CLK ON", "se0=", rd(GCC_BASE + GCC_QUP0_S0_CBCR), "vote=", rd(GCC_BASE + GCC_QUP_VOTE));
	return 1;
}

static void pins(void)
{
	for (unsigned n = 49; n <= 52; n++) {
		uintptr_t c = TLMM_CFG(TLMM_WEST, n);
		uint32_t was = rd(c);
		/* func qup00 (2) at [5:2], drive 6 mA (2) at [8:6], no pull. */
		wr(c, (was & ~0x3ffu) | (2u << 2) | (2u << 6));
		if (n == 49 || n == 52)
			con_hex2(n == 49 ? "GPIO49" : "GPIO52", "was=", was, "now=", rd(c));
	}
	con_hex2("GPIO8 RST", "cfg=", rd(TLMM_CFG(TLMM_NORTH, 8)), "io=", rd(TLMM_CFG(TLMM_NORTH, 8) + 4));
	con_hex2("GPIO9 IRQ", "cfg=", rd(TLMM_CFG(TLMM_NORTH, 9)), "io=", rd(TLMM_CFG(TLMM_NORTH, 9) + 4));
}

static void se_spi_init(void)
{
	uintptr_t se = QUP0_SE0;
	wr(se + SE_GENI_DMA_MODE_EN, 0);			/* FIFO mode */
	wr(se + SE_IRQ_EN, (1u << 2) | (1u << 3));		/* M and S irq lines */
	wr(se + SE_GENI_M_IRQ_EN, M_CMD_DONE | M_ERRORS);
	wr(se + SE_GENI_M_IRQ_CLEAR, 0xffffffffu);
	wr(se + SE_GENI_CLK_SEL, 0);
	wr(se + GENI_SER_M_CLK_CFG, (SPI_CLK_DIV << 4) | 1u);
	wr(se + SE_SPI_LOOPBACK, 0);
	wr(se + SE_SPI_CPHA, 0);				/* SPI mode 0 */
	wr(se + SE_SPI_CPOL, 0);
	wr(se + SE_SPI_DEMUX_SEL, 0);				/* CS0 */
	wr(se + SE_SPI_DEMUX_OUT_INV, 0);			/* CS active low */
	wr(se + SE_SPI_TRANS_CFG, 0);				/* no CS toggle per word */
	wr(se + SE_SPI_WORD_LEN, 8 - 4);			/* 8 bits per word */
	wr(se + SE_SPI_PRE_POST_DLY, 0);
	wr(se + SE_SPI_DELAY_COUNTERS, 0);
	/* geni_se_config_packing(8 bpw, 4 per FIFO word, MSB first): byte 0 = bits 7..0 */
	wr(se + SE_TX_PACKING_CFG0, 0x7f8fe);
	wr(se + SE_TX_PACKING_CFG1, 0xffefe);
	wr(se + SE_RX_PACKING_CFG0, 0x7f8fe);
	wr(se + SE_RX_PACKING_CFG1, 0xffefe);
	con_hex2("SPI SE0", "m_cfg=", rd(se + GENI_SER_M_CLK_CFG), "word=", rd(se + SE_SPI_WORD_LEN));
}

/* One full-duplex transfer, CS held for its whole length. len <= 32. */
static int spi_xfer(const uint8_t *tx, uint8_t *rx, unsigned len)
{
	uintptr_t se = QUP0_SE0;
	if (!wait_clear(se + GENI_STATUS, 1u, 10000))
		return -1;
	wr(se + SE_GENI_M_IRQ_CLEAR, 0xffffffffu);
	wr(se + SE_SPI_TX_TRANS_LEN, len);
	wr(se + SE_SPI_RX_TRANS_LEN, len);
	wr(se + SE_GENI_M_CMD0, SPI_FULL_DUPLEX << 27);
	for (unsigned i = 0; i < len; i += 4) {
		uint32_t w = 0;
		for (unsigned b = 0; b < 4 && i + b < len; b++)
			w |= (uint32_t)tx[i + b] << (8 * b);
		wr(se + SE_GENI_TX_FIFO, w);
	}
	uint64_t end = ticks() + 192000;	/* 10 ms */
	uint32_t irq;
	while (!((irq = rd(se + SE_GENI_M_IRQ_STATUS)) & (M_CMD_DONE | M_ERRORS)))
		if (ticks() > end) {
			wr(se + SE_GENI_M_CMD_CTRL, 1u << 1);	/* cancel */
			return -2;
		}
	unsigned got = 0, words = rd(se + SE_GENI_RX_FIFO_STATUS) & 0x1ffffffu;
	for (unsigned w = 0; w < words; w++) {
		uint32_t v = rd(se + SE_GENI_RX_FIFO);
		for (unsigned b = 0; b < 4 && got < len; b++)
			rx[got++] = (uint8_t)(v >> (8 * b));
	}
	wr(se + SE_GENI_M_IRQ_CLEAR, 0xffffffffu);
	return (irq & M_ERRORS) ? -3 : (int)got;
}

/* Novatek read: page select ff a[23:16]... then (addr & 0x7f), dummy, data. */
static int nvt_read(uint32_t addr, uint8_t *buf, unsigned n)
{
	uint8_t tx[32] = { 0xff, (uint8_t)(addr >> 15), (uint8_t)(addr >> 7) }, rx[32];
	int r = spi_xfer(tx, rx, 3);
	if (r < 0)
		return r;
	for (unsigned i = 0; i < sizeof(tx); i++)
		tx[i] = 0;
	tx[0] = addr & 0x7f;
	r = spi_xfer(tx, rx, n + 2);
	if (r < 0)
		return r;
	for (unsigned i = 0; i < n; i++)
		buf[i] = rx[i + 2];
	return 0;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

void surya_touch_spi_test(void)
{
	con_str("TOUCH", "step 3b: SPI chip ID read");
	if (!clocks_on())
		return;
	pins();
	se_spi_init();
	sleep_us(1000);

	uint8_t id[8] = { 0 };
	int r = nvt_read(0x1f64e, id, 6);		/* MMAP_MAGIC_NUMBER_0X1F64E */
	con_hex2("NVT ID", "rc=", (uint32_t)r, "b0-3=", be32(id));
	con_hex2("NVT ID", "b4-5=", (uint32_t)id[4] << 8 | id[5], "want=", 0x726603);
	if (r == 0 && id[3] == 0x72 && id[4] == 0x66 && id[5] == 0x03)
		con_str("NVT ID", "NT36672-class chip FOUND (72 66 03)");
	else if (r == 0 && id[3] == 0x75 && id[4] == 0x66 && id[5] == 0x03)
		con_str("NVT ID", "NT36675-class chip FOUND (75 66 03)");
	else
		con_str("NVT ID", "no match (chip may need bootloader reset)");

	uint8_t st[8] = { 0 };
	r = nvt_read(0x22d00 | 0x60, st, 6);	/* event buf | RESET_COMPLETE */
	con_hex2("NVT STATE", "rc=", (uint32_t)r, "b0-3=", be32(st));
	con_hex2("NVT STATE", "fw_state=", st[1], "(a3=running)", st[1] == 0xa3);
}
#endif
