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
    if (!(buf[0] & BIT(7)) || all_ff) {
        data->dx = 0;
        data->dy = 0;
        if ((x || y || all_ff) && now - guard_log_time >= 1000) {
            guard_log_time = now;
            LOG_WRN("PMW rejected motion=%02x squal=%u raw=%d/%d all_ff=%d",
                    buf[0], buf[4], x, y, all_ff);
        }
        return 0;
    }
    static int64_t sample_log_time;
    if ((x || y) && now - sample_log_time >= 1000) {
        sample_log_time = now;
        LOG_INF("PMW sample motion=%02x squal=%u raw=%d/%d",
                buf[0], buf[4], x, y);
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


if __name__ == "__main__":
    Path(sys.argv[2]).write_text(patch(Path(sys.argv[1]).read_text()))

