#pragma once

#include <stdbool.h>

#include <saa/module_select.h>

int saa_module_mux_apply(enum saa_module_profile profile);
enum saa_module_profile saa_module_mux_get_applied(void);
bool saa_module_mux_is_applied(void);
