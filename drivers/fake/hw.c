#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/drivers/fake/hw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DT_DRV_COMPAT fake_hw

static int fake_print(const struct device *dev)
{
	const struct fake_driver_api *api = (const struct fake_driver_api *)dev->api;
    printk("Fake HW print function called for device from another folder: %s\n", dev->name);

	return 123; //0x7B
}

/**
 * @brief LED strip driver API
 *
 * This is the mandatory API any LED strip driver needs to expose.
 */
// static DEVICE_API(fake, hw_func) =
// {
//     .fake_api_print = fake_print,
// };

static const struct fake_driver_api fake_drv_api_struct = {
    .fake_api_print = fake_print,
};

static int fake_hw_init(const struct device *dev)
{
    printk("Fake HW driver initialized for device: %s\n", dev->name);
    return 0;
}

DEVICE_DEFINE(fake_hw, "FAKE_HW_INIT", fake_hw_init, NULL, NULL, NULL,
              POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &fake_drv_api_struct);

// HW_DEVICE_INIT(0)

// DT_INST_FOREACH_STATUS_OKAY(HW_DEVICE_INIT)

#ifdef __cplusplus
}
#endif