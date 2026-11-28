# U5a: materials and their plugins

Status: 2026-11-24. **91 of 374 exports local**, differential test **310,000 checks, 0 mismatches** over 1,000 rounds
against `Render3DCore_7c_orig.dll`. The rest of the slice is forwarded; the reasons are below.

## Files (all new, none shared edited)

- `src/jet/include/jet/Render3DCoreMaterials.h` - the `Material` base (TS12 layout), the ten concrete material
  classes and the `MaterialPlugin` declaration.
- `src/jet/graphics/Render3DCore_7c/SimpleMaterials.cpp` - the implemented members.
- `src/jet/tests/diff_r3d_u5a.cpp` - differential test (stand-in Display, texture and object images).
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5a_materials.txt` - 91 mangled names, all local (no `=` aliases,
  no forwarders).

## Implemented (91 exports)

| Class | Count | Members |
|---|---:|---|
| `NoTextureMaterial` | 11 | `GetType`, `DuplicateTexture`, `Bind`, `Unbind`, `Activate`, `SetDefaultTextureStage`, `GetTexture` (both), `SetDisplayState`, `ClearDisplayState`, `InitInstance`, `Equals` |
| `OneTextureMaterial` | 16 | `SetResourcePriority`, `GetType`, `Get/SetAlphaHint`, `Get/SetDefaultTextureStage`, `Bind`, `Unbind`, `GetTexture` (both), `Activate`, `IsLoaded`, `CacheAlphaHint`, `SetDisplayState`, `ClearDisplayState`, `InitInstance`, `Equals` |
| `OneTextureNoAlphaMaterial` | 16 | the same set with its own type string, no-alpha display state and `CacheAlphaHint` (writes 0) |
| `BillboardMaterial` | 3 | `GetType`, `InitInstance`, `Equals` |
| `ColorOneTextureMaterial` | 7 | `GetType`, `Bind`, `Unbind`, `SetDisplayState`/`ClearDisplayState` (base + colour-material mode 5/0), `InitInstance`, `Equals` |
| `AutoTexMaterial` | 8 | `GetType`, `Bind`, `Unbind`, `Get/SetTextureGenerationParameters`, `ClearDisplayState`, `InitInstance`, `Equals` |
| `MVHSelectMaterial` | 7 | `GetType`, `Get/SetStageColor`, `SetDisplayState`, `ClearDisplayState`, `InitInstance`, `Equals` |
| `ReflectiveMaterial` | 9 | `SetResourcePriority`, `GetType`, `IsLoaded`, `Unbind`, `GetTexture` (both), `Activate`, `SetCubeMapMatrix`, `ClearDisplayState`, `Equals` |
| `GlossMaterial` | 7 | `GetType`, `Unbind`, `GetTexture` (both), `Activate`, `CacheAlphaHint` (writes 0), `Equals` |
| `GenericShaderMaterial` | 3 | `GetType`, `Get/SetAlphaHint` |

(`Material`'s own 4 counted above are the `ResultClass` returns of `IsLoaded`, not `Material` members. `Material`
itself has no exports in this slice; its ctor/dtor/`InitInstance`/`Equals`/`SetLightingState` are RenderSystem_7c
exports and are called there.)

## Forwarded and why

- **Constructors, destructors, vbase destructors, `CreateInstance`**: need the U4 cache objects
  (`MaterialChunkCache`/`AlphaBlendCache`/`AlphaMaskCache` construction and reference counting) or the plugin
  registry. Constructing a material also calls `SharedTextureCollection::GetTexture`, which needs a real texture
  manager. All material objects in the test are byte images with the original vftable, never our construction.
- **`operator==`**: `__RTDynamicCast` against the original class's RTTI; forwarding until the class objects are
  local.
- **`SetTexture` / `DuplicateTexture`**: duplicate/clone a `Texture` through the texture manager; U6/Texture not local.
- **`CacheChunk` / `RenderChunk` / `RenderNumChunks` / `RenderPrimitives` / `CreateGeometryChunk`**: need the U6
  geometry chunks, U4 caches and the light/vertex-program path.
- **`GenericShaderMaterial::SetResourcePriority`, `IsLoaded`, `GetTexture`, `Activate`, `Equals`, `Bind/Unbind`,
  `SetDisplayState`/`ClearDisplayState`**: walk the material's texture ring at +0xa4 and the vertex-program/cache
  pointers, so they stay with the cache unit.
- **`AutoTexMaterial::SetDisplayState`**: builds a 0x44-byte texture-coordinate matrix for Display slot 0x74.
- **`ReflectiveMaterial::SetDisplayState(Display*, bool)`, `Bind`, `InitInstance`**: cube-map texture-coordinate
  setup (Sphere mapping) and the 0xbc matrix copy; reserved for the render-state follow-up.
- **All `*MaterialPlugin` classes, `MaterialPluginX`, `StdMaterialPlugin`**: their `CreatePluginResources` constructs
  the U4 caches and their `CreateInstance` constructs materials; only their exported `IsSupported` is independent,
  and without the local class construction it would just be a display query.
- **All static data exports** (`DECORATED_TYPE`, `UNDECORATED_TYPE`, `UID`, vftables, vbtables): stay
  original-owned, as the workstream rules require. `GetType` returns the imported `UNDECORATED_TYPE` object, so the
  returned address and text are the original's.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u5a\src\jet" -B "D:\universal modder\r3d-agents\u5a\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u5a\build32" --config Release --target Render3DCore_7c
cl /nologo /EHsc /GR /Zp8 /std:c++20 /O2 src\jet\tests\diff_r3d_u5a.cpp /Fe:<testdir>\diff_r3d_u5a.exe
diff_r3d_u5a.exe <testdir> 1000
```

