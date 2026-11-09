"""Delay local scan activation until main has restored persisted settings."""
MAIN_SHA = "5dc27ac6c8b99209011b5f6f44d25b9039379f1a2bc256a0b347bf234f338d4b"
START = '''static bool saa_scanning_ready;

static int saa_start_scanner(const struct zmk_physical_layout *layout) {
    if (!layout->kscan) {
        return 0;
    }
    /* PM resume may synchronously scan and invoke the callback. */
    int err = kscan_config(layout->kscan, zmk_physical_layout_kscan_callback);
    if (err < 0) {
        return err;
    }
#if IS_ENABLED(CONFIG_PM_DEVICE_RUNTIME)
    err = pm_device_runtime_get(layout->kscan);
#elif IS_ENABLED(CONFIG_PM_DEVICE)
    err = pm_device_action_run(layout->kscan, PM_DEVICE_ACTION_RESUME);
    if (err == -EALREADY || err == -ENOSYS) {
        err = 0;
    }
#endif
    if (err < 0) {
        return err;
    }
    return kscan_enable_callback(layout->kscan);
}

/* Called once from main, after settings_load() has completed all handlers. */
int saa_start_scanning_after_settings(void) {
    if (saa_scanning_ready) {
        return 0;
    }
    if (!active) {
        return -ENODEV;
    }
    int err = saa_start_scanner(active);
    if (err == 0) {
        saa_scanning_ready = true;
    }
    return err;
}

'''
def physical(source):
    marker='int zmk_physical_layouts_select_layout(const struct zmk_physical_layout *dest_layout) {'
    assert source.count(marker)==1
    begin=source.index(marker)
    end=source.index('\nint zmk_physical_layouts_select(uint8_t index)',begin)
    old=source[begin:end]
    start=old.index('    if (active->kscan) {',old.index('    active = dest_layout;'))
    finish=old.index('\n    return 0;\n}',start)
    new=old[:start]+'''    if (saa_scanning_ready) {
        return saa_start_scanner(active);
    }
'''+old[finish:]
    new=new.replace('    if (active) {','    if (active && saa_scanning_ready) {')
    return source[:begin]+START+new+source[end:]

def main(source):
    marker='int main(void) {'
    assert source.count(marker)==1
    source=source.replace(marker,'extern int saa_start_scanning_after_settings(void);\n\n'+marker)
    marker='#ifdef CONFIG_ZMK_DISPLAY\n    zmk_display_init();'
    assert source.count(marker)==1
    return source.replace(marker,'''    int scan_err = saa_start_scanning_after_settings();
    if (scan_err < 0) {
        LOG_ERR("Unable to start scanner after settings: %d", scan_err);
    }

'''+marker)
