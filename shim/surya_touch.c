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

#define SPI_TX_ONLY		1u
#define FIFO_WORDS		16u	/* SE_HW_PARAM_0 bits 21:16 = 0x10 on surya */

/*
 * One SPI transfer with CS held for its whole length, FIFO polled.
 * rx == NULL: TX only (no RX FIFO traffic). Any length up to 64 KiB.
 */
static int spi_xfer(const uint8_t *tx, uint8_t *rx, unsigned len)
{
	uintptr_t se = QUP0_SE0;
	if (!wait_clear(se + GENI_STATUS, 1u, 10000))
		return -1;
	wr(se + SE_GENI_M_IRQ_CLEAR, 0xffffffffu);
	wr(se + SE_SPI_TX_TRANS_LEN, len);
	wr(se + SE_SPI_RX_TRANS_LEN, rx ? len : 0);
	wr(se + SE_GENI_M_CMD0, (rx ? SPI_FULL_DUPLEX : SPI_TX_ONLY) << 27);
	unsigned sent = 0, got = 0;
	uint64_t end = ticks() + 19200000;	/* 1 s */
	uint32_t irq = 0;
	for (;;) {
		while (sent < len && (rd(se + SE_GENI_TX_FIFO_STATUS) & 0xfffffffu) < FIFO_WORDS) {
			uint32_t w = 0;
			for (unsigned b = 0; b < 4 && sent < len; b++, sent++)
				w |= (uint32_t)(tx ? tx[sent] : 0) << (8 * b);
			wr(se + SE_GENI_TX_FIFO, w);
		}
		if (rx) {
			unsigned words = rd(se + SE_GENI_RX_FIFO_STATUS) & 0x1ffffffu;
			while (words--) {
				uint32_t v = rd(se + SE_GENI_RX_FIFO);
				for (unsigned b = 0; b < 4 && got < len; b++)
					rx[got++] = (uint8_t)(v >> (8 * b));
			}
		}
		irq = rd(se + SE_GENI_M_IRQ_STATUS);
		if ((irq & M_ERRORS) || ((irq & M_CMD_DONE) && sent >= len && (!rx || got >= len)))
			break;
		if (ticks() > end) {
			wr(se + SE_GENI_M_CMD_CTRL, 1u << 1);	/* cancel */
			return -2;
		}
	}
	wr(se + SE_GENI_M_IRQ_CLEAR, 0xffffffffu);
	return (irq & M_ERRORS) ? -3 : (int)len;
}

