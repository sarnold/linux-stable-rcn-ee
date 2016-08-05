/*
 * Allwinner system CCU
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Rewrite from 'sunxi-ng':
 * Copyright (C) 2016 Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/reset-controller.h>
#include <linux/iopoll.h>
#include <linux/slab.h>
#include <linux/rational.h>
#include <linux/of_address.h>

#include "ccu.h"

#define CCU_DEBUG 1

#if CCU_DEBUG
#define ccu_dbg(fmt, ...) pr_info("*ccu* " fmt, ##__VA_ARGS__)
#else
#define ccu_dbg(fmt, ...)
#endif

#define CCU_MASK(shift, width) (((1 << width) - 1) << shift)

/*
 * factors:
 *	n: multiplier (PLL)
 *	d1, d2: boolean dividers by 2 (d2 is p with 1 bit width - PLL)
 *	k: multiplier (PLL)
 *	m: divider
 *	p: divider by power of 2
 */
struct val_pll {
	int n, d1, k, m, p;
};
struct val_periph {
	int m, p;
};

static DEFINE_SPINLOCK(ccu_lock);

void ccu_set_field(struct ccu *ccu, int reg, u32 mask, u32 val)
{
	ccu_dbg("%s set %03x %08x\n",
		clk_hw_get_name(&ccu->hw), reg,
		(readl(ccu->base + reg) & ~mask) | val);

	spin_lock(&ccu_lock);
	writel((readl(ccu->base + reg) & ~mask) | val, ccu->base + reg);
	spin_unlock(&ccu_lock);
}

/* --- enable / disable --- */
static int ccu_enable(struct clk_hw *hw)
{
	struct ccu *ccu = hw2ccu(hw);

	ccu_dbg("%s enable\n", clk_hw_get_name(&ccu->hw));

	if (ccu->reset_reg)
		ccu_set_field(ccu, ccu->reset_reg, BIT(ccu->reset_bit),
						BIT(ccu->reset_bit));
	if (ccu->bus_reg)
		ccu_set_field(ccu, ccu->bus_reg, BIT(ccu->bus_bit),
						BIT(ccu->bus_bit));
	if (ccu->has_gate)
		ccu_set_field(ccu, ccu->reg, BIT(ccu->gate_bit),
						BIT(ccu->gate_bit));

	return 0;
}

void ccu_disable(struct clk_hw *hw)
{
	struct ccu *ccu = hw2ccu(hw);

	ccu_dbg("%s disable\n", clk_hw_get_name(&ccu->hw));

	if (ccu->has_gate)
		ccu_set_field(ccu, ccu->reg, BIT(ccu->gate_bit), 0);
	if (ccu->bus_reg)
		ccu_set_field(ccu, ccu->bus_reg, BIT(ccu->bus_bit), 0);
	if (ccu->reset_reg)
		ccu_set_field(ccu, ccu->reset_reg, BIT(ccu->reset_bit), 0);
}

/* --- PLL --- */
static void ccu_wait_pll_stable(struct ccu *ccu)
{
	u32 lock, reg;

	if (ccu->lock_reg) {
		lock = BIT(ccu->lock_bit);
		WARN_ON(readl_relaxed_poll_timeout(ccu->base + ccu->lock_reg,
							reg, !(reg & lock),
							100, 70000));
	} else {
//		udelay(500);
		usleep_range(500, 700);
	}
}

static unsigned long ccu_pll_find_best(struct ccu *ccu,
					unsigned long rate,
					unsigned long parent_rate,
					struct val_pll *p_v)
{
	long new_rate, best_rate = 0;
	long delta, best_delta = 1000000;
	unsigned long lmul, ldiv;
	int n, k, m, p, tmp;
	int n_max = 1 << ccu->n_width;
	int d1_max = 1 << ccu->d1_width;
	int k_max = 1 << ccu->k_width;
	int m_max = 1 << ccu->m_width;
	int p_max = (1 << ccu->p_width) - 1;

