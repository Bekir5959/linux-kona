// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2014 Broadcom Corporation
 * Copyright 2014 Linaro Limited
 */

#include "clk-kona.h"
#include "dt-bindings/clock/bcm21664.h"

#define BCM21664_CCU_COMMON(_name, _capname) \
	KONA_CCU_COMMON(BCM21664, _name, _capname)

/* Root CCU */

static struct peri_clk_data frac_1m_data = {
	.gate		= HW_SW_GATE(0x214, 16, 0, 1),
	.clocks		= CLOCKS("ref_crystal"),
};

#define BCM21664_ROOT_CCU_CLK_COUNT	(BCM21664_ROOT_CCU_FRAC_1M + 1)

static struct ccu_data root_ccu_data = {
	BCM21664_CCU_COMMON(root, ROOT),
	/* no policy control */
	.kona_clks	= {
		[BCM21664_ROOT_CCU_FRAC_1M] =
			KONA_CLK(root, frac_1m, peri),
		[BCM21664_ROOT_CCU_CLK_COUNT] = LAST_KONA_CLK,
	},
};

/* AON CCU */

static struct peri_clk_data hub_timer_data = {
	.policy		= POLICY(0x0010, 6),
	.gate		= HW_SW_GATE(0x0414, 16, 0, 1),
	.hyst		= HYST(0x0414, 8, 9),
	.clocks		= CLOCKS("bbl_32k",
				 "frac_1m",
				 "dft_19_5m"),
	.sel		= SELECTOR(0x0a10, 0, 2),
	.trig		= TRIGGER(0x0a40, 4),
};

static struct bus_clk_data hub_timer_apb_data = {
	.policy		= POLICY(0x0010, 6),
	.gate		= HW_SW_GATE(0x0414, 18, 3, 2),
	.hyst		= HYST(0x0414, 10, 11),
};

static struct peri_clk_data pmu_bsc_data = {
	.policy		= POLICY(0x0010, 8),
	.gate		= HW_SW_GATE(0x0418, 16, 0, 1),
	.hyst		= HYST(0x0418, 8, 9),
	.clocks		= CLOCKS("ref_crystal",
				 "pmu_bsc_var",
				 "bbl_32k"),
	.sel		= SELECTOR(0x0a04, 0, 2),
	.div		= DIVIDER(0x0a04, 3, 4),
	.trig		= TRIGGER(0x0a40, 0),
};

static struct bus_clk_data pmu_bsc_apb_data = {
	.policy		= POLICY(0x0010, 8),
	.gate		= HW_SW_GATE(0x0418, 18, 2, 3),
	.hyst		= HYST(0x0418, 10, 11),
};

#define BCM21664_AON_CCU_CLK_COUNT	(BCM21664_AON_CCU_PMU_BSC_APB + 1)

static struct ccu_data aon_ccu_data = {
	BCM21664_CCU_COMMON(aon, AON),
	.policy		= {
		.enable		= CCU_LVM_EN(0x0034, 0),
		.control	= CCU_POLICY_CTL(0x000c, 0, 1, 2),
	},
	.kona_clks	= {
		[BCM21664_AON_CCU_HUB_TIMER] =
			KONA_CLK(aon, hub_timer, peri),
		[BCM21664_AON_CCU_HUB_TIMER_APB] =
			KONA_CLK(aon, hub_timer_apb, bus),
		[BCM21664_AON_CCU_PMU_BSC] =
			KONA_CLK(aon, pmu_bsc, peri),
		[BCM21664_AON_CCU_PMU_BSC_APB] =
			KONA_CLK(aon, pmu_bsc_apb, bus),
		[BCM21664_AON_CCU_CLK_COUNT] = LAST_KONA_CLK,
	},
};

/* Master CCU */

static struct peri_clk_data sdio1_data = {
	.policy		= POLICY(0x0010, 6),
	.gate		= HW_SW_GATE(0x0358, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_52m",
				 "ref_52m",
				 "var_96m",
				 "ref_96m"),
	.sel		= SELECTOR(0x0a28, 0, 3),
	.div		= DIVIDER(0x0a28, 4, 14),
	.trig		= TRIGGER(0x0afc, 9),
};

static struct peri_clk_data sdio2_data = {
	.policy		= POLICY(0x0010, 5),
	.gate		= HW_SW_GATE(0x035c, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_52m",
				 "ref_52m",
				 "var_96m",
				 "ref_96m"),
	.sel		= SELECTOR(0x0a2c, 0, 3),
	.div		= DIVIDER(0x0a2c, 4, 14),
	.trig		= TRIGGER(0x0afc, 10),
};

