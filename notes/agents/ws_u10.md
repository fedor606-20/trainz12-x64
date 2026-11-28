# U10: WaterManager, WaterManagerGeneric, WaterManagerOceanSim, JetGetPluginsWaterSystem

Date: 2026-11-24. All 50 U10 exports are local. The module build at the end of the session had **490 of 490
exports local, 0 forwards** (U8/U9 landed while U10 was being tested). Files:

- `src/jet/graphics/WaterSystem_7c/WaterManager.{h,cpp}` (11 exports)
- `src/jet/graphics/WaterSystem_7c/WaterManagerGeneric.{h,cpp}` (18)
- `src/jet/graphics/WaterSystem_7c/WaterManagerOceanSim.{h,cpp}` (20)
- `src/jet/graphics/WaterSystem_7c/WaterManagerEntry.cpp` (entry, 1)
- `src/jet/graphics/WaterSystem_7c/implemented.d/u10.txt` (the 50 names/ordinals)

## Layouts (32-bit, checked by `JET_LAYOUT32`)

All three derive `WaterManager : public PluginInstance, protected PluginBase`; `PluginInstance` virtually
inherits `ReferenceCount`, so the vtordisp and the reference count move to the end of the most-derived object.

| class | size | fields |
|---|---:|---|
| WaterManager | 0x178 | vftable 0; PluginInstance vbptr 4, factory 8; `WaterManagerConfig` 0xc..0x16c; vtordisp 0x16c; ReferenceCount 0x170 (vftable, count 0x174) |
| WaterManagerGeneric | 0x190 | material 0x16c; frame animation 0x170; texture animation 0x174; RenderKernel 0x178; cube map 0x17c; scaleNormXY 0x180; vtordisp 0x184; ReferenceCount 0x188 |
| WaterManagerOceanSim | 0x1bc | tileSize 0x16c; tileScale 0x174; tileUVScale 0x180; two ints 0x188/0x18c set to 7 by Initialize; material 0x190; surface frame animation 0x194; detail frame animation 0x198; texture animation 0x19c; geometry animation 0x1a0; RenderKernel 0x1a4; cube map 0x1a8; scaleNormXY 0x1ac; vtordisp 0x1b0; ReferenceCount 0x1b4 |

OceanSim's ctor **does not write** tileSize/tileScale/tileUVScale or the two ints (the original leaves them
uninitialized; GetTileSize/GetTileScale return them as-is). Generic's ctor zeroes all six of its fields and sets
scaleNormXY = 1.0f.

## Vtable order (a real MSVC quirk)

Retail order: `0 Initialize(kernel,db,config,name,int)`, `1 Initialize(kernel,db,name,int)`,
`2 SetReflectionType`, `3 GetReflectionType`, `4 SetScaleNormXY`, `5 GetScaleNormXY`, `6 Update`, `7 GetMaterial`.
The base table has stub bodies for slots 0 and 2..7 (shared `xor eax,eax; ret` / empty bodies; in our build the
linker ICF-folds them with unrelated identical stubs, which is harmless).

**MSVC emits same-named virtual overloads in reverse declaration order**: declaring the five-argument
`Initialize` before the four-argument one produced `[4-arg, 5-arg]` and crashed every virtual call. Both headers
therefore declare the four-argument Initialize first; the compiled tables now match retail (verified by
`diff_ws_u10`: every slot hits the exported function by name, and the slot after the table is the RTTI locator).

The exported PluginInstance table (`??_7...6BPluginInstance@1@@`) has the two adjustor thunks; the compiler
generates them from the same class shape.

## Behaviour (addresses from the assembly)

- `WaterManager::GetConfig` 0x56e0 = this+0xc. `Init` 0x13f10 = true.
- `WaterManager::CreateInstance` 0x93e0: `PluginBase::LookupPluginType(&p, groupUid, name)`, only when
  `Result::code == 0`; then factory slot 3 (`CreateInstance`) raw; then `ReleaseReference`.
  `groupUid = 243CB347-3A8D-409e-9EA4-420CC0CCA611`.
- `WaterManager::Initialize` (4-arg) 0x9460: `Path(name).DelName(false)`, `fileName = db->GetFileName(name)`,
  `config.Load(fileName, nullptr)` (**the original passes a null database here**), then an interned copy of
  `fileName` and virtual slot 0. The five-argument base body returns false.
- Generic Initialize 0xb450: `++WaterManagerGeneric::instance`; material name is
  `Setf("Water%d%s", instance, config->materialName)`; material found through `RenderKernel+0x90`
  (`MaterialManager::Create`, slot 3) and `dynamic_cast<WaterMaterial*>`; detail block when
  `config->detailAnimation.animMaxHeight > 0`: `FrameAnimation(detail.sampleRate, detail.period)`,
  `WaterAnimation::CreateInstance(detail.animName)` (or the WANI frame data through
  `WaterTextureAnimation::Initialize` slot 0), texture animation slot 1, then release the animation; then
  `material->Initialize(db, config, frameAnimation, textureAnimation, flags)`, `SetScaleNormXY`, cube map,
  opacity texture. FrameAnimation uses the **detail** sample rate/period; scaleNormXY is taken from
  `detail.animScaleNormXY` only when it is still exactly 1.0f.
