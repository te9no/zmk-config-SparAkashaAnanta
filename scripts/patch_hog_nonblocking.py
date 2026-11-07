"""Replace pinned HOG queue implementation with bounded, nonblocking dispatch."""
import hashlib
from pathlib import Path
import sys
SOURCE_SHA256="2e8c12bb4197173d238a6be1e33abf2db5dedbd4a6cf29a83d5e2f6c79d4919b"
def patch(source):
    source=source.replace("\r\n","\n")
    if hashlib.sha256(source.encode()).hexdigest()!=SOURCE_SHA256:
        raise ValueError("HOG source changed; nonblocking path requires review")
    start=source.index("K_THREAD_STACK_DEFINE(hog_q_stack")
    return source[:start]+Path(__file__).resolve().parents[1].joinpath("src/hog_nonblocking.inc").read_text()
if __name__=="__main__":
    Path(sys.argv[2]).write_text(patch(Path(sys.argv[1]).read_text()))