	/* set the default values */
	p_v->d1 = 1;
	p_v->m = 1;
	p_v->k = 1;
	p_v->p = 0;

	if (ccu->features & CCU_FEATURE_N0)
		n_max--;

	/* no divider: parent_rate * n * k */
	if (d1_max == 1 && m_max == 1 && p_max == 0) {
		for (k = k_max; k > 0; k--) {
			int mul;

			mul = rate / parent_rate / k;
			for (n = mul; n <= mul + 1; n++) {
				if (n < ccu->n_min)
					continue;
				if (n > n_max)
					break;
				new_rate = parent_rate * n * k;
				delta = rate - new_rate;
				if (delta == 0) {
					p_v->n = n;
					p_v->k = k;
					return new_rate;
				}
				if (delta < 0)
					delta = -delta;
				if (delta < best_delta) {
					best_delta = delta;
					best_rate = new_rate;
					p_v->n = n;
					p_v->k = k;
				}
			}
		}
		return best_rate;
	}

	/* only one multiplier: parent_rate / d1 * n / m >> p */
	if (k_max == 1) {
		for (p = 0; p <= p_max + d1_max - 1; p++) {
			if (m_max > 1) {
				rational_best_approximation(rate, parent_rate,
							n_max - 1,
							m_max - 1,
							&lmul, &ldiv);
				if (ldiv == 0)
					continue;
				n = lmul;
				m = ldiv;
			} else {
				n = rate / (parent_rate >> p);
				if (n > n_max)
					continue;
				m = 1;
			}
			if (n < ccu->n_min) {
				tmp = (ccu->n_min + n - 1) / n;
				n *= tmp;
				m *= tmp;
				if (n > n_max || m > m_max)
					continue;
			}
			new_rate = parent_rate * n / m >> p;
			delta = rate - new_rate;
			if (delta == 0) {
				if (p > 1 && d1_max > 1) {
					p--;
					p_v->d1 = 2;
				} else {
					p_v->d1 = 1;
				}
				p_v->n = n;
				p_v->m = m;
				p_v->p = p;
				return new_rate;
			}
			if (delta < 0)
				delta = -delta;
			if (delta < best_delta) {
				best_delta = delta;
				best_rate = new_rate;
				p_v->n = n;
				p_v->m = m;
				if (p > 1 && d1_max > 1) {
					p--;
					p_v->d1 = 2;
				} else {
					p_v->d1 = 1;
				}
				p_v->p = p;
			}
		}
		return best_rate;
	}

	/* general case: parent * n / m * k >> p */
	for (p = 0; p <= p_max; p++) {
		for (k = k_max; k > 0; k--) {
			if (m_max > 1) {
				rational_best_approximation(rate, parent_rate,
							n_max - 1,
							m_max - 1,
							&lmul, &ldiv);
				if (ldiv == 0)
					continue;
				n = lmul;
				m = ldiv;
			} else {
				n = rate / k / (parent_rate >> p);
				if (n > n_max)
					continue;
				m = 1;
			}
			if (n < ccu->n_min) {
				tmp = (ccu->n_min + n - 1) / n;
				n *= tmp;
				m *= tmp;
				if (n > n_max || m > m_max)
					continue;
			}
			new_rate = parent_rate * n / m * k >> p;
			delta = rate - new_rate;
			if (delta == 0) {
				rational_best_approximation(k, m,
						k_max - 1,
						m_max - 1,
						&lmul, &ldiv);
				p_v->n = n;
				p_v->m = ldiv;
				p_v->k = lmul;
				p_v->p = p;
				return new_rate;
			}
			if (delta < 0)
				delta = -delta;
			if (delta < best_delta) {
				best_delta = delta;
				best_rate = new_rate;
				p_v->n = n;
				p_v->m = m;
				p_v->k = k;
				p_v->p = p;
			}
		}
	}
	return best_rate;
}

