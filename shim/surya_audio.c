/*
 * surya audio bring-up, step A1: probe of the speaker amplifier path.
 *
 * POCO X3 NFC (surya), live DT (SURYA_NOTES.md "Audio plan"):
 *   two TI TAS256x smart amps (ti,tas256x; left 0x4d, right 0x4c) on
 *   i2c@0xa8c000 = QUP wrapper 1 SE3 (global SE9), pins gpio46/47 (qup13,
 *   NORTH tile), reset 1 = PM6150L GPIO8 (active low; DT default
 *   output-low), reset 2 = TLMM gpio91, irq gpio58 / gpio30. Audio data
 *   comes from LPASS tertiary MI2S (gpio53-56), not touched here.
 *
 * Reads: GCC clock state, TLMM pin config, PM6150L GPIO8 over the SPMI
 * observer channel (EXT_READL, like the kernel's sweet_spmi.c).
 * Writes, all undone by a reset and none on a PMIC or amplifier register:
 * GCC votes for QUP1 + SE3, SE3 RCG to XO/1 unless DFS owns it, gpio46/47
 * muxed to qup13, SE3 GENI I2C setup (100 kHz). On the amps: only I2C
 * register-pointer writes followed by reads (page 0, 0x00-0x07 and 0x7d).
 * Same GENI I2C sequence as upstream's proven sweet touch driver (patch
 * 0060/0063, QUP1 SE1). Built only with SURYA_AUDIO=1 (-DSURYA_AUDIO_PROBE).
 */
#include <stdint.h>
#include "console.h"
#include "surya_audio.h"

#define GCC			0x00100000UL
#define GCC_VOTE		0x5200cUL
#define VOTE_QUP1_SE3		((0x0fU << 18) | (1U << 25))	/* core2x, core, m/s AHB, S3 */
#define GCC_SE3_RCG		0x183a8UL
#define CBCR_OFF		(1U << 31)
static const uint32_t qup1_cbcr[] = { 0x18004, 0x18008, 0x1800c, 0x18010, 0x183a4 };

#define TLMM_NORTH		0x03900000UL
#define TLMM_WEST		0x03500000UL
#define TLMM_SOUTH		0x03d00000UL
#define CFG(t, n)		((t) + (uintptr_t)(n) * 0x1000)

#define SE			0x00a8c000UL	/* QUP1 SE3 */
#define SE_STATUS		0x040
#define SE_MCLK			0x048
#define SE_IF_DIS		0x064
#define SE_FW			0x068
#define SE_CLK_SEL		0x07c
#define SE_DMA			0x258
#define SE_TX_LEN		0x26c
#define SE_RX_LEN		0x270
#define SE_SCL			0x278
#define SE_CMD			0x600
#define SE_CTRL			0x604
#define SE_IRQ			0x610
#define SE_IRQ_EN		0x614
#define SE_IRQ_CLR		0x618
#define SE_TX			0x700
#define SE_RX			0x780
#define SE_TX_WM		0x80c
#define SE_TX_COUNT		0x800
#define SE_RX_COUNT		0x804
#define SE_HW_TX		0xe24
#define SE_HW_RX		0xe28
#define SE_DONE			(1U << 0)
#define SE_NACK			(1U << 10)
#define SE_ERRORS		(0x3eU | (1U << 9) | (1U << 10) | (1U << 12) | \
				 (1U << 13) | (3U << 24) | (3U << 28))
#define SE_TX_WM_IRQ		(1U << 30)
#define SE_PACK0		0x0007f8feU
#define SE_PACK1		0x000ffefeU

/* SPMI arbiter v5 (live DT: core 0x0c440000, obsrvr 0x0e600000, cnfg 0x0c40a000) */
#define SP_CORE			0x0c440000UL
#define SP_OBS			0x0e600000UL

static inline uint32_t rd(uintptr_t a)
{
	uint32_t v = *(volatile uint32_t *)a;
	__asm__ volatile("dmb oshld" ::: "memory");
	return v;
}

static inline void wr(uintptr_t a, uint32_t v)
{
	__asm__ volatile("dmb oshst" ::: "memory");
	*(volatile uint32_t *)a = v;
	__asm__ volatile("dsb osh" ::: "memory");
}

static uint64_t ticks(void)
{
	uint64_t t;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(t));
	return t;
}

