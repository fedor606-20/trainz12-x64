# U4 caches: Render3DCore_7c

Status: 2026-11-24. **56 of 129 exports are local and differentially verified** (0 mismatches, 1,000 rounds);
73 keep forwarding. The unit was built in `D:\universal modder\r3d-agents\u4` and the main tree was only read.

## Files

- `src/jet/include/jet/Render3DCoreCaches.h` — all seven classes, exact TS12 layouts, original vtable slot order.
- `src/jet/graphics/Render3DCore_7c/AlphaCaches.cpp` — `AlphaBlendCache` (except the tree rebalance), `CacheBucket`,
  `AlphaMaskCache`, `UnsortedAlphaBlendCache`.
- `src/jet/graphics/Render3DCore_7c/MaterialChunkCaches.cpp` — `MaterialChunkCache`,
  `GenericMaterialChunkCache`, `ParticleFXCache`, `ProjectorCache`.
- `src/jet/tests/diff_r3d_u4.cpp` — differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u4_caches.txt` — 56 mangled names.

## Implemented exports (56)

`AlphaBlendCache` (21):

- `GetMaxBuckets`, `SetBlendFunction`, `GetBlendFunction`, `Get/SetSortEverythingRange`,
  `Get/SetSortPolysInChunkRange`, `Get/SetMaxBucketRange`, `SetMaxBuckets`.
- `IsEmpty`, `Reset`, `Begin`, `SetupDisplayState`, `CleanupDisplayState`.
- `GetFreeCacheRecord`, `AddToFreeList`, `AllocateIndexArray`.
- `CompareSortFaces`, `GetSortFunction`, private `operator=` (the original's no-op returning `this`).

`AlphaBlendCache::CacheBucket` (7): constructor, destructor, `operator=`, `Reset`, `EndOfTree`, `GetNextRecord`,
`GetHead`.

`AlphaMaskCache` (4): `Add`, `Reset`, `Begin`, `IsEmpty`.

`UnsortedAlphaBlendCache` (4): `Add`, `Reset`, `Begin`, `IsEmpty`.

`MaterialChunkCache` (7): `Reset`, `Begin`, `End`, `FlushPipe`, `Enter`, `Leave`, `IsEmpty`.

`GenericMaterialChunkCache` (5): `Add`, `Reset`, `Begin`, `Enter`, `Leave`.

`ParticleFXCache` (4): `Add`, `Reset`, `Begin`, `End`.

`ProjectorCache` (4): `Add`, `Reset`, `Begin`, `End`.

## Forwarded exports and reasons (73)

- **Constructors/destructors that publish `singleton`**: both `AlphaBlendCache` ctors, `~AlphaBlendCache`, its
  `vbase destructor`, all `AlphaMaskCache`/`MaterialChunkCache`/`UnsortedAlphaBlendCache` lifetime exports. A local
  constructor would write a second `singleton` variable; the original's singleton stays authoritative.
- **All remaining constructors/destructors/`vf1`/default-ctor closure** of `GenericMaterialChunkCache`,
  `ParticleFXCache` and `ProjectorCache`: no singleton, but owning them means emitting the class vtable, and every
  slot (including RTTI and the inherited `RenderingCache` slots) would have to be local. They stay with the original
  so the object layout and vtable remain exactly the original's.
- **`AlphaBlendCache::Add`, `End`, `Enter`, `Leave`, `RecalculateBucketParametrs`, `SortChunk`,
  `SortChunkIntoBuckets`, `SetupPrimitiveSort`**: need Material's vtable contract (U5), `primitiveLookup` data and
  the bucket-vector helpers 0x100029d0/0x10002a60/0x10002b10 (resize/construct/free of the `CacheBucket` vector).
- **`AlphaBlendCache::CacheBucket::Insert`, `SplitTree`, `RotateTree`**: forwarded. The original's `Insert` has the
  rebalancer inlined and optimised (0x10001190), and the out-of-line `SplitTree`/`RotateTree` differ from a direct
  SDK-source port; the differential test caught topology differences on the third insert (`head` stays the dummy
  root; the original rotates, the port did not). The rest of the tree (`EndOfTree`, `GetNextRecord`, `GetHead`,
  `Reset`) is local and verified on trees built by the original `Insert`.
- **`AlphaMaskCache::End`, `UnsortedAlphaBlendCache::End`**: draw through Material's virtual slots and save/restore
  the viewport display state; U5 contract.
- **`MaterialChunkCache::Add`**: the 32-pipe/8-slot arrays and per-pipe flush path are decoded but not verified;
  left forwarded rather than merged untested.
- **`GenericMaterialChunkCache::End`**: calls the material's slot 0x40 with a 7-argument list.
- **Data exports** (`vftable`, `vbtable`, `singleton`, `*CacheID`): original-owned; the proxy does not export them
  from our DLL.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u4\src\jet" -B "D:\universal modder\r3d-agents\u4\build32" -A Win32
-DCMAKE_PROJECT_INCLUDE="D:/universal modder/r3d-agents/u4/cmake_u4_test.cmake"
cmake --build "D:\universal modder\r3d-agents\u4\build32" --config Release --target Render3DCore_7c diff_r3d_u4
copy D:\universal modder\testbin\* D:\universal modder\r3d-agents\u4\testdir
rename Render3DCore_7c.dll -> Render3DCore_7c_orig.dll, copy our DLL and the main build's
KernelStandard/Kernel/KernelResource/KernelResources/RenderSystem/Render2DCore proxies beside it
D:\universal modder\r3d-agents\u4\build32\Release\diff_r3d_u4.exe D:\universal modder\r3d-agents\u4\testdir 1000
```