static struct peri_clk_data sdio3_data = {
	.policy		= POLICY(0x0010, 3),
	.gate		= HW_SW_GATE(0x0364, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_52m",
				 "ref_52m",
				 "var_96m",
				 "ref_96m"),
	.sel		= SELECTOR(0x0a34, 0, 3),
	.div		= DIVIDER(0x0a34, 4, 14),
	.trig		= TRIGGER(0x0afc, 12),
};

static struct peri_clk_data sdio4_data = {
	.policy		= POLICY(0x0010, 4),
	.gate		= HW_SW_GATE(0x0360, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_52m",
				 "ref_52m",
				 "var_96m",
				 "ref_96m"),
	.sel		= SELECTOR(0x0a30, 0, 3),
	.div		= DIVIDER(0x0a30, 4, 14),
	.trig		= TRIGGER(0x0afc, 11),
};

static struct peri_clk_data sdio1_sleep_data = {
	.policy		= POLICY(0x0010, 6),
	.clocks		= CLOCKS("ref_32k"),	/* Verify */
	.gate		= HW_SW_GATE(0x0358, 18, 2, 3),
};

static struct peri_clk_data sdio2_sleep_data = {
	.policy		= POLICY(0x0010, 5),
	.clocks		= CLOCKS("ref_32k"),	/* Verify */
	.gate		= HW_SW_GATE(0x035c, 18, 2, 3),
};

static struct peri_clk_data sdio3_sleep_data = {
	.policy		= POLICY(0x0010, 3),
	.clocks		= CLOCKS("ref_32k"),	/* Verify */
	.gate		= HW_SW_GATE(0x0364, 18, 2, 3),
};

static struct peri_clk_data sdio4_sleep_data = {
	.policy		= POLICY(0x0010, 4),
	.clocks		= CLOCKS("ref_32k"),	/* Verify */
	.gate		= HW_SW_GATE(0x0360, 18, 2, 3),
};

static struct bus_clk_data sdio1_ahb_data = {
	.policy		= POLICY(0x0010, 6),
	.gate		= HW_SW_GATE(0x0358, 16, 0, 1),
};

static struct bus_clk_data sdio2_ahb_data = {
	.policy		= POLICY(0x0010, 5),
	.gate		= HW_SW_GATE(0x035c, 16, 0, 1),
};

static struct bus_clk_data sdio3_ahb_data = {
	.policy		= POLICY(0x0010, 3),
	.gate		= HW_SW_GATE(0x0364, 16, 0, 1),
};

static struct bus_clk_data sdio4_ahb_data = {
	.policy		= POLICY(0x0010, 4),
	.gate		= HW_SW_GATE(0x0360, 16, 0, 1),
};

static struct bus_clk_data usb_otg_ahb_data = {
	.policy		= POLICY(0x0010, 11),
	.gate		= HW_SW_GATE(0x0348, 16, 0, 1),
};

#define BCM21664_MASTER_CCU_CLK_COUNT	(BCM21664_MASTER_CCU_USB_OTG_AHB + 1)

static struct ccu_data master_ccu_data = {
	BCM21664_CCU_COMMON(master, MASTER),
	.policy		= {
		.enable		= CCU_LVM_EN(0x0034, 0),
		.control	= CCU_POLICY_CTL(0x000c, 0, 1, 2),
	},
	.kona_clks	= {
		[BCM21664_MASTER_CCU_SDIO1] =
			KONA_CLK(master, sdio1, peri),
		[BCM21664_MASTER_CCU_SDIO2] =
			KONA_CLK(master, sdio2, peri),
		[BCM21664_MASTER_CCU_SDIO3] =
			KONA_CLK(master, sdio3, peri),
		[BCM21664_MASTER_CCU_SDIO4] =
			KONA_CLK(master, sdio4, peri),
		[BCM21664_MASTER_CCU_SDIO1_SLEEP] =
			KONA_CLK(master, sdio1_sleep, peri),
		[BCM21664_MASTER_CCU_SDIO2_SLEEP] =
			KONA_CLK(master, sdio2_sleep, peri),
		[BCM21664_MASTER_CCU_SDIO3_SLEEP] =
			KONA_CLK(master, sdio3_sleep, peri),
		[BCM21664_MASTER_CCU_SDIO4_SLEEP] =
			KONA_CLK(master, sdio4_sleep, peri),
		[BCM21664_MASTER_CCU_SDIO1_AHB] =
			KONA_CLK(master, sdio1_ahb, bus),
		[BCM21664_MASTER_CCU_SDIO2_AHB] =
			KONA_CLK(master, sdio2_ahb, bus),
		[BCM21664_MASTER_CCU_SDIO3_AHB] =
			KONA_CLK(master, sdio3_ahb, bus),
		[BCM21664_MASTER_CCU_SDIO4_AHB] =
			KONA_CLK(master, sdio4_ahb, bus),
		[BCM21664_MASTER_CCU_USB_OTG_AHB] =
			KONA_CLK(master, usb_otg_ahb, bus),
		[BCM21664_MASTER_CCU_CLK_COUNT] = LAST_KONA_CLK,
	},
};

