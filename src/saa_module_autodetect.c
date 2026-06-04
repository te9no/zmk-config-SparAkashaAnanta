#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(saa_module_autodetect, CONFIG_SAA_MODULE_AUTODETECT_LOG_LEVEL);

#define DT_DRV_COMPAT saa_module_detector

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define DETECTOR_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(DT_DRV_COMPAT)
#define ID_GPIO_COUNT DT_PROP_LEN(DETECTOR_NODE, id_gpios)
#define MODULE_NAME_COUNT DT_PROP_LEN(DETECTOR_NODE, module_names)

#define SAA_MODULE_GPIO_SPEC(node_id, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx)
#define SAA_MODULE_NAME(node_id, prop, idx) DT_PROP_BY_IDX(node_id, prop, idx)

BUILD_ASSERT(ID_GPIO_COUNT > 0, "saa,module-detector requires at least one id-gpios entry");
BUILD_ASSERT(ID_GPIO_COUNT <= 8, "saa,module-detector supports up to 8 ID bits");
BUILD_ASSERT(MODULE_NAME_COUNT > 0, "saa,module-detector requires module-names");

static const struct gpio_dt_spec id_gpios[] = {
	DT_FOREACH_PROP_ELEM_SEP(DETECTOR_NODE, id_gpios, SAA_MODULE_GPIO_SPEC, (,))
};

static const char *const module_names[] = {
	DT_FOREACH_PROP_ELEM_SEP(DETECTOR_NODE, module_names, SAA_MODULE_NAME, (,))
};

#if DT_NODE_HAS_PROP(DETECTOR_NODE, expected_module)
static const char *const expected_module = DT_PROP(DETECTOR_NODE, expected_module);
#else
static const char *const expected_module = NULL;
#endif

static int detected_id = -1;
static const char *detected_module = "UNKNOWN";

int saa_module_autodetect_get_id(void)
{
	return detected_id;
}

const char *saa_module_autodetect_get_name(void)
{
	return detected_module;
}

static int saa_module_autodetect_init(void)
{
	int id = 0;

	for (size_t i = 0; i < ARRAY_SIZE(id_gpios); i++) {
		const struct gpio_dt_spec *gpio = &id_gpios[i];

		if (!device_is_ready(gpio->port)) {
			LOG_ERR("module ID GPIO port %s is not ready", gpio->port->name);
			return -ENODEV;
		}

		int ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
		if (ret < 0) {
			LOG_ERR("failed to configure module ID GPIO %s.%u: %d",
				gpio->port->name, gpio->pin, ret);
			return ret;
		}

		ret = gpio_pin_get_dt(gpio);
		if (ret < 0) {
			LOG_ERR("failed to read module ID GPIO %s.%u: %d",
				gpio->port->name, gpio->pin, ret);
			return ret;
		}

		id |= (ret ? 1 : 0) << i;
	}

	detected_id = id;
	if (id < ARRAY_SIZE(module_names)) {
		detected_module = module_names[id];
	}

	LOG_INF("SAA module detected: id=%d name=%s", detected_id, detected_module);

	if (expected_module != NULL && strcmp(expected_module, detected_module) != 0) {
		LOG_WRN("SAA module mismatch: firmware expects %s but module ID reports %s",
			expected_module, detected_module);
	}

	return 0;
}

SYS_INIT(saa_module_autodetect_init, APPLICATION, CONFIG_SAA_MODULE_AUTODETECT_INIT_PRIORITY);

#endif
