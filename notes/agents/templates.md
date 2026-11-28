# Wave 5: the remaining template/free-operator exports

Status: 2026-11-25. Done: all 108 exports are local. `check_exports.py` on the rebuilt DLL reports
`2500 exports, TSV 2500; missing/wrong 0, extra 0`, and none of the 108 is an export forwarder.

## Exports

| Family | Exports | Notes |
|---|---:|---|
| `GeometryChunkTemplate<PackedVertexPNTA, GeometryChunkSingleStream>` and `<PackedVertexPNTTA, ...>` | 56 | ctors (0/5), protected virtual dtor (`??1...MAE@XZ`), `CreateInstance` (0/4), `AssignVertexType`, `DuplicateIndexArray`, `DuplicateVertexArray`, `ResizeVertexArray`, `GetVertexArray`, `GetVertexTypeID/Size/ItemOffset`, both item views of Vector2/3/4, float and udword, `InitInstance`, `InitializeBuffers`, `LoadBuffers`, both vtables |
| `ITree<Particle>` | 37 | ctor, dtor, `operator=`, `Add`, `Push`, `Pop`, `InsertBefore/After`, the three `Remove`s, `RemoveAll`, `RemoveAllChildren`, `DeleteAll`, `SetParent`, `First/Last/End/Next/Prev/Parent` (both constnesses), `Index` (both), `Count`, `CountAll`, `Exists` (both), `Traverse` (both), `IRemove`, `IDelete` |
| `StdMaterialPluginTemplate<AutoTexMaterial>` | 7 | three ctors, dtor, `operator=`, `CreateInstance`, vtable |
| Line free operators | 6 | `Line+Vector3`, `Line-Vector3`, `Matrix3*Line`, `Line*float`, `float*Line`, `Line/float` |
| `DefaultInterpolator<Quaternion>` | 2 | `operator=`, `Interpolate` |

The vtable-for-base entries of `MultiPassBumpChunkT`/`T2` (`ordinals 659`/`661`) stay forwarded: they belong to
U5b.

## Files

- `src/jet/graphics/Render3DCore_7c/Templates.cpp` (new): the out-of-line `ITree` members, the
  `StdMaterialPluginTemplate` members, `DefaultInterpolator<Quaternion>::Interpolate`, the six Line operators, and
  the explicit instantiations of the four exported template instantiations.
- `src/jet/include/jet/Templates.h` (new): `DefaultInterpolator<T>` and the Line operator declarations.
- `src/jet/tests/diff_r3d_templates.cpp` (new): differential test, ours vs `Render3DCore_7c_orig.dll` in one
  process.
- `src/jet/graphics/Render3DCore_7c/implemented.d/templates.txt` (new): the 108 mangled names.

## Shared-header changes (no data layout change)

- `include/jet/Render3DCoreParticle.h`: `ITree<T>`'s destructor is now declared out of line, and the class declares
  `operator=`, `Add/Push/InsertBefore/InsertAfter`, the three `Remove`s, `Pop`, `RemoveAll`, `RemoveAllChildren`,
  `DeleteAll`, `SetParent`, `Index` (+ const), `CountAll`, `Exists` (+ const), `Traverse` (+ const) and the private
  `IRemove`/`IDelete`. The six data members and every offset are unchanged. `Particle.cpp` already called
  `ITree<Particle>::DeleteAll()`; that call now resolves to this unit.
