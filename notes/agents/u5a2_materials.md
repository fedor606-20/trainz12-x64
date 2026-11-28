# U5a2: materials, second pass

Status: **105 new exports local, 196 of 374 in the slice (91 from U5a + 105 here)**, differential test
**113,034 checks, 0 mismatches** over 1,000 rounds against `Render3DCore_7c_orig.dll`.

Worked in `D:\universal modder\r3d-agents\u5a2`; the main tree was only read. No shared file was edited.

## Files

- `src/jet/graphics/Render3DCore_7c/MaterialLifetime.cpp` - the ten material constructors.
- `src/jet/graphics/Render3DCore_7c/MaterialPlugins.cpp` - the plugin constructors and assignments.
- `src/jet/graphics/Render3DCore_7c/MaterialOps.cpp` - `operator==`, material factories, plugin `IsSupported`,
  the one-texture `SetTexture`/`DuplicateTexture` paths, the GenericShader ring leaves and three Reflective leaves.
- `src/jet/tests/diff_r3d_u5a2.cpp` - differential test (stand-in Display, textures, manager/kernel, patched imports).
- `src/jet/graphics/Render3DCore_7c/implemented.d/u5a2_materials.txt` - **105** names, all newly local.

## Newly local (105)

| Group | Count | What |
|---|---:|---|
| `operator==` (RTTI) | 10 | all ten materials, including the exact texture/alpha/reflectivity checks |
| `CreateInstance` | 10 | `Material::CreateInstance(UNDECORATED_TYPE, manager)` plus the class `InitInstance` and the texture copy where the original does it |
| plugin `IsSupported` | 8 | 5 shared `mov al,1` bodies, plus AutoTex/Gloss/Reflective display format queries and fallback strings |
| material constructors | 10 | all `MaterialManager*/cache(s)/context + hidden virtual-base flag` entries |
| plugin constructors | 30 | 9 default, 10 `(name, id)`, 11 copy |
| plugin `operator=` | 11 | 2/3/4/5/15-field variants, Gloss/MVH share Reflective's 15-field body |
| GenericShader leaves | 8 | `GetTexture` (both), `SetResourcePriority`, `IsLoaded`, `Activate`, `Bind`, `Unbind`, `DuplicateTexture`, `Equals` |
| one-texture `SetTexture`/`DuplicateTexture` | 13 | OneTexture, OneTextureNoAlpha, Reflective, Gloss; both `SetTexture` overloads where exported |
| Reflective leaves | 3 | `GetNumPasses`, `ChangeTexGen`, `DuplicateTexture` |
| GenericShader display state | 2 | `SetDisplayState`/`ClearDisplayState` (ring bind, cull/blend save) |

### ABI notes that made it possible

- Every virtual and ordinary export in this set is `__cdecl` with `this` first on the stack. The bodies are plain
  namespace functions; `implemented.d` aliases each export to one.
- Constructors and assignments are `__thiscall`; their bodies are members of the local thunk objects
  `Jet::U5a2::MaterialThunks` / `PluginThunks`, so `ret N` and `ret N+4` (hidden virtual-base flag last) match.
- A material constructor writes the derived vbtable and the `ReferenceCount` value itself, then calls
  `RenderSystem_7c`'s `Material::Material(manager, false)`, then installs the class's **original** vtables/vbtable
  from `Render3DCore_7c_orig.dll` and its fields. A constructed object is byte-identical to an original one and
  still dispatches through the original tables; no vtable/vbtable data export is claimed.
- Plugin constructors call `RenderSystem_7c`'s `MaterialPlugin` base constructors/assignment and install the original
  derived vtable. Field ranges: NoTexture 4, OneTexture/OneTextureNoAlpha 5, AutoTex 3, ColorOne 5, GenericShader 2,
  Reflective/Gloss/MVH 6 + the dword at `+0x58`, Std 3, MaterialPluginX none.
