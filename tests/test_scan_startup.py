import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from patch_scan_startup import physical, main, START

HARNESS=r'''
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define IS_ENABLED(x) x
#define CONFIG_PM_DEVICE_RUNTIME 0
#define CONFIG_PM_DEVICE 1
#define PM_DEVICE_ACTION_RESUME 1
#define PM_DEVICE_ACTION_SUSPEND 2
#define ZMK_KEYMAP_LEN 58
#define LOG_ERR(...) ((void)0)
struct device { int id; };
struct zmk_physical_layout { const struct device *kscan; };
static struct device dev={1};
static struct zmk_physical_layout a={&dev}, b={&dev};
static const struct zmk_physical_layout *active;
static int configured,restored,early,unconfigured,events,fail_config;
static uint32_t selected_to_stock_map[58];
static void zmk_physical_layout_kscan_callback(void) {}
static int kscan_config(const struct device *d,void (*cb)(void)) {
    if(fail_config)return -EIO;
    configured=1;return 0;
}
static int kscan_disable_callback(const struct device *d) {return 0;}
static void synchronous_scan(void) {
    events++;if(!restored)early++;if(!configured)unconfigured++;
}
static int pm_device_action_run(const struct device *d,int action) {
    if(action==PM_DEVICE_ACTION_RESUME)synchronous_scan();return 0;
}
static int kscan_enable_callback(const struct device *d) {synchronous_scan();return 0;}
static int get_index_of_layout(const struct zmk_physical_layout *p){return p==&a?0:1;}
static const struct zmk_physical_layout *get_default_layout(void){return &a;}
static int zmk_physical_layouts_get_position_map(int a,int b,int c,uint32_t *map){return 0;}
SOURCE
int main(void) {
    assert(zmk_physical_layouts_select_layout(&a)==0);
    /* Persisted layout selection occurs during settings load. */
    assert(zmk_physical_layouts_select_layout(&b)==0);
#if FIXED
    assert(events==0 && active==&b);
    restored=1;
    fail_config=1;
    assert(saa_start_scanning_after_settings()==-EIO && events==0);
    fail_config=0;
    assert(saa_start_scanning_after_settings()==0);
    assert(events>0 && early==0 && unconfigured==0);
    int before=events;
    assert(saa_start_scanning_after_settings()==0 && events==before);
    assert(zmk_physical_layouts_select_layout(&a)==0 && events>before);
    assert(early==0 && unconfigured==0);
    puts("FIXED: configured before resume; no scan before settings; retry/idempotence pass");
#else
    assert(early>0 && unconfigured>0);
    puts("BASELINE: reproduced scan before callback/configuration readiness");
#endif
}
'''
class ScanStartup(unittest.TestCase):
    def test_original_and_fixed_lifecycle(self):
        if not os.environ.get('HOG_SOURCE'):self.skipTest('set HOG_SOURCE')
        original=(Path(os.environ['HOG_SOURCE']).parent/'physical_layouts.c').read_text()
        for fixed,source in ((False,original),(True,physical(original))):
            begin=source.index('int zmk_physical_layouts_select_layout(')
            end=source.index('\nint zmk_physical_layouts_select(uint8_t index)',begin)
            selected=(START if fixed else '')+source[begin:end]
            with tempfile.TemporaryDirectory() as tmp:
                p=Path(tmp);(p/'test.c').write_text(HARNESS.replace('SOURCE',selected))
                subprocess.run(['gcc','-std=c11',f'-DFIXED={int(fixed)}',str(p/'test.c'),'-o',str(p/'test')],check=True)
                subprocess.run([str(p/'test')],check=True)
    def test_main_after_full_settings_load(self):
        if not os.environ.get('HOG_SOURCE'):self.skipTest('set HOG_SOURCE')
        source=main((Path(os.environ['HOG_SOURCE']).parent/'main.c').read_text())
        self.assertLess(source.index('settings_load();'),source.index('int scan_err = saa_start_scanning_after_settings();'))
        self.assertLess(source.index('int scan_err ='),source.index('zmk_display_init();'))