Result: **2,604,392 checks, 0 mismatches**. Objects are created by each side's (forwarded) constructors, so both
sides carry the original's vtable; every implemented method is then called on identical input. Flat fields are
compared byte-wise, floats by bit pattern, and tree/ring/list pointers by index inside their own allocation.

The shared `CMakeLists.txt` `foreach(t u1b u1c u1d1 u1d2 u2 u3)` does not add a `u4` target. I did not edit it;
`cmake_u4_test.cmake` (in the workstream root, not in `src/jet`) adds the target through `CMAKE_PROJECT_INCLUDE`.
The main tree must add `u4` to that foreach (or an equivalent) for a normal build.

## Layout, NaN and ABI facts

- `AlphaBlendCache` 0x80, `ReferenceCount` vbase at +0x78; `CacheRecord` 0x34, `CacheBucket` 0x10.
  `AlphaMaskCache` 0x58, `UnsortedAlphaBlendCache` 0x64, `ProjectorCache` 0x2c, `ParticleFXCache` 0x34,
  `GenericMaterialChunkCache` 0x38 (entries 0x28), `MaterialChunkCache` 0xd2c.
- `RenderingCache` is 0x20 bytes before the virtual base: vftable +0, list +4/+8, vbptr +0xc, name +0x10,
  priority +0x14, host +0x18, bools +0x1c. Every cache's own fields start at +0x20.
- `AlphaBlendCache` fields: buckets vector `{count +0x20, capacity +0x24, data +0x28}`, `bucketIndex +0x2c`,
  `bucketStep +0x30` (40), `numBuckets +0x34` (40), `maxBucketRange +0x38` (200.0f), `bucketMultiplyer +0x3c`,
  `sortEverythingRange +0x40`, `sortPolysInChunkRange +0x44` (80.0f), ids +0x48/+0x4c, flags +0x50/+0x51,
  freelist +0x54, sortedFaces +0x58/+0x5c, blend factors +0x60 (4) / +0x64 (5), saved display factors +0x68/+0x6c,
  index pool +0x70/+0x74. `Reset` walks `numBuckets` elements at `(buckets + (bucketIndex + i) * 0x10)`; the
  destructor walks the constructed count at +0x20.
- Cache record free list chains through `right` (+0x28); a fresh record leaves `numIndices`/`numPrimitives`/
  `primitive` untouched exactly like the original.
- `CacheBucket::Reset` frees tree nodes back to the cache free list and rebuilds two sentinels; after freeing a node
  the walk continues at its parent (this was a hang in the first port).
- `MaterialChunkCache`: 32 pipes, chunk pointers +0xc20, counts +0xca4, three 8-pointer lists at +0x20/+0x420/+0x820.
  `FlushPipe` calls chunk vtable slot 0x3c for one entry (chunk, cache, display, modelView, extra) and slot 0x40 for
  many (chunk, cache, display, modelView list, extra list, light list, count, -1).
- `GenericMaterialChunkCache` groups by **material** (key at entry+0); per-entry lists are chunk, lights, modelView
  and grow in steps of 32 (the cache-level lists grow in steps of 128).
- `ParticleFXCache` pairs are `{Matrix4*, ParticleFXRenderItem*}`; `End` calls item vtable slot 0 as
  `(item, display, view, matrix)` after alpha blend (1,0) on and lighting/depth writes off.
- `ProjectorCache` is a ring of 0x40-byte blocks, 13 projectors each; `End` calls projector vtable slot 1 as
  `(projector, display, view)`.
- ABI: ordinary members are `__cdecl` (this is the first stack argument, `RET`), `IsEmpty` is `__thiscall`
  (`UBE`, ECX), constructors are `__thiscall` with the hidden virtual-base-construction flag (`RET 4`); the
  `GenericMaterialChunkCache` default-constructor closure has no hidden flag (`RET`).
- Virtual slot order was recovered from the original vtables and kept: after the eight `RenderingCache` slots,
  `AlphaBlendCache` has `IsEmpty` 8, `Add` 9, `SetSortEverythingRange` 10, `SetMaxBuckets` 11, `GetSortFunction` 12,
  `RecalculateBucketParametrs` 13, `SetupDisplayState` 14, `CleanupDisplayState` 15. `AlphaMaskCache` and
  `UnsortedAlphaBlendCache` add `IsEmpty`/`Add`; `MaterialChunkCache` adds `IsEmpty`/`FlushPipe`; `ParticleFXCache`
  adds `Add`; `GenericMaterialChunkCache` and `ProjectorCache` add none.
- `SetupDisplayState` saves the display's state-block (+0x54) source/destination factors at +0x68/+0x6c, then alpha
  test off, alpha blend on, depth write off; `CleanupDisplayState` restores the factors and the three modes in the
  original's order. The differential test uses a fake Display whose state block is compared byte-wise.
- NaN/-0/inf: scale and range arithmetic keeps the original SSE operand order; comparisons in the tree code follow
  `COMISS` + `JBE` (unordered takes the not-greater branch). The tree rebalance itself is forwarded, so its NaN
  behaviour is the original's.

## Blockers / follow-ups

- `CMakeLists.txt` needs `u4` added to the `foreach` that creates the `diff_r3d_*` targets.
- `AlphaBlendCache::Add`/`End`/`SortChunk*` need the U5 Material vtable (`Draw` slots 0x3c/0x44/0x40) and the
  `primitiveLookup` geometry table; after U5 they can be retried with this test recording the material calls.
- `CacheBucket::Insert`/`SplitTree`/`RotateTree` should be revisited together with the original's inlined
  rebalancer; the other tree methods and the bucket allocator are already local.
- `MaterialChunkCache::Add` and `GenericMaterialChunkCache::End` are decoded but were left forwarded.