static void udelay(unsigned us)
{
	uint64_t end = ticks() + (uint64_t)us * 192 / 10;
	while (ticks() < end)
		;
}

static int wait_clear(uintptr_t a, uint32_t mask)
{
	for (unsigned i = 0; i < 10000; i++) {
		if (!(rd(a) & mask))
			return 1;
		udelay(1);
	}
	return 0;
}

/* ---- SPMI observer read (one byte), PM6150L GPIO8 only ---------------- */
static int spmi_read(unsigned sid, unsigned addr, uint8_t *out)
{
	unsigned ppid = (sid << 8) | (addr >> 8);
	unsigned count = rd(SP_CORE + 4) & 0x7ff;
	unsigned apid = 0xffff;
	if (count == 0 || count > 512)
		return -1;
	for (unsigned i = 0; i < count; i++) {
		uint32_t map = rd(SP_CORE + 0x900 + 4 * i);
		if (map && ((map >> 8) & 0xfff) == ppid) {
			apid = i;
			break;
		}
	}
	if (apid == 0xffff)
		return -2;
	uintptr_t ch = SP_OBS + 0x80UL * apid;
	wr(ch, (1U << 27) | ((addr & 0xff) << 4));	/* EXT_READL, 1 byte */
	for (unsigned i = 0; i < 1000; i++) {
		uint32_t st = rd(ch + 8);
		if (st & 1) {
			if (st & 0xe)
				return -3;
			*out = (uint8_t)rd(ch + 0x18);
			return 0;
		}
		udelay(1);
	}
	return -4;
}

static void pmic_gpio8(void)
{
	static const uint8_t regs[] = { 0x04, 0x05, 0x08, 0x10, 0x40, 0x41, 0x42, 0x44, 0x45, 0x46 };
	uint8_t v[sizeof(regs)];
	int err = 0;
	for (unsigned i = 0; i < sizeof(regs); i++) {
		v[i] = 0xee;
		int e = spmi_read(4, 0xc700 + regs[i], &v[i]);
		if (e && !err)
			err = e;
	}
	if (err) {
		con_hex("PM6150L GPIO8 err", (uint32_t)-err);
		return;
	}
	/* type/subtype, status1, int_rt_sts | mode, vin, pull | out_src, out_ctl, en */
	con_hex2("PMIC GPIO8 A", "t/st/s1/rt=",
	    ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) | ((uint32_t)v[2] << 8) | v[3],
	    "mode/vin/pull=", ((uint32_t)v[4] << 16) | ((uint32_t)v[5] << 8) | v[6]);
	con_hex("PMIC GPIO8 B src/out/en", ((uint32_t)v[7] << 16) | ((uint32_t)v[8] << 8) | v[9]);
}

/* ---- GENI I2C (copy of upstream sweet_touch.c touch_msg_run, polled) --- */
static int i2c_dead;

static void i2c_abort(void)
{
	wr(SE + SE_CTRL, 2);
	for (unsigned i = 0; i < 5000 && !(rd(SE + SE_IRQ) & (1U << 5)); i++)
		udelay(1);
	if (!wait_clear(SE + SE_STATUS, 1))
		i2c_dead = 1;
	for (unsigned i = 0; i < 64 && (rd(SE + SE_RX_COUNT) & 0x1ffffff); i++)
		(void)rd(SE + SE_RX);
	wr(SE + SE_IRQ_CLR, 0xffffffffU);
}

