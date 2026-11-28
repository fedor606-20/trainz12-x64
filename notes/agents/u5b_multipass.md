# U5b: multipass material family

Status 2026-11-24: **44 of 254 exports local and differentially verified; 210 forwarded.** The local set is the part
of the slice that can be reconstructed without `Jet::Material` (U5a), the U4 caches, the U6 chunk templates or the
`MultiPassRenderState` display path. Build and test are clean:

```
diff_r3d_u5b.exe <testdir> 1000
2086009 checks, 0 mismatches
```

## What is local (44)

| Class | Exports |
|---|---|
| `MultiPassRenderState::MPLightRender` | ctor, dtor |
| `MultiPassLightRender` | ctor, `GetNumLights`, `GetNumPasses`, `GetTotalLights`, `AddTexture`, `AddVPProperty`, `AddTCProperty`, `AddScissorLight` |
| `MultiPassBumpChunkBase` | ctor, copy ctor |
| `MultiPassNoTextureMaterial` | 8 mode get/set (`SmoothBlend`, `SinglePass`, `Intensity`, `DebugPass`), `GetType`, `GetTexture` (both const/non-const), `Activate`, `UpdateTextureArray` |
| `MultiPassOneTextureMaterial` | `GetType`, `GetTexture` (both), `Get/SetAlphaHint`, `Activate`, `UpdateTextureArray` |
| `MultiPassBumpMaterial` | `GetType`, `GetTexture` (both), `Activate`, `UpdateTextureArray` |
| `MultiPassBump2Material`, `MultiPassBumpSpecMaterial`, `MultiPassBumpSpec2Material` | `GetType` each |
| the four material plugins | `IsSupported` each |

The exact 44 mangled names, with their alias targets, are in
`graphics/Render3DCore_7c/implemented.d/u5b_multipass.txt`.

## What is forwarded (210) and why

| Group | Count | Reason |
|---|---:|---|
| Material ctors/dtors/`vbase destructor`/`operator=` | 55 | call `Jet::Material`'s constructor/destructor (U5a) and the vbase tables; `MultiPassBumpChunkT/T2` also call `GeometryChunkTemplate` (U6) |
| Static data: vtables, vbtables, `UID`, `DECORATED_TYPE`, `UNDECORATED_TYPE` | 49 | shared data that must stay one object with the original until the class vtables and RTTI move together |
| Plugin `Initialize*`, `LoadLightPrograms`, `RegisterBuiltInPrograms` | 35 | create `MPLightRender` objects through the U4 caches and RenderSystem vertex/texture programs |
| `CreateInstance`/`InitInstance`/`CreateGeometryChunk`/`AllocateGeometryMemory` | 24 | allocate through `Material::CreateInstance` (U5a) or the U6 chunk templates |
| `MultiPassRenderState` (ctors, `StateSetup`, `LoadRenderProperties`, `LoadTextures`, light passes, `RenderGeometry`) | 15 | 12 KB of display/geometry/light state; needs U4, U5a, U6 and RenderSystem render state at once |
| `Bind`/`SetDisplayState`/`ClearDisplayState`/`CacheChunk`/`RenderChunk*`/`RenderPrimitives` | 13 | read the U5a `Material` head and the U4 caches, and drive the RenderSystem display state |
| Material equality/property/texture virtuals (`Equals`, `operator==`, `AddProperty`, `DuplicateTexture`, `IsLoaded`, `SetTexture`) | 11 | need RTTI casts plus `Material::Equals`/`SetTextureDefinition` (U5a); `SetTexture`/`DuplicateTexture`/`IsLoaded` are U5a's slice |
| `CreateNewLightPass`, `MultiPassNoTextureMaterial::RenderState`, the two `Unbind`s | 4 | light-pass program storage (U4) / U5a material state |
| `MultiPassBumpChunkBase::CreateTangentVectors` and the two `ChunkT/T2::InitializeBuffers` | 3 | geometry chunk virtual API (U6) |
| `MultiPassLightRender::Render` | 1 | light-pass render loop through the RenderSystem display state |