static unsigned long ccu_pll_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct ccu *ccu = hw2ccu(hw);
	const struct ccu_extra *extra = ccu->extra;
	unsigned long rate;
	int i, n, d1, m, k, p;
	u32 reg;

	reg = readl(ccu->base + ccu->reg);

	if (extra) {
		for (i = 0; i < extra->num_frac - 1; i++) {
			if ((reg & extra->frac[i].mask) == extra->frac[i].val)
				return rate = extra->frac[i].rate;
		}
	}

	rate = parent_rate;

	if (ccu->d1_width) {
		d1 = reg >> ccu->d1_shift;
		d1 &= (1 << ccu->d1_width) - 1;
		rate /= (d1 + 1);
	}

	if (ccu->n_width) {
		n = reg >> ccu->n_shift;
		n &= (1 << ccu->n_width) - 1;
		if (!(ccu->features & CCU_FEATURE_N0))
			n++;
		rate *= n;
	}

	if (ccu->m_width) {
		m = reg >> ccu->m_shift;
		m &= (1 << ccu->m_width) - 1;
		rate /= (m + 1);
	}

	if (ccu->k_width) {
		k = reg >> ccu->k_shift;
		k &= (1 << ccu->k_width) - 1;
		rate *= (k + 1);
	}

	if (ccu->p_width) {
		p = reg >> ccu->p_shift;
		p &= (1 << ccu->p_width) - 1;
		rate >>= p;
	}

	if (extra && (ccu->features & CCU_FEATURE_FIXED_POSTDIV))
		rate /= extra->fixed_div[0];

	return rate;
}

static int ccu_pll_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	struct ccu *ccu = hw2ccu(hw);
	const struct ccu_extra *extra = ccu->extra;
	unsigned long rate = req->rate;
	unsigned long parent_rate = req->best_parent_rate;
	struct val_pll v;
	int i;

	if (extra) {
		for (i = 0; i < extra->num_frac - 1; i++) {
			if (extra->frac[i].rate == rate)
				return rate;
		}

		if (ccu->features & CCU_FEATURE_FIXED_POSTDIV)
			rate *= extra->fixed_div[0];
	}

	rate = ccu_pll_find_best(ccu, rate, parent_rate, &v);

	if (extra && (ccu->features & CCU_FEATURE_FIXED_POSTDIV))
		rate /= extra->fixed_div[0];

	req->rate = rate;

	return 0;
}

static void ccu_pll_set_flat_factors(struct ccu *ccu, u32 mask, u32 val)
{
	u32 reg, m_val, p_val;
	u32 m_mask = CCU_MASK(ccu->m_shift, ccu->m_width);
	u32 p_mask = CCU_MASK(ccu->p_shift, ccu->p_width);

	reg = readl(ccu->base + ccu->reg);
	m_val = reg & m_mask;
	p_val = reg & p_mask;

	spin_lock(&ccu_lock);

	/* increase p, then m */
	if (ccu->p_width && p_val < (val & p_mask)) {
		reg &= ~p_mask;
		reg |= val & p_mask;
		writel(reg, ccu->base + ccu->reg);
		udelay(10);
	}
	if (ccu->m_width && m_val < (val & m_mask)) {
		reg &= ~m_mask;
		reg |= val & m_mask;
		writel(reg, ccu->base + ccu->reg);
		udelay(10);
	}

	/* set other factors */
	reg &= ~(mask & ~(p_mask | m_mask));
	reg |= val & ~(p_mask | m_mask);
	writel(reg, ccu->base + ccu->reg);

	/* decrease m */
	if (ccu->m_width && m_val > (val & m_mask)) {
		reg &= ~m_mask;
		reg |= val & m_mask;
		writel(reg, ccu->base + ccu->reg);
		udelay(10);
	}

	/* wait for PLL stable */
	ccu_wait_pll_stable(ccu);

	/* decrease p */
	if (ccu->p_width && p_val > (val & p_mask)) {
		reg &= ~p_mask;
		reg |= val & p_mask;
		writel(reg, ccu->base + ccu->reg);
		udelay(10);
	}

	spin_unlock(&ccu_lock);
}

