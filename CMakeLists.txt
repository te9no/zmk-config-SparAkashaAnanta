# Copyright (c) 2026
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_BEHAVIOR_DEBUG_TOGGLE)
  target_sources(app PRIVATE src/behavior_debug_toggle.c)
endif()