Some local names are virtual (`GetType`, `GetTexture`, `Activate`, `UpdateTextureArray`, `IsSupported`). The class
vtables remain the original's (forwarded data), so a virtual call from an existing object still enters the original
slot; the exported entry points are ours and are bit-identical, which is what the fragment verifies. This is the same
partial-cutover position the earlier units used for vtables.

## ABI, layout and numeric facts

- **Calling conventions** (checked against the assembly): the material leaves are `__cdecl` even when virtual (`this`
  at `[esp+4]`); all constructors, the four appenders and `MPLightRender::~MPLightRender` are `__thiscall`. The dtor has
  no deleting flag (`??1`, not `??_G`).
- **`MultiPassRenderState::MPLightRender`** is 0x50 bytes: vftable +0, reference count +4, six `{count, capacity,
  data}` arrays at +0x08, +0x14, +0x20, +0x2c, +0x38, +0x44. The dtor releases every element with
  `ReferenceCount::ReleaseReference` (no null check), frees the six blocks in reverse order, zeroes the counts, leaves
  the capacities and the six data pointers *dangling* (the original does too), and finally calls
  `Jet::ReferenceCount::~ReferenceCount`, whose prologue writes the base vftable at +0 (that is the final value at
  offset 0). The ctor's first `ReferenceCount` vftable store is overwritten by the class vftable, so the emulation
  writes the class vftable directly.
- **`MultiPassLightRender`** is 0x38 bytes: vftable +0, reference count +4, passes/capacity/data at +0x08/+0x0c/+0x10,
  the `Array<unsigned,1>` scissor list at +0x14, and the five per-light-type counts at +0x20..+0x30 with their sum at
  +0x34. The ctor maps its five arguments to +0x20 (1st), +0x2c (2nd), +0x24 (3rd), +0x28 (4th), +0x30 (5th).
  `GetNumLights(type)` is a raw `this + 0x20 + type*4` read; `GetTotalLights` is +0x34.
- **Light pass record** is 0x104 bytes. Appenders work on the last pass (`passData + (passes-1)*0x104`): textures at
  +0x00, vertex-property lists at +0x0c + which*0x0c, texture-combiner lists at +0x84 + which*0x0c. The texture list
  grows in steps of **4** 8-byte pairs (0x33c70); the VP/TC lists grow in steps of **5** (0x1000f720); the scissor list
  grows by exactly one `unsigned` per add. Each item is `{property, value}`.
- **Material tails**: A/B textures at +0xa4/+0xa8, the four `MultiPassNoTextureMaterial` mode bytes at
  +0xac/+0xad/+0xae/+0xaf, `Array<Texture const*,10>` at +0xb8, the OneTexture slot at +0xc4, alpha hint at +0xc8,
  the bump texture at +0xd0. `UpdateTextureArray` empties the array and appends one `Reserve(count+1)` at a time, so the
  capacity steps by 10 (the original's inlined Reserve allocates `capacity * sizeof(pointer)`).
- **`GetType` / `IsSupported` fallbacks** return the module's exported `UNDECORATED_TYPE` PString objects, resolved
  from `Render3DCore_7c_orig.dll` by name at run time, so the pointer, the interned node, its text and its reference
  counts are identical. `IsSupported` asks Display vtable slot 0x1c for codes 0x15, 0x11, 0x1b, short-circuiting; the
  failure path reproduces the original's inline PString assignment (destroy the old node, then lock, ++node refs,
  ++cache live refs, unlock, store).
- **NaN/inf/-0**: the local subset contains no float arithmetic, but every integer parameter is also exercised with
  raw awkward bit patterns (`0x80000000` -0, `0x7f800000` +inf, `0xff800000` -inf, qNaN/sNaN payloads) and the test
  compares stored words byte for byte, so no normalization or float round-trip is introduced. Float fields are not
  read or written by the local methods.
