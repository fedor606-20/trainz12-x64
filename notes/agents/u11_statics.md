# u11_statics: BackgroundResource, BackgroundLoadingThread, Facility, plugin entry

Status: 2026-11-24, done for the two background classes. 49 of the 167 exports are local and differentially verified;
118 still forward. `Facility` and `JetGetPluginsRender3DCore` stay the original's, and 109 template/free-function
exports that no other slice owns are left forwarded (see "Forwarded" below).

Isolated tree: `D:\universal modder\r3d-agents\u11\src\jet` (a copy; the main tree was not edited except for this
report). Files added:

- `include/jet/Render3DCoreBackground.h`
- `graphics/Render3DCore_7c/Background.cpp`
- `graphics/Render3DCore_7c/BackgroundThread.cpp`
- `graphics/Render3DCore_7c/implemented.d/u11_statics.txt` (49 names)
- `tests/diff_r3d_u11.cpp`

`src/jet/CMakeLists.txt` was only synced to the main tree's copy (it already had `u11` in the test loop; the copy was
one line behind). Nothing else was touched.

## Implemented (49)

`BackgroundResource` (36 of 40: 34 functions + the vtable + `mainThreadId`):

```
??0 ??1 ??_7 ??_F  Abort  ActionAbortNow  ActionBackgroundLoadData  ActionLoadComplete
ActionMainThreadInitialize  ActionNotifyFailure  ActionProcessFailure  ActionQueueBackground
ActionQueueRequest  ActionSendLoadComplete  ActionUnloadResource  AddObserver  BackgroundLoadData
CallCallbacks  CheckComplete  DeleteSelf  DoAction  GetCurrentState  GetResource  LoadFailed
LoadResource(Resource*)  LoadResource(ResourceDatabase*, PString)  Lock  MainThreadLoadComplete
MainThreadProcessFailure  OnLoad  QueueBackground  RemoveObserver  SetBackgroundThread  UnloadResource  Unlock
?mainThreadId
```

`BackgroundLoadingThread` (13 of 14): ctor, dtor, vtable, CheckForComplete, DequeueResource, GetThreadID, MainThread,
ProcessThread, QueueForDelete, QueueResource, SetThreadPriority, SignalComplete, `?mainThreadId`.

Both `mainThreadId` statics moved because their only writer (the constructor) and reader (`ActionProcessFailure`) are
local now. `BackgroundLoadingThread::results` is still the original's: the `ResultClass` template only needs the
symbol's address, never its contents.

## Forwarded (118), with reasons

- `Facility` (4: ctor, copy ctor, assignment, vtable). TS12's `Facility` vtable has at least 15 slots (0x1007d420);
  the RC1 SDK header only documents `GetName` and the 15 slot bodies are in other modules. A wrong vtable shape would
  break every concrete facility, so the class and its vtable stay the original's.
- `JetGetPluginsRender3DCore` (1). 0x100608e0 constructs 15 concrete plugins through the helpers at
  0x10060050..0x10060770 (material/geometry-chunk plugin classes from U5a/U5b/U6) plus a 0x34-byte `PluginList`
  and the static UIds at 0xdca40..0xdca74. None of those classes or statics are local in this unit.
- `BackgroundResource` debug tables (4): `actionNames`, `fsm`, `inputNames`, `stateNames`. The exported symbols are
  null pointers in the original (no code references them; the FSM is read directly at 0x1007cbc0) and they are
  debug-only, so they keep forwarding.
- The 109 `<global>` exports, which are template/free-function instantiations, not singletons:
  - `GeometryChunkTemplate<PackedVertexPNTA|PNTTA, GeometryChunkSingleStream>` (56): U6/U5b's template; the geometry
    chunk contract is not reconstructed in this slice.
  - `ITree<Particle>` (37): U9a's template in `Render3DCoreParticle.h`; no local translation unit currently emits its
    out-of-line members, and forcing another unit's template instantiation here would be a cross-unit copy.
  - `StdMaterialPluginTemplate<AutoTexMaterial>` (7): U5a's material plugin template, no local header.
  - `Line` free operators (6, `??D/??G/??H/??KJet`): U1d1's `Line`; `Line.cpp` implements the member operators only.
  - `DefaultInterpolator<Quaternion>` (2): no local Quaternion/DefaultInterpolator definition (animation/Foundation).
  - None of these 109 names appears in another stream's TSV (checked all 16 slices), so they were orphaned to this
    slice; they are still not safely movable without the owning units.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u11\src\jet" -B "D:\universal modder\r3d-agents\u11\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u11\build32" --config Release --target Render3DCore_7c diff_r3d_u11
