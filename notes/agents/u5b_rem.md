# U5b wave 4: remaining multipass lifetimes

Status 2026-11-24: **71 more exports local and differentially verified; the slice is now 115 of 254 local and 139
forwarded.** The new exports are the multipass material/plugin/geometry-chunk constructors and destructors, the
virtual-base destructors, `CreateInstance`/`InitInstance`, the material equality family, and the two bump-chunk
constructors plus `InitializeBuffers`.

Test results against `Render3DCore_7c_orig.dll` in the fixture, using the full `build32-r3d-u5b` build of the module:

```
diff_r3d_u5b_rem.exe <testdir> 1000    132066 checks, 0 mismatches
diff_r3d_u5b.exe     <testdir> 1000   2086009 checks, 0 mismatches   (the old 44, unchanged)
diff_r3d_u5a.exe     <testdir> 200      62000 checks, 0 mismatches   (U5a stays green)
diff_r3d_u5a2.exe    <testdir> 200      22634 checks, 0 mismatches
```

`check_exports.py` reports 2500 exports, 0 missing/wrong/extra. The full module now builds and the merged list has
1922 local exports (the other wave-4 units landed while this unit was finished); the U5b slice itself is 115/254
local and 139 forwarded.

## New local exports (71)

| Group | Exports | Notes |
|---|---:|---|
| plugin ctors/dtors/assign | 27 | six plugins: default/copy/(named for Bump, BumpSpec, OneTexture), dtor, `operator=` |
| material ctors | 6 | NoTexture, OneTexture, Bump, Bump2, BumpSpec, BumpSpec2 |
| material dtors | 6 | the exported virtual dtor receives the virtual-base subobject |
| material `vbase destructor` | 6 | adjust to the vbase, call dtor, then `ReferenceCount::~ReferenceCount` |
| material `CreateInstance` | 6 | factory call + `InitInstance` (+ bump texture for the bump family) |
| material `InitInstance` | 4 | NoTexture, OneTexture, Bump and BumpSpec (shared body) |
| material `Equals` | 4 | NoTexture, OneTexture, Bump/BumpSpec (shared body) |
| material `operator==` | 6 | RTTI type test + the same field comparison |
| chunk ctors/dtors | 4 | `MultiPassBumpChunkT/T2` |
| chunk `InitializeBuffers` | 2 | both alias one body |

The exact names and alias targets are in `implemented.d/u5b_rem.txt`.

## Remaining forwards (139) and why

| Group | Count | Reason |
|---|---:|---|
| static data: vtables, vbtables, `UID`, `DECORATED_TYPE`, `UNDECORATED_TYPE` | 49 | still one object with the original; the code resolves them from `Render3DCore_7c_orig.dll` at run time |
| plugin `Initialize*`, `LoadLightPrograms`, `RegisterBuiltInPrograms` | 35 | build `MPLightRender` pass programs through the U4 caches and RenderSystem vertex/texture-combiner managers; not started in this unit |
| plugin `CreateInstance` | 6 | need the material-manager virtual checks and `PluginInstance::Assign`; the direct construction path is ready, the manager contract is not |
| `MultiPassRenderState` (ctors, copy, `StateSetup`, `LoadRenderProperties`, `LoadTextures`, light passes, `RenderGeometry`, `SetBlendState`, `SetScissor`, `SetupExternalRender`, `StateCleanUp`, `GetAmbient*`, `DebugRender*`) | 17 | display/geometry/light state; needs U4/U5a/U6 and the RenderSystem render state together |
| `MultiPassNoTextureMaterial::SetTexture`, `DuplicateTexture`, `Bind`, `Unbind`, `SetDisplayState`, `ClearDisplayState`, `CacheChunk`, `RenderChunk`, `RenderNumChunks`, `RenderPrimitives`, `RenderChunkPerVertex`, `RenderState`, `AddProperty` | 15 | render/cache paths over U4/U6 and the display state; `SetTexture`/`DuplicateTexture` are U5a's texture-definition contract |
| `MultiPassOneTextureMaterial::Bind/Unbind/SetDisplayState/ClearDisplayState/CacheChunk/RenderChunk/RenderNumChunks/RenderPrimitives/RenderChunkPerVertex/SetTexture/DuplicateTexture` | 12 | same |
| `MultiPassBumpMaterial`/`Bump2`/`BumpSpec`/`BumpSpec2` `CreateGeometryChunk`, `AllocateGeometryMemory`, `SetTexture`, `IsLoaded` | 14 | allocate through the U6 `GeometryChunkTemplate` instantiations; the chunk types are header-only in U6 |
| `MultiPassLightRender::CreateNewLightPass`, `Render` | 2 | pass-record construction needs the pass ctor/destructor helpers and the program-manager contract; `Render` drives the display state |
| `MultiPassBumpChunkBase::CreateTangentVectors` | 1 | 1.6 KB of triangle/vertex math over `GeometryChunk`'s vertex-item API; not needed for the lifetime slice |
| `MultiPassRenderState` copy ctor and other small state methods | rest | list-copy helpers (`FUN_1000fcf0`, `FUN_1002ef90`) still belong to the state unit |