- `include/jet/Render3DCoreMaterials.h`: `StdMaterialPluginTemplate<T>` gained its three constructors,
  assignment, a local `CreatePluginResources` override and the three `RenderingCache*` fields at
  `+0x20/+0x24/+0x28` (`sizeof == 0x2c`, asserted). `StdMaterialPlugin` gained the protected
  `(const PString&, const UId&)` constructor (the original's `0x71120`) and its `CreatePluginResources` is protected
  (`...MBAX`, the original's mangling). `AutoTexMaterial` gained the five-argument constructor (imported from the
  original) and its `CreateInstance()` override, without which the class is abstract in this header. No data member
  of `StdMaterialPlugin` or `AutoTexMaterial` changed.
- `include/jet/Render3DCoreGeometry.h`: `~GeometryChunkTemplate` moved from `public` to `protected`. The original
  exports it as `??1...MAE@XZ`; access is part of the mangled name. Only the access changed.

## Test

```
cmake -S build32-r3d-tpl/shadow/jet -B build32-r3d-tpl/shadow-build -A Win32
cmake --build build32-r3d-tpl/shadow-build --config Release --target Render3DCore_7c
# the shared CMakeLists' workstream loop has no `templates` entry (this unit may not edit it);
# the test was built with build32-r3d-tpl/testproj/CMakeLists.txt pointing at the shadow include/tests
diff_r3d_templates.exe <testdir> 4096
```

Result: **30,852 checks, 0 mismatches** (4096 rounds; 9348/0 at 1024). Also re-ran existing workstream
tests against the rebuilt DLL:
`diff_r3d_u6` 227,780/0, `diff_r3d_u9a` 3,080/0, `diff_r3d_u5a2` 11,334/0, `diff_r3d_u1d1` 17,137/0
(386 pre-existing NaN-payload-only notes), `diff_r3d_u1` 2,900/0.

What the test covers:

- Line: 1024 random lines/vectors/matrices with ±0, ±inf, qNaN, sNaN and random payloads; all six operators,
  raw direction+point bytes compared.
- `DefaultInterpolator`: `operator=` identity and 1024 Slerp results, raw 16 bytes.
- `ITree`: every export called on fake `Particle`-layout nodes with a fake vtable (deleting destructor,
  `DeleteAll`, `RemoveAllChildren`, `Add`, `Remove` recorded). Returned node pointers are normalised to indices;
  tree links, counts and the virtual-call transcript are compared.
- `GeometryChunkTemplate`: for both exported instantiations, the exported default ctor and 5-argument ctor,
  `CreateInstance()` (shares the source block), `CreateInstance(v,p,i,f)`, `InitInstance`, the exported virtuals
  through the vtable (73 slots, `GetVertexTypeID/Size/ItemOffset/ItemStream/NumStreams/GetVertexArray`, the ten item
  views over all 39 item types), and the exported dtor/deleting dtor.
- `StdMaterialPluginTemplate<AutoTexMaterial>`: vtable length 7, slot-4 `IsSupported`, default/named/copy ctors,
  assignment, dtor and the three cache slots. `CreateInstance` is exported and local, but its body needs a real
  `MaterialManager`/`RenderKernel`; both our copy and the original's crash with the stand-in environment, so its
  behaviour is not compared here (the factory path itself is exercised by U5a2).

## ABI / behaviour facts worth keeping

- `Particle`'s `ITree<Particle>` subobject is at `+0x0c`; the six links of a node are `count +0x00, parent +0x04,
  head +0x08, tail +0x0c, next +0x10, prev +0x14` relative to that subobject (so `Particle+0x0c..+0x20`).
  `Add` appends at the tail, `Push` at the head, `Pop` takes the head, `Remove()` the tail.
- `ITree<T>::Iterator`/`ConstIterator` are non-POD (user-provided constructor). Every function returning one uses
  the hidden result pointer (`ret 8`), not EAX. The test calls them with an explicit `It*` result argument.
- `~ITree` and `DeleteAll` are the same body in the original; both call the child's `DeleteAll` (slot 29), unlink
  it and free it. `RemoveAllChildren` calls slot 28 and does not delete.
- `SetParent` does not write `_parent`; it calls the old parent's virtual `Remove` (slot 24) and the new parent's
  virtual `Add` (slot 18). `IDelete` calls the node's `DeleteAll`, `IRemove`, then the deleting destructor.
- `StdMaterialPluginTemplate` vtable slots: 0 `AddReference`, 1 `ReleaseReference`, 2 deleting destructor,
  3 `MaterialPlugin::CreateInstance()`, 4 `IsSupported` (true), 5 `CreateInstance(manager,kernel)`,
  6 `CreatePluginResources`. The original's dtor `0xd7a0` is a jump to `StdMaterialPlugin::~StdMaterialPlugin`,
  so the template destructor has no body of its own; the base releases `+0x20/+0x24/+0x28`.
- `CreateInstance` calls slot 6 first, allocates `sizeof(AutoTexMaterial) == 0xc4`, constructs it with
  `(manager, +0x20 chunk cache, +0x28 alpha mask cache, +0x24 alpha blend cache)` and then `PluginInstance::Assign`
  (the hidden virtual-base flag is 1). `CreatePluginResources` finds/creates `MaterialChunkCache` (0xd2c),
  `AlphaBlendCache` (0x80) and `AlphaMaskCache` (0x58) through the `RenderingCacheManager` at `RenderKernel+0x6c`.
- Line operators copy the direction through x87 (`fld/fstp`), which quiets a signalling NaN; the point arithmetic
  is SSE with the original operand order. `operator*(Matrix3, Line)` direction.x starts with `dx*m0` (not `m0*dx`);
  `operator/` forms the reciprocal once and multiplies. Using `SolidsDetail::{Add,Sub,Mul,Div}SS` gives the exact
  NaN payloads.
- `DefaultInterpolator<Quaternion>::operator=` is a no-op returning `this`; `Interpolate` calls
  `Quaternion::Slerp(a,b,t)` on a local and returns its four dwords.

## Build notes / blocker

The main tree was being edited by several units while this work ran. A full `Render3DCore_7c` CMake build currently
stops in another unit's header (`JET_LAYOUT32(sizeof(ReflectiveMaterial) == 0x108)` at
`Render3DCoreMaterials.h:502`), and earlier attempts hit the in-progress `U5aFinal.cpp`, `U5bFinal*`, `U10Live*`
and `Tails2*` files. The DLL tested here was therefore linked from the last complete `build32` object set (21:29)
plus freshly compiled `Templates.obj` and `Particle.obj` (the old `Particle.obj` still contained the old inline
empty `~ITree`, which would win COMDAT folding), with a merged implemented list = old list + `templates.txt`.
`Templates.cpp` also compiles cleanly against the current main headers with only that unrelated
`ReflectiveMaterial` assert disabled, and the resulting object defines all 108 names. Re-run the normal
`build32-r3d-tpl` build once the other units have merged.
