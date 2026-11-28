# U8b: WaterMaterialFresWave, WaterMaterialOneTex, the FresWave factory

Owner files:

- `src/jet/graphics/WaterSystem_7c/WaterMaterialFresWave.h/.cpp`
- `src/jet/graphics/WaterSystem_7c/WaterMaterialOneTex.h/.cpp`
- `src/jet/graphics/WaterSystem_7c/WaterMaterialFresWavePlugin.h/.cpp`
- `implemented.d/u8b.txt` (92 export lines: the 48+29 material exports, the seven FresWave-factory TODO exports, the
  OneTex factory's CreateInstance, and the seven concrete-material virtuals the full module export list carries
  in addition to the per-class extracts: DuplicateTexture/IsLoaded/SetTexture).

## Layouts

| class | size | notes |
|---|---:|---|
| `WaterMaterialFresWave` | 0x21c | first field at +0xe4; vtordisp +0x210; virtual `ReferenceCount` at +0x214 |
| `WaterMaterialOneTex` | 0xf4 | one `Texture*` at +0xe4; vtordisp +0xe8; `ReferenceCount` at +0xec |
| `WaterMaterialFresWavePlugin` | 0x50 | MaterialPlugin base 0x20; `Display*` +0x20; `VertexType*` +0x24; 10 stage nodes +0x28..+0x4c |
| `WaterMaterialOneTexPlugin` | 0x20 | no state of its own |
| water chunk (FresWave, no VP) | 0x60 | `GeometryChunkTemplate<PackedVertexPNTT, GeometryChunkSingleStream>`, record 0x28, type id 0x22 |
| water chunk (FresWave, VP) | 0x60 | `GeometryChunkTemplate<PackedVertexPA2A4T, GeometryChunkSingleStream>`, record 0x2c, type id 0x63 |
| water chunk (OneTex) | 0x60 | `GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream>`, record 0x20, type id 0x12 |

FresWave fields: six vertex programs at 0xe4..0xf8 (wave, wave-anim, wave-refl, wave-anim-refl, anim-refl, refl),
active VP 0xfc, active combiner 0x100, secondary VP 0x104, 15 constant registers at 0x108, 12 at 0x144, three
pixel programs at 0x174..0x17c, the factory's stage-node array pointer at 0x180, detail/reflection textures at
0x184/0x188, reflection camera 0x18c, three off-screen DisplayTextures 0x190/0x194/0x198, the PA2A4T vertex type
0x19c, an unused PTT type slot 0x1a0, animation pointers 0x1a4/0x1a8, frame 0x1ac, flag byte 0x1b0, time 0x1b4,
grid spacing 0x1b8, refraction term 0x1bc, UV offsets 0x1c0/0x1c4, UV velocities 0x1c8/0x1cc, reflection type
0x1d0 (-1 at construction), scale 0x1d4, amount 0x1d8, water-colour dirty 0x1dc, two light-anim colours 0x1e0 and
0x1f0, light-anim direction 0x200, light-anim frame 0x20c.

## Behaviour notes

- Statics: uid FresWave `4F7EA82E-1CB4-47B4-B1C2-2FCEA5A7D7B9`, OneTex `...D7CA`; types
  `WaterMaterialFresWave` / `m.WaterMaterialFresWave`, `WaterMaterialOneTex` / `m.WaterMaterialOneTex`; vertex
  names `WaterPA2A4T` (type 99) and `WaterPTT`. The statics are constructed by our module's dynamic initialisers.
- FresWave's ctor writes flags 0x27 by masking the previous heap byte the way the original does; `lastFrame` is
  -1, `reflectionType` -1, `scaleNormXY` 1, `refractionTerm` 0x3ca3d70a.
- `SetWaterColor` copies four floats, sets the dirty flag and copies alpha to `Material::opacity` (+0x80).
- `SetRefractiveIndex` keeps the original's x87 double expression `(1-r)^2/((r+1)^2)`.
- `SetReflectionType` follows the original exactly, including the `flags & 4` guard and the `InitVertexProgramms`
  calls on the failing paths. `SetDetailAnim` sets bit 5 from bit 4.
- `InitVertexProgramms` chooses the active VP/combiner and resolves the 15/12 local constant registers by name
  (lightDir, time, tanu, tanv, scroll, scale, localToCube, eyeToCube, locCamPos, localCamPos, fresTerm, lightColor,
  ambColor, waterColor, refractVec).
- `Initialize` copies the config colours/light direction, sets the capability bits (0x2 fres, 0x8 detail stage,
  0x10 animation textures, 0x1 detail), checks the vertex-program/combiner platform support, creates the six
  vertex programs and three pixel programs through the display's managers, and makes the three 128x128 off-screen
  DisplayTextures. The 64 KB scratch bitmap is allocated with `Alloc::New(0x10000)` and freed last.
- The factory's ten "textures" are Display texture-stage nodes built from two static `TextureArgs` each. The 19
  static TextureArgs are recreated with the imported `TextureArgs` constructor from the parameters of the
  original's initialiser functions 0x1001a970..0x1001b1a0. Display slot 0x11c builds the node; slot 0x124 destroys
  it; 0x128/0x12c/0x130/0x134 are the stage/constant setters used by the render squares.