`MultiPassNoTextureMaterial::AddProperty` is deliberately still forwarded: it compares lowered PString nodes with
four interned nodes that are internal data of the original (not exported), so the names would have to be guessed.

## ABI, layout and numeric facts

- **Material layout** (`5b` tail): NoTexture 0xd0, OneTexture 0xdc, the four bump classes 0xe0.
  - NoTexture: +0x9c chunk cache, +0xa0 blend cache, +0xa4/+0xa8 textures, +0xac..0xaf mode bytes,
    +0xb0/+0xb4 light renders, +0xb8/+0xbc/+0xc0 `Array<const Texture*, 10>`, +0xc4 vtordisp, +0xc8
    ReferenceCount vftable, +0xcc count.
  - OneTexture adds +0xc4 primary texture, +0xc8 alpha hint, +0xcc vertex type, vtordisp 0xd0, vbase 0xd4.
  - Bump adds +0xd0 bump texture, vtordisp 0xd4, vbase 0xd8. Bump2/BumpSpec/BumpSpec2 reuse this layout.
- **Ctor prelude**: with the hidden vbase flag set, the class vbtable goes at +4 and the ReferenceCount value 1 at
  `delta + 8`; the original also writes the KernelStandard `ReferenceCount` vftable in the pre-constructor state,
  but the Material constructor and `Finish` overwrite it, so only the count is observable (same decision as U5a2).
  `delta` is the vbtable's [1]: 0xc4/0xd0/0xd4.
- **Destructor entries are virtual-base adjusted**: `??1MultiPassNoTextureMaterial` receives object+0xc8,
  OneTexture +0xd4, the bump family +0xd8. `??_D` receives the object start and forwards.
- **`Material::~Material` receives object+0xa0**, not the object start (the RenderSystem implementation subtracts
  0xa0 internally). This was the one crash in the first test run.
- **`vbase destructor`**: after the class dtor, call `KernelStandard_7c!??1ReferenceCount@Jet@@MAE@XZ` on the vbase;
  its prologue writes the KernelStandard vtable at +vbase, which is visible in the object image.
- **`operator==`**: the original starts with `__RTDynamicCast(other, Material, <class>)`. Because the multipass
  classes derive from each other (NoTexture <- OneTexture <- Bump <- Bump2/BumpSpec <- BumpSpec2), a cast to a base
  succeeds for a derived object. The local test reproduces the hierarchy with the vftable COL type descriptors; a
  plain descriptor equality would have been wrong for the base classes.
- **Equality fields**: NoTexture compares +0xa4/+0xa8 by pointer equality, the four mode bytes and
  `Material::Equals`; OneTexture adds the +0xc4 texture and the +0xc8 alpha hint; the bump family adds +0xd0.
