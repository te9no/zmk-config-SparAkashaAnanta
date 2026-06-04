#define DT_DRV_COMPAT zmk_input_module_mux

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#include <dt-bindings/zmk/input_module.h>
#include <zmk/input_module.h>

LOG_MODULE_REGISTER(zmk_input_module, CONFIG_ZMK_INPUT_MODULE_LOG_LEVEL);

#define MUX_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(DT_DRV_COMPAT)
#define SETTINGS_PATH DT_PROP_OR(MUX_NODE, settings_key, "input_module")
#define SETTINGS_SELECTED SETTINGS_PATH "/selected"
#define DEFAULT_PROFILE DT_PROP_OR(MUX_NODE, default_profile, 0)

#define PROFILE_DEVICES_NAME(node_id) _CONCAT(zmk_input_module_devices_, node_id)
#define PROFILE_DEVICE_BY_IDX(node_id, prop, idx) DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx))

#define DEFINE_PROFILE_DEVICES(node_id)                                                            \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),                                            \
		    (static const struct device *const PROFILE_DEVICES_NAME(node_id)[] = {          \
			    DT_FOREACH_PROP_ELEM_SEP(node_id, devices, PROFILE_DEVICE_BY_IDX,       \
						     (, ))};),                                      \
		    ())

DT_FOREACH_CHILD(MUX_NODE, DEFINE_PROFILE_DEVICES)

struct input_module_profile_config {
	uint32_t id;
	const char *name;
	uint32_t capabilities;
	const struct device *const *devices;
	size_t devices_len;
};

#define PROFILE_DEVICES_PTR(node_id)                                                              \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices), (PROFILE_DEVICES_NAME(node_id)), (NULL))
#define PROFILE_DEVICES_LEN(node_id)                                                              \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices), (ARRAY_SIZE(PROFILE_DEVICES_NAME(node_id))), (0))

#define PROFILE_CONFIG_ENTRY(node_id)                                                             \
	{                                                                                         \
		.id = DT_PROP(node_id, profile_id),                                                \
		.name = DT_PROP(node_id, display_name),                                            \
		.capabilities = DT_PROP_OR(node_id, capabilities, 0),                              \
		.devices = PROFILE_DEVICES_PTR(node_id),                                           \
		.devices_len = PROFILE_DEVICES_LEN(node_id),                                       \
	},

static const struct input_module_profile_config profiles[] = {
	DT_FOREACH_CHILD(MUX_NODE, PROFILE_CONFIG_ENTRY)
};

static uint32_t selected_profile = DEFAULT_PROFILE;
static uint32_t applied_profile = DEFAULT_PROFILE;
static bool applied;
static struct k_work_delayable save_work;

__weak int device_init(const struct device *dev)
{
	if (device_is_ready(dev)) {
		return 0;
	}

	return -ENOSYS;
}

static const struct input_module_profile_config *find_profile(uint32_t profile_id)
{
	for (size_t i = 0; i < ARRAY_SIZE(profiles); i++) {
		if (profiles[i].id == profile_id) {
			return &profiles[i];
		}
	}

	return NULL;
}

static bool profile_valid(uint32_t profile_id)
{
	return find_profile(profile_id) != NULL;
}

static const char *enabled_disabled(bool enabled)
{
	return enabled ? "enabled" : "disabled";
}

static struct zmk_input_module_capabilities capabilities_from_flags(uint32_t flags)
{
	return (struct zmk_input_module_capabilities){
		.flags = flags,
		.kscan = (flags & ZMK_INPUT_MODULE_CAP_KSCAN) != 0,
		.encoder = (flags & ZMK_INPUT_MODULE_CAP_ENCODER) != 0,
		.adc = (flags & ZMK_INPUT_MODULE_CAP_ADC) != 0,
		.spi = (flags & ZMK_INPUT_MODULE_CAP_SPI) != 0,
		.i2c = (flags & ZMK_INPUT_MODULE_CAP_I2C) != 0,
	};
}

uint32_t zmk_input_module_selected_get(void)
{
	return selected_profile;
}

uint32_t zmk_input_module_applied_get(void)
{
	return applied_profile;
}

bool zmk_input_module_is_applied(void)
{
	return applied;
}

