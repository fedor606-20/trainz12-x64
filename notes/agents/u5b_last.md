# U5b last wave (u5b_last): the final 48 multipass exports

Status 2026-11-25: **all 48 remaining U5b exports now have local entry points; 0 forwarded from this slice.**
Worked in `D:\universal modder\trainz12-x64`, built in `build32-r3d-u5bl`.

```
check_exports.py build32-r3d-u5bl\Release\Render3DCore_7c.dll <tsv>   2500 exports, missing/wrong 0, extra 0
diff_r3d_u5b_last.exe testbin-r3d 1000                             160 checks, 0 mismatches
```

The exports are local in `implemented.d/u5b_last.txt` (48 lines). Three bodies still call the original module internally
for the parts that need the RenderSystem program-manager / vertex-type construction contracts (see "Known limits").

## Implemented here (48)

| Group | Count | Notes |
|---|---|---:|---|
| plugin `CreateInstance` | 6 | plugin virtual initialisation order then the family material constructor, `PluginInstance::Assign(material+4, plugin)` |
| plugin `InitializeCaches` | 1 | `AlphaBlendCache` (0x80) and `MaterialChunkCache` (0xd2c) through `RenderingCacheManager` at kernel+0x6c |
| plugin `InitializeLightRenders` | 3 | slot 0x24 first, two `MPLightRender`, then slots 0x28..0x3c; releases both on failure |
| plugin `Initialize*LightRenders` | 22 | per-type record creation, name selection and `AddVPProperty`; NoTexture passes literal names, the other families use the kernel name table |
| plugin `LoadLightPrograms` | 3 | empty name pair appends an empty pass; otherwise the original manager path |
| plugin `RegisterBuiltInPrograms` | 4 | built-in vertex/combiner texts replayed from the generated `U5bLastPrograms.cpp` |
| `MultiPassRenderState::StateSetup` | 1 | light classification/accumulation, clip tests, ambient colour, matrix assembly, scissor capture, stage array, blend state |
| `MultiPassRenderState::LoadRenderProperties` | 2 | all property types 0..0x18 for the vertex-program and texture-combiner constant setters |
| `MultiPassRenderState::SetScissor` | 1 | early exit, per-light sphere rects, Y flip, union then scissor intersection |
| `MultiPassRenderState::RenderSingleLightPasses` | 1 | six light-type groups, last render of each, one-item array per light |
| `MultiPassBumpChunkBase::CreateTangentVectors` | 1 | accumulation, vertex merge, LUT normalisation and the two crosses |
| `MultiPassOneTextureMaterial::RenderChunkPerVertex` | 1 | opacity clamp, texture stage, lighting state, per-vertex chunk path |

## Known limits (behaviour that still needs the original)

- `LoadLightPrograms` creates a vertex program and a texture combiner through the managers' `Create` when either name is
  non-null. That 10-argument protected `Create` path is delegated to the original module's `LoadLightPrograms`. The
  empty-name path is local and byte-compared against the original (it appends an empty pass).
- `InitializeFixedVertexType` (Bump, Bump2, BumpSpec2, OneTexture) delegates to the original class entry: it builds a
  `VertexType` with the family's name and `AddItem` tuples through RenderSystem's `VertexType` exports.
- `Initialize*LightRenders` does not yet append the per-type texture entries the original adds with `AddTexture` for the
  Bump/OneTexture/BumpSpec families (the vertex-property entries are matched). The light-render records and passes are
  otherwise identical.
- `CreateTangentVectors` matches the original in the flags==0 exit and the algorithm is transcribed; the triangle path
  was not differentially verified (the original reads its four item views through the chunk vtable in a slot order that
  the synthetic chunk in the test could not reproduce faithfully).

## ABI, layout and numeric facts

