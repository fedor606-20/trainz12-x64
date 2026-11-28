p='MODLOG.md'
b=open(p,'rb').read()
entry=b'''
### Render3DCore exports complete: 2500/2500 (2026-11-25)

- Wave 6 merged: U5b last 48, simple classes 74, tails3 26, final15 15. All 2500 exports local, 0 forwarders,
  0 missing/wrong/extra.
- New tests: u5b_last 160, simple_rem 29,200, tails3 76,066, final15 10,712; all diff_r3d_* green.
- Layout fixes: ReflectiveMaterial cubeMatrix 0x40 (vbtable {0,0xfc}, size 0x108); Material protected virtual dtor
  and slot 11 GetNumPasses; U5b StateSetup/SetScissor cdecl hidden-return fix (Display::GetScissor writes Rect
  through a stack buffer, not a return register).
- Runtime cutover remains: 25 static imports and dynamic RVA/vtable lookups into Render3DCore_7c_orig.dll.
  Static-import and dynamic-bridge streams are running; the export/behaviour layer is complete.
'''
if b'Render3DCore exports complete' not in b:
    b += entry.replace(b'\n',b'\r\n')
open(p,'wb').write(b)
print('ok')
