#pragma once

#include <zmk/input_module.h>

enum saa_module_profile {
	SAA_MODULE_PROFILE_UNSPECIFIED = 0,
	SAA_MODULE_PROFILE_KEY = 1,
	SAA_MODULE_PROFILE_ENC = 2,
	SAA_MODULE_PROFILE_JOY = 3,
	SAA_MODULE_PROFILE_TB = 4,
	SAA_MODULE_PROFILE_TPD = 5,
};

#define saa_module_capabilities zmk_input_module_capabilities

static inline enum saa_module_profile saa_module_select_get(void)
{
	return (enum saa_module_profile)zmk_input_module_selected_get();
}

static inline const char *saa_module_select_get_name(void)
{
	return zmk_input_module_profile_name(zmk_input_module_selected_get());
}

static inline const char *saa_module_select_name(enum saa_module_profile profile)
{
	return zmk_input_module_profile_name(profile);
}

static inline struct saa_module_capabilities
saa_module_select_capabilities(enum saa_module_profile profile)
{
	return zmk_input_module_profile_capabilities(profile);
}

static inline struct saa_module_capabilities saa_module_select_get_capabilities(void)
{
	return zmk_input_module_selected_capabilities();
}

static inline int saa_module_select_set(enum saa_module_profile profile)
{
	return zmk_input_module_select_set(profile);
}
