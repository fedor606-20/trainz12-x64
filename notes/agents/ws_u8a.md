# WaterSystem U7b/U8a: WaterMaterial base, WaterBumpMaterial, WaterOceanMaterial

## Result

- `build32\Release\WaterSystem_7c.dll` builds cleanly (VS 18 2026, Win32, Release).
- `tools\check_exports.py` reports **490/490 exports, 0 missing, 0 extra, 0 forwarded**.
- The U7b/U8a fragment (`implemented.d/u8a.txt`) makes **127 exports** local; the rest of the
  module is local in other units (module total 490, no forwards).
- `src/jet/tests/diff_ws_u8a.cpp`: **447 checks, 0 mismatches** against
  `WaterSystem_7c_orig.dll` (78 constructor/state/chunk checks plus 369 rendering-path call-log checks
  added in the 2026-11-24 behavioural pass; see below for the build/run command).

## Layouts

`WaterMaterial` = 0xf0: Material at 0, vbptr +4, own state +0x9c..+0xe0, vtordisp,
ReferenceCount at +0xe8. `WaterBumpMaterial` = 0x138 (ReferenceCount +0x130),
`WaterOceanMaterial` = 0x174 (ReferenceCount +0x16c). The header asserts every offset.

Bump fields: reflection texture +0xe4, VertexPrograms +0xe8/+0xec, TextureCombiners
+0xf0/+0xf4, render target +0xf8 (left uninitialized by the original ctor), refractive index
+0xfc (1.3333), color +0x100, VertexType +0x110, WaterTextureAnimation +0x114,
FrameAnimation +0x118, interpolation +0x11c, refraction +0x11d, UV offsets +0x120/+0x124
(left uninitialized), scaleNormXY +0x128.

Ocean fields: reflection +0xe4, programs +0xe8..+0xf4, VertexType +0xf8, render target +0xfc
(left uninitialized), refractive index +0x100, color +0x104, colourScale +0x114, depth
+0x124..+0x130, waveDir +0x134, texture animation +0x140 and frame animation +0x144 (both
left uninitialized), flags +0x148/+0x149, UV offsets +0x14c/+0x150 (left uninitialized),
elapsed time +0x154, wave attributes +0x158..+0x160, scaleNormXY +0x164.

Base ctor defaults (verified against the assembly): waterColor (0.15, 0.35, 0.35, 1.0),
lightColor (1, 1, 1, 1), ambient (0.1, 0.1, 0.1, 1), lightDir (0, 0, -1). The two caches are
`"ChunkCache"` (0xd2c) and `"AlphaBlendCache"` (0x80), found/created through the kernel's
`+0x6c` RenderingCacheManager and referenced.

Statics: `WaterBumpMaterial::uid` = 5EA5C88C-A2F1-423A-B5AB-FFB935941E25,
`WaterOceanMaterial::uid` = EF9795F6-F515-4AEE-A89F-6FBD9CF42FF6,
UNDECORATED_TYPE = "WaterBumpMaterial"/"WaterOceanMaterial",
DECORATED_TYPE = "m.WaterBumpMaterial"/"m.WaterOceanMaterial". The base PStrings carry the
program names (`psWaterMatInterp`, `vsWaterMat`, ...).

## Quirks kept

- Bump's ctor leaves the render target, both animation pointers and the UV offsets
  uninitialized; Ocean's leaves the animation pointers and UV offsets uninitialized. The
  differential ctor test relies on this (0xcd bytes match).
- Bump's dtor does not release its VertexType; Ocean's does.
- `WaterMaterial::GetType`/`CreateInstance`/`operator==` are the original's "return null"
  stubs.
- The base 6-parameter `CreateGeometryChunk` forwards to the 7-parameter form with the
  vertex-program flag set to 1.
- `SetWaterColor`/`SetAmbientColor`/`SetLightDir`/`SetLightColor` and `InitInstance` use
  x87 fld/fstp so signalling NaNs are quieted as in the original.

## Vtable handling (important)

VS 2026 places a new overload of an inherited virtual (`CreateGeometryChunk`) at the first
free base-table slot (23), while the TS12 compiler kept it at its declaration position (46).
`WaterMaterialTables.cpp` therefore records the compiler tables at load and rotates slots
23..46 back by one; the constructors install the corrected table and the exported vtable
symbols alias the corrected arrays. This restores the original slot order, e.g. slot 46 is
`CreateGeometryChunk`, slot 47 `InitInstance`, and the concrete extra virtuals are 48..51:
`CreateVertexType`, `SetupVertexProgramConstants`, `SetDisplayState`, `ClearDisplayState`.

## Geometry chunks

`CreateGeometryChunk` builds the original `GeometryChunkTemplate<PackedVertexPCTUD,
GeometryChunkSingleStream>` / `<PackedVertexPA1A2T, ...>` shape: a 0x60-byte
GeometryChunkSingleStream-subclass with a per-object CPU vertex array at +0x5c (head:
refcount/count at +0/+4, items at +0x10; +8/+c are left as allocated like the template).
Item offset tables match the binary; vertex type id is
99 in both; vertex sizes 0x18 (Bump) and 0x34 (Ocean); arrays viewed through the original's
raw slots (0xc4/0xbc) return the same strides and data as the original.

## Known remaining points

- The chunk `InitializeBuffers`/`LoadBuffers` allocate through display slots 0x104/0x108 and upload through
  the buffer manager's 0x20/0x2c (see the final-integration follow-up); the per-record CPU array head now
  matches the template ctor 0x10006bf0 (+8/+c left as allocated).
- Material destructors were not exercised by the test (the fixture's cache lifetime makes a
  standalone dtor call crash for the original too); they are compiler-generated in the
  original's shape.

