"""Characterize pending-motion loss using the supplied driver's actual C block.

Read-only host diagnostic, not a hardware test or a fix verification.
Usage: python3 scripts/audit_pmw_report_gap.py /path/to/pmw3610.c
"""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    source = Path(sys.argv[1]).read_text()
    start = source.index("    if (report_interval_min_ms > 0) {\n        // purge accumulated delta")
    end = source.index("    // fetch report value", start)
    block = source[start:end]
    harness = r'''
#include <stdint.h>
#include <stdio.h>
struct state { int32_t dx, dy; int64_t last_smp_time, last_rpt_time; };
static int step(struct state *data, int64_t now, int x, int y) {
    uint32_t report_interval_min_ms = 14;
''' + block + r'''
    printf("report at %lld ms: %ld/%ld\n", (long long)now,
           (long)data->dx, (long)data->dy);
    int result = data->dx;
    data->dx = data->dy = 0;
    data->last_rpt_time = now;
    return result;
}
int main(void) {
    struct state s = {0};
    step(&s, 5, 10, 0);
    printf("pending at 5 ms: %ld\n", (long)s.dx);
    int reported = step(&s, 25, 1, 0);
    printf("input total=11, reported=%d, pending=%ld, lost=%ld\n",
           reported, (long)s.dx, (long)(11 - reported - s.dx));
    return reported == 1 && s.dx == 0 ? 0 : 1;
}
'''
    with tempfile.TemporaryDirectory(prefix="saa-pmw-gap-") as directory:
        src = Path(directory) / "gap.c"
        exe = Path(directory) / "gap"
        src.write_text(harness)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("Confirmed the characterized loss path; actual device timing is not measured.")


if __name__ == "__main__":
    main()