- `Material::ReferenceCount` value sits at `vbtable[1] + 8` (the ctor's `[this+0xa8] = 1` for NoTexture).
- `operator==` compares two texture references as `(a - b) / 0x34 == 0`, **not** `/0x68`. This is a bug in U5a's
  local `OneTextureMaterial::Equals`/`OneTextureNoAlphaMaterial::Equals` (they use `0x68`); the new operator==
  bodies compare with `0x34` and do not call those methods.
- Original RTTI quirk found by the test: `__RTDynamicCast` from `Material` to `OneTextureMaterial` fails for a
  `OneTextureNoAlphaMaterial` object (but succeeds for the other one-texture derived classes and for Reflective <->
  Gloss). `eq_OneTexture` reproduces that with a `typeid` name check before the field comparison.
- GenericShader texture ring: circular blocks `{next, prev, count, items[8]}` with items `{int type, Texture*}`.
  `Equals` compares the ring head identity (plus `alphaHint`) and then `Material::Equals`.
- Texture virtual slots used: 2 duplicate/create, 3 `IsAvailable`, 9 `GetAlphaHint`, 11 `GetDimension`,
  15 `Activate`, 17 `Bind`, 18 `SetResourcePriority`.
- `SetTexture`: type 1 acts on the primary texture, type 6 returns success without a texture, everything else code 2.
  Missing texture is code 3. The ref overload uses `texture+0x10` as the definition name; the database overload uses
  the requested name. `OneTextureNoAlpha` writes `alphaHint = 0` (not `-1`) and does it only on success.
- Plugin `IsSupported` uses Display vtable slot 7 (`+0x1c`) and stores `NoTextureMaterial`/`OneTextureMaterial`/
  `OneTextureNoAlphaMaterial::UNDECORATED_TYPE` in the reason on failure.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u5a2\src\jet" -B "D:\universal modder\r3d-agents\u5a2\build32" -A Win32
  -DCMAKE_PROJECT_INCLUDE="D:/universal modder/r3d-agents/u5a2/cmake_u5a2_test.cmake"
cmake --build "D:\universal modder\r3d-agents\u5a2\build32" --config Release --target Render3DCore_7c diff_r3d_u5a2
cd testdir; ..\build32\Release\diff_r3d_u5a2.exe . 1000
```

Fixture: `testbin\*`, `Render3DCore_7c.dll` renamed `Render3DCore_7c_orig.dll`, our DLL, and the main build's
`Kernel*`, `RenderSystem_7c`, `Render2DCore_7c` proxies. Result: **113,034 checks, 0 mismatches**.

The shared `CMakeLists.txt` does not create `diff_r3d_u5a2` in the isolated copy (the main tree's newer copy already
lists `u5a2`). The workstream-local `cmake_u5a2_test.cmake` adds the target through `CMAKE_PROJECT_INCLUDE`.

The test patches three IAT slots in both modules and records the call arguments: `SharedTextureCollection::GetTexture`,
`Material::SetTextureDefinition` and `Material::CreateInstance`. Material constructors are compared as whole objects
(the same original vtables are installed by both sides, so bytes must match). `CreateInstance` is run only for
`NoTextureMaterial`; the other nine run `Material::InitInstance` on the returned object, which needs a real manager,
so their factory call shape is shared with the NoTexture body and is not exercised with the stand-in manager. The
plugin lifetime is exercised once (the base constructor registers in the global plugin list; repeating it every round
perturbs the process).

## Remaining forwarded (178) and why

- **Data exports (71)**: `vftable`, `vbtable`, `DECORATED_TYPE`, `UNDECORATED_TYPE`, `UID`. Original-owned by the
  workstream rules; `GetType` and the constructors use the original addresses, so identity and text stay exact.
- **Destructors and `vbase destructor` (41)**: the exports take the `ReferenceCount` sub-object pointer (except the
  plugin destructors, which take the plugin base) and chain through `RenderSystem_7c`'s `Material`/`MaterialPlugin`
  destructors. Objects are still destroyed by the original virtual tables this unit installs, so leaving the entries
  forwarded keeps destruction byte- and call-identical. Not exercised by the differential test.
- **Geometry/render (41)**: `CreateGeometryChunk`, `CacheChunk`, `RenderChunk`, `RenderNumChunks`,
  `RenderPrimitives`, `ReflectiveMaterial::SetSphericalTexureCoordinates`. These need U6's geometry-chunk templates
  (chunk object allocation, vertex/index streams) and the lighting/vertex-program path; U6 is being reconstructed in
  parallel.
- **Plugin factories (15)**: `*MaterialPlugin::CreateInstance(manager, kernel)` and `CreatePluginResources(kernel)`.
  They construct the U4 caches through the kernel's `RenderingCacheManager`, then construct a material; the cache
  construction path is the U4 unit's and the test would need its cache-manager fixture.
- **MultiPass material exports inside this slice (11)**: `MultiPassBumpMaterial`/`MultiPassOneTextureMaterial`/
  `MultiPassNoTextureMaterial` `SetTexture`, `IsLoaded`, `DuplicateTexture`. The class definitions belong to U5b
  (`Render3DCoreMultiPass.h`); this unit must not copy the other stream's implementation.
- **State/other (9)**:
  - `ReflectiveMaterial::Bind` and `GlossMaterial::Bind`: the 0x59670 body builds the cube-map tex-gen and lighting
    state; it needs the render-state follow-up and the light/vertex-program contract.
  - `AutoTexMaterial::SetDisplayState` and `ReflectiveMaterial::SetDisplayState`: the texture-coordinate matrix and
    cube-map tex-gen setup (Display slot 0x74 and the 0xbc matrix), not verified yet.
  - `GenericShaderMaterial::InitInstance`, `ReflectiveMaterial::InitInstance`, `GlossMaterial::InitInstance`: the
    factory bodies call these exported methods; they are still the original's (the `CreateInstance` bodies call the
    exported entry, which forwards), so the factory remains correct. Porting them needs the GenericShader ring
    append helper 0x100275c0 and Reflective's cube/matrix copy.
  - `GenericShaderMaterial::SetTexture` (both overloads) also uses that ring append helper.

## Cross-unit findings

- U5a's local `OneTextureMaterial::Equals` and `OneTextureNoAlphaMaterial::Equals` compare texture references with
  `(a-b)/0x68`, while the original `Equals` at 0xc2b0 and every `operator==` use `/0x34`. The U5a test did not
  distinguish them (its texture pointers were equal or farther apart). The new operator== avoids those methods.
- The original RTTI for `OneTextureNoAlphaMaterial` does not answer a cast to `OneTextureMaterial` (the other
  one-texture classes do); this is why its base-class `operator==` returns false for it.