- Ocean Initialize 0xc4a0: the material name is used **without** the `Water%d` prefix; detail and surface have
  separate `FrameAnimation`s (detail at +0x198, surface at +0x194). After the surface animation init it writes 7
  to +0x188/+0x18c; when `surface.animGeometry == 0` it stops (no geometry animation). Otherwise it allocates
  `WaterGeometryAnimation` (+0x1a0, scale 0.25f) and `Create` with the TileProp size/scale/UV scale.
- Update: Generic inlines the frame clock (time += dt; `sampleRate*time`, currentFrame = trunc % numFrames,
  deltaFrame = frac through double `floor`) then material slot 30. Ocean updates the surface clock with **no null
  check**, updates the detail clock when non-null, then `WaterGeometryAnimation::Update(surface clock)`.
- SetReflectionType/GetReflectionType forward to material slots 24/25; Generic SetScaleNormXY forwards to
  material slot 28, Ocean's only stores +0x1ac (the original does not forward). Null material:
  SetReflectionType false, GetReflectionType 1, GetMaterial/GetTextureAnimation/GetGeometryAnimation null.
- Destructor order: material ReleaseReference; texture animation dtor + Alloc::Delete; Generic: frame animation
  Alloc::Delete; cube map ReleaseReference. Ocean: texture animation, surface frame animation, detail frame
  animation, geometry animation dtor + delete, cube map.

## CreateCubeMapTexture