`<testdir>` = `testbin\*`, `Render3DCore_7c.dll` renamed `Render3DCore_7c_orig.dll`, our DLL, and the main build's
`Kernel_7c`, `KernelStandard_7c`, `KernelResource(s)_7c`, `RenderSystem_7c`, `Render2DCore_7c` proxies.

Result: `310000 checks, 0 mismatches`. Every implemented method is called through `GetProcAddress` on both modules
with identical object images. The test builds each object as a byte image at the TS12 offsets and installs the
original class's vftable, so it needs no local constructor. It compares the whole object, the return value, the
`Display` state block after the state members, the stand-in texture's recorded priority/activate calls, and for
`InitInstance` only the class fields from +0x9c (the base `Material::InitInstance` writes a fresh unique name in the
RenderSystem code that is already verified). Awkward values include `+/-0`, `+/-inf`, quiet and signalling NaNs;
`SetStageColor`, `GetTextureGenerationParameters` and `SetCubeMapMatrix` are covered by the x87 quieting round.

**Merge note:** `src/jet/CMakeLists.txt`'s `foreach(t u1b u1c u1d1 u1d2 u2 u3)` does not include `u5a`, so the
`diff_r3d_u5a` target is not generated yet. The source compiles standalone with the command above; add `u5a` to that
list (or an explicit `add_executable`) when merging.

## Layout, NaN and ABI facts

- `Material` own state ends at `+0x98` (`materialManager`); a concrete class field starts at `+0x9c`. The
  `ReferenceCount` virtual base sits after the most-derived fields: `Material` size 0xa8 (RC +0xa0),
  `NoTextureMaterial` 0xac (vtordisp +0xa0, RC +0xa4), the one-texture family 0xb4 (RC +0xac), `AutoTexMaterial` 0xc4
  (RC +0xbc), `MVHSelectMaterial` 0xc8 (RC +0xc0), `GenericShaderMaterial` 0xc0 (RC +0xb8) and Reflective/Gloss 0x108
  (RC +0x100). `Material`'s vbtable is `{0, 0x9c}`, `NoTextureMaterial`'s `{0, 0xa0}`.
- Vtable sizes: `Material` has 23 slots; `NoTextureMaterial` and `GenericShaderMaterial` stop at 23;
  `OneTextureMaterial` and every class below it add **CacheAlphaHint (23), SetDisplayState (24), ClearDisplayState (25)**.
  `CacheAlphaHint` is `__thiscall` (`UBEXXZ`), unlike every other implemented member (`__cdecl`, `UAA`/`MBA`/`IBA`).
- `Equals` compares a texture reference by its slot in the texture manager's 0x68-byte record:
  `((int)a - (int)b) / 0x68 == 0`, not pointer equality. Reflective also compares `reflectivity` with a NaN-failing
  equality, then the context count and every `void*` in the context array.
- x87 copies (`fld`/`fstp`) quiet a signalling NaN in `SetStageColor`, `GetTextureGenerationParameters` and
  `SetCubeMapMatrix`; `MOVSS` setters (`SetTextureGenerationParameters`, `SetResourcePriority`, opacity) preserve the
  input bits. Captured in the test.
- `Display` virtual slots the project header does not name are called at **0xa0, 0x78, 0x128, 0x12c, 0x130, 0x160**
  (`__cdecl`, this first). The named state calls go through the RenderSystem exports; the state block is
  `*(void**)(display+0x54)`, with alpha-blend bit at `+0xa1`, blend factors at +0x68/+0x6c and the two-sided/blend
  flags at material +0x84.
- `IsLoaded` and `DuplicateTexture` return `ResultClass` in memory: `this` at `[esp+4]`, hidden result pointer at
  `[esp+8]`; the test calls them as `int*(__cdecl*)(const void*, int*)`.

## Blockers / next

- U4 caches (and the texture clone path) block the constructors, `CreateInstance`, the plugin classes, the cache/
  render members and `GenericShaderMaterial`'s ring-based members.
- `AutoTexMaterial::SetDisplayState` needs the texture-coordinate matrix semantics (Display slot 0x74).
- No in-game check: this unit changes no code path the main menu uses until the material constructors/plugins are
  cut over; the differential test is the verification.
