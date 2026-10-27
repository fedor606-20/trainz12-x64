# Trainz 2012 → 64-bit: port plan

Goal: the game process (`trainz.exe` + Jet modules) rebuilt from reconstructed C++ source as x64, loading the
same content (.ja, .im/.kin, .texture, config.txt, TrainzScript .gs) so every installed add-on keeps working.

Method: **incremental reimplementation inside the running 32-bit game**, the way re3 (GTA III) and OpenRCT2
were done. Each reconstructed function or module replaces the original in the real game and is checked there
before the next one. The x64 build is only attempted once nothing original is left. This gives a playable game
at every step and catches mistakes while they are small.

## Phase 0: infrastructure (now)
- Ghidra project with every native module of the game process, auto-analysed.
- Free names applied: Jet DLL exports (12k mangled C++ signatures), MSVC RTTI (4186 classes, vtables),
  `Class::Method>` log strings, imported names.
- Headless scripts so analysis is reproducible: export function lists, call graphs, per-module progress.

## Phase 1: map
- Per module: functions, classes, vtables, who calls whom, which content formats it reads.
- Inventory of the formats add-ons use and the code that parses each one.
- Progress table: functions named / typed / reimplemented / verified.

## Phase 2: replace module by module (still 32-bit, still playable)
- Order: smallest, best-named Jet modules first (Kernel, Lexer, KernelResources, Render2DCore, Input*),
  then Render*, Interface, Water, then trainz.exe subsystems (asset DB, GS compiler/VM, train physics, world).
- Each rebuilt module is a drop-in DLL with the **same exports**, so the original exe loads it unchanged.
- trainz.exe functions are replaced in place through a loader DLL that patches calls to the new code.
- Verification per module: game starts, a fixed test route loads, the same JetLog output, the same frame
  (screenshot diff), the same GS script behaviour on a test session. Recorded traces act as an oracle.

- Granularity: stateless classes can move one at a time; stateful subsystems move as a whole module
  (see MODLOG "Replacement granularity"). Modules not seen loaded (Lexer, movie players)
  come last or not at all. RenderDirectX9 is the second renderer (plugins.ini lists it next to OpenGL;
  the launcher picks one), so it is ported too.

## Phase 3: x64
- Fix 64-bit hazards in the source: pointer-sized fields in on-disk structs, `int`↔pointer casts,
  32-bit handles in the GS VM, inline asm/x87 maths.
- Swap deps for x64 builds (table in MODLOG.md).
- Replace the embedded XULRunner 1.9 (menu HTML panels) with a 64-bit HTML engine such as WebView2.
- The live renderer is OpenGL: port RenderOpenGL first; it is also the natural base for a modern backend.
- Then real use of the hardware: multi-threaded asset loading, bigger caches, modern renderer backend.

## Phase 4: add-on compatibility
- Load-test every asset in the user's install (count, errors, warnings) on the original and on the port,
  compare.
- A set of sessions with scripted rolling stock run on both, compared by traces.

## Status (2026-10-19)
- Phase 0: done. Phase 1: trainz.exe 65.6% of functions named, 850 TrainzScript natives mapped, Jet SDK 1.0
  headers available as a naming reference.
- Phase 2, Kernel_7c: **done, 256/256 exports ours**, verified by differential probes and in game (Version,
  UId, FastDynamicArray, plugin system, LZSS/BlowFish, main loop, EditItem block). The game reaches the menu
  with the original Kernel_7c removed, JetLog identical to the baseline.
- Phase 2, KernelStandard_7c: forwarding proxy in game, unit U1 (math) ours and bit-exact: 34/1762 exports.
- Phase 3, Kernel_7c x64: builds as x64 against a KernelStandard stand-in; the task and EditItem probes give
  the original's output and a golden codec/container test matches the 32-bit build. 8 hazards found and fixed
  or recorded (MODLOG "Kernel_7c on x64").
- Phase 3 deps: x64 builds via vcpkg in `D:\trainz12-decomp\thirdparty` (OpenAL Soft, OpenSSL 3,
  libjpeg-turbo, nvtt); Cg, d3dx9, XULRunner, SpeedTree still open.

## Next steps (one fresh session each; read MODLOG.md first, work sequentially to save the usage limit)
After each step, refresh the README progress block: `python -I tools/progress.py --exports D:/trainz12-decomp/exports --write`.
1. ~~Kernel_7c: EditItem block + Compress/Encrypt vbtables → 256/256, probe + menu check.~~ Done 2026-10-19.
2. ~~Kernel_7c x64: build the module as x64 in a standalone test harness (no game yet) to find 64-bit hazards early.~~
   Done 2026-10-19.
3. ~~KernelStandard_7c: inventory and unit order (notes in `D:\trainz12-decomp\notes\KernelStandard.md`).~~ Done
   2026-10-19: 15 units U0-U14, replaced **top-down** (the foundation of Alloc/PString/strings goes last; MODLOG).
4. ~~KernelStandard_7c: U0 (pure-forwarding proxy, menu check) + U1 Math-1.~~ Done 2026-10-19: 34 exports,
   ~43 billion bit-exact checks, 0 mismatches, menu check clean.