diff_r3d_u11.exe <testdir> 1000
```

Test directory: `testbin/*` with `Render3DCore_7c.dll` renamed to `Render3DCore_7c_orig.dll`, our DLL beside it, plus
the main build's `KernelStandard_7c.dll`, `Kernel_7c.dll`, `KernelResource_7c.dll`, `KernelResources_7c.dll` and
`RenderSystem_7c.dll` (testbin has only the originals of the first three). `SetDllDirectory` + PATH point at the
directory.

Result: **600443 checks, 0 mismatches over 1000 rounds** (Win32 Release). The test builds fake
`BackgroundResource`, `Resource`, `BackgroundLoadingThread`, `BkgResObserver` and `ResourceDatabase` objects whose
virtual calls land in the test, and compares every visible field, return value and virtual call (with arguments) of:

- both constructors and the default constructor closure, `GetCurrentState`, `GetResource`, `SetBackgroundThread`;
- the full 10x9 `DoAction` matrix under four configurations (healthy; failing hooks + full queue + delete flags +
  locked; background retry; resource not available), including the exact lock/unlock placement of actions 7 and 11;
- `CheckComplete`, `LoadFailed`, `BackgroundLoadData`, `QueueBackground`, `UnloadResource`, `Abort(false/true)` and
  `DeleteSelf`;
- `AddObserver` growth (by fives), `RemoveObserver`, and `CallCallbacks` for every flag mask with both abort
  polarities;
- `LoadResource(Resource*)`, `LoadResource(ResourceDatabase*, PString)` (miss and hit), and both overloads called
  through vtable slots 3/4 to prove the slot order;
- vtable slots 0, 2, 5, 8, 9 against the exported symbols and slots 2..6 of the thread;
- `BackgroundLoadingThread`: ctor fields, both `mainThreadId`s, `GetThreadID`, `SetThreadPriority` (read back through
  KernelStandard's `Thread::GetPriority`), `DequeueResource` (not queued / is current), `QueueResource` full branch,
  `SignalComplete`'s priority heap with five distinct resources (root, count), `QueueForDelete` across two 13-entry
  blocks and `CheckForComplete` (delete list + complete queue, state 8 to 9).

The asynchronous path (`QueueResource` success -> `ProcessThread` -> `BackgroundResource::DoAction(LoadData)` ->
`SignalComplete`) is only exercised structurally: the worker is created and destroyed, and the queues are driven
without waking it. It is deterministic-by-construction but not covered end-to-end; a later unit that cuts these
classes over in the game should add a timed round.

## ABI and lifecycle facts

- `BackgroundResource` is 0x2c bytes: `ResourceObserver` vtable +0x00 (slot 0 `OnLoad`, slot 1 deleting dtor), +0x04
  thread, +0x08 `Resource*`, +0x0c observer count, +0x10 capacity, +0x14 entries, +0x18 locked, +0x19 delete-on-abort,
  +0x1a delete-on-failure, +0x1c state, +0x20 `CriticalSection` (0xc). Vtable: 2 `DeleteSelf`, 3
  `LoadResource(ResourceDatabase*, PString)`, 4 `LoadResource(Resource*)`, 5 `Abort`, 6/7 hooks returning 0, 8/9
  `MainThreadLoadComplete`/`MainThreadProcessFailure` returning 1. MSVC puts overloaded virtuals in **reverse
  declaration order**: declare `LoadResource(Resource*)` first in the class, or slots 3/4 swap.
- The observer array entries are `{unsigned long flags; BkgResObserver* observer;}` — flags first, observer second
  (0x1000eee0). `CallCallbacks` ignores its `unsigned long` argument and passes **`this`** to every callback
  (0x1000efe8 pushes `this`, not the phase); flag 1 aborts on false, flags 2/4/8 abort on true.
- The FSM is 10 states x 9 inputs, 16-byte entries at 0x1007cbc0: `{State next; Action action; int 0; int index}`.
  `DoAction` stores the next state *before* running the action; actions 7 (background load) and 11 (abort) run with
  the CriticalSection released, the others under it. Action 1 (`NoAction`) returns 0, action 0/unknown returns 1.
- `BackgroundLoadingThread` is 0x44 bytes: `ReferenceCount` base (vtable +0x00, refcount +0x04), +0x08 queue count,
  +0x0c capacity, +0x10 queue, +0x14 complete count, +0x18 capacity, +0x1c complete, +0x20 delete-list head, +0x24
  `Thread*`, +0x28 current resource, +0x2c `Semaphore(0, 0x7fffffff)`, +0x34 done, +0x38 thread id, +0x3c `Mutex`.
  Vtable: 0 `ReferenceCount::DeleteThis`, 1 deleting dtor, 2 `QueueResource`, 3 `DequeueResource`,
  4 `SignalComplete`, 5 `CheckForComplete`, 6 `QueueForDelete`.
- Both resource queues are binary max-heaps keyed by `Resource::GetPriority()`. The comparisons are the original's
  `COMISS`/`JBE`/`JA`, so a NaN priority never sifts up and never displaces a child (implemented with `!` on `>`).
  `RemoveAt` moves the last item into the hole, then sifts down when the removed key was above the last, up otherwise.
- `QueueResource`, `DequeueResource` and `SignalComplete` construct and destroy their `SingleLock` immediately
  (0x1000e750/0x1000e7d0/0x1000e870) — the lock is a no-op in the original. Synchronisation is the semaphore plus the
  mutex the worker holds while popping. `DequeueResource` takes the semaphore token after unlinking.
- The delete list is the original's pooled list: 0x40-byte blocks, 13 pointers at +0x0c, count at +0x08, circular
  links at +0x00/+0x04 (head's +0x04 is the newest block). `CheckForComplete` first calls `DeleteSelf` on every
  entry and frees the blocks, then drains the complete heap (pop root, sift down, `DoAction(CheckComplete)` outside
  the mutex).
- Lifecycle: the constructor creates a suspended `Jet::Thread(MainThread, this, false)`, sets the inherited priority
  and resumes it. `ProcessThread` takes a `SharedScratchPad`, loops on `Thread::IsDone()`/`done` and
  `Semaphore::Lock(-1)`, and releases the pad. The destructor deletes the delete list's resources, sets `done`,
  signals the semaphore, deletes the thread object (the original calls vtable slot 0 with flag 1), then frees both
  arrays. The dtor is protected, as the mangled `MAE` shows; allocate/deallocate through `operator new/delete`
  (Jet::Alloc) since both classes carry `JET_HEAP_OBJECT`.
- `??_FBackgroundResource` (0x1000e340) is exported from a `__fastcall BackgroundResourceDefaultClosure(void*,
  void*)` helper, the same pattern the other units use. The constructor it calls is `__thiscall` (this in ECX).
- **Export-thunk finding (important for the merge):** a `.def` export of a symbol whose mangled name also exists in
  `Render3DCore_7c_orig.lib` can be resolved by link.exe to an import thunk (`jmp dword ptr [__imp_orig.symbol]`)
  instead of the local definition. This happened to `LoadResource(ResourceDatabase*, PString)` in this unit, and the
  current main build shows the same for `?GetName@ReferencePoint@Jet@@QBAABVPString@2@XZ` (export RVA 0x3E4DE is
  `jmp [0x100400EC]`, and the IAT entry belongs to `Render3DCore_7c_orig.dll`) and for other ReferencePoint members
  around it. Those exports then call the original, so a diff test that calls the export compares the original with
  itself. Fix used here: export the name with an alias to a uniquely named free helper
  (`?LoadResource...=?BackgroundResourceLoadResourceDatabase@Jet@@YA_N...` in `u11_statics.txt`). The main agent
  should scan the merged DLL's implemented exports for `jmp [IAT]` entry points and alias the affected ones;
  `check_exports.py` does not catch this (the export is not a PE forwarder, and the symbol is also imported). Note
  the main tree has moved on since this workstream's copy (u3b/u6/u9b2 fragments are newer), so the exact export
  addresses will change.

## Merge notes

`python tools/check_exports.py <our dll> D:/trainz12-decomp/exports/Render3DCore_7c.tsv` reports `2500 exports,
TSV 2500; missing/wrong 0, extra 0`. A dry run of `merge_implemented.py` over the current main fragment set plus
`u11_statics.txt` (20 files) followed by `gen_def.py` succeeds: 1676 implemented, 824 forwarded, no duplicate names.
The main tree has advanced since this workstream's copy (`u3b_bounds`, `u6_geometry`, `u9b2_modifiers` are newer and
not in the copy), but none of the 49 names here overlaps them; `u6_geometry` also deliberately leaves the geometry
template instantiations forwarded, as this unit does.

## Blockers

- `Facility`: the 15+ slot interface is not documented by the RC1 header; needs a consumer-side inventory before it
  can move.
- `JetGetPluginsRender3DCore`: waits for U5a/U5b/U6 (the 15 plugin classes and the static UIds).
- The 109 orphan template/free-function exports: each needs its owning unit's header and instantiation context
  (U6/U5b geometry chunks, U9a's `ITree<Particle>`, U5a's `StdMaterialPluginTemplate`, U1d1's `Line` operators,
  the animation `DefaultInterpolator`).
- The async load path is not end-to-end tested (see above); `BackgroundLoadingThread` should not be cut over in the
  game until a timed round exists with a real `Resource`.