Generic 0xba00: six `Bitmap` objects. Missing/empty first cube name fails. If the passed config path is empty or
the first name starts with `\` or `/` or contains `:` the six names are loaded directly (database name through
slot 3 when a database is present, else an empty base path); otherwise each name is prefixed by the config path
and looked up through the database, and a failure falls back to reloading all six directly. Display =
`RenderKernel+0x7c` vtable slot 0x98; `DisplayFormat(DFORMAT_DEFAULT)`; slot 1
`(display, bitmaps, &fmt, 1,1,0,6,1.0f,1,1,1,1,1,1,&ref)`; slot 3 `(display,0,&fmt2,&bitmap,i)` per face; the
texture name is `"WaterCubeMap_%d"` with the current Generic instance. Ocean 0xc7d0 is the same without the
relative-path branch, uses `DisplayFormat(5)` and slot-1 argument 0 instead of 6, and names the texture
`"cubeMap"`. Texture objects use the RenderSystem `Texture` constructor import and then the original
`Texture` vftable, as U6 does.

## Material SetTexture ABI

The retail Material table has slot 9 = `SetTexture(int, const AutoReference<Texture>&, float)` and slot 10 =
`SetTexture(int, const ResourceDatabase*, const PString&, float, const String& request)`. `WaterMaterial.h`
declares those two in the opposite order, so the managers call them through raw vtable slots with the retail ABI
(Result returned in memory, `__cdecl`, `this` pushed):

- slot 9: `int* (__cdecl*)(void*, int* result, int type, const void* autoReference, float amount)`
- slot 10: `int* (__cdecl*)(void*, int* result, int type, const ResourceDatabase*, const PString*, float,
  const String*)`

Cube maps use type 9 with amount 1.0f (after AddReference on the member and releasing the temporary).
Opacity: only when `materialProperties.opacityTex` is non-empty. The name is lowercased. If the config path is
present and the lowercased name is relative (not starting `\` or `/`, no `:`), the path is
`Path(configName).AddPath(opacityName)`, interned and passed with type 6 and `opacityAmount`; when that call's
Result is non-zero, or the relative branch was skipped, the plain lowercased opacity name is passed with type 6
(the fallback ignores the result). The local Material base header's slot swap is the reason for the raw calls.

## Entry (0x162f0) and factories

Order: Bump, Ocean, FresWave, OneTex material factories; `WaterAnimationPN`; OceanSim; Generic.

| factory | name | UId | parent |
|---|---|---|---|
| WaterBumpMaterialPlugin | WaterBumpMaterial | 5EA5C88C-A2F1-423a-B5AB-FFB935941E25 | — |
| WaterOceanMaterialPlugin | WaterOceanMaterial | EF9795F6-F515-4aee-A89F-6FBD9CF42FF6 | — |
| WaterMaterialFresWavePlugin | WaterMaterialFresWave | 4F7EA82E-1CB4-47b4-B1C2-2FCEA5A7D7B9 | — |
| WaterMaterialOneTexPlugin | WaterMaterialOneTex | 4F7EA82E-1CB4-47b4-B1C2-2FCEA5A7D7CA | — |
| WaterAnimationPN | WaterAnimationPN | E43947F9-6280-4141-B8BC-8847196B62AF | WaterAnimation group 2D54F63B-FF0A-4877-9561-0BBC3E2AC6DB |
| WaterManagerOceanSim | WaterManagerOceanSim | FBE986AD-89CD-4298-84C3-7B2D2F347899 | WaterManager group 243CB347-3A8D-409e-9EA4-420CC0CCA611 |
| WaterManagerGeneric | WaterManagerGeneric | 00A106DC-1BBC-4078-AE59-99EA0AF8042A | WaterManager group |

Material factories are the shared 0x20-byte `MaterialPlugin` base (0x50 and a zeroed 0x20..0x4c tail for
FresWave) with U7a's mirrored vtable (`WaterMaterialInternal.h`). The manager and animation factories are local
`PluginFactoryOf<>` objects (`new` uses the inherited PluginInstance heap operators). **Remaining runtime
dependency**: U7a's `WsEnsureMirrors()` loads `WaterSystem_7c_orig.dll` to mirror the four material vtables; the
entry inherits that design, so the module still needs the renamed original present for material plugin creation.

`MANAGER_NAME` values are `"WaterManagerGeneric"` / `"WaterManagerOceanSim"` (retail PString statics at
0x390c4/0x390c8, initialized from the same strings).

## Quirks / deviations kept

- The base GetScaleNormXY stub returns 0.0f (`fldz`) in ours; retail points at a shared `xor eax,eax; ret`
  body that does not define ST0. The base manager is never created by the game (the derived factories are), so
  this is only observable by constructing a bare WaterManager and calling slot 5.
- `WaterManagerGeneric::instance` is incremented by every Initialize and is never decremented (retail).
- Generic's material name starts at 1 for the first manager; the cube map uses the same instance number.
- The two manager `Initialize` four-argument bodies fold to the same address in retail; our compiler/linker also
  folds them (both containers showed the same address).
- Failure paths do not release the animation/frame objects, matching retail (the destructor cleans them later).

## Existing U4 table mismatch (not edited, U4 owns it)

`WaterAnimation`/`WaterAnimationPN` in U4 have the retail table order rotated: U4 is
`[GetXYFrame, Initialize, GetFrame]`, retail is `[Initialize, GetFrame, GetXYFrame]` (retail PN vtable
0x1001d950). U10's detail-animation setup uses a compatibility helper: for a `WaterAnimationPN` (the U4 factory
object) it calls `WaterAnimationPN::Initialize` directly, otherwise it uses retail slot 0. This works today but
U4's own vtable exports remain rotated; the U4 owner should declare `Initialize` and `GetFrame` in the base before
`GetXYFrame` (same reverse-overload rule if needed). U6's `CalculateBumpMap` also reads GetFrame through slot 1,
which is `Initialize` in the current U4 layout; that will need the same fix or a direct call.

## Test

`diff_ws_u10` (built standalone in the session temp directory, one process per module; the plugin registry is
global) loads `WaterSystem_7c.dll` / `WaterSystem_7c_orig.dll` from `testdir-u10` and compares canonical lines:

- constructor layout of the three managers (vtable/vbptr/config vtable/ReferenceCount/vtordisp fields),
- GetConfig, Init, scale/reflection defaults, null material/texture/geometry, the Ocean tile getters,
- base vtable stub slots and the full eight-slot tables of both managers by exported function name,
- Generic/Ocean frame-clock Update values,
- `Initialize` with a hand-made database object returning the config's default manager name (missing file path:
  returns false, config unchanged),
- entry registration under hooked `PluginType` constructor and `LookupPluginType` imports (the Kernel plugin
  library is not initialized in the harness): names, UIds and group ids for the animation and two managers,
- `WaterManager::CreateInstance` for both manager names through the real registered factories (factory slots
  0/1 replaced by no-ops so the uninitialised PluginType base is not touched), and null for an unknown name,
- `WaterAnimation::CreateInstance("WaterAnimationPN")` through the same registry.

Result: **66 of 66 normalized lines identical, both processes exit 0** (test built `/Od`; the `/O2` harness is
not stable with the retail DLL because it calls raw vtable stubs and leaked factories at exit, while the `/Od`
run is clean). The test does not cover the cube-map/display path (needs a real RenderKernel/Display) or a
successful config load (needs a real ResourceDatabase); those were reviewed against the assembly instead.

Final export check: `tools/check_exports.py build32-ws-d/Release/WaterSystem_7c.dll
D:\trainz12-decomp\exports\WaterSystem_7c.tsv` -> `DLL 490 exports, TSV 490; missing/wrong 0, extra 0`. All 50
U10 names are local (none forwarded).


## Final integration note (2026-11-24)

The inherited runtime mirror dependency is gone: `WaterMaterialStatics.cpp` now builds the four
factory vtables locally from the imported base methods, so `WsEnsureMirrors()` never loads
`WaterSystem_7c_orig.dll`. The whole module is 490/490 local.
