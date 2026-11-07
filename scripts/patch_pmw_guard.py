"""Generate an Ananta-only driver variant; never modify the shared west module."""
import hashlib
from pathlib import Path
import sys

SOURCE_SHA256 = "993c19d9119313dbe9eb3ca4662b9aa92dc273fed4b510f874129e1cd9f42e00"


def patch(source):
    source = source.replace("\r\n", "\n")
    if hashlib.sha256(source.encode()).hexdigest() != SOURCE_SHA256:
        raise ValueError("PMW source changed: review guard against the new upstream before building")
    marker = '    LOG_DBG("x/y: %d/%d", x, y);'
    guard = '''
    /* Motion is latched before delta registers are read. Do not report stale
     * deltas when MOT is clear, or an all-ones disconnected-bus response.
     * Keep diagnostics bounded so a noisy sensor cannot flood CDC. */
    bool all_ff = true;
    for (size_t i = 0; i < sizeof(buf); ++i) {
        all_ff = all_ff && buf[i] == 0xff;
    }
    static int64_t guard_log_time;
    if (all_ff) {
        data->dx = 0;
        data->dy = 0;
        if ((x || y || all_ff) && now - guard_log_time >= 1000) {
            guard_log_time = now;
            LOG_WRN("PMW rejected motion=%02x squal=%u raw=%d/%d all_ff=%d",
                    buf[0], buf[4], x, y, all_ff);
        }
        return 0;
    }
    /* MOT clear invalidates this sample, not previously measured movement. */
    if (!(buf[0] & BIT(7))) {
        x = 0;
        y = 0;
    }
'''
    if source.count(marker) != 1:
        raise ValueError("Expected one motion reporting hook")
    source = source.replace(marker, guard + marker)
    old = '''        uint8_t perf = value & 0x0F; // reset bit[3..0] to 0x0 (normal operation)
        if (enabled) {
            perf |= 0xF0; // set bit[3..0] to 0xF (force awake)
        }'''
    if 'static int pmw3610_set_performance' in source:
        if source.count(old) != 1:
            raise ValueError("Expected performance control hook")
        source = source.replace(old, '''        /* Match efog normal-run mode; avoid forced-run activity transitions.
         * Studio frame capture's separate force-run path is unchanged. */
        ARG_UNUSED(enabled);
        uint8_t perf = 0x00;''')
    return source


def isolate_motion_work(source):
    submit = '    k_work_submit(&data->trigger_work);'
    init = '    k_work_init(&data->trigger_work, pmw3610_work_callback);'
    callback = 'static void pmw3610_gpio_callback('
    report = '    pmw3610_report_data(dev);\n    pmw3610_set_interrupt(dev, true);'
    for marker in (submit, init, callback, report):
        if source.count(marker) != 1:
            raise ValueError('Expected exactly one PMW motion queue hook: ' + marker)
    declarations = '''/* Ananta comparison: motion SPI must not occupy the key scan sysworkq.
 * Priority 9 yields to radio host threads (7/8), while the input queue (0)
 * and cooperative key/system queue remain ahead of us. No time slicing is
 * configured, so matching display priority can delay motion after a key redraw.
 * Started during serialized device init, before motion interrupts are enabled. */
K_THREAD_STACK_DEFINE(saa_pmw_stack, 2048);
static struct k_work_q saa_pmw_queue;
static bool saa_pmw_queue_started;

'''
    source = source.replace(callback, declarations + callback)
    source = source.replace(submit, '    k_work_reschedule_for_queue(&saa_pmw_queue, &data->trigger_work, K_NO_WAIT);')
    # The old shared queue implicitly serialized motion against activity work.
    # Keep the full motion transaction protected when moving to another thread.
    # Zephyr mutexes are recursive; report_data also takes this same mutex.
    source = source.replace(report, '''    k_mutex_lock(&data->lock, K_FOREVER);
    int err = pmw3610_report_data(dev);
    if (err < 0 || data->capture_active) {
        data->dx = 0;
        data->dy = 0;
    }
    if (!data->capture_active && (data->dx || data->dy)) {
        /* IRQ remains disabled until the pending report is drained. This
         * also flushes the final small movement when no new motion arrives. */
        int64_t remaining = (int64_t)data->runtime.report_interval_min_ms -
                            (k_uptime_get() - data->last_rpt_time);
        k_work_reschedule_for_queue(&saa_pmw_queue, &data->trigger_work,
                                    K_MSEC(MAX(remaining, 1)));
    } else if (!data->capture_active) {
        pmw3610_set_interrupt(dev, true);
    }
    k_mutex_unlock(&data->lock);''')
    source = source.replace(init, '''    if (!saa_pmw_queue_started) {
        k_work_queue_start(&saa_pmw_queue, saa_pmw_stack,
                           K_THREAD_STACK_SIZEOF(saa_pmw_stack), 9, NULL);
        k_thread_name_set(&saa_pmw_queue.thread, "saa_pmw_motion");
        saa_pmw_queue_started = true;
    }
''' + init.replace('k_work_init(', 'k_work_init_delayable('))
    source = source.replace(
        'CONTAINER_OF(work, struct pixart_data, trigger_work)',
        'CONTAINER_OF(k_work_delayable_from_work(work), struct pixart_data, trigger_work)')
    return source


def preserve_pending_motion(source):
    old = '''        // purge accumulated delta, if last sampled had not been reported on last report tick
        if (now - data->last_smp_time >= report_interval_min_ms) {
            data->dx = 0;
            data->dy = 0;
        }'''
    new = '''        /* A delayed sample does not invalidate previously measured movement.
         * Keep it for the next report; the report interval check below still
         * limits output frequency. Invalid-burst guards remain unchanged. */
'''
    if source.count(old) != 1:
        raise ValueError("Expected one PMW pending-motion purge")
    return source.replace(old, new)


if __name__ == "__main__":
    # Private headers are copied beside the generated driver so the shared
    # module and its other builds keep their original work item layout.
    source_dir = Path(sys.argv[1]).parent
    output_dir = Path(sys.argv[2]).parent
    header = (source_dir / 'pixart.h').read_text()
    old = 'struct k_work trigger_work;'
    if header.count(old) != 1:
        raise ValueError('Expected one private motion work item')
    (output_dir / 'pixart.h').write_text(header.replace(old, 'struct k_work_delayable trigger_work;'))
    (output_dir / 'pmw3610.h').write_text((source_dir / 'pmw3610.h').read_text())
    Path(sys.argv[2]).write_text(preserve_pending_motion(
        isolate_motion_work(patch(Path(sys.argv[1]).read_text()))))