/* Novatek read: page select ff a[23:16]... then (addr & 0x7f), dummy, data. */
static int nvt_read(uint32_t addr, uint8_t *buf, unsigned n)
{
	uint8_t tx[80], rx[80];
	if (n + 2 > sizeof(tx))
		return -4;
	tx[0] = 0xff; tx[1] = (uint8_t)(addr >> 15); tx[2] = (uint8_t)(addr >> 7);
	int r = spi_xfer(tx, 0, 3);
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

#if SURYA_TOUCH_STEP >= 3
/*
 * Step 3c: download the touch firmware and show touches on screen.
 * Port of sm7150-mainline nt36xxx_core.c: eng_reset_idle, bootloader_reset,
 * nvt_bin_header_parser, download_firmware_hw_crc, boot_download (hw_crc = 2,
 * NT36675 memory map, chip id 0c xx xx 72 66 03). The firmware blob
 * (novatek_ts_huaxing_fw.bin) is the boot image ramdisk. Writes go only to the
 * touch IC's SRAM/registers; it forgets everything on power loss.
 */
#include "fdt.h"

#define NVT_EVENT_BUF		0x22d00u
#define NVT_BOOT_RDY		0x3f10du
#define NVT_ILM_LEN		0x3f118u
#define NVT_DLM_LEN		0x3f130u
#define NVT_ILM_DES		0x3f128u
#define NVT_DLM_DES		0x3f12cu
#define NVT_G_ILM_CRC		0x3f100u
#define NVT_G_DLM_CRC		0x3f104u
#define NVT_BLD_CRC_EN		0x3f30eu
#define NVT_SPI_RD_FAST		0x3f310u
#define NVT_SWRST_N8		0x3f0feu
#define NVT_ENG_RST		0x7fff80u
#define NVT_CHUNK		4096u
#define TOUCH_W			1080
#define TOUCH_H			2400

static uint8_t wbuf[NVT_CHUNK + 4];

static int nvt_page(uint32_t addr)
{
	uint8_t p[3] = { 0xff, (uint8_t)(addr >> 15), (uint8_t)(addr >> 7) };
	return spi_xfer(p, 0, 3);
}

static int nvt_write(uint32_t addr, const uint8_t *data, unsigned n)
{
	while (n) {
		unsigned c = n > NVT_CHUNK ? NVT_CHUNK : n;
		int r = nvt_page(addr);
		if (r < 0)
			return r;
		wbuf[0] = (addr & 0x7f) | 0x80;
		for (unsigned i = 0; i < c; i++)
			wbuf[1 + i] = data[i];
		r = spi_xfer(wbuf, 0, c + 1);
		if (r < 0)
			return r;
		addr += c; data += c; n -= c;
	}
	return 0;
}

static int nvt_w8(uint32_t addr, uint8_t v) { return nvt_write(addr, &v, 1); }

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int nvt_bootloader_reset(void)
{
	int r = nvt_w8(NVT_SWRST_N8, 0x69);
	sleep_us(35000);
	if (r == 0)
		r = nvt_w8(NVT_SPI_RD_FAST, 0);
	return r;
}

struct part { uint32_t bin, sram, size, crc; };

static int nvt_download(const uint8_t *fw, uint32_t fwsize)
{
	/* find the size actually written: last 4 KiB block ending in "NVT"/"MOD" */
	uint32_t need = 0;
	for (uint32_t i = fwsize / 4096; i > 0 && !need; i--) {
		const uint8_t *m = fw + i * 4096 - 3;
		if ((m[0] == 'N' && m[1] == 'V' && m[2] == 'T') || (m[0] == 'M' && m[1] == 'O' && m[2] == 'D'))
			need = i * 4096;
	}
	if (!need) {
		con_str("NVT FW", "no NVT/MOD marker: not a Novatek blob");
		return -10;
	}
	if (((fw[need - 4096] + fw[need - 4095]) & 0xff) != 0xff) {
		con_hex2("NVT FW", "ver=", fw[need - 4096], "bar=", fw[need - 4095]);
		return -11;
	}
	con_hex2("NVT FW", "size=", fwsize, "fw_ver=", fw[need - 4096]);

	/* nvt_bin_header_parser, ilm_dlm_num = 2 */
	struct part p[24];
	uint32_t end = le32(fw), info = 0;
	for (uint32_t pos = 0x30; pos < end; pos += 0x10)
		info++;
	uint32_t ovly = (fw[0x28] & 0x10) ? (fw[0x28] & 0x0f) : 0;
	uint32_t nparts = 2 + info + ovly;
	if (nparts > 24)
		return -12;
	for (uint32_t l = 0; l < nparts; l++) {
		const uint8_t *h;
		if (l < 2) {
			p[l].bin = le32(fw + l * 12);
			p[l].sram = le32(fw + 4 + l * 12);
			p[l].size = le32(fw + 8 + l * 12);
			p[l].crc = le32(fw + 0x18 + l * 4);
			continue;
		}
		h = l < 2 + info ? fw + 0x30 + 0x10 * (l - 2) : fw + p[1].bin + 0x10 * (l - 2 - info);
		p[l].sram = le32(h); p[l].size = le32(h + 4); p[l].bin = le32(h + 8); p[l].crc = le32(h + 12);
		if (p[l].bin + p[l].size > fwsize)
			return -13;
	}
	con_hex2("NVT PARTS", "n=", nparts, "info/ovly=", info << 16 | ovly);
	con_hex2("NVT ILM", "sram=", p[0].sram, "size=", p[0].size);
	con_hex2("NVT DLM", "sram=", p[1].sram, "size=", p[1].size);

	/* download_firmware_hw_crc */
	int r = nvt_bootloader_reset();
	if (r)
		return r;
	for (uint32_t l = 0; l < nparts; l++) {
		if (!p[l].size)
			continue;
		r = nvt_write(p[l].sram, fw + p[l].bin, p[l].size + 1);
		if (r)
			return r;
	}
	/* ILM / DLM descriptors: 3-byte address, 3-byte length, 4-byte CRC (LE) */
	uint8_t v[4];
	for (int l = 0; l < 2; l++) {
		v[0] = p[l].sram; v[1] = p[l].sram >> 8; v[2] = p[l].sram >> 16;
		nvt_write(l ? NVT_DLM_DES : NVT_ILM_DES, v, 3);
		v[0] = p[l].size; v[1] = p[l].size >> 8; v[2] = p[l].size >> 16;
		nvt_write(l ? NVT_DLM_LEN : NVT_ILM_LEN, v, 3);
		v[0] = p[l].crc; v[1] = p[l].crc >> 8; v[2] = p[l].crc >> 16; v[3] = p[l].crc >> 24;
		nvt_write(l ? NVT_G_DLM_CRC : NVT_G_ILM_CRC, v, 4);
	}
	uint8_t c = 0;
	nvt_read(NVT_BLD_CRC_EN, &c, 1);
	nvt_w8(NVT_BLD_CRC_EN, c | 0x80);			/* bootloader CRC enable */
	nvt_w8(NVT_EVENT_BUF | 0x60, 0);			/* clear reset-complete */
	nvt_w8(NVT_EVENT_BUF | 0x50, 0xae);			/* enable fw CRC */
	nvt_w8(NVT_BOOT_RDY, 1);				/* boot ready */
	sleep_us(10000);

	uint8_t st[8] = { 0 };
	int ok = 0;
	for (int i = 0; i < 50 && !ok; i++) {
		/* Linux buf[1] == our st[0]: its regmap buffer keeps the dummy byte at [0] */
		if (nvt_read(NVT_EVENT_BUF | 0x60, st, 6) == 0 && st[0] >= 0xa0 && st[0] <= 0xaf)
			ok = 1;
		else
			sleep_us(10000);
	}
	con_hex2("NVT BOOT", "ok=", ok, "st0-3=", be32(st));
	/* fw info: fw_ver, ~fw_ver, x_num, y_num, abs_x_max (BE16), abs_y_max (BE16) */
	uint8_t info16[16] = { 0 };
	nvt_read(NVT_EVENT_BUF | 0x78, info16, 16);
	con_hex2("NVT FWINFO", "b0-3=", be32(info16), "b4-7=", be32(info16 + 4));
	int fw_ok = ((info16[0] + info16[1]) & 0xff) == 0xff;
	con_hex2("NVT FWINFO", "valid=", fw_ok, "max_xy=", (uint32_t)(info16[4] << 8 | info16[5]) << 16 | (info16[6] << 8 | info16[7]));
	return (ok || fw_ok) ? 0 : -14;
}

static void dot(int x, int y, uint32_t color)
{
	for (int dy = -8; dy <= 8; dy++)
		for (int dx = -8; dx <= 8; dx++) {
			int px = x + dx, py = y + dy;
			if (px >= 0 && px < FB_W && py >= 0 && py < FB_H)
				con_fb[py * (FB_STRIDE / 4) + px] = color;
		}
}

static void nvt_touch_loop(unsigned seconds)
{
	con_str("TOUCH", "draw on the screen now!");
	uint64_t end = ticks() + (uint64_t)seconds * 19200000;
	unsigned events = 0, printed = 0;
	while (ticks() < end) {
		uint8_t pt[65];		/* Linux point[1..65]; its point[0] is the dummy byte */
		if (nvt_read(NVT_EVENT_BUF, pt, sizeof(pt)) != 0) {
			sleep_us(20000);
			continue;
		}
		for (int i = 0; i < 10; i++) {
			const uint8_t *q = pt + 6 * i;
			int id = q[0] >> 3, st = q[0] & 7;
			if (id == 0 || id > 10 || (st != 1 && st != 2))
				continue;
			int x = (q[1] << 4) | (q[3] >> 4), y = (q[2] << 4) | (q[3] & 0xf);
			if (x >= TOUCH_W || y >= TOUCH_H)
				continue;
			dot(x, y, id == 1 ? WHITE : (id == 2 ? GREEN : YELLOW));
			events++;
			if (printed < 6 && (events % 25) == 1) {
				con_hex2("TOUCH XY", "x=", x, "y=", y);
				printed++;
			}
		}
		sleep_us(8000);
	}
	con_hex("TOUCH EVENTS", events);
}

static void surya_touch_fw(uintptr_t dtb)
{
	uint64_t lo = 0, hi = 0;
	if (!fdt_chosen_u64(dtb, "linux,initrd-start", &lo) || !fdt_chosen_u64(dtb, "linux,initrd-end", &hi) || hi <= lo) {
		con_str("NVT FW", "no ramdisk: build with the firmware as ramdisk");
		return;
	}
	con_str("TOUCH", "step 3c: firmware download");
	/* eng_reset_idle */
	nvt_w8(NVT_ENG_RST, 0x5a);
	sleep_us(45000);
	int r = nvt_bootloader_reset();
	uint8_t id[8] = { 0 };
	nvt_read(0x1f64e, id, 6);
	con_hex2("NVT ID2", "rc=", (uint32_t)r, "b0-3=", be32(id));
	r = nvt_download((const uint8_t *)lo, (uint32_t)(hi - lo));
	con_hex("NVT DOWNLOAD rc", (uint32_t)r);
	if (r == 0)
		nvt_touch_loop(60);
}
#endif

void surya_touch_spi_test(uintptr_t dtb)
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
	con_hex2("NVT STATE", "fw_state=", st[0], "(a3=running)", st[0] == 0xa3);
#if SURYA_TOUCH_STEP >= 3
	surya_touch_fw(dtb);
#else
	(void)dtb;
#endif
}
#endif
