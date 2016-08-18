/*
 * generic_onboard_device.c	The generic onboard USB device driver
 *
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 * Author: Peter Chen <peter.chen@freescale.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This driver is only for the USB HUB devices which need to control
 * their external pins(clock, reset, etc), and these USB HUB devices
 * are soldered at the board.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

struct usb_generic_onboard_data {
	struct clk *clk;
};

static int usb_generic_onboard_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_generic_onboard_data *hub_data;
	int ret = -EINVAL;
	struct gpio_desc *gpiod_reset = NULL;
	struct device_node *node = dev->of_node;
	u32 duration_us = 50, clk_rate = 0;

	/* Only support device tree now */
	if (!node)
		return ret;

	hub_data = devm_kzalloc(dev, sizeof(*hub_data), GFP_KERNEL);
	if (!hub_data)
		return -ENOMEM;

	hub_data->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(hub_data->clk)) {
		dev_dbg(dev, "Can't get clock: %ld\n",
			PTR_ERR(hub_data->clk));
		hub_data->clk = NULL;
	} else {
		ret = clk_prepare_enable(hub_data->clk);
		if (ret) {
			dev_err(dev,
				"Can't enable external clock: %d\n",
				ret);
			return ret;
		}

		of_property_read_u32(node, "clock-frequency", &clk_rate);
		if (clk_rate) {
			ret = clk_set_rate(hub_data->clk, clk_rate);
			if (ret) {
				dev_err(dev, "Error setting clock rate\n");
				goto disable_clk;
			}
		}
	}

	gpiod_reset = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	ret = PTR_ERR_OR_ZERO(gpiod_reset);
	if (ret) {
		dev_err(dev, "Failed to get reset gpio, err = %d\n", ret);
		goto disable_clk;
	}

	of_property_read_u32(node, "reset-duration-us", &duration_us);

	if (gpiod_reset) {
		gpiod_direction_output(gpiod_reset, 1);

		gpiod_set_value(gpiod_reset, 1);
		usleep_range(duration_us, duration_us + 10);
		gpiod_set_value(gpiod_reset, 0);
	}

	dev_set_drvdata(dev, hub_data);
	return ret;

disable_clk:
	clk_disable_unprepare(hub_data->clk);
	return ret;
}

static int usb_generic_onboard_remove(struct platform_device *pdev)
{
	struct usb_generic_onboard_data *hub_data = platform_get_drvdata(pdev);

	clk_disable_unprepare(hub_data->clk);
	return 0;
}

static const struct of_device_id usb_generic_onboard_dt_ids[] = {
	{.compatible = "generic-onboard-device"},
	{ },
};
MODULE_DEVICE_TABLE(of, usb_generic_onboard_dt_ids);

static struct platform_driver usb_generic_onboard_driver = {
	.probe = usb_generic_onboard_probe,
	.remove = usb_generic_onboard_remove,
	.driver = {
		.name = "usb_generic_onboard",
		.of_match_table = usb_generic_onboard_dt_ids,
	 },
};

static int __init usb_generic_onboard_init(void)
{
	return platform_driver_register(&usb_generic_onboard_driver);
}
subsys_initcall(usb_generic_onboard_init);

static void __exit usb_generic_onboard_exit(void)
{
	platform_driver_unregister(&usb_generic_onboard_driver);
}
module_exit(usb_generic_onboard_exit);

MODULE_AUTHOR("Peter Chen <peter.chen@freescale.com>");
MODULE_DESCRIPTION("Generic Onboard USB HUB driver");
MODULE_LICENSE("GPL v2");