static int ccu_pll_set_rate(struct clk_hw *hw,
				unsigned long rate,
				unsigned long parent_rate)
{
	struct ccu *ccu = hw2ccu(hw);
	const struct ccu_extra *extra = ccu->extra;
	struct val_pll v;
	u32 mask, val;

	mask =  CCU_MASK(ccu->n_shift, ccu->n_width) |
		CCU_MASK(ccu->d1_shift, ccu->d1_width) |
		CCU_MASK(ccu->k_shift, ccu->k_width) |
		CCU_MASK(ccu->m_shift, ccu->m_width) |
		CCU_MASK(ccu->p_shift, ccu->p_width);
	val = 0;

	if (extra && extra->num_frac) {
		int i;

		for (i = 0; i < extra->num_frac - 1; i++) {
			if (extra->frac[i].rate == rate) {
				ccu_set_field(ccu, ccu->reg,
						extra->frac[i].mask,
						extra->frac[i].val);
				return 0;
			}
		}
		mask |= extra->frac[i].mask;
		val |= extra->frac[i].val;
	}

	if (extra && (ccu->features & CCU_FEATURE_FIXED_POSTDIV))
		rate *= extra->fixed_div[0];


	rate = ccu_pll_find_best(ccu, rate, parent_rate, &v);

	if (!(ccu->features & CCU_FEATURE_N0))
		v.n--;

	val |=  (v.n << ccu->n_shift) |
		((v.d1 - 1) << ccu->d1_shift) |
		((v.k - 1) << ccu->k_shift) |
		((v.m - 1) << ccu->m_shift) |
		(v.p << ccu->p_shift);

	if (ccu->upd_bit)			/* (upd_bit is never 0) */
		val |= BIT(ccu->upd_bit);

	if (ccu->features & CCU_FEATURE_FLAT_FACTORS) {
		ccu_pll_set_flat_factors(ccu, mask, val);
		return 0;
	}
	ccu_set_field(ccu, ccu->reg, mask, val);
	ccu_wait_pll_stable(ccu);

	return 0;
}

const struct clk_ops ccu_pll_ops = {
	.enable		= ccu_enable,
	.disable	= ccu_disable,
/*	.is_enabled	= NULL;	(don't disable the clocks at startup time) */

	.determine_rate	= ccu_pll_determine_rate,
	.recalc_rate	= ccu_pll_recalc_rate,
	.set_rate	= ccu_pll_set_rate,
};

/* --- mux parent --- */
static u8 ccu_get_parent(struct clk_hw *hw)
{
	struct ccu *ccu = hw2ccu(hw);

	if (!ccu->mux_width)
		return 0;

	return (readl(ccu->base + ccu->reg) >> ccu->mux_shift) &
				((1 << ccu->mux_width) - 1);
}

static int ccu_set_parent(struct clk_hw *hw, u8 index)
{
	struct ccu *ccu = hw2ccu(hw);
	u32 mask;

	if (!ccu->mux_width)
		return 0;

	mask = CCU_MASK(ccu->mux_shift, ccu->mux_width);

	ccu_set_field(ccu, ccu->reg, mask, index << ccu->mux_shift);

	return 0;
}

/* --- mux --- */
static void ccu_mux_adjust_parent_for_prediv(struct ccu *ccu,
					     int parent_index,
					     unsigned long *parent_rate)
{
	const struct ccu_extra *extra = ccu->extra;
	int prediv = 1;
	u32 reg;

	if (!(extra &&
	      (ccu->features & (CCU_FEATURE_MUX_FIXED_PREDIV |
				CCU_FEATURE_MUX_VARIABLE_PREDIV))))
		return;

	reg = readl(ccu->base + ccu->reg);
	if (parent_index < 0)
		parent_index = (reg >> ccu->mux_shift) &
					((1 << ccu->mux_width) - 1);

	if (ccu->features & CCU_FEATURE_MUX_FIXED_PREDIV) {
		if (parent_index < ARRAY_SIZE(extra->fixed_div))
			prediv = extra->fixed_div[parent_index];
	}

	if (ccu->features & CCU_FEATURE_MUX_VARIABLE_PREDIV)
		if (parent_index == extra->variable_prediv.index) {
			u8 div;

			div = reg >> extra->variable_prediv.shift;
			div &= (1 << extra->variable_prediv.width) - 1;
			prediv = div + 1;
		}

	*parent_rate /= prediv;
}

