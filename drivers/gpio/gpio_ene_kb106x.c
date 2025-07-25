/*
 * Copyright (c) 2025 ENE Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ene_kb106x_gpio

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/dt-bindings/gpio/ene-kb106x-gpio.h>
#include <zephyr/sys/util_macro.h>
#include <reg/gpio.h>
#include <reg/gptd.h>

struct gpio_kb106x_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	sys_slist_t cb;
};

struct gpio_kb106x_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	/* base address of GPIO port */
	struct gpio_regs *gpio_regs;
	struct gptd_regs *gptd_regs;
};

static void gpio_kb106x_isr(const struct device *dev)
{
	const struct gpio_kb106x_config *config = dev->config;
	struct gpio_kb106x_data *context = dev->data;
	uint32_t pending_flag = config->gptd_regs->GPTDPF;

	gpio_fire_callbacks(&context->cb, dev, pending_flag);
	config->gptd_regs->GPTDPF = pending_flag;
}

static int kb106x_gpio_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	const struct gpio_kb106x_config *config = dev->config;

	/* ene specific flags. low voltage mode, input voltage threshold (ViH & ViL) 1.8V */
	if (flags & ENE_GPIO_VOLTAGE_1P8) {
		WRITE_BIT(config->gpio_regs->GPIOLV, pin, 1);
	} else {
		WRITE_BIT(config->gpio_regs->GPIOLV, pin, 0);
	}
	/* ene specific flags. max current driving ability, max support 16 mA */
	if (flags & ENE_GPIO_DRIVING_16MA) {
		WRITE_BIT(config->gpio_regs->GPIODC, pin, 1);
	} else {
		WRITE_BIT(config->gpio_regs->GPIODC, pin, 0);
	}
	/* pull-up function */
	if (flags & GPIO_PULL_UP) {
		WRITE_BIT(config->gpio_regs->GPIOPU, pin, 1);
	} else {
		WRITE_BIT(config->gpio_regs->GPIOPU, pin, 0);
	}
	/* output data high/low */
	if (flags & GPIO_OUTPUT_INIT_HIGH) {
		WRITE_BIT(config->gpio_regs->GPIOD, pin, 1);
	} else if (flags & GPIO_OUTPUT_INIT_LOW) {
		WRITE_BIT(config->gpio_regs->GPIOD, pin, 0);
	}
	/* output enable function */
	if (flags & GPIO_OUTPUT) {
		/* [patch] setting open-drain only when output is enabled */
		/* output type push-pull/open-drain */
		if (flags & GPIO_SINGLE_ENDED) {
			if (flags & GPIO_LINE_OPEN_DRAIN) {
				WRITE_BIT(config->gpio_regs->GPIOOD, pin, 1);
			} else {
				WRITE_BIT(config->gpio_regs->GPIOOD, pin, 0);
			}
		} else {
			WRITE_BIT(config->gpio_regs->GPIOOD, pin, 0);
		}
		WRITE_BIT(config->gpio_regs->GPIOOE, pin, 1);
	} else {
		WRITE_BIT(config->gpio_regs->GPIOOE, pin, 0);
		/* [patch] disable open-drain when output is disabled */
		WRITE_BIT(config->gpio_regs->GPIOOD, pin, 0);
	}
	/* input enable function */
	if (flags & GPIO_INPUT) {
		WRITE_BIT(config->gpio_regs->GPIOIE, pin, 1);
	} else {
		WRITE_BIT(config->gpio_regs->GPIOIE, pin, 0);
	}

	return 0;
}

static int kb106x_gpio_port_get_raw(const struct device *dev, gpio_port_value_t *value)
{
	const struct gpio_kb106x_config *config = dev->config;

	*value = config->gpio_regs->GPIOIN;
	return 0;
}

static int kb106x_gpio_port_set_masked_raw(const struct device *dev, gpio_port_pins_t mask,
					   gpio_port_value_t value)
{
	const struct gpio_kb106x_config *config = dev->config;

	config->gpio_regs->GPIOD |= (value & mask);
	return 0;
}

static int kb106x_gpio_port_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_kb106x_config *config = dev->config;

	config->gpio_regs->GPIOD |= pins;
	return 0;
}

static int kb106x_gpio_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_kb106x_config *config = dev->config;

	config->gpio_regs->GPIOD &= ~pins;
	return 0;
}

static int kb106x_gpio_port_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_kb106x_config *config = dev->config;

	config->gpio_regs->GPIOD ^= pins;
	return 0;
}

static int kb106x_gpio_pin_interrupt_configure(const struct device *dev, gpio_pin_t pin,
					       enum gpio_int_mode mode, enum gpio_int_trig trig)
{
	const struct gpio_kb106x_config *config = dev->config;