/* One message: TX (<= 3 bytes) or RX (<= 16 bytes). 0, or -1 NACK, -2 error. */
static int i2c_msg(uint8_t addr, const uint8_t *tx, unsigned nt, uint8_t *rx,
    unsigned nr, int stretch)
{
	unsigned sent = 0, got = 0;
	if (i2c_dead || nt > 3 || nr > 16 || (nt && nr) || (!nt && !nr))
		return -2;
	if (!wait_clear(SE + SE_STATUS, 1)) {
		i2c_dead = 1;
		return -2;
	}
	wr(SE + SE_IRQ_CLR, 0xffffffffU);
	wr(SE + SE_TX_WM, nt ? 1 : 0);
	wr(SE + (nr ? SE_RX_LEN : SE_TX_LEN), nr ? nr : nt);
	wr(SE + SE_CMD, ((nr ? 2U : 1U) << 27) | ((uint32_t)addr << 9) | (stretch ? 4U : 0U));
	for (unsigned i = 0; i < 20000; i++) {
		uint32_t irq = rd(SE + SE_IRQ);
		if (irq & SE_ERRORS) {
			int err = (irq & SE_NACK) ? -1 : -2;
			for (unsigned j = 0; j < 5000 && !(irq & SE_DONE); j++) {
				udelay(1);
				irq = rd(SE + SE_IRQ);
			}
			if ((irq & SE_DONE) && !(rd(SE + SE_STATUS) & 1) &&
			    !(rd(SE + SE_TX_COUNT) & 0xfffffff) && !(rd(SE + SE_RX_COUNT) & 0x1ffffff)) {
				wr(SE + SE_IRQ_CLR, irq);
				return err;
			}
			break;
		}
		if (nt && !sent && (irq & SE_TX_WM_IRQ)) {
			uint32_t w = 0;
			for (unsigned j = 0; j < nt; j++)
				w |= (uint32_t)tx[j] << (8 * j);
			wr(SE + SE_TX, w);
			sent = nt;
			wr(SE + SE_TX_WM, 0);
		}
		uint32_t count = rd(SE + SE_RX_COUNT) & 0x1ffffff;
		if (count > 64 || count > (nr - got + 3) / 4)
			break;
		while (count--) {
			uint32_t w = rd(SE + SE_RX);
			for (unsigned j = 0; j < 4 && got < nr; j++)
				rx[got++] = (uint8_t)(w >> (8 * j));
		}
		if (irq & SE_DONE) {
			irq = rd(SE + SE_IRQ);
			if (irq & SE_ERRORS)
				continue;
			wr(SE + SE_IRQ_CLR, irq);
			return (got == nr && sent == nt) ? 0 : -2;
		}
		wr(SE + SE_IRQ_CLR, irq & ((1U << 26) | (1U << 27) | SE_TX_WM_IRQ));
		udelay(1);
	}
	i2c_abort();
	return -2;
}

static int i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, unsigned len)
{
	int e = i2c_msg(addr, &reg, 1, 0, 0, 1);
	if (e)
		return e;
	return i2c_msg(addr, 0, 0, buf, len, 0);
}

static int i2c_setup(void)
{
	wr(GCC + GCC_VOTE, rd(GCC + GCC_VOTE) | VOTE_QUP1_SE3);
	uint32_t dfs = rd(GCC + GCC_SE3_RCG + 0x14), cfg = rd(GCC + GCC_SE3_RCG + 4);
	con_hex2("SE3 RCG", "cfg=", cfg, "dfsr=", dfs);
	if (dfs & 1) {
		if ((rd(GCC + GCC_SE3_RCG + 0x1c) & 0x371f) != 1) {
			con_str("AUDIO", "SE3 DFS level 0 is not XO/1: stop");
			return 0;
		}
	} else {
		wr(GCC + GCC_SE3_RCG + 4, (cfg & ~(0x371fU | (1U << 20))) | 1U);
		wr(GCC + GCC_SE3_RCG, rd(GCC + GCC_SE3_RCG) | 1U);
		if (!wait_clear(GCC + GCC_SE3_RCG, 1)) {
			con_str("AUDIO", "SE3 RCG update timeout: stop");
			return 0;
		}
	}
	for (unsigned i = 0; i < sizeof(qup1_cbcr) / sizeof(qup1_cbcr[0]); i++) {
		if (!wait_clear(GCC + qup1_cbcr[i], CBCR_OFF)) {
			con_hex2("AUDIO CBCR off", "reg=", qup1_cbcr[i], "val=", rd(GCC + qup1_cbcr[i]));
			return 0;
		}
	}
	uint32_t fw = rd(SE + SE_FW), tx = rd(SE + SE_HW_TX), rx = rd(SE + SE_HW_RX);
	con_hex2("SE3", "fw=", fw, "status=", rd(SE + SE_STATUS));
	con_hex2("SE3 HW", "tx=", tx, "rx=", rx);
	if (((fw >> 8) & 0xff) != 3 || (rd(SE + SE_IF_DIS) & 1)) {
		con_str("AUDIO", "SE3 has no I2C firmware loaded: stop");
		return 0;
	}
	if (rd(SE + SE_STATUS) & 0x1001) {
		con_str("AUDIO", "SE3 busy: stop");
		return 0;
	}
	/* pins: qup13 = function 1, 2 mA, no pull (DT qupv3_se9_i2c_active) */
	for (unsigned n = 46; n <= 47; n++) {
		uintptr_t c = CFG(TLMM_NORTH, n);
		wr(c, (rd(c) & ~0x3ffU) | (1U << 2));
	}
	wr(SE + 0xe18, 0);
	wr(SE + 0xe1c, 0);
	wr(SE + 0x644, 0);
	wr(SE + SE_IRQ_CLR, 0xffffffffU);
	wr(SE + 0x648, 0xffffffffU);
	wr(SE + SE_DMA, 0);
	wr(SE + 0x28, rd(SE + 0x28) | 0x7f);
	wr(SE + 0xe30, rd(SE + 0xe30) | 0x0f);
	wr(SE + 0x24, 0x7f);
	wr(SE + 0x20, 1);
	wr(SE + SE_CLK_SEL, 0);
	wr(SE + SE_MCLK, (7U << 4) | 1U);
	wr(SE + SE_SCL, (10U << 20) | (11U << 10) | 26U);	/* 100 kHz */
	wr(SE + 0x254, 0);
	wr(SE + 0x260, SE_PACK0);
	wr(SE + 0x264, SE_PACK1);
	wr(SE + 0x284, SE_PACK0);
	wr(SE + 0x288, SE_PACK1);
	wr(SE + SE_TX_WM, 0);
	wr(SE + 0x810, 0);
	wr(SE + 0x814, ((rx >> 16) & 0x3f) - 1);
	wr(SE + SE_IRQ_EN, SE_DONE | SE_ERRORS | (1U << 26) | (1U << 27) | SE_TX_WM_IRQ);
	if (rd(SE + 0x260) != SE_PACK0 || (rd(SE + SE_TX_COUNT) & 0xfffffff) ||
	    (rd(SE + SE_RX_COUNT) & 0x1ffffff)) {
		con_str("AUDIO", "SE3 packing/FIFO check failed: stop");
		return 0;
	}
	return 1;
}