/* --- periph --- */
static unsigned long ccu_p_best_rate(struct ccu *ccu,
					unsigned long rate,
					unsigned long parent_rate,
					struct val_periph *p_v)
{
	int p;
	int p_max = (1 << ccu->p_width) - 1;

	p_v->m = 1;

	for (p = p_max - 1; p >= 0 ; p--) {
		if (parent_rate >> p > rate)
			break;
	}
	p_v->p = ++p;

	return parent_rate >> p;
}

static unsigned long ccu_mp_best_rate(struct ccu *ccu,
					unsigned long rate,
					unsigned long parent_rate,
					struct val_periph *p_v)
{
	const struct ccu_extra *extra = ccu->extra;
	unsigned long new_rate, best_rate = 0;
	long delta, best_delta = 1000000;
	unsigned int m_max = 1 << ccu->m_width;
	unsigned int p_max = (1 << ccu->p_width) - 1;
	int i, m, p, div;

	p_v->m = 1;
	p_v->p = 0;

	if (extra && (ccu->features & CCU_FEATURE_M_TABLE)) {
		for (i = 0; i < ARRAY_SIZE(extra->m_table); i++) {
			m = extra->m_table[i];
			if (m == 0)
				break;
			for (p = p_max; p >= 0; p--) {
				new_rate = parent_rate / m >> p;
				delta = rate - new_rate;
				if (delta == 0) {
					p_v->m = i + 1;
					p_v->p = p;
					return new_rate;
				}
				if (delta < 0)
					delta = -delta;
				if (delta < best_delta) {
					best_delta = delta;
					best_rate = new_rate;
					p_v->m = i + 1;
					p_v->p = p;
				}
			}
		}

		return best_rate;
	}

	for (p = p_max; p >= 0; p--) {
		div = parent_rate / rate >> p;
		for (m = div; m <= div + 1; m++) {
			if (m > m_max)
				break;
			new_rate = parent_rate / m >> p;
			delta = rate - new_rate;
			if (delta == 0) {
				p_v->m = m;
				p_v->p = p;
				return new_rate;
			}
			if (delta < 0)
				delta = -delta;
			if (delta < best_delta) {
				best_delta = delta;
				best_rate = new_rate;
				p_v->m = m;
				p_v->p = p;
			}
		}
	}

	return best_rate;
}

static unsigned long ccu_periph_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct ccu *ccu = hw2ccu(hw);
	const struct ccu_extra *extra = ccu->extra;
	int m, p;
	u32 reg;

	ccu_mux_adjust_parent_for_prediv(ccu, -1, &parent_rate);

	if (!ccu->m_width && !ccu->p_width)
		return parent_rate;

	reg = readl(ccu->base + ccu->reg);
	m = (reg >> ccu->m_shift) & ((1 << ccu->m_width) - 1);

	if (extra && (ccu->features & CCU_FEATURE_M_TABLE))
		m = extra->m_table[m] - 1;

	if (!ccu->p_width)
		return parent_rate / (m + 1);

	reg = readl(ccu->base + ccu->reg);
	p = (reg >> ccu->p_shift) & ((1 << ccu->p_width) - 1);

	return parent_rate / (m + 1) >> p;
}

