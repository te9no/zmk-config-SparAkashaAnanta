#define DT_DRV_COMPAT saa_behavior_module_select

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <saa/module_select.h>

LOG_MODULE_REGISTER(saa_module_select, CONFIG_SAA_MODULE_SELECT_LOG_LEVEL);

#define SETTINGS_PATH "saa/module"
#define SETTINGS_SELECTED SETTINGS_PATH "/selected"

static enum saa_module_profile active_profile = CONFIG_SAA_MODULE_SELECT_DEFAULT;
static struct k_work_delayable save_work;

static bool saa_module_select_valid(enum saa_module_profile profile)
{
	return profile >= SAA_MODULE_PROFILE_UNSPECIFIED && profile <= SAA_MODULE_PROFILE_IQS;
}

const char *saa_module_select_name(enum saa_module_profile profile)
{
	switch (profile) {
	case SAA_MODULE_PROFILE_UNSPECIFIED:
		return "UNSPECIFIED";
	case SAA_MODULE_PROFILE_KEY:
		return "KEY";
	case SAA_MODULE_PROFILE_ENC:
		return "ENC";
	case SAA_MODULE_PROFILE_JOY:
		return "JOY";
	case SAA_MODULE_PROFILE_TB:
		return "TB";
	case SAA_MODULE_PROFILE_TPD:
		return "TPD";
	case SAA_MODULE_PROFILE_IQS:
		return "IQS";
	default:
		return "INVALID";
	}
}

enum saa_module_profile saa_module_select_get(void)
{
	return active_profile;
}

const char *saa_module_select_get_name(void)
{
	return saa_module_select_name(active_profile);
}

static void saa_module_select_save_work_handler(struct k_work *work)
{
	uint8_t persisted = active_profile;
	int ret = settings_save_one(SETTINGS_SELECTED, &persisted, sizeof(persisted));

	if (ret < 0) {
		LOG_ERR("failed to save SAA module profile: %d", ret);
	}
}

int saa_module_select_set(enum saa_module_profile profile)
{
	if (!saa_module_select_valid(profile)) {
		LOG_ERR("invalid SAA module profile: %d", profile);
		return -EINVAL;
	}

	if (active_profile == profile) {
		LOG_INF("SAA module profile already %s", saa_module_select_name(profile));
		return 0;
	}

	active_profile = profile;
	LOG_INF("SAA module profile selected: %s", saa_module_select_name(active_profile));
	k_work_reschedule(&save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));

	return 0;
}

static int saa_module_select_settings_set(const char *name, size_t len, settings_read_cb read_cb,
					  void *cb_arg)
{
	const char *next;

	if (!settings_name_steq(name, "selected", &next) || next != NULL) {
		return -ENOENT;
	}

	if (len != sizeof(uint8_t)) {
		return -EINVAL;
	}

	uint8_t persisted;
	int ret = read_cb(cb_arg, &persisted, sizeof(persisted));

	if (ret < 0) {
		return ret;
	}

	if (!saa_module_select_valid(persisted)) {
		LOG_WRN("ignoring invalid persisted SAA module profile: %u", persisted);
		return 0;
	}

	active_profile = persisted;
	LOG_INF("SAA module profile loaded: %s", saa_module_select_name(active_profile));

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(saa_module_select, SETTINGS_PATH, NULL,
			       saa_module_select_settings_set, NULL, NULL);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event)
{
	return saa_module_select_set(binding->param1);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
				      struct zmk_behavior_binding_event event)
{
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_module_select_driver_api = {
	.binding_pressed = on_keymap_binding_pressed,
	.binding_released = on_keymap_binding_released,
	.locality = BEHAVIOR_LOCALITY_GLOBAL,
};

static int behavior_module_select_init(const struct device *dev)
{
	k_work_init_delayable(&save_work, saa_module_select_save_work_handler);
	LOG_INF("SAA module profile active: %s", saa_module_select_name(active_profile));
	return 0;
}

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define SAA_MODULE_SELECT_INST(n)                                                     \
	BEHAVIOR_DT_INST_DEFINE(n, behavior_module_select_init, NULL, NULL, NULL,     \
				POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,         \
				&behavior_module_select_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SAA_MODULE_SELECT_INST)

#endif
