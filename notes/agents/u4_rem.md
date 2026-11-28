# U4 remainder (wave 4): Render3DCore_7c caches

Status: 2026-11-24. **All 74 remaining U4 exports are local.** The U4 slice is 129/129 implemented; the module built
at 1804/2500 local with the other workstreams (the remaining forwards belong to other units). `diff_r3d_u4_rem`
passes with **1,102 checks, 0 mismatches**; the original `diff_r3d_u4` passes **2,604,392 checks, 0 mismatches**
(1,000 rounds); `diff_r3d_u5a` (62,000), `diff_r3d_u5a2` (22,634) and `diff_r3d_u5b` (417,172) also pass with 0
mismatches. No game run.

## Implemented exports (74)

- `AlphaBlendCache`: both constructors (the `bool` one is the compiler's vbase constructor), `~AlphaBlendCache`,
  vbase destructor, both vftables and the vbtable, `Add`, `End`, `Enter`, `Leave`, `RecalculateBucketParametrs`,
  `SetupPrimitiveSort`, `SortChunk`, `SortChunkIntoBuckets`, `CacheBucket::Insert`/`SplitTree`/`RotateTree`,
  `singleton`, `AlphaBlendCacheID`.
- `AlphaMaskCache`: constructor/destructor, vbase destructor, both vftables, vbtable, `End`, `singleton`,
  `AlphaMaskCacheID`.
- `UnsortedAlphaBlendCache`: constructor/destructor, vbase destructor, both vftables, vbtable, `End`,
  `SetTrainzDelayedRender`, `m_ceebtrainz_delayed_alpha_render`, `singleton`, `UnsortedAlphaBlendCacheID`.
- `MaterialChunkCache`: default and protected `(PString,float)` constructors, destructor, vbase destructor, both
  vftables, vbtable, `Add`, `FlushPipe`, `singleton`, `MaterialChunkCacheID`.
- `GenericMaterialChunkCache`: `(PString,float)` constructor, destructor, vbase destructor, both vftables,
  vbtable, default constructor closure, `End`, `GenericMaterialChunkCacheID`.
- `ParticleFXCache`: constructor/destructor, vbase destructor, both vftables, vbtable, `ParticleFXCacheID`.
- `ProjectorCache`: constructor/destructor, vbase destructor, both vftables, vbtable, `ProjectorCacheID`.

All 74 mangled names are in `implemented.d/u4_rem.txt`; a check against the built DLL reports 0 absent and 0
forwarded for that file. No U4 forwards remain.

`SortChunk`/`SortChunkIntoBuckets` are implemented and exported, but the differential test only exercises
`SetupPrimitiveSort` and the bucket-insertion path of `Add`; the two index-sorting paths need a real
`GeometryChunk` vertex/index buffer and were not driven here.

## Files

- `src/jet/include/jet/Render3DCoreCaches.h` - concrete declarations, lifetimes, statics, the missing virtuals and
  the corrected `MaterialChunkCache` field names (the pipe's material is at +0xc20; chunks/lights/model views are
  the +0x20/+0x420/+0x820 blocks).
- `src/jet/graphics/Render3DCore_7c/U4Lifetime.cpp` - seven constructors/destructors, the `PString` CacheIDs,
  singletons, delayed-render flag and the default constructor closure.
- `.../U4CacheEnds.cpp` - `AlphaBlendCache::Add`/`End`/`Enter`/`Leave`, `RecalculateBucketParametrs`,
  `SetupPrimitiveSort`, bucket-vector helpers, `AlphaMaskCache::End`, `UnsortedAlphaBlendCache::End`.
- `.../U4Sort.cpp` - `SortChunk`, `SortChunkIntoBuckets`, the bit-trick square root and the dynamic
  `GeometryChunk::primitiveLookup` lookup.
- `.../U4Tree.cpp` - class wrappers; `.../U4TreeAsm.cpp` - exact assembly transcriptions of `Insert`, `SplitTree`
  and `RotateTree`.
- `.../MaterialChunkCaches.cpp` - corrected `FlushPipe`, `MaterialChunkCache::Add`, `GenericMaterialChunkCache::Add`
  and `End`.
- `src/jet/tests/diff_r3d_u4_rem.cpp` - differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u4_rem.txt` - the 74 mangled exports.

## Facts that mattered

- The cache tree's `CacheBucket::head` is the **permanent root** (distance 0), not a header in front of a separate
  root; `z` is nil. Insert descending from `head` and keeps `head` in place, so C++ bottom-up RB insertion produced
  a different shape. The exact original instruction sequence (top-down recolour plus inline rotations and the
  final `SplitTree`) is kept in `U4TreeAsm.cpp`; that made both tree tests exact, including negative and NaN keys.
- `AlphaBlendCache::Add` passes the **unscaled** square root to `GetFreeCacheRecord`; only the bucket index uses
  `bucketMultiplyer * distance`.
- `AlphaBlendCache::End` calls the cache's own virtual `SetupDisplayState`/`CleanupDisplayState` (vtable 0x38 and
  0x3c), not the display's vtable. That explains the restored depth-write bit.
- `AlphaMaskCache::End` saves only the alpha-test reference (state +0xcc), restores it with function 5 and calls
  `Reset`. `UnsortedAlphaBlendCache::End` saves both the alpha-test function (+0xd0) and reference (+0xcc), restores
  both, then restores alpha-test/depth-write bits from state +0xa0.
- `MaterialChunkCache` pipe `i` uses block `i` of each 8-pointer list; the material itself is `pipeMaterials[i]`.
  `FlushPipe` is **protected** in the original (`MAA` mangling), so the declaration is protected too.
- The bucket vector starts at capacity **128** for 40 buckets (the original growth helper rounds to 128), not 40.
- `GenericMaterialChunkCache::End` uses `RenderNumChunks` when a material has more than one chunk and `RenderChunk`
  with the first elements otherwise. `MaterialChunkCache::Add` flushes a full pipe / the fullest of 32 pipes.
- The CacheIDs' original texts are `AlphaBlendCache`, `AlphaMaskCache`, `ChunkCache`, `GenericChunkCache`,
  `ParticleFXCache`, `ProjectorCache`, `UnsortedAlphaBlendCache`.
- `GeometryChunk::primitiveLookup` is RenderSystem data and the shared header does not mark it dllimport, so
  `U4Sort.cpp` resolves it from `RenderSystem_7c.dll` at run time instead of emitting a local unresolved data
  reference.
- `RefCount`/vtable convention: the destructor lives in the `for ReferenceCount` vtable (slot 1 is the scalar
  deleting destructor); the `for RenderingCache` vtable starts with `SetHost`. The test destroys objects through
  that slot.

## Build/test

```
cmake -S src/jet -B build32-r3d-u4 -A Win32
cmake --build build32-r3d-u4 --config Release --target Render3DCore_7c diff_r3d_u4 diff_r3d_u4_rem
```

`diff_r3d_u4_rem` is not in the shared `CMakeLists.txt` foreach. It was built through
`build32-r3d-u4/u4_rem_test.cmake` passed as `-DCMAKE_PROJECT_INCLUDE`; the main tree should add `u4_rem` to the
`diff_r3d_*` list (or an equivalent rule) later. Test data: `testdir-u4rem` (our DLL + `Render3DCore_7c_orig.dll` and
the Jet proxies).