static int ccu_periph_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	struct ccu *ccu = hw2ccu(hw);
	unsigned long best_parent_rate = 0, best_rate = 0;
	struct clk_hw *best_parent = NULL;
	unsigned long (*round)(struct ccu *,
				unsigned long,
				unsigned long,
				struct val_periph *p_v);
	struct val_periph v;
	unsigned int i;

	if (ccu->m_width)
		round = ccu_mp_best_rate;
	else if (ccu->p_width)
		round = ccu_p_best_rate;
	else
		return __clk_mux_determine_rate(hw, req);

	for (i = 0; i < clk_hw_get_num_parents(hw); i++) {
		unsigned long new_rate, parent_rate;
		struct clk_hw *parent;

		parent = clk_hw_get_parent_by_index(hw, i);
		if (!parent)
			continue;

		parent_rate = clk_hw_get_rate(parent);
		ccu_mux_adjust_parent_for_prediv(ccu, i, &parent_rate);
		new_rate = round(ccu, req->rate, parent_rate, &v);

		if (new_rate == req->rate) {
			best_parent = parent;
			best_parent_rate = parent_rate;
			best_rate = new_rate;
			goto out;
		}

		if ((req->rate - new_rate) < (req->rate - best_rate)) {
			best_rate = new_rate;
			best_parent_rate = parent_rate;
			best_parent = parent;
		}
	}

	if (best_rate == 0)
		return -EINVAL;

out:
	req->best_parent_hw = best_parent;
	req->best_parent_rate = best_parent_rate;
	req->rate = best_rate;

	return 0;
}

static int ccu_periph_set_rate(struct clk_hw *hw,
				unsigned long rate,
				unsigned long parent_rate)
{
	struct ccu *ccu = hw2ccu(hw);
	const struct ccu_extra *extra = ccu->extra;
	struct val_periph v;
	u32 mask;

	if (!ccu->m_width && !ccu->p_width)
		return 0;

	ccu_mux_adjust_parent_for_prediv(ccu, -1, &parent_rate);

	if (extra && (ccu->features & CCU_FEATURE_MODE_SELECT)) {
// mmc2: always time-out...
#if 1 //test
		/* fixme: should use new mode */
		if (rate == extra->mode_select.rate)
// clock too high? - test
//			rate /= 2;
			rate /= 4;	// 100MHz => 25MHz
#else
		mask = BIT(extra->mode_select.bit);
		if (rate == extra->mode_select.rate) {
			ccu_set_field(ccu, ccu->reg, mask, mask);
//			rate *= 2;
			rate /= 2;
		} else {
			ccu_set_field(ccu, ccu->reg, mask, 0);
		}
#endif
	}

	if (ccu->m_width)				/* m and p */
		ccu_mp_best_rate(ccu, rate, parent_rate, &v);
	else
		ccu_p_best_rate(ccu, rate, parent_rate, &v);

	mask = CCU_MASK(ccu->m_shift, ccu->m_width) |
		CCU_MASK(ccu->p_shift, ccu->p_width);

	if ((ccu->features & CCU_FEATURE_SET_RATE_GATE) &&
	    ccu->has_gate)
		ccu_set_field(ccu, ccu->reg, BIT(ccu->gate_bit), 0);
	ccu_set_field(ccu, ccu->reg, mask, ((v.m - 1) << ccu->m_shift) |
					    (v.p << ccu->p_shift));
	if ((ccu->features & CCU_FEATURE_SET_RATE_GATE) &&
	    ccu->has_gate)
		ccu_set_field(ccu, ccu->reg, BIT(ccu->gate_bit),
						BIT(ccu->gate_bit));

	return 0;
}

const struct clk_ops ccu_periph_ops = {
	.enable		= ccu_enable,
	.disable	= ccu_disable,
/*	.is_enabled	= NULL;	(don't disable the clocks at startup time) */

	.get_parent	= ccu_get_parent,
	.set_parent	= ccu_set_parent,

	.determine_rate	= ccu_periph_determine_rate,
	.recalc_rate	= ccu_periph_recalc_rate,
	.set_rate	= ccu_periph_set_rate,
};

/* --- fixed factor --- */
/* mul is n_width - div is m_width */
unsigned long ccu_fixed_factor_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct ccu *ccu = hw2ccu(hw);

	return parent_rate / ccu->m_width * ccu->n_width;
}