4b. ~~KernelStandard_7c: the other leaf units, U2 Matrix3/4 + Quaternion, U3 small PODs, U4 sync, U5 Rand/LCRand.~~
   Done 2026-10-19: 242/1762 exports ours, each unit bit-exact against the original (NaN payloads aside; MODLOG).
5. ~~Small Jet modules: KernelResources, KernelResource, Render2DCore, InputWin32, InputSystem.~~
   Done 2026-10-21: all five ours in game. KernelResources 51/51, Render2DCore 85/85 and InputWin32 (the
   game's keyboard and mouse driver) are ours and verified against the originals (MODLOG "Step 5"; InputWin32
   also with real clicks and keys in game). InputSystem 327/327 ours and verified (2026-10-21: units U1-U8 plus
   the binding scanner; the game runs without InputSystem_7c_orig.dll). KernelResource 187/187 ours and verified
   (2026-10-21; the game runs without KernelResource_7c_orig.dll). Step 5 is done.
6. ~~Decide the menu engine replacement and the Cg path.~~ Done 2026-10-21: Mozilla only serves the online
   web dialog (the add-on HTML is the game's own renderer), so WebView2 behind an IWebBrowser adapter; Cg 3.1 x64
   runtime (MODLOG "Step 6/7 decisions").
7. ~~SpeedTree.~~ Done 2026-10-21: not in trainz.exe; it is the TNI plugin plugins\TNISpeedTree.dll (SpeedTree 5,
   524 .srt trees in the user's content). Keep it 32-bit in a helper process behind a TNI bridge.
7b. ~~KernelStandard_7c units U6-U14 (needed before x64).~~ Done 2026-10-26: U6-U13 (2026-10-21..26), U14a (the string
    classes) and U14b (the foundation cut-over: Alloc, PackedHeap(MP), PoolBlockAlloc, ReferenceCount,
    PString/PStringCache, KernelStandardInit and every data export). 1761/1762 exports ours; only
    HardwareException::GetCallStack (an x86 stack walk) is forwarded, and the crash report writers are called in the
    original by address. Both move with the x64 rewrite.
8. Render stack (RenderSystem, Render3D*, RenderOpenGL), then InterfaceSystem, WaterSystem.
   8a. ~~RenderSystem_7c: inventory, unit order, U0 forwarding proxy in game.~~ Done 2026-10-26: 2687 exports in
       units U1-U9 (`notes/RenderSystem.md`); every other render module and trainz.exe stand on it.
   8b. ~~RenderSystem_7c U1 (value types, vertex formats) and U4 (modifiers).~~ Done 2026-10-26: U1 939 exports,
       U4 54 more (993/2687 ours); `tests/diff_rs_u1.cpp` and `tests/diff_rs_u4.cpp` 0 mismatches, menu check clean.
   8c. RenderSystem_7c U2 + U3 (shader languages, vertex programs, combiners). U2a (symbols, registers,
       instructions, preprocessor defines; 99 exports, 1092/2687) done 2026-10-26, `tests/diff_rs_u2.cpp`. U2b1
       (ExtendedGfxLanguage and the texture combiner records; 44 exports, 1136/2687) done 2026-10-26,
       `tests/diff_rs_u2b.cpp`. U2b2a (the quick parser and the pre-parser: flex skeleton, Lemon engine, internal
       code) done 2026-10-27. U2b2b1 (the texture program parser) and U2b2b2 (the vertex
       program parser, a later Lemon with 16-byte values) done 2026-10-27. U3 split into U3a1 (VertexProgram and
       TextureCombiner; 50 exports, 1186/2687, `tests/diff_rs_u3.cpp`) done 2026-10-27, U3a2 (their managers and
       resources; 35 exports, 1221/2687) done 2026-10-27, U3b1 (VertexProgramBasic; 54 exports, 1275/2687) done
       2026-10-27, and U3b2 (CGShaderLoader/Technique/Constant, GLSLShaderBase; 48 exports, 1323/2687) done
       2026-10-27. U3 is complete. Next: 8d, U5 (geometry).
   8d. RenderSystem_7c U5 (geometry), U6 (textures), U7 (materials), U8 (layers), U9 (Display, RenderKernel).
   8e. Render3DCore, Render3DFoundation, RenderOpenGL, then InterfaceSystem, WaterSystem (own inventories).
   8f. RenderDirectX9_7c, the launcher's other renderer (d3dx9_42; x64 uses d3dx9_43). Checks with DirectX
       picked in the launcher.
   8g. InputDirectX_7c, the DirectInput driver (joysticks, wheels, force feedback through InputSystem's
       ForceEffect). Loaded at the menu but keyboard and mouse come from InputWin32; left out of step 5.
9. trainz.exe subsystems through the loader DLL: asset DB, GS compiler/VM, physics, world.

## Honest scale
trainz.exe alone is ~6.7 MB of x86 (tens of thousands of functions); with the Jet modules ~9 MB.
Comparable community projects took small teams 1–3 years. Phase 0–1 are days; Phase 2 is the bulk.