- **`.def` aliasing gotcha**: when an implemented entry maps to a plain C symbol, link.exe cuts the export name at the
  first `@` (`?GetNumPasses@...` became `?GetNumPasses`). The local cdecl leaves therefore live in
  `Jet::MultiPass::Leaves` (decorated names, like the existing RenderSystem aliases); this is required for any future
  alias of a decorated original name to a C function.

## Files

- `src/jet/include/jet/Render3DCoreMultiPass.h` - layouts, offsets, static asserts
- `src/jet/graphics/Render3DCore_7c/MultiPassState.cpp`
- `src/jet/graphics/Render3DCore_7c/MultiPassMaterials.cpp`
- `src/jet/graphics/Render3DCore_7c/MultiPassPlugins.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5b_multipass.txt` (44 names)
- `src/jet/tests/diff_r3d_u5b.cpp`
- `testbuild/CMakeLists.txt` - local harness for the test executable only

## Test

```
cmake -S D:\universal modder\r3d-agents\u5b\src\jet -B D:\universal modder\r3d-agents\u5b\build32 -A Win32
cmake --build D:\universal modder\r3d-agents\u5b\build32 --config Release --target Render3DCore_7c
cmake -S D:\universal modder\r3d-agents\u5b\testbuild -B D:\universal modder\r3d-agents\u5b\testbuild\build -A Win32
cmake --build D:\universal modder\r3d-agents\u5b\testbuild\build --config Release --target diff_r3d_u5b
diff_r3d_u5b.exe D:\universal modder\r3d-agents\u5b\testdir 1000
```

The test directory is `testbin/*` with `Render3DCore_7c.dll` renamed to `Render3DCore_7c_orig.dll`, the built
`Render3DCore_7c.dll` beside it and the main build's live `KernelStandard_7c`, `Kernel_7c`, `KernelResource_7c`,
`KernelResources_7c`, `RenderSystem_7c` and `Render2DCore_7c` proxies copied in. It compares raw object bytes, return
values, reference counts, virtual-call logs, allocated array contents and interned strings on the same inputs.

The test's stand-ins are flat buffers plus local vtable arrays (no game class is derived): the `Activate` texture
vtable records slot 0x3c calls, the Display vtable records capability codes, and the `MPLightRender` arrays hold
reference-count dummies whose `DeleteThis` (vtable slot 0) counts calls. `this`-taking entries are called through
`__thiscall`/`__cdecl` function pointers matching the exports.

## Blockers / merge notes

- `src/jet/CMakeLists.txt` has no `u5b` in the `diff_r3d_*` foreach, and the stream rules forbid editing it. The test
  is therefore built by `testbuild/CMakeLists.txt`; **the merge must add `u5b` to that loop** (or copy the target
  definition) so `diff_r3d_u5b` exists in the main build.
- The local leaves resolve `Render3DCore_7c_orig.dll` at run time for the original class vtables and the
  `UNDECORATED_TYPE` objects. That is the same requirement every forwarder in the proxy already has, but it must be
  revisited when U5a/U5b data exports become local: `GetType`/`IsSupported` then return the local statics instead.
- `ReferenceCount::~ReferenceCount` is called through a run-time lookup of
  `KernelStandard_7c!??1ReferenceCount@Jet@@MAE@XZ` because the local header declares it protected. A local derived
  proxy destructor is **not** equivalent here: the compiler-generated derived dtor corrupted the shared KernelStandard
  allocator across rounds in the differential test, while the direct import call matches the original exactly.
- U5a owns the `Material` class and 11 multipass virtuals (`SetTexture` x6, `DuplicateTexture` x3, `IsLoaded` x2). Those
  are intentionally not claimed here. When U5a lands, its class definitions will need the same field offsets the header
  documents; nothing in this stream defines a `Material`-derived class, so there is no header collision.
