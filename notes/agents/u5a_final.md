# U5a wave 5 (u5a_final): material data, destructors, plugin factories, Reflective render

Status: 2026-11-25. `implemented.d/u5a_final.txt` carries **118 new local exports**. The full
`Render3DCore_7c` builds in `build32-r3d-u5af`; `check_exports.py` reports **2500 exports, TSV 2500,
missing/wrong 0, extra 0**. The generated `.def` now has **163 forwarded** entries (2337 local of 2500 for the
whole tree; the wave-4 figure was 1938 local). Differential test `diff_r3d_u5a_final`: **3,776 checks, 0
mismatches** over 300 rounds against `Render3DCore_7c_orig.dll`. The existing units stay green (rounds in
parentheses): `diff_r3d_u5a` 31,000/0 (100), `diff_r3d_u5a2` 11,334/0 (100), `diff_r3d_u5a_rem` 7,597/0 (100),
`diff_r3d_u6` 227,780/0 (100), `diff_r3d_u4` 260,388/0 (60), `diff_r3d_u4_rem` 1,102/0, `diff_r3d_u7`
15,000/0, `diff_r3d_u8` 15,500/0, `diff_r3d_u9b` 260/0, `diff_r3d_u9b2` 8,248/0, `diff_r3d_u10`
172,200/0, `diff_r3d_u10_rem` 73,900/0, `diff_r3d_u11` 60,596/0.

Only `Render3DCore_7c` was built and no game run was made.

## Files

