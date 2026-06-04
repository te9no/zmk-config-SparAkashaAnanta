#pragma once

#include <stdbool.h>
#include <stdint.h>

enum saa_module_profile {
	SAA_MODULE_PROFILE_UNSPECIFIED = 0,
	SAA_MODULE_PROFILE_KEY = 1,
	SAA_MODULE_PROFILE_ENC = 2,
	SAA_MODULE_PROFILE_JOY = 3,
	SAA_MODULE_PROFILE_TB = 4,
	SAA_MODULE_PROFILE_TPD = 5,
};

struct saa_module_capabilities {
	bool spi;
	bool i2c;
	bool adc;
	bool encoder;
};

enum saa_module_profile saa_module_select_get(void);
const char *saa_module_select_get_name(void);
const char *saa_module_select_name(enum saa_module_profile profile);
struct saa_module_capabilities saa_module_select_capabilities(enum saa_module_profile profile);
struct saa_module_capabilities saa_module_select_get_capabilities(void);
int saa_module_select_set(enum saa_module_profile profile);
