#pragma once

#include <stdbool.h>
#include <stdint.h>

struct zmk_input_module_capabilities {
	uint32_t flags;
	bool kscan;
	bool encoder;
	bool adc;
	bool spi;
	bool i2c;
};

uint32_t zmk_input_module_selected_get(void);
uint32_t zmk_input_module_applied_get(void);
bool zmk_input_module_is_applied(void);
const char *zmk_input_module_profile_name(uint32_t profile_id);
uint32_t zmk_input_module_profile_flags(uint32_t profile_id);
struct zmk_input_module_capabilities zmk_input_module_profile_capabilities(uint32_t profile_id);
struct zmk_input_module_capabilities zmk_input_module_selected_capabilities(void);
int zmk_input_module_select_set(uint32_t profile_id);
int zmk_input_module_apply(uint32_t profile_id);
