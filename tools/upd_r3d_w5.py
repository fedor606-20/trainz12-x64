p='README.md'
s=open(p,encoding='utf-8').read()
import re
s=re.sub(r'\| Render3DCore_7c \| 458 KB \| 3,168 \| \d+/2500 \| [^|]+ \|','| Render3DCore_7c | 458 KB | 3,168 | 2337/2500 | 🟡 partial |',s)
open(p,'w',encoding='utf-8').write(s)
p='notes/Render3DCore.md'
s=open(p,encoding='utf-8').read().replace('1938/2500','2337/2500')
s += """

## Wave 5 (2026-11-24/25)

Wave 5 merged: U5a final (118 data/vtable/dtor/factory/render exports), U5b final (+91), U10 live paths
(39/39), U9b seven local definitions (six still call private RVAs), 108 template/ITree/Line exports, 43
tails2 and 27 tails. Module **2337/2500**, 163 forwarded. New tests: `u5a_final` 3,776, `u5b_final`
127,177, `tails2` 4,174, `templates` 30,852, `livepaths` 80,002; all 0 mismatches.

Wave 6 covers the last 163: U5b plugin/state (48), simple classes Camera/DebugRender/Tube/Transformation/
Primitive3D/ViewPoint/ViewPort/LightLoader (~76), tails3 (27) and the two material chunk paths + AutoTex
plugin vtable (4).
"""
open(p,'w',encoding='utf-8').write(s)
print('ok')
