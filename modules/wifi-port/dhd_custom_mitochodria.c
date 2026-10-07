// SPDX-License-Identifier: GPL-2.0-only
/*
 * Mitochodria Broadcom PCIe Board Integration
 *
 * Controls DT-described Wi-Fi power, host wake and PCIe enumeration.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 */
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <typedefs.h>
#include <osl.h>
#include <dhd.h>
#include <dhd_linux.h>

extern int sprd_pcie_configure_device(struct platform_device *pdev);
extern int sprd_pcie_unconfigure_device(struct platform_device *pdev);
extern int dhd_init_wlan_mem_43752(void);
extern void dhd_exit_wlan_mem_43752(void);
extern void *dhd_wlan_mem_prealloc_43752(int section, unsigned long size);

static struct platform_device *mitochodria_rc;
static int mitochodria_reg = -1, mitochodria_host = -1;
static bool mitochodria_ready;
static DEFINE_MUTEX(mitochodria_lock);

static int mitochodria_power(int on)
{
	if (!mitochodria_ready)
		return -ENODEV;
	mutex_lock(&mitochodria_lock);
	gpio_set_value_cansleep(mitochodria_reg, !!on);
	msleep(on ? 200 : 100);
	mutex_unlock(&mitochodria_lock);
	pr_info("C8WIFI power=%d\n", !!on);
	return 0;
}

static int mitochodria_carddetect(int present)
{
	int ret;

	if (!mitochodria_ready)
		return -ENODEV;
	ret = present ? sprd_pcie_configure_device(mitochodria_rc) :
			sprd_pcie_unconfigure_device(mitochodria_rc);
	pr_info("C8WIFI enumerate=%d ret=%d\n", !!present, ret);
	return ret;
}

int dhd_get_wlan_oob_gpio_43752(void)
{
	return mitochodria_ready ? gpio_get_value_cansleep(mitochodria_host) : -ENODEV;
}

struct resource dhd_wlan_resources_43752 = {
	.name = "mitochodria-wlan-hostwake",
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
};

struct wifi_platform_data dhd_wlan_control_43752 = {
	.set_power = mitochodria_power,
	.set_carddetect = mitochodria_carddetect,
	.mem_prealloc = dhd_wlan_mem_prealloc_43752,
};

int dhd_wlan_init(void)
{
	struct device_node *np, *rc;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "android,bcmdhd_wlan");
	if (!np)
		return -ENODEV;
	mitochodria_reg = of_get_named_gpio(np, "wl_reg_on_gpio", 0);
	mitochodria_host = of_get_named_gpio(np, "wl_host_wake_gpio", 0);
	rc = of_parse_phandle(np, "sprd,rc-ctrl", 0);
	of_node_put(np);
	if (!rc)
		return -ENODEV;
	mitochodria_rc = of_find_device_by_node(rc);
	of_node_put(rc);
	if (!mitochodria_rc)
		return -EPROBE_DEFER;
	if (!platform_get_drvdata(mitochodria_rc)) {
		ret = -EPROBE_DEFER;
		goto put_rc;
	}
	if (mitochodria_reg < 0 || mitochodria_host < 0) {
		ret = mitochodria_reg < 0 ? mitochodria_reg : mitochodria_host;
		goto put_rc;
	}
	ret = gpio_request_one(mitochodria_reg, GPIOF_OUT_INIT_LOW, "mitochodria-wlan-on");
	if (ret)
		goto put_rc;
	ret = gpio_request_one(mitochodria_host, GPIOF_IN, "mitochodria-wlan-hostwake");
	if (ret)
		goto free_reg;
	ret = gpio_to_irq(mitochodria_host);
	if (ret < 0)
		goto free_host;
	dhd_wlan_resources_43752.start = ret;
	dhd_wlan_resources_43752.end = ret;
	ret = dhd_init_wlan_mem_43752();
	if (ret)
		goto free_host;
	mitochodria_ready = true;
	pr_info("C8WIFI board ready reg=%d host=%d irq=%lu\n",
		mitochodria_reg, mitochodria_host, (unsigned long)dhd_wlan_resources_43752.start);
	return 0;
free_host:
	gpio_free(mitochodria_host);
free_reg:
	gpio_free(mitochodria_reg);
put_rc:
	put_device(&mitochodria_rc->dev);
	mitochodria_rc = NULL;
	return ret;
}

int dhd_wlan_deinit(void)
{
	if (mitochodria_ready) {
		mitochodria_power(0);
		mitochodria_ready = false;
		dhd_exit_wlan_mem_43752();
		gpio_free(mitochodria_host);
		gpio_free(mitochodria_reg);
		put_device(&mitochodria_rc->dev);
		mitochodria_rc = NULL;
	}
	return 0;
}
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