int ccu_fixed_factor_determine_rate(struct clk_hw *hw,
				struct clk_rate_request *req)
{
	struct ccu *ccu = hw2ccu(hw);
	unsigned long *parent_rate = &req->best_parent_rate;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_PARENT) {
		unsigned long best_parent;

		best_parent = req->rate / ccu->n_width * ccu->m_width;
		*parent_rate = clk_hw_round_rate(clk_hw_get_parent(hw),
						 best_parent);
	}

	req->rate = *parent_rate / ccu->m_width * ccu->n_width;
	return 0;
}

int ccu_fixed_factor_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long parent_rate)
{
	return 0;
}

const struct clk_ops ccu_fixed_factor_ops = {
	.disable	= ccu_disable,
	.enable		= ccu_enable,
/*	.is_enabled	= NULL, */

	.determine_rate	= ccu_fixed_factor_determine_rate,
	.recalc_rate	= ccu_fixed_factor_recalc_rate,
	.set_rate	= ccu_fixed_factor_set_rate,
};

/* --- phase --- */
static int ccu_phase_get_phase(struct clk_hw *hw)
{
	struct ccu *ccu = hw2ccu(hw);
	struct clk_hw *parent, *grandparent;
	unsigned int parent_rate, grandparent_rate;
	u16 step, parent_div;
	u32 reg;
	u8 delay;

	reg = readl(ccu->base + ccu->reg);
	delay = (reg >> ccu->p_shift);
	delay &= (1 << ccu->p_width) - 1;

	if (!delay)
		return 180;

	/* Get our parent clock, it's the one that can adjust its rate */
	parent = clk_hw_get_parent(hw);
	if (!parent)
		return -EINVAL;

	/* And its rate */
	parent_rate = clk_hw_get_rate(parent);
	if (!parent_rate)
		return -EINVAL;

	/* Now, get our parent's parent (most likely some PLL) */
	grandparent = clk_hw_get_parent(parent);
	if (!grandparent)
		return -EINVAL;

	/* And its rate */
	grandparent_rate = clk_hw_get_rate(grandparent);
	if (!grandparent_rate)
		return -EINVAL;

	/* Get our parent clock divider */
	parent_div = grandparent_rate / parent_rate;

	step = DIV_ROUND_CLOSEST(360, parent_div);
	return delay * step;
}

static int ccu_phase_set_phase(struct clk_hw *hw, int degrees)
{
	struct ccu *ccu = hw2ccu(hw);
	struct clk_hw *parent, *grandparent;
//	struct ccu *ccu_parent;
//	u32 reg
	unsigned int parent_rate, grandparent_rate;
	u32 mask;
	u8 delay = 0;
	u16 step, parent_div;

	if (degrees == 180)
		goto set_phase;

	/* Get our parent clock, it's the one that can adjust its rate */
	parent = clk_hw_get_parent(hw);
	if (!parent)
		return -EINVAL;

#if 0
	/* no delay when new mode */
	ccu_parent = hw2ccu(parent);
	if (ccu_parent->extra && ccu_parent->extra->mode_select.rate != 0) {
		reg = readl(ccu_parent->base + ccu_parent->reg);
		if (reg & BIT(ccu_parent->extra->mode_select.bit))
			goto set_phase;
	}
#endif

	/* And its rate */
	parent_rate = clk_hw_get_rate(parent);
	if (!parent_rate)
		return -EINVAL;

	/* Now, get our parent's parent (most likely some PLL) */
	grandparent = clk_hw_get_parent(parent);
	if (!grandparent)
		return -EINVAL;

	/* And its rate */
	grandparent_rate = clk_hw_get_rate(grandparent);
	if (!grandparent_rate)
		return -EINVAL;

	/* Get our parent divider */
	parent_div = grandparent_rate / parent_rate;

	/*
	 * We can only outphase the clocks by multiple of the
	 * PLL's period.
	 *
	 * Since our parent clock is only a divider, and the
	 * formula to get the outphasing in degrees is deg =
	 * 360 * delta / period
	 *
	 * If we simplify this formula, we can see that the
	 * only thing that we're concerned about is the number
	 * of period we want to outphase our clock from, and
	 * the divider set by our parent clock.
	 */
	step = DIV_ROUND_CLOSEST(360, parent_div);
	delay = DIV_ROUND_CLOSEST(degrees, step);

set_phase:
	mask = CCU_MASK(ccu->p_shift, ccu->p_width);
	ccu_set_field(ccu, ccu->reg, mask, delay << ccu->p_shift);

	return 0;
}

