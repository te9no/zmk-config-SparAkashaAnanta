#pragma once

#include <saa/module_select.h>
#include <zmk/input_module.h>

static inline int saa_module_mux_apply(enum saa_module_profile profile)
{
	return zmk_input_module_apply(profile);
}

static inline enum saa_module_profile saa_module_mux_get_applied(void)
{
	return (enum saa_module_profile)zmk_input_module_applied_get();
}

static inline bool saa_module_mux_is_applied(void)
{
	return zmk_input_module_is_applied();
}