static void amp_probe(uint8_t addr, const char *label)
{
	uint8_t r[8] = {0}, id = 0;
	int e = i2c_read(addr, 0x00, r, 8);
	if (e) {
		con_hex2(label, "addr=", addr, e == -1 ? "NACK err=" : "ERROR err=", (uint32_t)-e);
		return;
	}
	int e2 = i2c_read(addr, 0x7d, &id, 1);
	con_hex2(label, "r00-03=", ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) | ((uint32_t)r[2] << 8) | r[3],
	    "r04-07=", ((uint32_t)r[4] << 24) | ((uint32_t)r[5] << 16) | ((uint32_t)r[6] << 8) | r[7]);
	con_hex2(label, "rev_id(7d)=", id, "err=", (uint32_t)-e2);
}

void surya_audio_probe(void)
{
	con_str("AUDIO", "surya step A1: speaker amp probe");
	uint32_t vote = rd(GCC + GCC_VOTE);
	con_hex2("GCC VOTE", "5200c=", vote, "qup1 18-21,25=", ((vote >> 18) & 0xf) | (((vote >> 25) & 1) << 4));
	con_hex2("GCC QUP1", "m_ahb=", rd(GCC + 0x1800c), "se3=", rd(GCC + 0x183a4));
	con_hex2("TLMM 46/47", "cfg46=", rd(CFG(TLMM_NORTH, 46)), "cfg47=", rd(CFG(TLMM_NORTH, 47)));
	con_hex2("TLMM 91 RST2", "cfg=", rd(CFG(TLMM_NORTH, 91)), "io=", rd(CFG(TLMM_NORTH, 91) + 4));
	con_hex2("TLMM 58/30 IRQ", "io58=", rd(CFG(TLMM_WEST, 58) + 4), "io30=", rd(CFG(TLMM_SOUTH, 30) + 4));
	con_hex2("TLMM 53/56 MI2S", "cfg53=", rd(CFG(TLMM_WEST, 53)), "cfg56=", rd(CFG(TLMM_WEST, 56)));
	pmic_gpio8();
	if (!i2c_setup())
		return;
	amp_probe(0x4c, "AMP 4C (right)");
	amp_probe(0x4d, "AMP 4D (left)");
	con_str("AUDIO", "step A1 done");
}
