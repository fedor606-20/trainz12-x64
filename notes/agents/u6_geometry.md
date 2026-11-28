# U6 `u6_geometry`: geometry interface and chunk templates

Status: 2026-11-24. Done. The 13 exported functions (`Geometry`, `TriangleGeometry`) are local and differentially
verified; the two chunk templates and all 58 TS12 instantiations are reconstructed header-only and compared against the
original's instances by RVA (0 mismatches). The 58 exported names of the two template instantiations stay forwarded, as
the workstream rules require.

Workspace: `D:\universal modder\r3d-agents\u6\src\jet` (isolated copy; the main tree was only read). No shared file was
edited.

## Files (for the merge)

- `src/jet/include/jet/Render3DCoreGeometry.h` (new): `Jet::Geometry`, `Jet::TriangleGeometry`,
  `GeometryChunkTemplate<Vertex, Base>`, `GeometryChunkNmlTemplate<Vertex, Base>`, the block helpers and the alias
  catalogue of all 58 instantiations.
- `src/jet/graphics/Render3DCore_7c/Geometry.cpp` (new): the 13 exported members.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u6_geometry.txt` (new): the 13 mangled names.
- `src/jet/tests/diff_r3d_u6.cpp` (new): the differential test. It names its import libraries and the module load order
  itself (see "Test"), so the shared `CMakeLists.txt` needs no change; the main tree already lists `u6` in the
  workstream-test loop.

Not for the merge: `r3d-agents/u6/testbuild/` (scratch CMake project used only because this copy's `CMakeLists.txt`
predates the `u6` entry) and `r3d-agents/u6/testdir/`.

## Exports: 13 of 13 local

```
??0Geometry@Jet@@QAE@XZ                      ??0Geometry@Jet@@QAE@ABV01@@Z
??1Geometry@Jet@@UAE@XZ                      ??4Geometry@Jet@@QAEAAV01@ABV01@@Z
??0TriangleGeometry@Jet@@QAE@XZ              ??0TriangleGeometry@Jet@@QAE@ABV01@@Z
??1TriangleGeometry@Jet@@UAE@XZ              ??4TriangleGeometry@Jet@@QAEAAV01@ABV01@@Z
??_7Geometry@Jet@@6B@                        ??_7TriangleGeometry@Jet@@6B@
?StoreMatrix@Geometry@Jet@@QAAXABVViewPort@2@ABVMatrix4@2@@Z
?GetRenderMatrix@Geometry@Jet@@QBA?AVMatrix4@2@ABVViewPort@2@@Z
?InitInstance@Geometry@Jet@@IBAXAAV12@@Z
```

`check_exports.py` on the built DLL: `2500 exports, TSV 2500; missing/wrong 0, extra 0`.

## Template families: 58 instantiations, header-only

`GeometryChunkTemplate<Vertex, Base>` (40 instantiations in TS12) and `GeometryChunkNmlTemplate<Vertex, Base>`
(18), every one over `GeometryChunkSingleStream`:

- plain 40: `PC PCNT PCT PCTT PN PNT PNTA PNTT PNTTA PW1IC PW1ICT PW1ICTT PW1IN PW1INT PW1INTT PW2IC PW2ICT PW2ICTT
  PW2IN PW2INT PW2INTT PW3IC PW3ICT PW3ICTT PW3IN PW3INT PW3INTT PW4IC PW4ICT PW4ICTT PW4IN PW4INT PW4INTT PWb4IC
  PWb4ICNT PWb4ICT PWb4ICTT PWb4IN PWb4INT PWb4INTT` (prefix `PackedVertex`).
- Nml 18: `PC PCT PCTT PW1IC PW1ICT PW1ICTT PW2IC PW2ICT PW2ICTT PW3IC PW3ICT PW3ICTT PW4IC PW4ICT PW4ICTT PWb4IC
  PWb4ICT PWb4ICTT`.

Every member the original's range has (0x1000ca00..0x10076078) is reproduced: 5-argument and default constructors,
destructor, `CreateInstance()` / `CreateInstance(vertices, primitives, indices, faceNormals)`, `InitializeBuffers`,
`LoadBuffers`, `DuplicateVertexArray`, `DuplicateIndexArray`, `ResizeVertexArray`, both `GetVertexArray`s,
`AssignVertexType`, `GetVertexTypeID`, `GetVertexSize`, `GetItemOffset`, the ten item views (46..55) and the protected
`InitInstance`. The Nml family adds the normals block, its `InitInstance`/`DuplicateVertexArray`/`ResizeVertexArray`
overrides and the special `GetVector3Array` for item 4.

Only `GeometryChunkTemplate<PackedVertexPNTA, GeometryChunkSingleStream>` and `<PackedVertexPNTTA, ...>` have export
entries in the original (58 names: ctors, dtor, `AssignVertexType`, `CreateInstance`, array views, ...). These are
**not** in `implemented.d/u6_geometry.txt`; the `.def` keeps forwarding them. The multipass bump chunks
(`MultiPassBumpChunkT`/`T2`, U5b) derive from exactly those two classes and can now include the header.

`GenericGeometryChunk*`: Render3DCore instantiates no template on the generic chunk classes. The
`Jet::GenericGeometryChunkSingleStream/MultiStream` symbols in the u6 decomp are import thunks
(`jmp [RenderSystem_7c!...]`), not local code; materials build those classes directly through RenderSystem (U5a). The
header therefore leaves the `Base` parameter open (the ctor forwards `(vertices, primitives, indices, faceNormals,
flags)`), and no generic instantiation is added.

## Test

```
cmake -S src/jet -B build32 -A Win32
cmake --build build32 --config Release --target Render3DCore_7c
# scratch project, this copy only:
cmake -S testbuild -B testbuild/build -A Win32
cmake --build testbuild/build --config Release --target diff_r3d_u6
diff_r3d_u6.exe <testdir> 1000
```

Result: **293,480 checks, 0 mismatches (58 template classes)** on the clean build, 2026-11-24. Also re-ran the merged
`diff_r3d_u1`, `diff_r3d_u10` and `diff_r3d_u9b` from the main build against the same DLL: 0 mismatches each.

What the test does:

- the 13 exports through `GetProcAddress` by mangled name; object bytes from +4 (the matrix, past the vtable), return
  values (`operator=`, `GetRenderMatrix`) and side effects (target vptr in `InitInstance`) are compared raw, including
  awkward bit patterns (±0, ±inf, qNaN, sNaN);
- vtable lengths and the answer-0 stub slots (5 for `Geometry`, 17 for `TriangleGeometry`);
- our template objects are constructed directly from the header, the original's with the original
  `CreateInstance(unsigned,...)` reached by RVA from `u6_geometry_decomp.c` (one address per class, in the test). Both
  are queried through their own vtables with the same arguments: field block, block refs/count, vtable length,
  `GetVertexTypeID/Size/ItemOffset/ItemStream/NumStreams`, `GetVertexArray`, all five item views over all 39 item types
  (stride and data offset relative to the vertex/normal block), `AssignVertexType` with a fake `Display` + fake
  `VertexType`, `ResizeVertexArray` (grow and shrink, kept records), `DuplicateVertexArray`, and both `CreateInstance`
  paths (shared blocks, flags, counts).

Test-binary facts worth knowing:

- It includes the header, so it links `KernelStandard_7c.lib` and `RenderSystem_7c.lib` by `#pragma comment(lib,
  "D:/trainz12-decomp/implib/...")` (the shared CMakeLists gives workstream tests no link libraries).
- The proxy DLLs' `DllMain` answers `ERROR_DLL_INIT_FAILED` when `KernelStandard_7c.dll` (or any proxy) is the first
  module loaded directly in a bare console process; they initialize correctly when they arrive as dependencies of the
  3D module. The test therefore forces `__imp_??_7Geometry@Jet@@6B@` from `Render3DCore_7c_orig.lib` first, which puts
  `Render3DCore_7c_orig.dll` first in the import table. **testdir must be on `PATH`** (static imports load before
  `main`; `/DELAYLOAD` from a `#pragma comment(linker, ...)` is silently ignored by this linker, and the link options
  are not available in the shared CMakeLists).
