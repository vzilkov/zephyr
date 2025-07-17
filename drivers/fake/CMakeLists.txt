# SPDX-License-Identifier: Apache-2.0

# zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/dac.h)

# zephyr_library_named(fake_hw)
zephyr_library()
zephyr_library_sources_ifdef(CONFIG_HW_FAKE hw.c)