const struct clk_ops ccu_phase_ops = {
	.get_phase	= ccu_phase_get_phase,
	.set_phase	= ccu_phase_set_phase,
};

/* --- reset --- */
static inline
struct ccu_reset *rcdev_to_ccu_reset(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct ccu_reset, rcdev);
}

static void ccu_set_reset_clock(struct ccu_reset *ccu_reset,
				int reg, int bit, int enable)
{
	u32 mask;

	if (!reg)			/* compatibility */
		return;

	ccu_dbg("reset %03x %d %sassert\n",
		reg, bit, enable ? "de-" : "");

	mask = BIT(bit);

	spin_lock(&ccu_lock);
	if (enable)
		writel(readl(ccu_reset->base + reg) | mask,
			ccu_reset->base + reg);
	else
		writel(readl(ccu_reset->base + reg) & ~mask,
			ccu_reset->base + reg);
	spin_unlock(&ccu_lock);
}

static int ccu_reset_assert(struct reset_controller_dev *rcdev,
			    unsigned long id)
{
	struct ccu_reset *ccu_reset = rcdev_to_ccu_reset(rcdev);
	const struct ccu_reset_map *map = &ccu_reset->reset_map[id];

	ccu_set_reset_clock(ccu_reset, map->reg, map->bit, 0);

	return 0;
}

static int ccu_reset_deassert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct ccu_reset *ccu_reset = rcdev_to_ccu_reset(rcdev);
	const struct ccu_reset_map *map = &ccu_reset->reset_map[id];

	ccu_set_reset_clock(ccu_reset, map->reg, map->bit, 1);

	return 0;
}

const struct reset_control_ops ccu_reset_ops = {
	.assert		= ccu_reset_assert,
	.deassert	= ccu_reset_deassert,
};

/* --- init --- */
int __init ccu_probe(struct device_node *node,
			struct clk_hw_onecell_data *data,
			struct ccu_reset *resets)
{
	struct clk_hw *hw;
	struct ccu *ccu;
	void __iomem *reg;
	int i, ret;

#if 1
	reg = of_iomap(node, 0);
	if (!reg) {
		pr_err("%s: Clock mapping failed\n",
			of_node_full_name(node));
		return -EINVAL;
	}
#else
	reg = of_io_request_and_map(node, 0, of_node_full_name(node));
	if (IS_ERR(reg)) {
		pr_err("%s: Clock mapping failed %d\n",
			of_node_full_name(node), (int) PTR_ERR(reg));
		return PTR_ERR(reg);
	}
#endif

	/* register the clocks */
	for (i = 0; i < data->num; i++) {
		hw = data->hws[i];
#if CCU_DEBUG
		if (!hw) {
			ccu_dbg("%s: Bad number of clocks %d != %d\n",
				of_node_full_name(node),
				i + 1, data->num);
			data->num = i;
			break;
		}
#endif
		ccu = hw2ccu(hw);
		ccu->base = reg;
		ret = clk_hw_register(NULL, hw);
		if (ret < 0) {
			pr_err("%s: Register clock[%d] %s failed %d\n",
				of_node_full_name(node), i,
				clk_hw_get_name(hw), ret);
			data->num = i;
			break;
		}
	}
	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, data);
	if (ret < 0)
		goto err;

	/* register the resets */
	resets->rcdev.of_node = node;
	resets->base = reg;

	ret = reset_controller_register(&resets->rcdev);
	if (ret) {
		pr_err("%s: Reset register failed %d\n",
			of_node_full_name(node), ret);
		goto err;
	}

	return ret;

err:
	/* don't do anything, otherwise no uart anymore */
	return ret;
}
