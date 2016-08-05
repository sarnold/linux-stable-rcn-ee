/*
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef _CCU_H_
#define _CCU_H_

struct device_node;

#define CCU_HW(_name, _parent, _ops, _flags)			\
	.hw.init = &(struct clk_init_data) {			\
		.flags		= _flags,			\
		.name		= _name,			\
		.parent_names	= (const char *[]) { _parent },	\
		.num_parents	= 1,				\
		.ops 		= _ops,				\
	}
#define CCU_HW_PARENTS(_name, _parents, _ops, _flags)		\
	.hw.init = &(struct clk_init_data) {			\
		.flags		= _flags,			\
		.name		= _name,			\
		.parent_names	= _parents,			\
		.num_parents	= ARRAY_SIZE(_parents),		\
		.ops 		= _ops,				\
	}

#define CCU_REG(_reg) .reg = _reg
#define CCU_RESET(_reg, _bit) .reset_reg = _reg, .reset_bit = _bit
#define CCU_BUS(_reg, _bit) .bus_reg = _reg, .bus_bit = _bit
#define CCU_GATE(_bit) .has_gate = 1, .gate_bit = _bit
#define CCU_LOCK(_reg, _bit) .lock_reg = _reg, .lock_bit = _bit
#define CCU_MUX(_shift, _width) .mux_shift = _shift, .mux_width = _width
#define CCU_N(_shift, _width) .n_shift = _shift, .n_width = _width
#define CCU_D1(_shift, _width) .d1_shift = _shift, .d1_width = _width
#define CCU_D2(_shift, _width) .p_shift = _shift, .p_width = _width
#define CCU_K(_shift, _width) .k_shift = _shift, .k_width = _width
#define CCU_M(_shift, _width) .m_shift = _shift, .m_width = _width
#define CCU_P(_shift, _width) .p_shift = _shift, .p_width = _width
#define CCU_UPD(_bit) .upd_bit = _bit
/* with ccu_fixed_factor_ops */
#define CCU_FIXED(_mul, _div) .n_width = _mul, .m_width = _div
/* with ccu_phase_ops */
#define CCU_PHASE(_shift, _width) .p_shift = _shift, .p_width = _width

#define CCU_FEATURE_MUX_VARIABLE_PREDIV	BIT(0)
#define CCU_FEATURE_MUX_FIXED_PREDIV	BIT(1)
#define CCU_FEATURE_FIXED_POSTDIV	BIT(2)
#define CCU_FEATURE_N0			BIT(3)
#define CCU_FEATURE_MODE_SELECT		BIT(4)
#define CCU_FEATURE_FLAT_FACTORS	BIT(5)
#define CCU_FEATURE_SET_RATE_GATE	BIT(6)
#define CCU_FEATURE_M_TABLE		BIT(7)

/* extra */
#define CCU_EXTRA_FRAC(_frac) .frac = _frac, .num_frac = ARRAY_SIZE(_frac)
#define CCU_EXTRA_POST_DIV(_div) .fixed_div[0] = _div

/* fractional values */
struct frac {
	unsigned long rate;
	u32 mask;
	u32 val;
};

/* extra features */
struct ccu_extra {
	const struct frac *frac; /* array - last is the fractional mask/value */
	u8 num_frac;

	u8 fixed_div[4];		/* index = parent */

	struct {
		u8 index;
		u8 shift;
		u8 width;
	} variable_prediv;

	struct {
		unsigned long rate;
		u8 bit;
	} mode_select;

	u8 m_table[8];
};

struct ccu {
	struct clk_hw hw;

	void __iomem *base;
	u16 reg;

	u16 reset_reg, bus_reg, lock_reg;
	u8  reset_bit, bus_bit, lock_bit;
	u8 has_gate, gate_bit;

	u8 mux_shift, mux_width;
	u8 n_shift, n_width, n_min;
	u8 d1_shift, d1_width;
//	u8 d2_shift, d2_width;		// d2 is p (never d2 + p)
	u8 k_shift, k_width;
	u8 m_shift, m_width;
	u8 p_shift, p_width;

	u8 upd_bit;

	u16 features;

	const struct ccu_extra *extra;
};

struct ccu_reset_map {
	u16	reg;
	u16	bit;
};

struct ccu_reset {
	void __iomem			*base;
	const struct ccu_reset_map	*reset_map;
	struct reset_controller_dev	rcdev;
};

extern const struct clk_ops ccu_fixed_factor_ops;
extern const struct clk_ops ccu_periph_ops;
extern const struct clk_ops ccu_pll_ops;
extern const struct clk_ops ccu_phase_ops;
extern const struct reset_control_ops ccu_reset_ops;

static inline struct ccu *hw2ccu(struct clk_hw *hw)
{
	return container_of(hw, struct ccu, hw);
}

int ccu_probe(struct device_node *node,
		struct clk_hw_onecell_data *data,
		struct ccu_reset *resets);

/* functions exported for specific features */
void ccu_set_field(struct ccu *ccu, int reg, u32 mask, u32 val);
int ccu_fixed_factor_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req);
unsigned long ccu_fixed_factor_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate);
int ccu_fixed_factor_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);

#endif /* _CCU_H_ */
