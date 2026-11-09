#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/pm/device.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
LOG_MODULE_REGISTER(scan_test);
#define ZMK_KEYMAP_LEN 58
struct zmk_physical_layout { const struct device *kscan; };
static const struct zmk_physical_layout *active;
static uint32_t selected_to_stock_map[58];
static bool settings_restored, held=true, enabled, last_held;
static kscan_callback_t callback;
static int early, missing_callback, reads, delivered, last_pressed;
static void tick(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(scan_work,tick);
static void scan(void) {
    reads++;
    if(!settings_restored) early++;
    if(!callback) missing_callback++;
    if(held!=last_held) {
        last_held=held;
        if(callback) callback(active->kscan,0,1,held);
    }
}
static void tick(struct k_work *work) {
    if(enabled) {scan();k_work_reschedule(&scan_work,K_MSEC(1));}
}
static int configure(const struct device *dev,kscan_callback_t cb) {callback=cb;return 0;}
static int enable(const struct device *dev) {
    enabled=true;scan();k_work_reschedule(&scan_work,K_MSEC(1));return 0;
}
static int disable(const struct device *dev) {
    enabled=false;k_work_cancel_delayable(&scan_work);return 0;
}
static int scanner_pm(const struct device *dev,enum pm_device_action action) {
    if(action==PM_DEVICE_ACTION_RESUME)return enable(dev);
    if(action==PM_DEVICE_ACTION_SUSPEND)return disable(dev);
    return -ENOTSUP;
}
static int scanner_init(const struct device *dev) {pm_device_init_suspended(dev);return 0;}
static const struct kscan_driver_api api={.config=configure,.enable_callback=enable,.disable_callback=disable};
PM_DEVICE_DEFINE(scanner,scanner_pm);
DEVICE_DEFINE(scanner,"scanner",scanner_init,PM_DEVICE_GET(scanner),NULL,NULL,POST_KERNEL,80,&api);
static const struct zmk_physical_layout a={DEVICE_GET(scanner)},b={DEVICE_GET(scanner)};
static void zmk_physical_layout_kscan_callback(const struct device *dev,uint32_t row,uint32_t col,bool pressed) {
    delivered++;last_pressed=pressed;
}
static int get_index_of_layout(const struct zmk_physical_layout *p){return p==&a?0:1;}
static const struct zmk_physical_layout *get_default_layout(void){return &a;}
static int zmk_physical_layouts_get_position_map(int a,int b,int c,uint32_t *map){return 0;}
#include "selector.inc"
static int initial_layout(void) {return zmk_physical_layouts_select_layout(&a);}
SYS_INIT(initial_layout,APPLICATION,90);
/* Real settings_load invokes this handler; storage I/O itself is not modeled. */
static int restore_settings(void) {
    __ASSERT_NO_MSG(zmk_physical_layouts_select_layout(&b)==0);
    k_msleep(30); /* Permit actual system workqueue scheduling during restoration. */
#if FIXED
    __ASSERT_NO_MSG(reads==0 && delivered==0 && active==&b);
#endif
    settings_restored=true;return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(test_settings,"test",NULL,NULL,restore_settings,NULL);
#include "tested_main.inc"
int main(void) {
    __ASSERT_NO_MSG(tested_main()==0 && settings_restored);
    k_msleep(10);
#if FIXED
    __ASSERT_NO_MSG(early==0 && missing_callback==0 && delivered==1 && last_pressed);
    __ASSERT_NO_MSG(saa_start_scanning_after_settings()==0);
    held=false;k_msleep(10);
    __ASSERT_NO_MSG(delivered==2 && !last_pressed);
    for(int i=0;i<20;i++) {held=!held;k_msleep(3);}
    __ASSERT_NO_MSG(delivered==22 && !last_pressed);
    __ASSERT_NO_MSG(zmk_physical_layouts_select_layout(&a)==0);
    held=true;k_msleep(5);held=false;k_msleep(5);
    __ASSERT_NO_MSG(delivered==24 && !last_pressed && early==0 && missing_callback==0);
    __ASSERT_NO_MSG(pm_device_action_run(DEVICE_GET(scanner),PM_DEVICE_ACTION_SUSPEND)==0);
    int before=reads;held=true;k_msleep(10);
    __ASSERT_NO_MSG(reads==before);
    __ASSERT_NO_MSG(pm_device_action_run(DEVICE_GET(scanner),PM_DEVICE_ACTION_RESUME)==0);
    k_msleep(5);held=false;k_msleep(5);
    __ASSERT_NO_MSG(delivered==26 && !last_pressed && early==0 && missing_callback==0);
    printk("FIXED: early=%d missing_callback=%d delivered=%d; settings/held/release/layout-switch/suspend-resume PASS\n",early,missing_callback,delivered);
#else
    __ASSERT_NO_MSG(early>0 && missing_callback>0 && delivered==0);
    printk("BASELINE: early=%d missing_callback=%d held_press_lost=YES\n",early,missing_callback);
#endif
    disable(DEVICE_GET(scanner));
    printk("ALL_TESTS_PASS\n");return 0;
}
