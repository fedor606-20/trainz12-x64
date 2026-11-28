# U5a wave 4: material render, cache, chunk and display-state exports

Status: 2026-11-24. **45 new exports local; the U5a slice is 241 of 374** (196 from U5a/U5a2 + 45 here).
Differential test `diff_r3d_u5a_rem`: **73,309 checks, 0 mismatches** over 1,000 rounds against
`Render3DCore_7c_orig.dll`. The three existing unit tests stay green: `diff_r3d_u5a` 310,000/0,
`diff_r3d_u5a2` 113,034/0, `diff_r3d_u6` 293,480/0. `check_exports.py` on the built DLL:
`2500 exports, TSV 2500; missing/wrong 0, extra 0`. The generated `.def` has 1,938 local and 562 forwarded entries
(the rest of the tree is still moving in parallel; the U5a slice itself is 241 local / 133 forwarded).

Worked in `D:\universal modder\trainz12-x64`; only the U5a files listed below were edited. The build directory is
`build32-r3d-u5a` (`cmake -S src\jet -B build32-r3d-u5a -G "Visual Studio 18 2026" -A Win32`).

## Files

- `src/jet/graphics/Render3DCore_7c/U5aRem.cpp` (new) - all bodies below.
- `src/jet/include/jet/Render3DCoreMaterials.h` (edited) - added the override declarations for every implemented
  virtual to the ten concrete classes. The classes stay abstract (`CreateInstance() const` is still the original's),
  so no local vftable/vbtable is emitted.
- `src/jet/tests/diff_r3d_u5a_rem.cpp` (new) - the differential test. It needs no CMake target in the tree; it was
  built with `cl /nologo /EHsc /GR /Zp8 /std:c++20 /O2` against `src/jet/include`.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5a_rem.txt` (new) - 45 mangled names, all local (no aliases).

## Implemented (45)

| Group | Count | Exports |
|---|---:|---|
| `CreateGeometryChunk` | 5 | `NoTexture`, `OneTexture`, `OneTextureNoAlpha`, `ColorOneTexture`, `AutoTex` |
| `CacheChunk` | 6 | `NoTexture`, `OneTexture`, `OneTextureNoAlpha`, `Gloss`, `MVHSelect`, `GenericShader` |
| `RenderChunk` | 8 | all except `Reflective` |
| `RenderNumChunks` | 8 | all except `Reflective` |
| `RenderPrimitives` | 8 | all except `Reflective` |
| `SetDisplayState` | 2 | `AutoTex` (its texture-generation matrix), `Reflective` |
| `Bind` | 2 | `Reflective`, `Gloss` (both are the folded `if (pass == 0) SetDisplayState(display, false)`) |
| `InitInstance` | 3 | `Reflective`, `Gloss`, `GenericShader` |
| `SetTexture` | 2 | `GenericShader` (database and `AutoReference<Texture>` overloads) |
| `SetSphericalTexureCoordinates` | 1 | `Reflective` |

## Test

```
cmake -S src\jet -B build32-r3d-u5a -G "Visual Studio 18 2026" -A Win32
cmake --build build32-r3d-u5a --config Release --target Render3DCore_7c
cl /nologo /EHsc /GR /Zp8 /std:c++20 /O2 /I src\jet\include src\jet\tests\diff_r3d_u5a_rem.cpp ^
   /Fe:build32-r3d-u5a\Release\diff_r3d_u5a_rem.exe
copy build32-r3d-u5a\Release\Render3DCore_7c.dll <fixture>\
diff_r3d_u5a_rem.exe <fixture> 1000
```

`<fixture>` is `testbin-r3d` (copy of `testbin`, `Render3DCore_7c.dll` renamed `Render3DCore_7c_orig.dll`, our DLL,
and `build32\Release` `Kernel*`, `RenderSystem_7c`, `Render2DCore_7c`). The test:

- calls every export through `GetProcAddress` on both modules with identical byte-image materials/chunks/display;
- replaces the `Material::SetLightingState` and `SetTextureDefinition` imports in both IATs with recorders (the real
  function is RenderSystem code that needs a working light/vertex-program), and records opacity, the two matrices,
  the chunk/program/lights pointers and the unknown flag;
- detours the four U4 cache `Add` exports of the original and installs fake cache singletons so the original
  `CacheChunk` paths can run without constructing a cache;
- compares the whole fake display and state block, the material mutation, the recorded cache/geometry/primitive calls,
  the GenericShader texture ring semantically (types and texture ids, not block addresses), and the spherical UV
  array bit-for-bit (including NaN payloads).

Not exercised by the test: the three `InitInstance` bodies (they call the RenderSystem `Material::InitInstance`,
which needs the PString cache and a real manager) and the shader `CreateGeometryChunk` delegation branch (see below).
They are linked and included in the fragment.

## Layout / ABI facts learned here

- **`SetLightingState` matrix order.** The first `Matrix4 const*` is the **per-chunk model matrix** (the argument of
  `RenderChunk`/`RenderPrimitives`, or the internal billboard matrix) and the second is the **camera view matrix**
  (`Camera::cachedViewTransform`, +0x1b8). The header's parameter names `modelView`/`view` are the other way round.
  With a null viewport the original passes `nullptr` first and the model matrix second (plain materials); Billboard
  passes `nullptr` first and its billboard matrix second.
- **Billboard** builds a 4x4 with identity rotation and the translation row (`m[12..14]`) of the model matrix, passes
  it to Display slot `0x54`, then uses it as the first matrix of `SetLightingState`. Its SLS view matrix is a local
  temporary; the test compares its material/display effects and the other arguments, not that temporary's bytes.
- **`RenderNumChunks`** takes the vertex program from each **chunk** (`chunk->GetVertexProgram(0)`), the lights argument
  is a `FastDynamicArray<DisplayLight*>*` per chunk, and the per-chunk opacity is `chunk->opacity * material->opacity`
  (the null-view branch multiplies in the other order, as the reference does).
- **`RenderPrimitives`** passes the chunk to `GeometryChunk::RenderPrimitives` at slot `0x3c` (index 15) with
  `(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0)`.
- **CacheChunk**: the plain materials use the U4 singletons; `GenericShader` uses its own `chunkCache` (+0x9c) and
  `alphaBlendCache` (+0xa0). The opaque test is `1.0 <= material->opacity * chunk->opacity` (with `alphaHint != 1` for
  the one-texture family and GenericShader). Below it the material threshold at +0x20 selects
  `AlphaBlendCache` (`(int)(value * 100.0f + 0.1f) < 0x122`) or `UnsortedAlphaBlendCache` (distance 0.0).
  `OneTextureMaterial::CacheChunk` first calls `IsLoaded()` (slot 0x58) and, when the code is 0, `CacheAlphaHint()`
  (slot 0x5c; `__thiscall`).
- **`CreateGeometryChunk` hint semantics** (all values are `StandardChunkHint`):
  - `system & 1` enables the user bits at +0x10; `bits & 1` selects the Nml template (`PC` for no-texture, `PCT` for
    the one-texture family), otherwise the plain template (`PN`/`PNT`); `AutoTex` has only these two.
  - `ColorOneTexture` has no Nml path: it always builds `PCNT`, clears `bits & 1` **before**
    `MakeGeometryChunkFlags`, and uses `bits & 2` (not bit 0) for its dynamic/shared calls.
  - the shared tail: version 1 or (version 2 and bit 1) -> `SetVertexDataDynamic(0, -2, true)`; bit 0 **or** bit 1 ->
    `SetVertexDataShared(0, 0, false)` (the `-3` body in the decompiler is unreachable because bit 1 is the same
    condition); `size == 1` -> `SetIndexDataDynamic()`.
  - the shader branch (`system & 1`, bit 1, not bit 2 and display byte +0x211) still belongs to the U6 shader chunks;
    those five local exports forward that branch to the original export.
- **`SetSphericalTexureCoordinates`** calls the **const** item views at vtable offsets **0xc4 (Vector3)** and
  **0xbc (Vector2)**. The original's view record is returned through a hidden stack buffer, not in EAX:EDX; the body
  therefore calls `void(__cdecl*)(self, out, type, stream)`. The exact SSE order is v then u, with every `ADDSS`
  keeping the first product as its destination (this determines which NaN payload survives); the loop then scales by
  ±0.5 and adds 0.5, and sets vertex-data dirty `(true, 0, -2)`.
- **`AutoTexMaterial::SetDisplayState`** calls `SetTextureCoordGen(0, 2, 2)` then Display slot 0x74 with the matrix
  `{a,0,0,0; 0,b,0,0; 0,0,1,0; c*a,d*b,0,1}` where `{a,b,c,d}` is the texture-generation array at +0xa8.
- **`ReflectiveMaterial::SetDisplayState`** sets flags2 bit 2 from `display->State()[0x75] & 1`, binds the cube at
  unit 1, and for `contextCount >= 2` uses tex-gen mode 5/3 (3 coordinates for a 1D cube, 2 otherwise) when the
  corresponding flags2 bit is set; then it passes each context to Display slots 0x130 (colour = reflectivity in all
  four channels) and 0x128.
- **`ReflectiveMaterial::InitInstance`** copies the cube reference, `reflectivity`, context count/data, flags2 bits
  0..1, and the 0x40-byte cube matrix through x87 (sNaN quieting), then calls `Material::InitInstance` and copies the
  primary texture. `GlossMaterial::InitInstance` is the same body.
- **GenericShader texture ring**: circular 0x14-byte blocks `{next, prev, count, items[1]}` with items
  `{int type, Texture*}`; appending to a full block (`count == 1`) allocates a new block. `SetTexture` type > 0xf
  returns 2; missing texture 3; type 6 sets `alphaHint = 1`. The database overload takes the manager's reference when
  an item exists and releases it after appending otherwise; the reference overload addrefs/releases around the ring
  append. `SetResourcePriority` and the definition weight go through x87 (sNaN quieting).

## Remaining forwarded in the slice (133) and why

- **Data (71)** - every material's two vftables, vbtable, `DECORATED_TYPE`, `UNDECORATED_TYPE`, `UID`, and every
  plugin vftable. Original-owned as in U5a/U5a2: the constructors install those tables and `GetType` returns those
  addresses, so object identity/text stays exact.
- **Destructors and vbase destructors (31)** - the exports take the `ReferenceCount` sub-object pointer and chain
  through RenderSystem_7c's `Material`/`MaterialPlugin` destructor. Objects are still destroyed through the original
  tables this unit installs; keeping them forward keeps destruction call-identical.
- **Plugin factories (15)** - 8 `CreateInstance(manager, kernel)` + 7 `CreatePluginResources(kernel)`:
  `AutoTex`, `ColorOneTexture`, `GenericShader`, `Gloss`, `MVHSelect`, `NoTexture`, `OneTexture`,
  `OneTextureNoAlpha`, `Reflective`, `Std` (some inlined into `CreateInstance`). They need the U4 cache constructors
  and cache IDs, and `NoTexture`/`OneTexture` additionally create a shader chunk through Display slot 0x11c. The U4
  constructors and the cache IDs are still original-owned; the material constructors themselves are local.
- **MultiPass materials (11)** - `MultiPassBump/NoTexture/OneTextureMaterial` `SetTexture`/`IsLoaded`/
  `DuplicateTexture`. They are U5b classes (`Render3DCoreMultiPass.h`) and must not be copied here.
- **Reflective render paths (4)** - `CreateGeometryChunk`, `RenderChunk`, `RenderNumChunks`, `RenderPrimitives`.
  The render paths need the private matrix helpers `0x1001aaf0`/`0x1001ab30` (cell-space view transform multiplied
  into the cube matrix) and the shader chunk cache; the chunk factory needs the shader chunk branch as well.
- **GenericShader `CreateGeometryChunk` (1)** - builds a `GenericGeometryChunkSingleStream`/`MultiStream` through
  the display's shader chunk cache and the RVA-allocated `GenericGeometryChunk*` classes; U6 does not reconstruct
  those classes (they are generic RenderSystem chunks, not the template instantiations).

(The `MaterialPluginX` destructor is counted in the 31 destructors and its vftable in the 71 data exports.)

## Cross-unit findings

- The project `GeometryChunk` header's item-view virtuals return the 8-byte `VertexItemPtr` in EAX:EDX, while the
  original virtuals return the same record through a hidden stack buffer (the record has a non-trivial copy). A unit
  that calls those slots through the typed header method is ABI-wrong; `SetSphericalTexureCoordinates` uses the
  original 4-argument form. The typed calls also select one slot lower (0xc0/0xb8) than the original (0xc4/0xbc), so
  the header's virtual order before the item views is off by one. Worth fixing in `Geometry.h` before another unit
  calls item views through the vtable.
- `Material::SetLightingState`'s two matrix parameters are named the wrong way round in `Render3DCoreMaterials.h`;
  see above.
- The U4 cache singletons and `Add` bodies are original-owned at this point. `U5aRem.cpp` resolves them from
  `Render3DCore_7c_orig.dll` by name; when U4 makes the singletons/Add local, the lookup should become a direct call.