/* Slave CCU */

static struct peri_clk_data uartb_data = {
	.policy		= POLICY(0x0010, 20),
	.gate		= HW_SW_GATE(0x0400, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_156m",
				 "ref_156m"),
	.sel		= SELECTOR(0x0a10, 0, 2),
	.div		= FRAC_DIVIDER(0x0a10, 4, 12, 8),
	.trig		= TRIGGER(0x0afc, 2),
};

static struct peri_clk_data uartb2_data = {
	.policy		= POLICY(0x0010, 19),
	.gate		= HW_SW_GATE(0x0404, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_156m",
				 "ref_156m"),
	.sel		= SELECTOR(0x0a14, 0, 2),
	.div		= FRAC_DIVIDER(0x0a14, 4, 12, 8),
	.trig		= TRIGGER(0x0afc, 3),
};

static struct peri_clk_data uartb3_data = {
	.policy		= POLICY(0x0010, 18),
	.gate		= HW_SW_GATE(0x0408, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_156m",
				 "ref_156m"),
	.sel		= SELECTOR(0x0a18, 0, 2),
	.div		= FRAC_DIVIDER(0x0a18, 4, 12, 8),
	.trig		= TRIGGER(0x0afc, 4),
};

static struct peri_clk_data bsc1_data = {
	.policy		= POLICY(0x0010, 24),
	.gate		= HW_SW_GATE(0x0458, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_104m",
				 "ref_104m",
				 "var_13m",
				 "ref_13m"),
	.sel		= SELECTOR(0x0a64, 0, 3),
	.trig		= TRIGGER(0x0afc, 23),
};

static struct peri_clk_data bsc2_data = {
	.policy		= POLICY(0x0010, 23),
	.gate		= HW_SW_GATE(0x045c, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_104m",
				 "ref_104m",
				 "var_13m",
				 "ref_13m"),
	.sel		= SELECTOR(0x0a68, 0, 3),
	.trig		= TRIGGER(0x0afc, 24),
};

static struct peri_clk_data bsc3_data = {
	.policy		= POLICY(0x0010, 29),
	.gate		= HW_SW_GATE(0x0470, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_104m",
				 "ref_104m",
				 "var_13m",
				 "ref_13m"),
	.sel		= SELECTOR(0x0a7c, 0, 3),
	.trig		= TRIGGER(0x0afc, 18),
};

static struct peri_clk_data bsc4_data = {
	.policy		= POLICY(0x0010, 30),
	.gate		= HW_SW_GATE(0x0474, 18, 2, 3),
	.clocks		= CLOCKS("ref_crystal",
				 "var_104m",
				 "ref_104m",
				 "var_13m",
				 "ref_13m"),
	.sel		= SELECTOR(0x0a80, 0, 3),
	.trig		= TRIGGER(0x0afc, 19),
};

static struct bus_clk_data uartb_apb_data = {
	.policy		= POLICY(0x0010, 20),
	.gate		= HW_SW_GATE_AUTO(0x0400, 16, 0, 1),
};

static struct bus_clk_data uartb2_apb_data = {
	.policy		= POLICY(0x0010, 19),
	.gate		= HW_SW_GATE_AUTO(0x0404, 16, 0, 1),
};

static struct bus_clk_data uartb3_apb_data = {
	.policy		= POLICY(0x0010, 18),
	.gate		= HW_SW_GATE_AUTO(0x0408, 16, 0, 1),
};

static struct bus_clk_data bsc1_apb_data = {
	.policy		= POLICY(0x0010, 24),
	.gate		= HW_SW_GATE_AUTO(0x0458, 16, 0, 1),
	.hyst		= HYST(0x0458, 8, 9),
};

