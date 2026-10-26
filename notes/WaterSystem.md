# WaterSystem_7c: inventory and unit order

714 functions, 490 exports. Decompiled source studied: `D:\trainz12-decomp\ghidra` decompile of the module
(IDA Hex-Rays dump with the project's names: `WaterSystem_7c.dll.c/h` next to the installed original). Sources:
`src/jet/graphics/WaterSystem_7c/`, header `src/jet/include/jet/WaterSystem*.h`.

Replacement order: leaf classes first, the managers last (the managers own the module state and call everything
below them; the module is replaced unit by unit the same way as RenderSystem_7c, `notes/RenderSystem.md`).

## Units

| Unit | Classes | Code addresses | Exports | Status |
|---|---|---|---:|---|
| U1 | FrameAnimation, PNoise, WaterGrid<float>/<Vector2> | 10001010..10002120, 100050C0..10005130, 100086A0..10008780, 10015CE0, 10091D0 | 40 | ✅ ours (2026-10-26) |
| U2 | WaterManagerConfig, AnimProp, MaterialProp, TileProp, PhysicsProp | 10002A90..1000AA70 | 36 | ✅ ours (2026-10-27) |
| U3 | DisplacementHull | 10001030..10001080, 10015CE0 (dtor fold) | 6 | ✅ ours (2026-10-27) |
| U4 | WaterAnimation, WaterAnimationPN | 10003440..100056F0, 10004890, 10004A80..10004FE0 | 24 | ✅ ours (2026-10-27) |
| U5 | WaterGeometry, PhysicsWaterGeometry, WaterGeometryAnimation, WaterGeometryVS | 10002130..10002A00, 100055B0..10008320, 100088E0..100091E0 | 33 | ✅ ours (2026-10-27) |
| U6 | WaterTextureAnimation | 10003490..10016C70 | 13 | ✅ ours (2026-10-27) |
| U7 | WaterMaterial (base) + WaterBumpMaterialPlugin, WaterOceanMaterialPlugin, WaterMaterialFresWavePlugin, WaterMaterialOneTexPlugin | 10003530..10003700, 1000D080, 1000D220, 10007240..10007340, 10012900..10012F00, 10013E60..10013F20, 100159D0..10015AD0 | ~35 | ⬜ |
| U8 | WaterBumpMaterial, WaterOceanMaterial, WaterMaterialFresWave, WaterMaterialOneTex | 10005710..10006B20, 1000D290..10011930, 10013040..10013800, 10013FD0..10015410 | ~150 | ⬜ |
| U9 | WaterTile, Water, WaterTiled, WaterSurface | 10003700..10003F20, 10015C10..10015D00, 100171A0..10017810 | ~40 | ⬜ |
| U10 | WaterManager, WaterManagerGeneric, WaterManagerOceanSim, JetGetPluginsWaterSystem | 100092A0..1000CB90, 100162F0 | ~35 | ⬜ |

Module state (statics, vftables, the shader strings `aVswatermat*`/`aPswatermat*` at 100207A0..10039004) stays
the original's until the units that own it are ours; the rest forwards to `WaterSystem_7c_orig.dll` through the
generated .def, like the other modules.

## U1 notes (the classes done so far)

- **FrameAnimation** (0x18 bytes, POD): `{numFrames, currentFrame, time, sampleRate, period, deltaFrame}`.
  `ctor(sampleRate, period)`: numFrames = period*sampleRate, rest zero. `Update(dt)`: time += dt;
  currentFrame = (int)(sampleRate*time) % numFrames (truncation toward zero); deltaFrame = frac(sampleRate*time)
  (double floor, as the original). The dtor is a shared no-op (0x10091D0, identical-code folding with other
  empty bodies); `GetSampleRate` shares its body with `WaterGrid::GetData` (0x1008750).
- **PNoise** (0x383C bytes, POD, no vftable): Perlin noise. Layout: numOctaves (+0, =4 after the ctor);
  perm[1..256] (+4); the ctor fills 256 random unit gradients per dimension (1D at +12340, 2D at +8228,
  3D at +2060), Fisher-Yates-shuffles the permutation, then wraps every table twice for wrap-around hashing
  (perm +1028, 1D +13364, 2D +10276, 3D +5120; the 2D/3D wraps are entry-shifted copies). The hash chains are
  1-based (`perm[0]` is numOctaves itself; the last cell's "next" hash reads it via the byte-truncated index —
  original quirk, kept). Noise1/2/3 = classic Perlin with the 1985 smoothstep; Turbulence* = fBm (octaves from
  the field; Turbulence3 doubles x and y but **not** z — original quirk, kept). The tables come from CRT
  `rand()`, so the object is only reproducible from the same seed.
- **WaterGrid<T>** (0x10 bytes): `{vftable, width, height, data}`; the only virtual is `Initialize(const Size&)`
  (allocates, no init); the dtor frees via Alloc::Delete; `operator=` returns void (original signature). Element
  at `data[x + y*width]`. Instantiations: `float` and `Vector2`. The allocation guard is MSVC 2005's array-new
  overflow idiom (bytes u32-wrapped, -1 when >= 2^30 for float, 2^29 for Vector2).

## U2/U3 notes (done 2026-10-27)

- **Layouts**: AnimProp 0x60 {PString animName, Size animSize, 8 floats/ints of timing and scale, Perlin and FFT
  sections, void* frameData (WANI bytes, shared on copy)}; MaterialProp 0x50 {name, RI 1.3333, three Colors,
  lightDir, fres, opacityTex, opacityAmount}; TileProp 0x24 {reserved float (never set), tileName, Size
  {2,2}, Vector3 scale, Vector2 uvScale}; PhysicsProp 0xc {waveSpeed 100, viscosity 0.5, deltaTime 0.02};
  WaterManagerConfig 0x160 {managerName, surface+detail AnimProps, MaterialProp, TileProp, PhysicsProp,
  cubeTextures[6] of interned empty strings}; virtuals: slot 0 Load(ConfigData&, db, path), slot 1
  Load(PString&, db).
- **Keys**: the config keys are interned PString statics the original creates at load (init function
  0x100198e0..0x10019cc0); ours are static PStrings with the same strings (not exported).
- **The db lookup**: both DisplacementHull::Initialize and the config Loads call db's vtable slot 3 by its raw
  shape, a __cdecl (db, Node** out, const PString& name) -> const PString* (shared helper CallDbSlot3 in
  WaterSystemInternal.h; ResourceDatabase's virtuals are __cdecl with `this` on the stack, slot 3 =
  GetFileName, whose by-value PString return slot the caller reuses).
- **AnimProp::LoadFile(File&)**: "WANI" binary; any other magic reads as failure so LoadFile(PString) falls
  back to parsing a .txt config (the "DetailAnim" key). The fallback config parse only runs when the binary
  read fails or the file does not open.
- The config ctor's detail-anim resolution: a relative name is tried under the config's own directory (through
  the db when one is given), then resolved through the db as-is; the name is used as-is when absolute, when it
  starts with \ or /, or when the config path is empty.

## U4 notes (done 2026-10-27)

- **Hierarchy**: WaterAnimation : PluginInstance (+ protected PluginBase for the lookup); WaterAnimationPN :
  WaterAnimation. The vtables, vbtables, vtordisps and the adjustor thunks are the compiler's, from the same
  class shape; the layout is WA 0x24 {vftable, vbptr, pluginFactory, Size, animMaxHeight, vtordisp,
  ReferenceCount} and PN 0x3870 {WA part, frameCount, frameCapacity, frames, numFrames, PNoise, vtordisp,
  ReferenceCount}.
- GetXYFrame is a null stub in the original (folded with other "return 0" bodies at 0x10015db0); Init is a
  "return true" stub (0x10013f10). CreateInstance looks the factory up by groupUid and calls its slot-3
  CreateInstance raw (VCall), then releases it, as the original.
- PN renders each frame by sampling TurbulenceTiledD over the grid (period 10, looping in z every 25 — the
  original multiplies by the float 1/25 constant 0x3d23d70a instead of dividing), then rescales the heights
  into +-animMaxHeight (CalcAnimation). The grids are Jet::Alloc objects (JET_HEAP_OBJECT added to WaterGrid).
- groupUid = 2D54F63B-FF0A-4877-9561-0BBC3E2AC6DB, ANIM_NAME = "WaterAnimationPN", the factory id =
  E43947F9-6280-4141-B8BC-8847196B62AF (RegisterWaterAnimationFactory, called by U10's JetGetPluginsWaterSystem).

## U6 notes (done 2026-10-27)

- WaterTextureAnimation 0x18 {FastDynamicArray<Texture*,13>, scale, maxHeight, Size}: per-frame bump textures
  from the animation's height grids, or one raw ARGB buffer (CreateAnimationTexture). The vftable: the two
  Initialize overloads, CreateAnimationTexture, Destroy (the dtor calls Destroy).
- The texture objects carry the ORIGINAL's Texture vftable (the game calls their virtuals): the ctor is the
  imported RenderSystem one, followed by the original's explicit vftable write, done through a linker
  /alternatename onto the imported ??_7Texture@Jet@@6B@. The RenderKernel's texture manager is reached via its
  raw vtable slot 38 and the DisplayTexture's slots 1/3 (make/fill) are called raw through VCall.
- RenderSystemShell.h: WaterSystem's local view of the RenderSystem classes it needs (Texture for now; grows
  with U5/U7/U8) — everything imported by name from RenderSystem_7c.dll.
- CalculateBumpMap: 127.5-scaled normals; the gradients wrap at the edges, the dx lags one sample behind (the
  original's behaviour), the centre is clamped to +-maxHeight into alpha, and the float math follows the x87
  rule (the 1/maxHeight division in double, 127.5/sqrt in double).

## U5 notes (done 2026-10-27)

- WaterGeometry 0x40 {GeometryChunk, WaterMaterial*, Size, Vector3 scale, boundsMin/Max, radius}: the chunk is
  made through the material's raw vtable slot 46 (CreateGeometryChunk) with a StandardChunkHint, the vertex
  arrays through the chunk's slots 49/47 (the {stride, pointer} pairs of items 1, 4, 15, 16), the indices via
  GeometryChunk::GetIndexArray, the upload via slot 4 and Render via the material's slot 14. The vftable: the
  two Initialize overloads (simple first), Render, Update (no-op), Destroy (protected).
- PhysicsWaterGeometry 0x7c: three w*w height buffers, the wave coefficients ((4-8c)*k, (speed*dt-2)*k,
  2c*k with c = speed^2*viscosity^2/scale.x^2, k = 1/(speed*dt+2)) and a static cos-wave flavour selected by
  the bool; Update writes the heights plus the wave attributes (items 24/25) and pings the buffers. The time
  accumulator is a module static. Displace(hull,...) overlays negative heights.
- WaterGeometryAnimation 0x1c {geometry, frames, xyFrames, currentFrame, width, height, scale 0.1}: Create
  samples the animation's frames (its vtable slots 1 and 2) into WaterGrids and morphs the chunk in Update
  (the original's quirks kept: the last row reads past the grid, the right neighbour wraps to the row's second
  element).
- WaterGeometryVS 0x48 {frequency, material}: the vertex-program variant fills the attribute items 24/25 with
  cos/sin border waves and rand-based interior terms (rand()/32768), the gradients, and all (0,0,1) normals.
- The PackedVertexPCT record is a local shell (4 corners, 24 bytes each: position, color 0xffffffff, texCoord);
  RenderKernel stays opaque (its +124 member feeds the chunk's slot 4, its +148 the TextureManager).
