p='MODLOG.md'
b=open(p,'rb').read()
entry=b'''
### Render3DCore wave 5: materials, templates, tails, live paths (2026-11-25)

- Module **2337/2500** local, 163 forwarded; 2500/2500 names/ordinals.
- U5a final 118 (data/vtables/dtors/factories/render), U5b final +91, U10 230/230, 108 template/ITree/Line
  exports, 43 tails2, 27 tail paths, U9b seven local definitions (six still call private original RVAs).
- New tests: u5a_final 3,776; u5b_final 127,177; tails2 4,174; templates 30,852; livepaths 80,002; all 0.
- ReflectiveMaterial layout fixed (cubeMatrix 0x40, vbtable {0,0xfc}, size 0x108); Material slot 11 is
  GetNumPasses and has a protected virtual dtor.
- Wave 6: U5b plugin/state 48, simple Camera/DebugRender/Tube/Transformation/Primitive3D/ViewPoint 76,
  tails3 27, AutoTex vtable + Reflective/GenericShader chunk 4.
'''
if b'Render3DCore wave 5' not in b:
    b += entry.replace(b'\n',b'\r\n')
open(p,'wb').write(b)
print('ok')
