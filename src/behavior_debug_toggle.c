/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_debug_toggle

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_ctrl.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static bool debug_enabled = IS_ENABLED(CONFIG_ZMK_BEHAVIOR_DEBUG_TOGGLE_DEFAULT_ON);

static const struct log_backend *debug_uart_backend(void) {
    return log_backend_get_by_name("log_backend_uart");
}

static void debug_toggle_apply(bool enabled) {
    const struct log_backend *backend = debug_uart_backend();

    if (backend == NULL) {
        return;
    }

    if (enabled) {
        log_backend_enable(backend, NULL, CONFIG_ZMK_BEHAVIOR_DEBUG_TOGGLE_LEVEL);
    } else {
        log_backend_disable(backend);
    }
}

static int debug_toggle_init(void) {
    debug_toggle_apply(debug_enabled);
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    debug_enabled = !debug_enabled;
    debug_toggle_apply(debug_enabled);
    LOG_INF("Debug logging %s", debug_enabled ? "enabled" : "disabled");
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_debug_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_debug_toggle_driver_api);

SYS_INIT(debug_toggle_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif