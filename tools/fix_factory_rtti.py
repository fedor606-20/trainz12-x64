p='src/jet/graphics/WaterSystem_7c/WaterMaterialStatics.cpp'
s=open(p,encoding='utf-8').read()
old='''    void* const tables[] = {WsBumpVftable, WsFresWaveVftable, WsOneTexVftable, WsOceanVftable};
    for (void* const table : tables) {
        auto* const slots = static_cast<void**>(table);
        slots[0] = addRef;
        slots[1] = releaseRef;
        slots[3] = bareCreate;
    }
'''
new='''    // The hand-written factory tables have no generated RTTI. Point the locator word at RenderSystem's
    // MaterialPlugin locator so a dynamic_cast to the plugin base still finds a valid hierarchy; the concrete
    // factory type has no local RTTI by design.
    void* locator = nullptr;
    if (HMODULE const rs = GetModuleHandleA("RenderSystem_7c.dll")) {
        if (void* const pluginVtable = reinterpret_cast<void*>(
                GetProcAddress(rs, "??_7MaterialPlugin@Jet@@6B@"))) {
            locator = static_cast<void**>(pluginVtable)[-1];
        }
    }

    void* const tables[] = {WsBumpVftable, WsFresWaveVftable, WsOneTexVftable, WsOceanVftable};
    for (void* const table : tables) {
        auto* const slots = static_cast<void**>(table);
        slots[-1] = locator;
        slots[0] = addRef;
        slots[1] = releaseRef;
        slots[3] = bareCreate;
    }
'''
if old not in s: raise SystemExit('anchor')
s=s.replace(old,new)
open(p,'w',encoding='utf-8').write(s)
print('ok')
