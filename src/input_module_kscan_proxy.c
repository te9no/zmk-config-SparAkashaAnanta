#define DT_DRV_COMPAT zmk_input_module_kscan_proxy

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <zmk/input_module.h>

LOG_MODULE_DECLARE(zmk_input_module, CONFIG_ZMK_INPUT_MODULE_LOG_LEVEL);

struct input_module_kscan_route_config {
	uint32_t profile_id;
	const struct device *kscan;
};

struct input_module_kscan_proxy_config {
	const struct input_module_kscan_route_config *routes;
	size_t routes_len;
	kscan_callback_t child_callback;
};

struct input_module_kscan_proxy_data {
	const struct input_module_kscan_route_config *active_route;
	kscan_callback_t callback;
};

static const struct input_module_kscan_route_config *
find_active_route(const struct device *dev)
{
	const struct input_module_kscan_proxy_config *config = dev->config;
	uint32_t profile_id = zmk_input_module_is_applied() ? zmk_input_module_applied_get()
							    : zmk_input_module_selected_get();

	for (size_t i = 0; i < config->routes_len; i++) {
		if (config->routes[i].profile_id == profile_id) {
			return &config->routes[i];
		}
	}

	return NULL;
}

static const struct input_module_kscan_route_config *
refresh_active_route(const struct device *dev)
{
	struct input_module_kscan_proxy_data *data = dev->data;
	const struct input_module_kscan_route_config *route = find_active_route(dev);

	if (route != data->active_route) {
		data->active_route = route;
	}

	return route;
}

static void input_module_kscan_proxy_child_callback(const struct device *dev,
						    const struct device *child_dev, uint32_t row,
						    uint32_t column, bool pressed)
{
	struct input_module_kscan_proxy_data *data = dev->data;
	const struct input_module_kscan_route_config *route = refresh_active_route(dev);

	if (route == NULL || route->kscan != child_dev || data->callback == NULL) {
		return;
	}

	data->callback(dev, row, column, pressed);
}

static int input_module_kscan_proxy_configure(const struct device *dev,
					      kscan_callback_t callback)
{
	const struct input_module_kscan_proxy_config *config = dev->config;
	struct input_module_kscan_proxy_data *data = dev->data;
	const struct input_module_kscan_route_config *route = refresh_active_route(dev);

	if (callback == NULL) {
		return -EINVAL;
	}

	data->callback = callback;

	if (route == NULL) {
		LOG_DBG("%s has no active kscan route for profile %s", dev->name,
			zmk_input_module_profile_name(zmk_input_module_applied_get()));
		return 0;
	}

	if (!device_is_ready(route->kscan)) {
		LOG_WRN("%s selected kscan %s is not ready", dev->name, route->kscan->name);
		return -ENODEV;
	}

	return kscan_config(route->kscan, config->child_callback);
}

static int input_module_kscan_proxy_enable_callback(const struct device *dev)
{
	const struct input_module_kscan_route_config *route = refresh_active_route(dev);

	if (route == NULL) {
		return 0;
	}

	if (!device_is_ready(route->kscan)) {
		LOG_WRN("%s selected kscan %s is not ready", dev->name, route->kscan->name);
		return -ENODEV;
	}

	return kscan_enable_callback(route->kscan);
}

static int input_module_kscan_proxy_disable_callback(const struct device *dev)
{
	const struct input_module_kscan_route_config *route = refresh_active_route(dev);

	if (route == NULL || !device_is_ready(route->kscan)) {
		return 0;
	}

	return kscan_disable_callback(route->kscan);
}

static const struct kscan_driver_api input_module_kscan_proxy_api = {
	.config = input_module_kscan_proxy_configure,
	.enable_callback = input_module_kscan_proxy_enable_callback,
	.disable_callback = input_module_kscan_proxy_disable_callback,
};

#define ROUTE_ENTRY(node_id)                                                                       \
	{                                                                                          \
		.profile_id = DT_PROP(node_id, profile_id),                                        \
		.kscan = DEVICE_DT_GET(DT_PHANDLE(node_id, kscan)),                                \
	},

#define ROUTES_NAME(n) _CONCAT(input_module_kscan_proxy_routes_, n)
#define CHILD_CALLBACK_NAME(n) _CONCAT(input_module_kscan_proxy_child_callback_, n)

#define KSCAN_PROXY_DEFINE(n)                                                                      \
	static void CHILD_CALLBACK_NAME(n)(const struct device *child_dev, uint32_t row,           \
					   uint32_t column, bool pressed)                         \
	{                                                                                          \
		input_module_kscan_proxy_child_callback(DEVICE_DT_GET(DT_DRV_INST(n)), child_dev,  \
						       row, column, pressed);                         \
	}                                                                                          \
	static const struct input_module_kscan_route_config ROUTES_NAME(n)[] = {                  \
		DT_INST_FOREACH_CHILD(n, ROUTE_ENTRY)};                                           \
	static const struct input_module_kscan_proxy_config input_module_kscan_proxy_config_##n = {\
		.routes = ROUTES_NAME(n),                                                        \
		.routes_len = ARRAY_SIZE(ROUTES_NAME(n)),                                        \
		.child_callback = CHILD_CALLBACK_NAME(n),                                        \
	};                                                                                         \
	static struct input_module_kscan_proxy_data input_module_kscan_proxy_data_##n;             \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, &input_module_kscan_proxy_data_##n,                   \
			      &input_module_kscan_proxy_config_##n, POST_KERNEL,                   \
			      CONFIG_KSCAN_INIT_PRIORITY, &input_module_kscan_proxy_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_PROXY_DEFINE)
