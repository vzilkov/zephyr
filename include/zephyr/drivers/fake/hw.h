#include <zephyr/device.h>

struct fake_driver_api {
    /** Print function for the fake hardware */
    int (*fake_api_print)(const struct device *dev);
};