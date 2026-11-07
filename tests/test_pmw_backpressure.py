import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("guard",ROOT/"scripts/patch_pmw_guard.py")
guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)
class Backpressure(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("PMW_SOURCE"),"set PMW_SOURCE")
    def test_partial_enqueue_and_cancelled_axis(self):
        s=guard.retain_rejected_input(Path(os.environ["PMW_SOURCE"]).read_text())
        block=s[s.index("    // fetch report value"):s.index("\n    return 0;\n}",s.index("    // fetch report value"))]
        code=r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#define K_NO_WAIT 0
#define CLAMP(v,l,h) ((v)<(l)?(l):(v)>(h)?(h):(v))
struct state {int dx,dy;bool saa_sync_pending;int64_t last_rpt_time;};
struct cfg {int evt_type,x_input_code,y_input_code;};
static int fail_x,fail_y,received_x,received_y,syncs;
static int input_report(void *dev,int type,int code,int value,bool sync,int timeout){
 if((code==0 && fail_x)||(code==1 && fail_y))return -1;
 if(code==0)received_x+=value;else received_y+=value;
 if(sync)syncs++;
 return 0;
}
static void flush(struct state *data){
 struct cfg c={0,0,1};const struct cfg *config=&c;void *dev=0;
 int report_interval_min_ms=14;int64_t now=100;
""" + block + r"""
}
int main(void){
 struct state s={.dx=10,.dy=20};
 fail_y=1;flush(&s);
 assert(s.dx==0 && s.dy==20 && s.saa_sync_pending);
 assert(received_x==10 && received_y==0 && syncs==0);
 /* Fresh reverse Y motion cancels pending Y: X still needs a sync. */
 s.dy-=20;fail_y=0;flush(&s);
 assert(!s.saa_sync_pending && received_x==10 && syncs==1);
 s.dx=70000;s.dy=-70000;fail_x=fail_y=1;flush(&s);
 assert(s.dx==70000 && s.dy==-70000);
 fail_x=fail_y=0;
 for(int i=0;i<3;i++)flush(&s);
 assert(s.dx==0 && s.dy==0 && received_x==70010 && received_y==-70000);
 s.dx=4;s.dy=5;fail_x=1;flush(&s);
 assert(s.dx==4 && s.dy==0);
 fail_x=0;flush(&s);
 assert(received_x==70014 && received_y==-69995);
 return 0;
}
"""
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/"t.c";e=Path(d)/"t";c.write_text(code)
            subprocess.run(["cc","-std=c11","-Wall","-Werror",str(c),"-o",str(e)],check=True)
            subprocess.run([str(e)],check=True)
