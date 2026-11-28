# U5b wave 5 (u5b_final): multipass static data, render paths, light render, state lifetime

Status 2026-11-25: **91 of the 139 remaining U5b exports are local; 48 stay forwarded.** Worked in
`D:\universal modder\trainz12-x64`, built in `build32-r3d-u5bf`.

```
check_exports.py build32-r3d-u5bf\Release\Render3DCore_7c.dll <tsv>   2500 exports, missing/wrong 0, extra 0
diff_r3d_u5b_final.exe <testbin-r3d> 1000                            127,177 checks, 0 mismatches
diff_r3d_u5b_rem.exe   <testbin-r3d> 1000                            132,066 checks, 0 mismatches
```

The 11 multipass texture virtuals U5a owns (`SetTexture` x6, `DuplicateTexture` x3, `IsLoaded` x2) remain outside
this slice, as in wave 4.

## Implemented here (91)

| Group | Count | Notes |
|---|---:|---|
| static data: 25 vftables, 6 vbtables, 6 `UID`, 6 `DECORATED_TYPE`, 6 `UNDECORATED_TYPE` | 49 | local objects; tables are byte copies of the original image, type names/UIDs are built from their original text/bytes |
| `MultiPassNoTextureMaterial` render/cache/display paths + `AddProperty` + `CreateGeometryChunk` | 12 | `Bind`, `Unbind`, `SetDisplayState`, `ClearDisplayState`, `RenderState`, `CacheChunk`, `RenderChunk`, `RenderNumChunks`, `RenderPrimitives`, `CreateGeometryChunk`, `AddProperty`; `SetTexture`/`DuplicateTexture` are U5a's |
| `MultiPassOneTextureMaterial` render/cache/chunk paths | 8 | `Bind`, `Unbind`, `CacheChunk`, `RenderChunk`, `RenderNumChunks`, `RenderPrimitives`, `CreateGeometryChunk`, `AllocateGeometryMemory`; `RenderChunkPerVertex` still forwards |
| bump material geometry allocation | 6 | `CreateGeometryChunk` (Bump) and `AllocateGeometryMemory` for OneTexture/Bump/Bump2/BumpSpec/BumpSpec2 (the four bump classes share the two bodies) |
| `MultiPassLightRender` | 3 | destructor, `CreateNewLightPass`, `Render` |
| `MultiPassBumpChunkBase::operator=` | 1 | `mov eax,ecx; ret 4` |
| `MultiPassRenderState` lifetime and state leaves | 14 | both constructors, copy constructor, destructor, `SetBlendState`, `LoadTextures`, `RenderGeometry`, `StateCleanUp`, `SetupExternalRender`, `GetAmbientLights`, `GetAmbientLight0`, the two debug render walks (no-ops), `RenderMixedLightPasses` (alias of `RenderSingleLightPasses`) |

The exact mangled names and alias targets are in `implemented.d/u5b_final.txt`.

## Remaining forwards (48) and why

| Group | Count | Reason |
|---|---:|---|
| plugin `Initialize*` / `LoadLightPrograms` / `RegisterBuiltInPrograms` | 35 | create `MPLightRender` records and load vertex/combiner programs through the U4 caches and the RenderSystem program managers |
| plugin `CreateInstance` | 6 | needs the cache contract plus the manager's virtual checks and `PluginInstance::Assign` |
| `MultiPassRenderState::StateSetup`, `LoadRenderProperties` x2, `SetScissor`, `RenderSingleLightPasses` | 5 | per-light property-record decoding and scissor/light-pass setup; the local state leaves still call the original entries |
| `MultiPassBumpChunkBase::CreateTangentVectors` | 1 | 1.6 KB of triangle/vertex math over the chunk item views |
| `MultiPassOneTextureMaterial::RenderChunkPerVertex` | 1 | per-vertex path driven by `ViewPort` render hints |