static struct bus_clk_data bsc2_apb_data = {
	.policy		= POLICY(0x0010, 23),
	.gate		= HW_SW_GATE_AUTO(0x045c, 16, 0, 1),
	.hyst		= HYST(0x045c, 8, 9),
};

static struct bus_clk_data bsc3_apb_data = {
	.policy		= POLICY(0x0010, 29),
	.gate		= HW_SW_GATE_AUTO(0x0470, 16, 0, 1),
	.hyst		= HYST(0x0470, 8, 9),
};

static struct bus_clk_data bsc4_apb_data = {
	.policy		= POLICY(0x0010, 30),
	.gate		= HW_SW_GATE_AUTO(0x0474, 16, 0, 1),
	.hyst		= HYST(0x0474, 8, 9),
};

#define BCM21664_SLAVE_CCU_CLK_COUNT	(BCM21664_SLAVE_CCU_BSC4_APB + 1)

static struct ccu_data slave_ccu_data = {
	BCM21664_CCU_COMMON(slave, SLAVE),
       .policy		= {
		.enable		= CCU_LVM_EN(0x0034, 0),
		.control	= CCU_POLICY_CTL(0x000c, 0, 1, 2),
	},
	.kona_clks	= {
		[BCM21664_SLAVE_CCU_UARTB] =
			KONA_CLK(slave, uartb, peri),
		[BCM21664_SLAVE_CCU_UARTB2] =
			KONA_CLK(slave, uartb2, peri),
		[BCM21664_SLAVE_CCU_UARTB3] =
			KONA_CLK(slave, uartb3, peri),
		[BCM21664_SLAVE_CCU_BSC1] =
			KONA_CLK(slave, bsc1, peri),
		[BCM21664_SLAVE_CCU_BSC2] =
			KONA_CLK(slave, bsc2, peri),
		[BCM21664_SLAVE_CCU_BSC3] =
			KONA_CLK(slave, bsc3, peri),
		[BCM21664_SLAVE_CCU_BSC4] =
			KONA_CLK(slave, bsc4, peri),
		[BCM21664_SLAVE_CCU_UARTB_APB] =
			KONA_CLK(slave, uartb_apb, bus),
		[BCM21664_SLAVE_CCU_UARTB2_APB] =
			KONA_CLK(slave, uartb2_apb, bus),
		[BCM21664_SLAVE_CCU_UARTB3_APB] =
			KONA_CLK(slave, uartb3_apb, bus),
		[BCM21664_SLAVE_CCU_BSC1_APB] =
			KONA_CLK(slave, bsc1_apb, bus),
		[BCM21664_SLAVE_CCU_BSC2_APB] =
			KONA_CLK(slave, bsc2_apb, bus),
		[BCM21664_SLAVE_CCU_BSC3_APB] =
			KONA_CLK(slave, bsc3_apb, bus),
		[BCM21664_SLAVE_CCU_BSC4_APB] =
			KONA_CLK(slave, bsc4_apb, bus),
		[BCM21664_SLAVE_CCU_CLK_COUNT] = LAST_KONA_CLK,
	},
};

/* MM CCU */

/*
 * The multimedia (MM) CCU lives at 0x3c000000 and gates the clocks for the
 * multimedia island (camera, DSI, V3D/GPU, ...).  Only the two clocks needed
 * to bring up the VideoCore-IV V3D block are modelled here:
 *
 *   mm_switch_axi  - the MM AXI switch clock, parent of all MM AXI clients
 *   v3d_axi        - the V3D block's AXI interface clock
 *
 * Register offsets, gate/policy bit positions and the source-select encoding
 * were taken from the downstream Samsung "java" (BCM23550) kernel
 * (arch/arm/mach-java/clock.c and .../rdb/brcm_rdb_mm_clk_mgr_reg.h) and
 * cross-checked against the identical mach-hawaii (BCM21664) RDB.  The CCU
 * register block layout (WR_ACCESS, POLICY, LVM_EN) is the standard Kona CCU
 * layout, identical to the master/slave CCUs above.
 *
 * NOTE: The whole MM island must be powered (MM power domain) for any of
 * these registers to respond; that power domain is not yet modelled in
 * mainline.  See the V3D device tree node for how this is wired to runtime PM.
 */