- **Plugin layout**: +0x20 `AlphaBlendCache*`, +0x24 `MaterialChunkCache*`, +0x28/+0x2c the two `MPLightRender*`,
  +0x30 `VertexType*`, +0x34..+0x6c fifteen program-name `char const*` (indices 0..4 vertex files, 5..14 combiners).
  `CreateInstance` reads the cache/light-render/vertex-type pointers from the `MaterialManager` at +0x20..+0x30.
- **`MPLightRender`**: six `Array` heads at +0x08/+0x14/+0x20/+0x2c/+0x38/+0x44. Its appender (0x31990) grows capacity
  to the next multiple of three. Each element is a 0x38-byte `MultiPassLightRender` whose five light counts are at
  +0x20..+0x30 and total at +0x34.
- **Per-type array offsets**: ambient +0x08, parallel +0x14, point +0x20, spot +0x2c, cube +0x38, cube-spot +0x44;
  the same render type always goes to plugin+0x28 and plugin+0x2c.
- **Name argument order is not uniform**: Bump/OneTexture pass (vertex file, combiner) as arguments 4/5; NoTexture's
  body pushes the combiner fourth and the vertex file fifth. `LoadLightPrograms` itself uses the fourth for the
  combiner and the fifth for the vertex program.
- **Kernel name fields**: ambient +0x34/+0x48, parallel +0x38/+0x50, point +0x3c/+0x54, spot +0x40/+0x58,
  cube +0x44/+0x68, cube-spot reuses spot (+0x40/+0x60), see `Initialize*`.
- **`StateSetup`**: sets +0x24, clears +0x26 when +0x25; light type 0 accumulates ambient, 1 parallel→+0x34,
  2 point→+0x30, 3→+0x38 unconditionally, 4 cube→+0x40 or +0x3c when the cube's +0x1a8 is zero. Clip result 2 (BACK)
  skips the insertion. The world→view matrix is `camera+0x1b8 * inverse(modelView)` (0x1001ab30 order); +0x84 is its
  transpose, +0xa8 is filled by Display slot 0x70, +0xe8 by `Display::GetScissor`, +0xfc is the stage array whose
  capacity grows to `(n+9)/10*10`.
- **`SetScissor`**: only for viewport +0x60 == 3; the index array selects lights by ordinal from the light list;
  each light's rect comes from `ViewPort::CalculateSphereSize` with the state's +0xa8 matrix; the Y flip uses
  `GetTargetSize().height`; the result is intersected with the state's saved scissor (not unioned).
- **`LoadRenderProperties`**: the property array items are `{unsigned type, int register}`; the scale is
  `light+0x24 * material+0x50`, half for types 0xc..0xf; cube/spot cases use light +0x180/+0x184; case 0x18 uploads a
  light texture to display slot 0x44 and tracks it in the state texture array.
- **`CreateTangentVectors`**: view order tangent(0x1a), normal(1), position(4), uv2(0xf); determinant is
  `d2.y*d1.x - d2.x*d1.y`; merge compares position and normal exactly; the LUT normaliser rounds once per Newton step;
  the final tangent is `normal x (normal x tangent)`.
- **`RenderChunkPerVertex`**: material+0x74 is clamped to 128 and restored; material+0xc4 texture slot 0x44 with 0;
  program = chunk slot 0xf0(0); opacity = material+0x80 * chunk+0x3c; camera +0x1b8 is the lighting model-view.

## Files

- new `src/jet/graphics/Render3DCore_7c/U5bLast.h`
- new `src/jet/graphics/Render3DCore_7c/U5bLastState.cpp` - state, per-vertex, tangent
- new `src/jet/graphics/Render3DCore_7c/U5bLastPlugins.cpp` - plugin bodies and helpers
- new `src/jet/graphics/Render3DCore_7c/U5bLastExports.cpp` - extern "C" entry points recorded in implemented.d
- new `src/jet/graphics/Render3DCore_7c/U5bLastPrograms.cpp` - generated built-in program texts
- new `src/jet/graphics/Render3DCore_7c/implemented.d/u5b_last.txt` - 48 exports
- new `src/jet/tests/diff_r3d_u5b_last.cpp`
- edited `U5bFinalState.cpp` - the five state entry points now call the local `Last` bodies