`AddProperty` is now local: it lowercases the name/value through `PStringCache::ToLower`, compares with the interned
`false`/`off` and `smoothblend`/`singlepass`/`intensity`/`debug` nodes (same cache as the original's statics), writes
the mode bytes at +0xac..+0xaf, then calls `Material::AddProperty` (RenderSystem).

## ABI, layout and numeric facts

- **Static data.** The 25 exported vftables are exact copies of the original's first slot through the slot before
  `CompleteObjectLocator` (sizes 24/26 for the Material halves, 2 for the PluginInstance halves, 7 for the plugin
  tables, 73 for the two `GeometryChunkTemplate` tables, 1 each for the chunk base tables, 2 each for the light
  render tables). The copies are filled once at load from the renamed original, so their slot pointers are the
  original addresses and the material constructors (wave 4) still install those addresses.
- **Type names**: `m.mpnotex`/`mpnotex`, `m.mponetex`/`mponetex`, `m.mpbumptex`/`mpbumptex`,
  `m.mpbump2tex`/`mpbump2tex`, `m.mpbumpspec`/`mpbumpspec`, `m.mpbumpspec2`/`mpbumpspec2`. The UIDs are the 16
  bytes at RVAs `0xdcc5c` (NoTexture), `0xdcc8c` (OneTexture), `0xdcbf0` (Bump), `0xdcc08` (Bump2),
  `0xdcc28` (BumpSpec), `0xdcc40` (BumpSpec2).
- **`MultiPassRenderState` layout** (first time written down completely): +0 mode, +4 display, +8 view, +0xc
  material, +0x10 chunk, +0x14 numIndices, +0x18 indices, +0x1c numPrimitives, +0x20 primitive, +0x24
  StateSetup flag, +0x25 opaque/alpha bool, +0x26 second bool, +0x28 opacity, +0x2c..+0x40 six
  `FastDynamicArray<DisplayLight*>` heads, +0x44..+0x80 view matrix, +0x84..+0xa4 its 3x3 part, +0xa8..+0xe4 the
  display model-view buffer, +0xe8 scissor rect, +0xf8 bound vertex program, +0xfc count/cap/data of
  `Array<unsigned>` (step 10), +0x108 the material's texture array.
- Constructor 11-arg mode is 0 normally and 2 when the light list has more than one item or the last bool is set;
  the 15-arg form uses 1/3. `+0x25` is the 7th argument (opaque), `+0x26` the 10th. Both constructors return the
  object, then call `StateSetup`; the copy constructor deep-copies the six light lists and the +0xfc array and
  copies the matrices/rect/texture pointer.
- **`SetBlendState`** is `__thiscall` in the original; it reads the material at +0xc and its two-side flag +0x84
  bit 1: two-sided materials use `(SRC_SRC_ALPHA, DST_ONE_MINUS_SRC_ALPHA)`, otherwise the material's +0x78/+0x7c
  factors. The `SetAlphaBlendMode` argument is the raw saved byte, not a normalised bool (`ClearDisplayState`
  passes e.g. 5); the local code calls the import through a raw-int function pointer to keep that ABI.
- **`RenderGeometry`** dispatch: mode 0 slot 0x38 (no indices), mode 1 slot 0x3c, mode 2 `0x4c` (rebind program) +
  `0x54` (geometry pass), mode 3 `0x4c` + `0x50` (primitive pass). The setup flag uses chunk slot 0x44; a failed
  setup returns false and leaves the state. `StateCleanUp` restores the scissor, calls slot 0x58 for multipass
  modes, releases the display texture slots with display slot 0x12c, walks the six light lists and calls
  slot 0 of every light (argument 1), then clears +0xf8.
- **`LightRender` records**: pass stride 0x104, program +0xfc, combiner +0x100, arrays at +0x0c/+0x84 (ten each).
  `CreateNewLightPass` grows the pass array by exactly one record (capacity = count+1), copies the empty record,
  and takes one reference on the program and the combiner through the vbase at object+4. The destructor releases
  those, frees the scissor array (+0x1c), runs every pass destructor (freeing each non-empty pair array and the
  texture array), frees the pass block and calls `ReferenceCount::~ReferenceCount` last.
- **Chunk allocation**: `FinishChunkAllocation` clears hint bit 0, sets bit 2 when bit 1 was set and bit 2 was not
  (`dynamic = bit1`, and the hint must have system bit 0), calls chunk slot 0x64 `SetVertexDataDynamic(0,-2,1)`
  only for hint version 1 or a dynamic hint, slot 0x6c `SetVertexDataShared(0,0,0)` when dynamic, and slot 0x74
  `SetIndexDataDynamic` when hint `size == 1`. Version 0 therefore performs none of these (wave 4's provisional
  version-0 call was wrong and was corrected here).
- **Bump chunks** are allocated as 0x64 bytes and constructed by the original PNTA/PNTTA template constructor plus
  the original first/second vtables; the plain materials use the local U6 PN/PNT/Nml chunk classes.
- **AddProperty** value semantics: `false` and `off` clear the mode byte, everything else sets it; names are
  matched after `PStringCache::ToLower`, exactly like the original's interned static nodes.

## Files

- new `src/jet/graphics/Render3DCore_7c/U5bFinal.h` - shared layout/helpers/thunk declarations
- new `src/jet/graphics/Render3DCore_7c/U5bFinalData.cpp` - 49 static exports
- new `src/jet/graphics/Render3DCore_7c/U5bFinalRender.cpp` - material render/cache/chunk paths
- new `src/jet/graphics/Render3DCore_7c/U5bFinalState.cpp` - state lifetime and leaves
- new `src/jet/graphics/Render3DCore_7c/U5bFinalChunks.cpp` - light render and chunk assignment
- new `src/jet/graphics/Render3DCore_7c/implemented.d/u5b_final.txt` - 91 exports
- new `src/jet/tests/diff_r3d_u5b_final.cpp` - differential test
- unchanged but used: `MultiPassState.cpp`, `MultiPassMaterials.cpp`, `MultiPassPlugins.cpp`,
  `U5bLifetime.cpp`, `include/jet/Render3DCoreMultiPass.h`, `include/jet/Render3DCore*.h`

## Test

```
cmake -S src\jet -B build32-r3d-u5bf -G "Visual Studio 18 2026" -A Win32
cmake --build build32-r3d-u5bf --config Release --target Render3DCore_7c
cl /nologo /EHsc /GR /Zc:wchar_t- /Zp8 /I src\jet\include src\jet\tests\diff_r3d_u5b_final.cpp
copy build32-r3d-u5bf\Release\Render3DCore_7c.dll testbin-r3d\
diff_r3d_u5b_final.exe testbin-r3d 1000
```

The test compares all 49 static exports byte for byte, the display/cache paths through a recording Display (import
entry patches and vtable recorders), `AddProperty` against the original's interned property nodes, the light-pass
records and reference counts, the chunk/alloc numeric fields and hint mutation, the null-view render early exit, and
the state object image through a real `StateSetup` call. It needs the renamed originals preloaded before the local
proxies (it does that itself), like `diff_r3d_u5b_rem`.

## Merge notes

- `src/jet/CMakeLists.txt`'s test loop does not list `u5b_final` (the unit may not edit it). Add `u5b_final` to the
  `diff_r3d_*` foreach, or build the test by hand as above.
- The local static data objects are now exported by this DLL; the material constructors still install the original
  addresses deliberately, so the wave-4 chunk tests' pointer comparisons stay valid. Moving the constructors to the
  local tables and rewriting those comparisons is the next data cutover step.
