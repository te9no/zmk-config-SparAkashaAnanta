#define DT_DRV_COMPAT zmk_input_module_sensor_proxy

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/input_module.h>

LOG_MODULE_DECLARE(zmk_input_module, CONFIG_ZMK_INPUT_MODULE_LOG_LEVEL);

struct input_module_sensor_route_config {
	uint32_t profile_id;
	const struct device *sensor;
	uint16_t triggers_per_rotation;
};

struct input_module_sensor_proxy_config {
	const struct input_module_sensor_route_config *routes;
	size_t routes_len;
};

struct input_module_sensor_proxy_data {
	const struct input_module_sensor_route_config *active_route;
	struct sensor_value remainder;
};

static const struct input_module_sensor_route_config *
find_active_route(const struct device *dev)
{
	const struct input_module_sensor_proxy_config *config = dev->config;
	uint32_t profile_id = zmk_input_module_is_applied() ? zmk_input_module_applied_get()
							    : zmk_input_module_selected_get();

	for (size_t i = 0; i < config->routes_len; i++) {
		if (config->routes[i].profile_id == profile_id) {
			return &config->routes[i];
		}
	}

	return NULL;
}

static const struct input_module_sensor_route_config *
refresh_active_route(const struct device *dev)
{
	struct input_module_sensor_proxy_data *data = dev->data;
	const struct input_module_sensor_route_config *route = find_active_route(dev);

	if (route != data->active_route) {
		data->active_route = route;
		data->remainder = (struct sensor_value){0};
	}

	return route;
}

static int input_module_sensor_proxy_sample_fetch(const struct device *dev,
						  enum sensor_channel chan)
{
	const struct input_module_sensor_route_config *route = refresh_active_route(dev);

	if (route == NULL) {
		return -ENODEV;
	}

	return sensor_sample_fetch_chan(route->sensor, chan);
}

static int normalize_rotation_value(const struct device *dev,
				    const struct input_module_sensor_route_config *route,
				    const struct sensor_value *raw, struct sensor_value *val)
{
	struct input_module_sensor_proxy_data *data = dev->data;
	int trigger_degrees;
	int triggers;

	if (route->triggers_per_rotation == 0) {
		*val = *raw;
		return 0;
	}

	trigger_degrees = 360 / route->triggers_per_rotation;
	if (trigger_degrees <= 0) {
		return -EINVAL;
	}

	data->remainder.val1 += raw->val1;
	data->remainder.val2 += raw->val2;
	data->remainder.val1 += data->remainder.val2 / 1000000;
	data->remainder.val2 %= 1000000;

	triggers = data->remainder.val1 / trigger_degrees;
	data->remainder.val1 %= trigger_degrees;

	*val = (struct sensor_value){
		.val1 = 0,
		.val2 = triggers,
	};

	return 0;
}

static int input_module_sensor_proxy_channel_get(const struct device *dev,
						 enum sensor_channel chan,
						 struct sensor_value *val)
{
	const struct input_module_sensor_route_config *route = refresh_active_route(dev);
	struct sensor_value raw;
	int ret;

	if (route == NULL) {
		return -ENODEV;
	}

	ret = sensor_channel_get(route->sensor, chan, &raw);
	if (ret < 0) {
		return ret;
	}

	if (chan == SENSOR_CHAN_ROTATION) {
		return normalize_rotation_value(dev, route, &raw, val);
	}

	*val = raw;
	return 0;
}

static int input_module_sensor_proxy_trigger_set(const struct device *dev,
						 const struct sensor_trigger *trig,
						 sensor_trigger_handler_t handler)
{
	const struct input_module_sensor_route_config *route = refresh_active_route(dev);

	if (route == NULL) {
		LOG_DBG("%s has no active sensor route for profile %s", dev->name,
			zmk_input_module_profile_name(zmk_input_module_selected_get()));
		return 0;
	}

	if (!device_is_ready(route->sensor)) {
		LOG_WRN("%s selected sensor %s is not ready", dev->name, route->sensor->name);
		return -ENODEV;
	}

	return sensor_trigger_set(route->sensor, trig, handler);
}

static const struct sensor_driver_api input_module_sensor_proxy_api = {
	.trigger_set = input_module_sensor_proxy_trigger_set,
	.sample_fetch = input_module_sensor_proxy_sample_fetch,
	.channel_get = input_module_sensor_proxy_channel_get,
};

#define ROUTE_ENTRY(node_id)                                                                       \
	{                                                                                          \
		.profile_id = DT_PROP(node_id, profile_id),                                        \
		.sensor = DEVICE_DT_GET(DT_PHANDLE(node_id, sensor)),                              \
		.triggers_per_rotation = DT_PROP_OR(node_id, triggers_per_rotation, 0),             \
	},

#define ROUTES_NAME(n) _CONCAT(input_module_sensor_proxy_routes_, n)

#define SENSOR_PROXY_DEFINE(n)                                                                     \
	static const struct input_module_sensor_route_config ROUTES_NAME(n)[] = {                   \
		DT_INST_FOREACH_CHILD(n, ROUTE_ENTRY)};                                            \
	static const struct input_module_sensor_proxy_config input_module_sensor_proxy_config_##n = {\
		.routes = ROUTES_NAME(n),                                                        \
		.routes_len = ARRAY_SIZE(ROUTES_NAME(n)),                                        \
	};                                                                                         \
	static struct input_module_sensor_proxy_data input_module_sensor_proxy_data_##n;             \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, &input_module_sensor_proxy_data_##n,                    \
			      &input_module_sensor_proxy_config_##n, POST_KERNEL,                   \
			      CONFIG_SENSOR_INIT_PRIORITY, &input_module_sensor_proxy_api);

DT_INST_FOREACH_STATUS_OKAY(SENSOR_PROXY_DEFINE)