- Not exercised: `InitializeBuffers`/`LoadBuffers` (need a Display and its vertex-buffer manager, plus a real index
  array); their bodies follow the original instruction for instruction
  (`0x3e2a0`/`0x5c500`: flags 0x10000/0x100, `AssignVertexType`, display slots 0x104/0x108, manager slots
  0x20/0x2c, then the static buffers). U5 should cover them when a real display exists.

## Layout and ABI facts (from the TS12 assembly)

- `Geometry`: 0x44 bytes, vftable +0, the stored `Matrix4` at +4. `TriangleGeometry`: no added fields, 0x44 bytes.
- `Geometry` vtable: six words at 0x7ea28: `??_E` dtor, four copies of the module's shared `xor eax,eax; ret` stub
  (0x100079e0) and a null sixth word. `TriangleGeometry` vtable (0x7ea44) has 17 executable words: the same dtor then
  sixteen stubs (its next word is the RTTI COL). The four/sixteen methods are not exported and their signatures are not
  recoverable; the header keeps the answer-0 behaviour with `int vfN()` placeholders. C++ cannot emit the null fifth
  word of `Geometry`, so our `Geometry` vtable has five executable slots (a scan sees 5 on both sides).
- `??1TriangleGeometry` is the same address as `??1Geometry` (linker fold) and writes `Geometry`'s vtable. Our
  destructor chain does the same: the compiler drops the `TriangleGeometry` vtable store because the base destructor
  overwrites it. The test compares the post-destructor vptr to the `Geometry` vtable.
