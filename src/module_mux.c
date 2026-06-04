#define DT_DRV_COMPAT saa_module_mux

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <saa/module_mux.h>
#include <saa/module_select.h>

LOG_MODULE_REGISTER(saa_module_mux, CONFIG_SAA_MODULE_SELECT_LOG_LEVEL);

static enum saa_module_profile applied_profile = SAA_MODULE_PROFILE_UNSPECIFIED;
static bool applied;

#define MUX_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(DT_DRV_COMPAT)

static const char *enabled_disabled(bool enabled)
{
	return enabled ? "enabled" : "disabled";
}

static void log_capabilities(enum saa_module_profile profile)
{
	struct saa_module_capabilities caps = saa_module_select_capabilities(profile);

	LOG_INF("SAA module mux profile=%s spi=%s i2c=%s adc=%s encoder=%s",
		saa_module_select_name(profile), enabled_disabled(caps.spi),
		enabled_disabled(caps.i2c), enabled_disabled(caps.adc),
		enabled_disabled(caps.encoder));
}

#define INIT_OPTIONAL_DEVICE(prop_name)                                                            \
	do {                                                                                       \
		COND_CODE_1(DT_NODE_HAS_PROP(MUX_NODE, prop_name),                                 \
			    ({                                                                     \
				    const struct device *dev =                                     \
					    DEVICE_DT_GET(DT_PHANDLE(MUX_NODE, prop_name));        \
				    int ret = device_init(dev);                                    \
				    if (ret < 0) {                                                 \
					    LOG_ERR("failed to initialize %s: %d", dev->name, ret); \
					    return ret;                                            \
				    }                                                              \
				    LOG_INF("initialized %s", dev->name);                          \
			    }),                                                                    \
			    (LOG_DBG("no mux candidate for " STRINGIFY(prop_name));))              \
	} while (false)

static int initialize_profile(enum saa_module_profile profile)
{
	switch (profile) {
	case SAA_MODULE_PROFILE_KEY:
		INIT_OPTIONAL_DEVICE(key_kscan);
		break;
	case SAA_MODULE_PROFILE_ENC:
		INIT_OPTIONAL_DEVICE(left_encoder);
		INIT_OPTIONAL_DEVICE(right_encoder);
		break;
	case SAA_MODULE_PROFILE_JOY:
		INIT_OPTIONAL_DEVICE(joystick_input);
		INIT_OPTIONAL_DEVICE(left_encoder);
		INIT_OPTIONAL_DEVICE(right_encoder);
		break;
	case SAA_MODULE_PROFILE_TB:
		INIT_OPTIONAL_DEVICE(trackball_input);
		break;
	case SAA_MODULE_PROFILE_TPD:
		INIT_OPTIONAL_DEVICE(touchpad_input);
		break;
	case SAA_MODULE_PROFILE_UNSPECIFIED:
	default:
		break;
	}

	return 0;
}

int saa_module_mux_apply(enum saa_module_profile profile)
{
	if (profile < SAA_MODULE_PROFILE_UNSPECIFIED || profile > SAA_MODULE_PROFILE_TPD) {
		LOG_ERR("invalid SAA module mux profile: %d", profile);
		return -EINVAL;
	}

	applied_profile = profile;
	applied = true;
	log_capabilities(profile);
	return initialize_profile(profile);
}

enum saa_module_profile saa_module_mux_get_applied(void)
{
	return applied_profile;
}

bool saa_module_mux_is_applied(void)
{
	return applied;
}

static int saa_module_mux_init(const struct device *dev)
{
	LOG_INF("SAA module mux ready; waiting for settings commit");
	return 0;
}

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define SAA_MODULE_MUX_INST(n)                                                                \
	DEVICE_DT_INST_DEFINE(n, saa_module_mux_init, NULL, NULL, NULL, POST_KERNEL,          \
			      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, NULL);

DT_INST_FOREACH_STATUS_OKAY(SAA_MODULE_MUX_INST)

#endif