- **`InitInstance`** copies from the source object into the argument: textures with AddReference(new) /
  ReleaseReference(old), then the mode bytes / alpha hint / bump texture.
- **`CreateInstance`**: `Material::CreateInstance(UNDECORATED_TYPE, materialManager)` then `InitInstance`; the bump
  family copies the bump texture afterwards. Tests hook the RenderSystem factory entry.
- **Plugin ctors**: base `MaterialPlugin(name, uid)` comes from RenderSystem_7c. The 0x20 fields are five cache
  pointers (NoTexture has four); +0x34..+0x6c are 15 `char const*` program names.
  - `MultiPassNoTextureMaterialPlugin` has **no** name table (the original default ctor only zeroes the four caches).
  - `MultiPassOneTextureMaterialPlugin` has the original's copy-paste quirk: +0x50/+0x54 repeat the two ambient
    names instead of parallel-light names.
  - `BumpSpec2/BumpSpec` named ctor and `Bump/Bump2` default ctor take the 15 names; null arguments fall back to the
    class defaults.
- **Plugin dtor**: +0x20/+0x24 are rendering caches (release through the vbptr: `cache+0xc`, `vbptr[1]`, +0xc);
  +0x28/+0x2c/+0x30 are plain `ReferenceCount` pointers. Bump family dtor sets its own vtable first, then delegates.
- **Geometry chunks**: the exported template ctor is the original module's
  `GeometryChunkTemplate<PackedVertexPNTA/PNTTA, GeometryChunkSingleStream>` ctor; the class ctor only overwrites the
  two vtable slots:
  - T: +0x00 `??_7MultiPassBumpChunkT@Jet@@6B?$GeometryChunkTemplate...PNTA...`, +0x60
    `??_7MultiPassBumpChunkT@Jet@@6BMultiPassBumpChunkBase@1@@`.
  - T2: the PNTTA pair.
  - dtor sets +0x00 to the template vtable, drops one reference on the +0x5c counted block (delete and null at
    zero), then calls `RenderSystem_7c!??1GeometryChunkSingleStream`.
  - `InitializeBuffers` calls slot 0 of the Display subobject at display+0x60 and then the PNTA template's
    `InitializeBuffers` (the T2 body calls the PNTA instantiation too).
- **Plugin name literals** live in each module, so their addresses differ between ours and the original; the test
  compares the text, not the pointers. Material objects compare byte-for-byte except the per-instance
  `Array<Texture*>` block and the PString nodes created by `Material::InitInstance`.

## Files

- `src/jet/graphics/Render3DCore_7c/U5bLifetime.cpp` - all new lifetimes (thunks and cdecl leaves)
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5b_rem.txt` - 71 export names
- `src/jet/tests/diff_r3d_u5b_rem.cpp` - new differential test
- unchanged but used: `MultiPassState.cpp`, `MultiPassMaterials.cpp`, `MultiPassPlugins.cpp`,
  `include/jet/Render3DCoreMultiPass.h`

## Blockers / merge notes

- `src/jet/CMakeLists.txt`'s test loop lists `u5b`, not `u5b_rem`; the new test was compiled and run by hand:
  `cl /MD /EHsc /GR /Zc:wchar_t- /Zp8 /I src/jet/include tests/diff_r3d_u5b_rem.cpp`. Add `u5b_rem` to the loop
  (or a target) when merging.
- During the work the shared tree was transiently blocked by other wave-4 files (`U9bRem.cpp`, `TailsPlane.cpp`,
  `TailsLights.cpp`, `U10Collision.cpp`); they were fixed by their owners and the final full build above is clean.
  No main-tree file outside this unit's allowed set was edited.
- The test fixture needs the renamed originals preloaded before the local proxies (`KernelStandard_7c_orig.dll`
  first) or the proxies' static initialisers fail with error 1114. `diff_r3d_u5b_rem` does that itself; the old
  `diff_r3d_u5b`, `diff_r3d_u5a` and `diff_r3d_u5a2` need the all-original fixture (copy the `*_orig.dll` files
  back under their plain names).
