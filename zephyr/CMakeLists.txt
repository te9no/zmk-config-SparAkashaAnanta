if(CONFIG_ZMK_SPLIT_ROLE_CENTRAL AND CONFIG_ZMK_RUNTIME_COMBO AND CONFIG_ZMK_BEHAVIOR_LOCAL_ID_TYPE_SETTINGS_TABLE)
  zephyr_library()
  zephyr_library_sources(combo_settings_order.c)
endif()
