p='PORT_PLAN.md'
s=open(p,encoding='utf-8').read()
old='''        WaterSystem is already in progress alongside RenderSystem. U7b moved 18 direct
        `WaterMaterial` state exports, bringing it to **196/490**. Continue with
        `WaterMaterial` lifetime and vtables, geometry creation, and the factory
        `CreateInstance` methods. See `notes/WaterSystem.md`.'''
new='''        WaterSystem_7c is **done**: all 490 exports are local (0 forwarders, no import or
        runtime load of `WaterSystem_7c_orig.dll`). U7b/U8 (the material base, Bump, Ocean,
        FresWave and OneTex), U9 (WaterTile/Water/WaterTiled/WaterSurface) and U10 (the three
        managers plus `JetGetPluginsWaterSystem`) landed together. The game reaches the main
        menu and Railyard with the retail DLL removed from the lab copy. Differential tests:
        U1 (1,000+ grid/noise checks; the CRT rand table stream is the only allowed
        difference), U5 4 scenarios, U7 71 scenarios, U8a 78 checks, U9 35 checks, U10 66
        checks, all 0 mismatches. The remaining U8a simplifications (display-state save,
        lerp detail constants, VP matrix quirk) are listed in `notes/WaterSystem.md`.'''
if old not in s: raise SystemExit('port plan anchor missing')
s=s.replace(old,new)
open(p,'w',encoding='utf-8').write(s)
print('ok')