- The water chunk templates are local (the class is not exported from RenderSystem_7c for these vertex records).
  They reproduce the original's 0x10011ad0.. functions: shared vertex block {refs,count,itemSize,capacity},
  CreateInstance/CreateInstance(args), InitializeBuffers/LoadBuffers, DuplicateVertexArray, DuplicateIndexArray,
  ResizeVertexArray, the 0x10-based vertex view, AssignVertexType, type id/size/item offset and the five
  item-array accessors. Item offsets: PNTT from RenderSystem's exported table; PA2A4T local table
  `{1:0, 15:36, 24:12, 25:20}`; PNT from RenderSystem's exported table.
- `CreateGeometryChunk` hint behaviour: `hint->version == 1` calls chunk slot 100 with (0, -2, 1);
  `hint->[4] == 1` calls slot 0x74.
- The render methods call the display's raw slots exactly where the original does: 0xc0/0xc4 around a chunk,
  0x54 SetVertexProgram, 0x128/0x12c/0x130/0x134 for the off-screen texture stages, 0x104/0x108 to allocate
  buffers, 0x20c the buffer manager and 0x20/0x2c for the uploads.

## Factories

- `WaterMaterialFresWavePlugin` is fully local: default/named/copy ctors, assignment, dtor (releases the vertex
  type, destroys the ten stage nodes, then the imported MaterialPlugin dtor), `CreatePluginResources` and
  `CreateInstance` (Alloc::New(0x21c), construct, then `PluginInstance::Assign(material+4, this)`).
- `WaterMaterialOneTexPlugin::CreateInstance` is local (Alloc::New(0xf4), same assign). The rest of that factory's
  lifetime stays U7a's.
- The published factory vtables are U7a's mirrored arrays. U8b fills their remaining local slots at load:
  FresWave slots 0/1/2/3/5 and OneTex slots 0/1/3/5. Slot 4 stays U7a's IsSupported. The RTTI locator and the
  mirrored data remain read from the original module exactly as U7a does.
- Water chunk coordinates use `GeometryChunkTemplate<PackedVertexPNT,...>` in OneTex and the two water records in
  FresWave; both templates are emitted only locally (no new exports).

## Dependencies and shared-header notes

- `WaterMaterial.h` was being rewritten by U7b/U8a while this unit was written. The 7-argument
  `CreateGeometryChunk` and the 6-argument base wrapper need their final `__cdecl` declarations; our headers
  follow the base exactly.
- The `WaterMaterial` base's slot 5 (the 6-argument `CreateGeometryChunk`) must exist: FresWave/OneTex inherit it
  (original slot 5 is 0x100036c0, which forwards to the 7-argument virtual at slot 46).
- `RenderSystem_7c` imports used: `Material::SetLightingState`, `Material::MakeGeometryChunkFlags`,
  `Material::CreateInstance(const UId&, MaterialManager*)`, Display's `Get/SetViewModeParams`; the camera
  `GetViewTransform` is imported from `Render3DCore_7c` (the original WaterSystem imports it too; the CMake line
  does not list that library, so the .cpp adds the import library with `#pragma comment(lib, ...)`).
- Two Material entry points were declared with the wrong access/constness in the shared headers while this unit was
  written; this unit calls them through their exported names (`SetWaterMaterialLightingState`,
  `SetWaterMaterialTextureDefinition`), which stays valid regardless of the header state.
- The `TextureArgs` reconstruction in RenderTypes.h is smaller than the original's record: the original statics
  carry extra pointer fields (0x1001c300 etc. at +8/+0x10). Our factory builds its statics with the imported
  constructor and the field values recovered from the initialisers, so it is consistent with the proxy
  RenderSystem; it is not byte-identical to the original's static objects.

## Build and test status (2026-11-24)

- `build32-ws-b` Release target `WaterSystem_7c` builds clean.
- `tools/check_exports.py ... WaterSystem_7c.tsv --forbid-dll WaterSystem_7c_orig.dll`: 490/490 exports,
  0 missing, 0 extra, no forwarders (the generated .def has no `=WaterSystem_7c_orig` lines).
- Differential probes against `WaterSystem_7c_orig.dll` (same process, original loaded first):
  - one probe covers statics (uid bytes, UNDECORATED/DECORATED/vertex names), both vbtables
    `{0,0x210}` / `{0,0xe8}`, the 50 material vtable slots (none null, 48/49 local), the FresWave state methods
    over the full 0x21c object (RI, scale, UV velocity, two Updates, grid spacing, detail interpolation, Vector2
    return) and the OneTex state: 30 checks, 0 mismatches;
  - the factory probe covers default/copy/assign ctor, the vtable IsSupported call, bytes +0x20..+0x50 and the
    vector destructor on both modules: 16 checks, 0 mismatches.
  - `CreatePluginResources` against a stand-in Display logs the same register + ten stage creations + ten
    destructions in the same order; only the shared TextureArgs record bytes differ (header gap above).