- Matrix copy paths differ on purpose: the copy constructor is `REP MOVSD` (bitwise, an sNaN keeps its bits); the
  assignment, `StoreMatrix` and `InitInstance` go through the shared x87 helper 0x1000bf00 (sixteen
  `fld dword; fstp dword`), which **quiets a signalling NaN**. The test compares both, and the first version of
  `Geometry.cpp` (SSE copies everywhere) failed on exactly that.
- `StoreMatrix(const ViewPort&, const Matrix4&)` stores only when `ViewPort::viewportNumber` (+0x58, U8) is 0; otherwise
  it is a no-op. `GetRenderMatrix(const ViewPort&)` ignores the viewport. `InitInstance` copies the matrix only.
- Chunk templates: object 0x60 (Nml 0x64) over `GeometryChunkSingleStream`; the vertex block pointer at +0x5c (Nml
  normal block at +0x60). A block is `{refs, count, two words left as the heap held them, items at +0x10}`; only
  refs/count are written. Items are `sizeof(Vertex)` bytes.
- `CreateInstance()` allocates 0x60/0x64, default-constructs, `GeometryChunkSingleStream::InitInstance`, then shares the
  vertex block (Nml: both blocks through the shared helper 0x5f420; the plain template shares only +0x5c via its
  exported `InitInstance` 0x2d130).
- `CreateInstance(v,p,i,f)` passes `this->flags` (+0x18).
- `InitializeBuffers`: flags 0x10000 dynamic / 0x100 no-index; display slot 0x104/0x108; manager slot 0x20/0x2c.
  `LoadBuffers` uploads into existing static buffers. `DuplicateVertexArray` copies `maxVertices` records whatever the
  block held, then clears/rebuilds the static vertex buffer. `ResizeVertexArray` copies `min(maxVertices, n)` records,
  sets `maxVertices = numVertices = n`; the Nml override resizes the normal block first with `min(normalCount, n)`.
- Item views: `PackedVertexX::vertexItemOffset[type].offset` (imported data), stride 0 and null data for offset < 0,
  otherwise stride `sizeof(Vertex)` and `block + 0x10 + offset`. The Nml template answers item 4 (normal) from the
  normal block: stride 12, `normals + 0x10` (0x10 when the block is null, as in the original).
- Vertex type ids come from `PackedVertexX::GetVertexTypeID()` (exported by RenderSystem): e.g. PN 2, PC 3, PNT 18,
  PNTA 39, PNTTA 41, PWb4IC 9. `AssignVertexType` asks the display only when `+0x54` is empty, through the base
  `GeometryChunk::SetVertexType(display, id, 0)`, then returns `&GetVertexType(0)`.

## Blockers / open items

- None for the 13 exports. The two template instantiations' export names stay forwarded by design; a later unit that
  wants them local must add their names to `implemented.d` (they are already compiled wherever the header is used).
- U5b's `MultiPassBumpChunkT`/`T2` can now derive from `PackedVertexPNTAGeometryChunk`/`PackedVertexPNTTAGeometryChunk`;
  they add their own block after +0x60 (Nml: +0x64), which was not part of this unit.
- `InitializeBuffers`/`LoadBuffers` are untested (no display);
- The unknown `Geometry`/`TriangleGeometry` virtual signatures remain placeholders; the vtable shape and behaviour match,
  the method names/signatures cannot be recovered from the exports.