`U5bLastExports.cpp` also carries `/alternatename` aliases for two `DebugAccess` statics that the in-tree
`SimpleRemainScene.cpp` imports but the module's link set does not provide yet; remove them when that unit lands its
import set.

## Test

```
cmake -S src\jet -B build32-r3d-u5bl -G "Visual Studio 18 2026" -A Win32
cmake --build build32-r3d-u5bl --config Release --target Render3DCore_7c
cl /nologo /EHsc /GR /Zc:wchar_t- /Zp8 /MD /O2 /I src\jet\include src\jet\tests\diff_r3d_u5b_last.cpp ^
   /Fe:build32-r3d-u5bl\Release\diff_r3d_u5b_last.exe
copy build32-r3d-u5bl\Release\Render3DCore_7c.dll testbin-r3d\
diff_r3d_u5b_last.exe testbin-r3d 1000
```

The test calls local and original entries in one process: the flags==0 tangent exit, the twenty per-type plugin
initialisers (with the original's three `LoadLightPrograms` entry points detoured so both sides record the same
arguments; original results needed a detour because those bodies call the leaf directly), `InitializeLightRenders`
(vtable call order), the empty `LoadLightPrograms` record, and the `SetScissor` viewport-mode early exit.

Existing tests were re-run with this DLL from `D:\universal modder\testbin-r3d`: `diff_r3d_u4`, `u5b`, `u5b_rem`,
`u6`, `u7`, `u8`, `u9b`, `u9b2`, `u10`, `u11` all report 0 mismatches. `diff_r3d_u5a` reports its pre-existing
`GetType` static-PString mismatches with the previous DLL as well (also 1000/31000), so it is not a regression from
this slice.

## Merge notes

- `src/jet/CMakeLists.txt`'s test loop still does not list `u5b_last`; build the test by hand as above (the unit may
  not edit CMakeLists).
- The generated program texts are copies of the original module's private `.rdata` built-in program lists; they are
  registered through RenderSystem's static `RegisterBuiltInPrograms` exports, so no manager implementation is needed.

## Follow-up 2026-11-25: diff_r3d_u5b_final regression

Symptom: the merged `build32` DLL faulted at `Render3DCore_7c.dll+64df9` (c0000005) in `MultiPass::Last::StateSetup`
while `diff_r3d_u5b_final` constructed a `MultiPassRenderState`; wave 5 passed because its `StateSetup` body still
called the original.

Root cause: `StateSetup` called `Display::GetScissor` through a function pointer declared as
`Rect (__cdecl*)(void*)`. The exported entry is a cdecl member returning `Rect` by value and reads `this` at
`[esp+4]` and the hidden return buffer at `[esp+8]`; the free-function declaration made MSVC pass the hidden buffer
first and the display second. The callee therefore treated the display as the return buffer and wrote the rect
(0x0a in the fixture's patched `GetScissor` hook) over the display vtable. The next call through display slot 0x54
read `[display]` (0x0a) and faulted.

The same hidden-return ordering applies to `ViewPort::CalculateSphereSize` in `SetScissor`: it also takes the return
`Rect*` as its second stack argument after `this`. Both are now declared as `void(__cdecl*)(void*, Rect*, ...)` with
an explicit output rectangle.

Verification after the fix (`build32`, copied to `testbin-r3d`):

```
check_exports.py build32\Release\Render3DCore_7c.dll <tsv>   2500 exports, missing/wrong 0, extra 0
diff_r3d_u5b_final.exe . 1000                              127177 checks, 0 mismatches
diff_r3d_u5b_last.exe <fixture> 100                        160 checks, 0 mismatches
u4 130178, u5b 104285, u5b_rem 6666, u6 224130, u7 7500,
u8 4650, u9b 120, u9b2 2492, u10 51660, u11 18596         all 0 mismatches
```
