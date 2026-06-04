#define DT_DRV_COMPAT zmk_behavior_input_module_select

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/input_module.h>

LOG_MODULE_DECLARE(zmk_input_module, CONFIG_ZMK_INPUT_MODULE_LOG_LEVEL);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event)
{
	return zmk_input_module_select_set(binding->param1);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event)
{
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_input_module_select_driver_api = {
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
	.locality = BEHAVIOR_LOCALITY_GLOBAL,
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define INPUT_MODULE_SELECT_INST(n)                                                           \
	BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                       \
				CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                         \
				&behavior_input_module_select_driver_api);

DT_INST_FOREACH_STATUS_OKAY(INPUT_MODULE_SELECT_INST)

#endif