/*
 * NB: no .policy on these clocks (and none on the CCU below).  The MM CCU
 * policy engine is coupled to the power-island DFS/voltage tables, which are
 * not modelled here; driving it (stop/start LVM_EN + POLICY_CTL) times out
 * ("mm_ccu policy engine never started").  The bootloader already programs
 * the MM POLICY*_MASK bits (V3D + MM_SWITCH), so clk-kona can gate these
 * clocks directly, which was verified on hardware (writing V3D_CLKGATE
 * EN=1/SEL=1 with the policy engine untouched brings V3D up and IDENT0 reads
 * 0x02443356).  This mirrors root_ccu, which likewise has no policy control.
 */
static struct peri_clk_data mm_switch_axi_data = {
	.gate		= HW_SW_GATE(0x0200, 16, 0, 1),	/* MM_AXI_SWITCH_CLKGATE */
	.hyst		= HYST(0x0200, 8, 9),
	/*
	 * Gate-only: the selector/divider are deliberately NOT modelled.
	 * Committing a divider change goes through DIV_TRIG (0x0afc) via
	 * __clk_trigger(), which - exactly like the policy engine - waits on a
	 * status bit that never clears without the MM DFS/voltage context we
	 * don't set up, and would time out ("error initializing divider").
	 * We leave source-select and divider at the bootloader's values
	 * (var_312m, confirmed on hardware via AXI_DIV PLL_SELECT=2) and only
	 * gate the clock.  Consequence: the rate reported to the framework is
	 * var_312m undivided - cosmetic; V3D bring-up doesn't depend on it.
	 * TODO: model the divider once the MM CCU DFS/trigger path is understood.
	 */
	.clocks		= CLOCKS("var_312m"),
};

static struct bus_clk_data v3d_axi_data = {
	.gate		= HW_SW_GATE(0x0270, 16, 0, 1),	/* V3D_CLKGATE */
	.hyst		= HYST(0x0270, 8, 9),
	.clocks		= CLOCKS("mm_switch_axi"),
	/*
	 * TODO: V3D also has an AXI-level soft reset in the MM reset manager
	 * (MM_RST base 0x3c000f00, SOFT_RSTN0 offset 0x04, V3D_SOFT_RSTN bit 5,
	 * mask 0x20).  Not modelled here - if V3D comes up wedged, that reset
	 * likely needs to be deasserted via a reset-controller before enable.
	 */
};

#define BCM21664_MM_CCU_CLK_COUNT	(BCM21664_MM_CCU_V3D_AXI + 1)

static struct ccu_data mm_ccu_data = {
	BCM21664_CCU_COMMON(mm, MM),
	/* no policy control - see note above mm_switch_axi_data */
	.kona_clks	= {
		[BCM21664_MM_CCU_MM_SWITCH_AXI] =
			KONA_CLK(mm, mm_switch_axi, peri),
		[BCM21664_MM_CCU_V3D_AXI] =
			KONA_CLK(mm, v3d_axi, bus),
		[BCM21664_MM_CCU_CLK_COUNT] = LAST_KONA_CLK,
	},
};

/* Device tree match table callback functions */

static void __init kona_dt_root_ccu_setup(struct device_node *node)
{
	kona_dt_ccu_setup(&root_ccu_data, node);
}

static void __init kona_dt_aon_ccu_setup(struct device_node *node)
{
	kona_dt_ccu_setup(&aon_ccu_data, node);
}

static void __init kona_dt_master_ccu_setup(struct device_node *node)
{
	kona_dt_ccu_setup(&master_ccu_data, node);
}

static void __init kona_dt_slave_ccu_setup(struct device_node *node)
{
	kona_dt_ccu_setup(&slave_ccu_data, node);
}

static void __init kona_dt_mm_ccu_setup(struct device_node *node)
{
	kona_dt_ccu_setup(&mm_ccu_data, node);
}

CLK_OF_DECLARE(bcm21664_root_ccu, BCM21664_DT_ROOT_CCU_COMPAT,
			kona_dt_root_ccu_setup);
CLK_OF_DECLARE(bcm21664_aon_ccu, BCM21664_DT_AON_CCU_COMPAT,
			kona_dt_aon_ccu_setup);
CLK_OF_DECLARE(bcm21664_master_ccu, BCM21664_DT_MASTER_CCU_COMPAT,
			kona_dt_master_ccu_setup);
CLK_OF_DECLARE(bcm21664_slave_ccu, BCM21664_DT_SLAVE_CCU_COMPAT,
			kona_dt_slave_ccu_setup);
CLK_OF_DECLARE(bcm21664_mm_ccu, BCM21664_DT_MM_CCU_COMPAT,
			kona_dt_mm_ccu_setup);
