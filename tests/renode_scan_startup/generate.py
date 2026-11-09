from pathlib import Path
import sys,hashlib
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from patch_key_state import HASH
from patch_scan_startup import physical,main,START
src=Path(sys.argv[1]);out=Path(sys.argv[2]);fixed=sys.argv[3]=='1'
p=(src/'physical_layouts.c').read_text();m=(src/'main.c').read_text()
assert hashlib.sha256(p.encode()).hexdigest()==HASH['physical_layouts.c']
assert hashlib.sha256(m.encode()).hexdigest()==HASH['main.c']
if fixed:p=physical(p);m=main(m)
a=p.index('int zmk_physical_layouts_select_layout(');b=p.index('\nint zmk_physical_layouts_select(uint8_t index)',a)
(out/'selector.inc').write_text((START if fixed else '')+p[a:b])
a=m.index('int main(void)')
(out/'tested_main.inc').write_text(m[a:].replace('int main(void)','static int tested_main(void)',1))
