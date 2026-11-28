p='README.md'
s=open(p,encoding='utf-8').read()
import re
s=re.sub(r'\| Render3DCore_7c \| 458 KB \| 3,168 \| \d+/2500 \| [^|]+ \|','| Render3DCore_7c | 458 KB | 3,168 | 2500/2500 | 🟡 cutover |',s)
open(p,'w',encoding='utf-8').write(s)
p='notes/Render3DCore.md'
s=open(p,encoding='utf-8').read().replace('2337/2500','2500/2500')
s += """

## Exports complete (2026-11-25)

All 2500 exports are local, 0 forwarders. U5b last 48, simple classes Camera/DebugRender/Tube/
Transformation/Primitive3D/ViewPoint/ViewPort/LightLoader 74, tails3 26, final 15 (MultiPass
SetTexture/DuplicateTexture/IsLoaded and the four OrientedBox helpers). New tests: u5b_last 160,
simple_rem 29,200, tails3 76,066, final15 10,712; every diff_r3d_* green.

`check_exports --forbid-dll` still reports 25 static imports and many dynamic RVA/vtable lookups
into `Render3DCore_7c_orig.dll`; that cutover is the remaining work (notes/agents/static_cutover.md,
notes/agents/dynamic_cutover.md).
"""
open(p,'w',encoding='utf-8').write(s)
print('ok')
