/* SPDX-License-Identifier: MIT */
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zmk/event_manager.h>
#include <cormoran/zmk/custom_settings.h>
#include <cormoran/runtime_combo/runtime_combo.h>

LOG_MODULE_REGISTER(ananta_combo_settings, LOG_LEVEL_INF);

/* NVS walks newer records first. A newly saved combo can therefore be
 * validated before the older behavior/local_id table has been restored.
 * Preload that table, retaining the IDs used by existing Studio keymaps.
 * Run after application device initialization and before main's full load.
 */
static int preload_behavior_ids(void) {
    int ret = settings_subsys_init();
    if (ret == 0) {
        ret = settings_load_subtree("behavior");
    }
    LOG_INF("Behavior ID preload result=%d", ret);
    return ret;
}
SYS_INIT(preload_behavior_ids, APPLICATION, 99);

/* Mock module keys can emit startup releases before main loads settings.
 * Rebuild any cache populated by those events once persisted combos exist.
 */
static int combo_settings_ready(const zmk_event_t *event) {
    if (as_zmk_custom_settings_initialized(event)) {
        zmk_runtime_combo_invalidate_cache();
        LOG_INF("Restored combo cache: %u slots", zmk_runtime_combo_count());
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(ananta_combo_settings, combo_settings_ready);
ZMK_SUBSCRIPTION(ananta_combo_settings, zmk_custom_settings_initialized);