- `src/jet/graphics/Render3DCore_7c/U5aFinal.cpp` (new) - all bodies and the compiler-emitted data.
- `src/jet/tests/diff_r3d_u5a_final.cpp` (new) - differential test. `src/jet/CMakeLists.txt` was not edited, so the
  target is built by hand (see Test).
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5a_final.txt` (new) - 118 mangled names, all local.
- `src/jet/include/jet/Render3DCoreMaterials.h` (edited) - complete class declarations: virtual destructors,
  `operator==`/`CreateInstance`/`DuplicateTexture`/`SetTexture`/`GetNumPasses` overrides, the three static data
  objects per concrete class, the full plugin hierarchy (MaterialPluginX/Std/.../Gloss and the auto-texture
  template base), and the `ReflectiveMaterial::cubeMatrix` size fix. No layout change other than that fix.
- `src/jet/graphics/Render3DCore_7c/MaterialLifetime.cpp` (edited) - constructors install this module's
  vftables/vbtables (own-module lookup first, original fallback).
- `src/jet/graphics/Render3DCore_7c/MaterialPlugins.cpp` (edited) - same local-first lookup for plugin vtables
  and material UID objects.
- `src/jet/graphics/Render3DCore_7c/SimpleMaterials.cpp` (edited) - `ReflectiveMaterial::IsLoaded` returns 3
  when the primary texture is absent (the original's 0x27d10).
- `src/jet/tests/diff_r3d_u5a.cpp`, `diff_r3d_u5a2.cpp` (edited) - the two expectations that assumed the
  original-owned tables/strings now accept this module's local data (see Test).

## Implemented (118)

| Group | Count | Exports |
|---|---:|---|
| Concrete-class vtables | 20 | `NoTexture`, `OneTexture`, `OneTextureNoAlpha`, `Billboard`, `ColorOneTexture`, `AutoTex`, `MVHSelect`, `Reflective`, `Gloss`, `GenericShader` `{for Material}` and `{for PluginInstance}` |
| Vbtables | 10 | the same material classes |
| `??1` destructors | 10 | the same material classes |
| `??_D` vbase destructors | 10 | the same material classes |
| `DECORATED_TYPE` / `UNDECORATED_TYPE` / `UID` | 30 | three per concrete material |
| Material plugin vtables | 10 | `MaterialPluginX`, `Std`, `NoTexture`, `GenericShader`, `OneTexture`, `OneTextureNoAlpha`, `ColorOneTexture`, `MVHSelect`, `Reflective`, `Gloss` |
| Material plugin `??1` | 10 | the same + `MaterialPluginX` |
| `CreateInstance(manager, kernel)` | 8 | `NoTexture`, `GenericShader`, `OneTexture`, `OneTextureNoAlpha`, `ColorOneTexture`, `MVHSelect`, `Reflective`, `Gloss` (the auto-texture factory belongs to the Templates unit) |
| `CreatePluginResources(kernel)` | 7 | the one-texture family, `MVHSelect`, `Reflective`, `Gloss` (non-virtual, `IBA`) and `Std` (virtual, `MBA`) |
| Reflective render | 3 | `RenderChunk`, `RenderNumChunks`, `RenderPrimitives` |

The class *members* whose exported names already belong to U5a/U5a2 (`CreateInstance() const`,
`operator==`, `SetTexture`, `DuplicateTexture`, `GetNumPasses`, `IsSupported`) were also defined, because the
compiler needs a real address for every vtable slot; the `.def` still exports the wave-4 free function under the
original name and the member forwards to it.

## Still forwarded in the U5a scope (4) and why

- **`??_7AutoTexMaterialPlugin`, `??1AutoTexMaterialPlugin`** - VS18 does not emit a vftable for this class
  unless a constructor is defined in a TU that constructs it (the key function is its constructor, which the
  original does not export - only the *named*/copy base constructors exist). Forcing one imports
  `StdMaterialPlugin::StdMaterialPlugin()` from the original, which would make the module fail to load when the
  original is absent. The plugin keeps using the original's table/factory; `AutoTexMaterial` itself is local.
- **`?CreateGeometryChunk@ReflectiveMaterial`** - builds the U6 `GeometryChunkSingleStream` template
  instantiations (`FUN_1005b1a0`, `FUN_1005b420`, ...), the display shader-chunk cache (slot 0x140/0x138), the
  preprocessor defines and the vertex-type string table. Those chunk classes belong to U6 and are not
  reconstructed there as generic classes yet.
- **`?CreateGeometryChunk@GenericShaderMaterial`** - same U6 generic chunk classes plus the display's vertex
  program cache (slot 0x110), the `%MAXCONST` preprocessor array and the `GenericGeometryChunk*` RTTI classes.

Additionally, two neighbouring units' groups were never in this unit's ownership: **11 MultiPass** material
exports (U5b, `Render3DCoreMultiPass.h`) and **7 `StdMaterialPluginTemplate<AutoTexMaterial>`** exports
(Templates unit, `Templates.cpp`/`implemented.d/templates.txt`). The template factory is local *there*, so
auto-texture materials still get a local factory on that path.

## Layout, ABI and behaviour facts learned here

- **Material vftable sizes.** `NoTextureMaterial` and `GenericShaderMaterial` have 23 slots (0..22);
  every one-texture class has 26 (0..25, adding `CacheAlphaHint`, `SetDisplayState`, `ClearDisplayState`).
  Slot 11 is `Material::GetNumPasses` - the project's `ReservedVirtual11` placeholder was renamed and is a plain
  thunk to RenderSystem_7c in the original. The `PluginInstance` vftable has **two** slots:
  `DeleteThis` then the destructor; the vbase vtable's RTTI pointer sits at -4 of each exported table.
- **Vbtables.** `{0,0x9c}` Material, `{0,0xa0}` NoTexture, `{0,0xa8}` one-texture family, `{0,0xb8}` AutoTex,
  `{0,0xbc}` MVHSelect, `{0,0xb4}` GenericShader, `{0,0xfc}` Reflective/Gloss. The second word is the vtordisp
  offset; the virtual base (PluginInstance) sits at `4 + value`.
- **`ReflectiveMaterial::cubeMatrix` is 0x40 bytes, not 0x44.** The original's matrix at +0xbc is 16 floats and
  ends at +0xfb; +0xfc is the vtordisp and +0x100 the PluginInstance vtable (RC). The header's old `0x44`
  shifted the vbase to +0x104 and made the class 0x10c. With 0x40 the compiler emits the original
  `{0,0xfc}` vbtable and size 0x108.
- **`Material` needs a declared virtual destructor.** Without it the compiler chains concrete destructors to
  `PluginInstance::~PluginInstance`, skipping `Material::~Material`. It is declared protected, as the original.
  MSVC 14.51 emits the same vbase-pointer `??1` ABI as VC8 (the body starts `mov eax,[ecx-<vbase+4>]`), so the
  exported destructors are ABI-identical.
- **Vtable emission needs an ODR use.** A class whose only defined virtuals are overrides and whose destructor
  is not the key function gets no vftable in a TU that never constructs it (probe: an override class with a
  dtor body emits no `??_7`). Every material gets a private `X(MaterialManager*)` emission constructor and a
  never-called `EmitMaterialTables()` placement-new chain; that is what emits the ten vbtables and the plugin
  tables. `AutoTexMaterialPlugin` cannot use that trick (see above).
- **Static strings/UIDs.** `PString("...")` and `UId("...")` at load time intern into the same KernelStandard
  cache nodes the original's module initialisers use, so `GetType`'s node pointer and text match; the static
  *object* address differs (it is this module's data). The texts/GUIDs: `notex`, `onetex`,
  `onetexnoalpha`, `billboard`, `coloronetex`, `autotex`, `mvhselect`, `reflect`, `gloss`, `shader` with the
  GUIDs in `U5aFinal.cpp` (from the original's 0x1007a000 init routines). `DECORATED_TYPE` is `m.<name>`.
- **Plugin vtables are 6 slots** for `MaterialPluginX`/`NoTexture`/`GenericShader`/the one-texture family
  (`AddReference`, `ReleaseReference`, dtor, `CreateInstance() const`, `IsSupported`,
  `CreateInstance(manager,kernel)`), and **7** for `Std`/`AutoTex`, which add
  `CreatePluginResources` as the seventh virtual. All other `CreatePluginResources` are *non-virtual* helper
  members (`IBA`, protected) that `CreateInstance` calls. `MaterialPluginX`'s defaults are the original's
  `xor eax,eax; ret` stubs (IsSupported false, factory null); `Std` answers true and has a null factory.
- **Plugin factories.** The kernel's `RenderingCacheManager` is at `kernel+0x6c`, the display at `kernel+0x7c`.
  Cache slots are on the **plugin object**, not the kernel: `NoTexture` {+0x20 blend, +0x24 chunk, +0x28
  stage, +0x2c display}, one-texture family {+0x20 chunk, +0x24 mask, +0x28 blend, +0x2c stage, +0x30
  display}, `GenericShader` {+0x20 chunk, +0x24 blend}, `MVHSelect` {.., +0x34 count, +0x38 context, +0x58
  display}, `Reflective`/`Gloss` two contexts {+0x34 count, +0x38/+0x3c, +0x58 display}. The stage node is
  Display slot 0x11c `(flag=0, descriptor1, descriptor2)`. The descriptors are anonymous .bss statics of the
  original (`0x100dcd60/0xa8`, `0x100dce08/0x50`, `0x100dceb0/0xf8`, `0x100dccb8/0xd00`,
  `0x100dcf60/0xfa8`, `0x100dcff0/0xdd038`, and Gloss `0x100dcac8/0xb10`, `0x100dcb58/0xba0`); the factory
  passes their addresses, so the RVA is taken from the original module. Assign links the new material to the
  plugin via `Kernel_7c`'s private `PluginInstance::Assign` (called through its export with the same
  `material+4, plugin` arguments the original uses).
- **Reflective render flow.** `SetDisplayState(display, program == nullptr)`; opacity is
  `chunk->opacity * material->opacity` (material-first when the viewport is null); the camera view matrix is
  `viewport->camera` (+0x20) +0x1b8 (update when +0x25c is set). If `flags2 & 1` or the display's combiner count
  (slot 0x2c) is 1, the cube branch runs when `chunk && (flags2 & 2) && display slot 0xc && combiner == 1`:
  the cell-space transform (Camera::GetCellSpaceViewTransform) is transposed into a matrix with last row
  (0,0,0,1), multiplied **before** the cube matrix (`0x1001aaf0` copies the right operand then calls
  `0x1001ab30`; every `ADDSS` keeps the first product as destination, which selects the NaN payload), then
  passed to Display slots 0x64 and 0x74(1). Otherwise `SetSphericalTexureCoordinates(chunk, modelView)`.
  The tail is Display 0x54(modelView), chunk `RenderGeometry`/`RenderPrimitives` (slots 0x38/0x3c),
  Display 0x78(1), then `ClearDisplayState`. `RenderNumChunks` additionally calls `ChangeTexGen(display,
  update)` whenever a chunk's program changes null-ness.
- **`ReflectiveMaterial::IsLoaded`** returns `3` (texture not found) when the primary texture is null, then `4`
  if the cube or the texture is unavailable. The wave-4 body returned 0 in the first case.

## Test

```
cmake -S src\jet -B build32-r3d-u5af -G "Visual Studio 18 2026" -A Win32
cmake --build build32-r3d-u5af --config Release --target Render3DCore_7c
cl /nologo /EHsc /GR /Zp8 /std:c++20 /O2 /I src\jet\include src\jet\tests\diff_r3d_u5a_final.cpp ^
   /Fe:build32-r3d-u5af\Release\diff_r3d_u5a_final.exe