const char *zmk_input_module_profile_name(uint32_t profile_id)
{
	const struct input_module_profile_config *profile = find_profile(profile_id);

	return profile == NULL ? "INVALID" : profile->name;
}

uint32_t zmk_input_module_profile_flags(uint32_t profile_id)
{
	const struct input_module_profile_config *profile = find_profile(profile_id);

	return profile == NULL ? 0 : profile->capabilities;
}

struct zmk_input_module_capabilities zmk_input_module_profile_capabilities(uint32_t profile_id)
{
	return capabilities_from_flags(zmk_input_module_profile_flags(profile_id));
}

struct zmk_input_module_capabilities zmk_input_module_selected_capabilities(void)
{
	return zmk_input_module_profile_capabilities(selected_profile);
}

static void log_capabilities(uint32_t profile_id)
{
	struct zmk_input_module_capabilities caps = zmk_input_module_profile_capabilities(profile_id);

	LOG_INF("input module profile=%s kscan=%s encoder=%s adc=%s spi=%s i2c=%s",
		zmk_input_module_profile_name(profile_id), enabled_disabled(caps.kscan),
		enabled_disabled(caps.encoder), enabled_disabled(caps.adc),
		enabled_disabled(caps.spi), enabled_disabled(caps.i2c));
}

int zmk_input_module_apply(uint32_t profile_id)
{
	const struct input_module_profile_config *profile = find_profile(profile_id);

	if (profile == NULL) {
		LOG_ERR("invalid input module profile: %u", profile_id);
		return -EINVAL;
	}

	applied_profile = profile_id;
	applied = true;
	log_capabilities(profile_id);

	for (size_t i = 0; i < profile->devices_len; i++) {
		const struct device *dev = profile->devices[i];
		int ret = device_init(dev);

		if (ret < 0) {
			LOG_ERR("failed to initialize %s: %d", dev->name, ret);
			return ret;
		}

		LOG_INF("initialized %s", dev->name);
	}

	return 0;
}

static void save_work_handler(struct k_work *work)
{
	int ret = settings_save_one(SETTINGS_SELECTED, &selected_profile, sizeof(selected_profile));

	if (ret < 0) {
		LOG_ERR("failed to save input module profile: %d", ret);
	}
}

int zmk_input_module_select_set(uint32_t profile_id)
{
	if (!profile_valid(profile_id)) {
		LOG_ERR("invalid input module profile: %u", profile_id);
		return -EINVAL;
	}

	if (selected_profile == profile_id) {
		LOG_INF("input module profile already %s",
			zmk_input_module_profile_name(selected_profile));
		return 0;
	}

	selected_profile = profile_id;
	LOG_INF("input module profile selected for next boot: %s",
		zmk_input_module_profile_name(selected_profile));
	k_work_reschedule(&save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));

	return 0;
}

static int input_module_settings_set(const char *name, size_t len, settings_read_cb read_cb,
				     void *cb_arg)
{
	const char *next;

	if (!settings_name_steq(name, "selected", &next) || next != NULL) {
		return -ENOENT;
	}

	if (len != sizeof(uint32_t)) {
		return -EINVAL;
	}

	uint32_t persisted;
	int ret = read_cb(cb_arg, &persisted, sizeof(persisted));

	if (ret < 0) {
		return ret;
	}

	if (!profile_valid(persisted)) {
		LOG_WRN("ignoring invalid persisted input module profile: %u", persisted);
		return 0;
	}

	selected_profile = persisted;
	LOG_INF("input module profile loaded: %s",
		zmk_input_module_profile_name(selected_profile));

	return 0;
}

static int input_module_settings_commit(void)
{
	return zmk_input_module_apply(selected_profile);
}

SETTINGS_STATIC_HANDLER_DEFINE(zmk_input_module, SETTINGS_PATH, NULL, input_module_settings_set,
			       input_module_settings_commit, NULL);

static int input_module_init(const struct device *dev)
{
	k_work_init_delayable(&save_work, save_work_handler);
	LOG_INF("input module mux ready; default profile=%s",
		zmk_input_module_profile_name(selected_profile));
	return 0;
}

DEVICE_DT_DEFINE(MUX_NODE, input_module_init, NULL, NULL, NULL, POST_KERNEL,
		 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);