## Test run

```
cmake --build build32 --config Release --target WaterSystem_7c diff_ws_u8a
copy build32\Release\WaterSystem_7c.dll <fixture>\
copy build32\Release\diff_ws_u8a.exe <fixture>\
diff_ws_u8a.exe <fixture>
```

The fixture needs `WaterSystem_7c_orig.dll`, the Jet modules (`RenderSystem_7c_orig.dll`,
`KernelStandard_7c_orig.dll`, `Render3DCore_7c.dll` + `_orig`, `DebugInterface_7c.dll`,
`ijl10.dll`, `msvcp80.dll`, `msvcr80.dll`). Result: **447 checks, 0 mismatches**: 78 constructor,
state, VertexType and chunk checks plus 369 call-log checks from the rendering-path stand-ins (see
the follow-up below).

## Follow-up 2026-11-24 (behavioural pass, U8a)

- `WaterBumpMaterial::CalculateLerpTexture` (0x10006840) and `WaterOceanMaterial::CalculateLerpTexture`
  (0x10015060) now follow the retail body: render target from +0xf8/+0xfc, the two frames from the
  `WaterTextureAnimation` array (the second is `(frame + 1) % numFrames`), the delta vector
  (`FrameAnimation` +0x14) into the lerp combiner slot 0x30, the `DirectX` PString through
  ExtendedGfxLanguage slot 0x1c, `GetLocalConstantNumber("texSize")` and slot 0x5c with the
  0.5-scaled UV offsets, Bind slots 0x14/0x3c, display slot 0xac(1), depth off, texture binds 0/1,
  `RenderSquare`, End slots 0x18 on both, target null, depth on, stages 0..3.
- `WaterBumpMaterial::SetDisplayState` (0x10006070) has the 0x2f38c frame gate (shared static,
  initial 0xffffffff), IAT `SetCullMode`, reflection bind 3 and the interpolation/direct-texture
  branches. `ClearDisplayState` (0x10006160) unbinds stages 3..0, ends the combiner/program and
  culls 1. Ocean's two slots stay the 0x10003540 no-ops. The Bump bodies touch no fog/blend or
  +0x14..+0x1c fields; the only display field read is +0x3c.
- `SetupVertexProgramConstants` (0x100061d0 / 0x100149e0) reproduces the camera basis, the binary's
  flip matrix (m0=m6=-1, m9=m15=+1), the `view * basis * flip` inverse with the exact product order of
  0x10005180, the shifted local copy (0x100064b4 copies P+4 to B+4), the transposed modelView product,
  the 0x14/0x2b uploads and the 0x0f/0x10/0x11 axes. Refraction keeps the x87 split: `(1-r)^2` is
  rounded to float first, `(1+r)^2` stays double, then the float quotient.
- Ocean `RenderChunk` (0x10014830) now has the 0x2f854 frame gate, calls display slot 0x54(modelView),
  then the material's Setup with the camera at `view + 0x20`, and uses IAT `SetCullMode`; it no longer
  calls the display's 0xc4 slot.
- Chunk head: `CreateArray` leaves +8/+c as `Alloc::New` returned them, as the template ctor
  0x10006bf0 does (nothing reads them). `DuplicateVertexArray`/`ResizeVertexArray` already match
  0x10006de0 / 0x10006f00.

Known deviation: the original's shifted copy (0x100064b4/0x100064bb) reads one float past the
`basis*flip` matrix (`[esp+0x168]`, uninitialised stack) and uploads it as the first component of
the 0x2b constant. We zero it; the test normalises the first uploaded vector and compares the other
eight floats bit-exactly.

The 369 new checks use stand-in Display/ViewPort/Camera/VertexProgram/TextureCombiner/Texture objects
whose vtable slots log the calls; the imported `Display::SetRenderTarget/SetDepthTestMode/SetCullMode`,
`VertexProgram::SetLocalConstant(Color)`, `TextureCombiner::SetConstantRegister(Color)` and
`ExtendedGfxLanguage::GetLocalConstantNumber` methods are IAT-patched to loggers. The test also asserts
the DirectX/texSize counts (4 `Defines` calls, 3 `GetLocalConstantNumber("texSize")` calls, i.e. the
false branch skips the lookup) and that the Ocean 0x10003540 display slots log nothing. Export check:
490/490 names and ordinals, 0 missing/extra, no `WaterSystem_7c_orig.dll` dependency.

## Remaining forwards in this scope

None. All 127 U7b/U8a exports are local: WaterMaterial (39), WaterBumpMaterial (43 incl.
DECORATED_TYPE), WaterOceanMaterial (43), and the two factory `CreateInstance` bodies
(aliased to the local cdecl thunks); the FresWave/OneTex factories are local in their own
unit.


## Follow-up 2026-11-24 (final integration)

- The chunk `InitializeBuffers`/`LoadBuffers` stubs were replaced with the original's display-slot
  0x104/0x108 allocation and buffer-manager 0x20/0x2c upload path; `AssignVertexType` now fetches
  the VertexType through Display slot 0x10c. This closes the largest rendering gap listed above.
- `WaterMaterialStatics.cpp` no longer loads or mirrors `WaterSystem_7c_orig.dll`: the four factory
  tables are built from the imported `PluginType::AddReference/ReleaseReference` and
  `MaterialPlugin::CreateInstance` addresses. RTTI locators are absent by design.
- The `WaterAnimation`/`WaterAnimationPN` virtual order was corrected to `Initialize, GetFrame,
  GetXYFrame`; U6's slot-1 `GetFrame` call and U10's factory path now agree.
- Final module: 490/490 exports local, 0 forwards. Clean build `build32-final`.