copy build32-r3d-u5af\Release\Render3DCore_7c.dll <fixture>\
diff_r3d_u5a_final.exe <fixture> 300
```

`<fixture>` is a copy of `testbin-r3d` (original DLL renamed `Render3DCore_7c_orig.dll`, the main build's Jet
proxies). The test:

- compares every material's `GetType` text, `DECORATED_TYPE` text, the 16 UID bytes and the two vbtable words
  bit-for-bit; checks each vftable length (23/26), that every slot is in the module's text and that the RTTI
  pointer is present, and that the `PluginInstance` table has exactly two slots;
- installs each module's material table on a zeroed image and calls slots 1/2/3/11/22/24/25, comparing the
  result, the object mutation and the fake Display's call counts;
- calls every exported `??1` with the vbase pointer (`4 + vbtable[1]`) on a zeroed image carrying that
  vbtable and compares every non-table byte afterwards;
- calls the three Reflective render paths with a fake Display/ViewPort/Camera/Chunk and a recording
  `Material::SetLightingState` import patched in both IATs, and compares the recorded arguments, the display
  slot sequence and the chunk render calls;
- calls the eight plugins' exported constructors and `CreatePluginResources` with a fake kernel and patched
  `RenderingCacheManager::FindCache/AddCache`, comparing the display stage calls and the cache-slot presence.

The two wave-4 expectations that had to change: `diff_r3d_u5a.cpp` compares `GetType` by text (the static
`PString` object is now local; the interned node/text is shared) and `diff_r3d_u5a2.cpp` ignores the
module-local table words (+0, +4 and the vbase table) in object comparisons and compares the factory's type
argument by its interned text. Everything else is unchanged.

## Notes for other units

- `ReflectiveMaterial::cubeMatrix`/Gloss: fixed to 0x40; any unit that assumed 0x44 or size 0x10c must follow.
- `Material` now declares its protected virtual destructor and `GetNumPasses` at slot 11; U5b/MultiPass
  classes that derive from the concrete materials inherit the local tables automatically.
- `GeometryChunk::GetVertexProgram` is at vtable byte 0xf0, not the 0x3c written in an older comment in
  `Geometry.h` (the render bodies reach it through the typed call and the fake chunk tables use 0xf0).
