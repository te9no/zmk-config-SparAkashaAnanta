import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('scaler', ROOT / 'scripts/patch_runtime_scaling.py')
scaler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scaler)


class Scaling(unittest.TestCase):
    def test_changed_source_rejected(self):
        with self.assertRaises(ValueError):
            scaler.patch('changed')

    def test_compiled_pinned_function(self):
        source = ROOT.parents[1] / '.zmk-workspace/profiles/saa-xiao-pinmux/west/modules/zmk-module-runtime-input-processor/src/pointing/input_processor_runtime.c'
        if not source.exists():
            self.skipTest('pinned west checkout required')
        patched = scaler.patch(source.read_text())
        function = patched.split('static int scale_val(', 1)[1].split('static int runtime_processor_handle_event', 1)[0]
        harness = '''
#include <stdint.h>
#include <assert.h>
#define CLAMP(x,l,h) ((x)<(l)?(l):((x)>(h)?(h):(x)))
#define LOG_DBG(...) ((void)0)
#define ZMK_INPUT_PROC_CONTINUE 0
struct input_event { int32_t value; };
struct zmk_input_processor_state { int16_t *remainder; };
static int scale_val(''' + function + '''
int main(void) {
    int16_t remainder = 0;
    struct zmk_input_processor_state state = {&remainder};
    struct input_event event = {1000};
    scale_val(&event, 100, 100, &state); assert(event.value == 1000);
    event.value = -1000; scale_val(&event, 100, 100, &state); assert(event.value == -1000);
    event.value = 1000; scale_val(&event, 100, 200, &state); assert(event.value == 500);
    event.value = 1000; scale_val(&event, 100, 1, &state); assert(event.value == 32767 && remainder == 0);
    event.value = -1000; scale_val(&event, 100, 1, &state); assert(event.value == -32768 && remainder == 0);
    event.value = 1; scale_val(&event, 1, 2, &state); assert(event.value == 0 && remainder == 1);
    event.value = 1; scale_val(&event, 1, 2, &state); assert(event.value == 1 && remainder == 0);
    event.value = 1; scale_val(&event, 40000, 40000, &state); assert(event.value == 1);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'test.c'
            exe = Path(tmp) / 'test'
            src.write_text(harness)
            subprocess.run(['cc', '-Wall', '-Werror', '-fsanitize=undefined', str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_layer_tap(self):
        keymap = (ROOT / 'config/SAA.keymap').read_text()
        block = keymap.split('&lt {', 1)[1].split('};', 1)[0]
        self.assertIn('flavor = "balanced"', block)
        self.assertIn('require-prior-idle-ms = <150>', block)
