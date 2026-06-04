#define DT_DRV_COMPAT saa_module_mux

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <saa/module_mux.h>
#include <saa/module_select.h>

LOG_MODULE_REGISTER(saa_module_mux, CONFIG_SAA_MODULE_SELECT_LOG_LEVEL);

static enum saa_module_profile applied_profile = SAA_MODULE_PROFILE_UNSPECIFIED;
static bool applied;

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

int saa_module_mux_apply(enum saa_module_profile profile)
{
	if (profile < SAA_MODULE_PROFILE_UNSPECIFIED || profile > SAA_MODULE_PROFILE_TPD) {
		LOG_ERR("invalid SAA module mux profile: %d", profile);
		return -EINVAL;
	}

	applied_profile = profile;
	applied = true;
	log_capabilities(profile);

	/*
	 * The mux intentionally does not initialize the conflicting concrete drivers yet.
	 * This is the post-settings apply point where delayed driver bring-up will be added.
	 */
	return 0;
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