	if (mode & GPIO_INT_EDGE) {
		WRITE_BIT(config->gptd_regs->GPTDEL, pin, 0);
		if (trig & GPIO_INT_HIGH_1) {
			if (trig & GPIO_INT_LOW_0) { /* Falling & Rising edge trigger */
				/* Enable toggle trigger */
				WRITE_BIT(config->gptd_regs->GPTDCHG, pin, 1);
			} else { /* Rising edge */
				/* Disable toggle trigger */
				WRITE_BIT(config->gptd_regs->GPTDCHG, pin, 0);
				WRITE_BIT(config->gptd_regs->GPTDPS, pin, 1);
			}
		} else { /* Falling edge */
			/* Disable Toggle trigger */
			WRITE_BIT(config->gptd_regs->GPTDCHG, pin, 0);
			WRITE_BIT(config->gptd_regs->GPTDPS, pin, 0);
		}
	} else {
		WRITE_BIT(config->gptd_regs->GPTDEL, pin, 1);
		/* Disable Toggle trigger */
		WRITE_BIT(config->gptd_regs->GPTDCHG, pin, 0);
		if (trig & GPIO_INT_HIGH_1) {
			WRITE_BIT(config->gptd_regs->GPTDPS, pin, 1);
		} else {
			WRITE_BIT(config->gptd_regs->GPTDPS, pin, 0);
		}
	}

	/* clear pending flag */
	WRITE_BIT(config->gptd_regs->GPTDPF, pin, 1);

	/* Check if GPIO port needs interrupt support */
	if ((mode & GPIO_INT_DISABLE) || (mode & GPIO_INT_ENABLE) == 0) {
		/* Set the mask to disable the interrupt */
		WRITE_BIT(config->gptd_regs->GPTDIE, pin, 0);
	} else {
		/* Enable the interrupt */
		WRITE_BIT(config->gptd_regs->GPTDIE, pin, 1);
	}

	/* Check GPIO wakeup enable */
	if (trig & GPIO_INT_TRIG_WAKE) {
		WRITE_BIT(config->gptd_regs->GPTDWE, pin, 1);
	} else {
		WRITE_BIT(config->gptd_regs->GPTDWE, pin, 0);
	}
	return 0;
}

static int kb106x_gpio_manage_callback(const struct device *dev, struct gpio_callback *cb, bool set)
{
	struct gpio_kb106x_data *context = dev->data;

	gpio_manage_callback(&context->cb, cb, set);
	return 0;
}

static uint32_t kb106x_gpio_get_pending_int(const struct device *dev)
{
	const struct gpio_kb106x_config *const config = dev->config;

	return config->gptd_regs->GPTDPF;
}

static const struct gpio_driver_api kb106x_gpio_api = {
	.pin_configure = kb106x_gpio_pin_configure,
	.port_get_raw = kb106x_gpio_port_get_raw,
	.port_set_masked_raw = kb106x_gpio_port_set_masked_raw,
	.port_set_bits_raw = kb106x_gpio_port_set_bits_raw,
	.port_clear_bits_raw = kb106x_gpio_port_clear_bits_raw,
	.port_toggle_bits = kb106x_gpio_port_toggle_bits,
	.pin_interrupt_configure = kb106x_gpio_pin_interrupt_configure,
	.manage_callback = kb106x_gpio_manage_callback,
	.get_pending_int = kb106x_gpio_get_pending_int,
};

#define KB106X_GPIO_INIT(n)                                                                        \
	static int kb106x_gpio_##n##_init(const struct device *dev)                                \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, 0, irq), DT_INST_IRQ_BY_IDX(n, 0, priority),     \
			    gpio_kb106x_isr, DEVICE_DT_INST_GET(n), 0);                            \
		irq_enable(DT_INST_IRQ_BY_IDX(n, 0, irq));                                         \
		IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, 1, irq), DT_INST_IRQ_BY_IDX(n, 1, priority),     \
			    gpio_kb106x_isr, DEVICE_DT_INST_GET(n), 0);                            \
		irq_enable(DT_INST_IRQ_BY_IDX(n, 1, irq));                                         \
		return 0;                                                                          \
	};                                                                                         \
	static const struct gpio_kb106x_config port_##n##_kb106x_config = {                        \
		.common = {.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n)},                   \
		.gpio_regs = (struct gpio_regs *)DT_INST_REG_ADDR_BY_IDX(n, 0),                    \
		.gptd_regs = (struct gptd_regs *)DT_INST_REG_ADDR_BY_IDX(n, 1),                    \
	};                                                                                         \
	static struct gpio_kb106x_data gpio_kb106x_##n##_data;                                     \
	DEVICE_DT_INST_DEFINE(n, &kb106x_gpio_##n##_init, NULL, &gpio_kb106x_##n##_data,           \
			      &port_##n##_kb106x_config, PRE_KERNEL_1, CONFIG_GPIO_INIT_PRIORITY,  \
			      &kb106x_gpio_api);

DT_INST_FOREACH_STATUS_OKAY(KB106X_GPIO_INIT)
