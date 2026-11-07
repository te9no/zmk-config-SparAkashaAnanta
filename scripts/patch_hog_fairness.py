"""Bound each HOG worker to one report so competing report types get a turn."""
import hashlib
from pathlib import Path
import sys

SOURCE_SHA256 = "2e8c12bb4197173d238a6be1e33abf2db5dedbd4a6cf29a83d5e2f6c79d4919b"

def patch(source):
    source = source.replace("\r\n", "\n")
    if hashlib.sha256(source.encode()).hexdigest() != SOURCE_SHA256:
        raise ValueError("HOG source changed; review queue fairness against the new revision")
    for kind in ("keyboard", "consumer", "mouse"):
        start = source.index(f"void send_{kind}_report_callback(")
        end = source.index(f"K_WORK_DEFINE(hog_{kind}_work", start)
        old = source[start:end]
        new = old.replace(
            f"while (k_msgq_get(&zmk_hog_{kind}_msgq, &report, K_NO_WAIT) == 0)",
            f"if (k_msgq_get(&zmk_hog_{kind}_msgq, &report, K_NO_WAIT) == 0)")
        hook = "        bt_conn_unref(conn);"
        assert old.count(hook) == 1 and new != old
        new = new.replace(hook, hook + f"""
        /* Producers may refill this queue while notification waits for a TX
         * buffer. Return to the shared queue after one report so keyboard,
         * consumer and mouse work all get a turn. Preserve each FIFO. */
        if (k_msgq_num_used_get(&zmk_hog_{kind}_msgq) > 0) {{
            k_work_submit_to_queue(&hog_work_q, work);
        }}""")
        source = source[:start] + new + source[end:]
    return source

if __name__ == "__main__":
    Path(sys.argv[2]).write_text(patch(Path(sys.argv[1]).read_text()))
