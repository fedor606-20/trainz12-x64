# KernelStandard_7c.dll: reconstruction plan

Status: analysis only, finished 2026-10-19 (drafted by one session, corrected and rendered by the next; hand-over in
`notes\HANDOVER_KernelStandard.md`). No KernelStandard code is reconstructed yet. The x64 harness's stand-in
(`src\jet\tests\x64\KernelStandardStub.cpp`) is not a reconstruction, but its notes on PString, Options, ReferenceCount
and Mutex were checked against the decompile and are folded in below.
Private notes, derived from our own Ghidra project and the original binaries; no decompiled code or SDK text is quoted.
Reproducible inputs: `notes\data\` (call graph `calls.tsv`, data references `datarefs.tsv`, instruction stats `insnstats.tsv`,
class table, address map; scripts `notes\ghidra\ClassGraph.java`, `InsnStats.java`, `notes\data\*.py`) and the full
decompile `decomp\KernelStandard_7c.dll.c` (2250 functions, 0 failures).

## 0. Summary

* The module is the Jet base library: allocator, interned strings, string classes, files/IO/archives, config parser,
  options, diagnostics, threads, math, Bitmap/JPEG, a few Win32 wrappers. **1762 exports** (1649 code, 113 data/vftable),
  `.text` 327 509 bytes, 2276 Ghidra functions (292 521 B), 77% named. One msvcr80/msvcp80 dependency, ijl10.dll, DBGHELP,
  WINMM, VERSION.
* **Only 666 of the 1762 exports are imported by anything in the install** (trainz.exe 447, the six tool exes 191-311,
  21 Jet modules 6-168). 1096 exports (mostly whole classes: MultiByteString, StringTables*, BlockReader, BPE, PoolBlockAlloc,
  SWMRGuard, JIRF writers, FileSub*...) are used only inside the module. Original `Kernel_7c.dll` uses 121.
  The final x64 module needs the 666 plus whatever the module itself calls, but for the 32-bit drop-in every name must exist
  (forwarders cover the rest).
* **~13% of `.text` is a JPEG library and ~4% CRT/import-thunk glue**: 41.6 KB (0x100432e0-0x1004d571, 84 functions) is a statically linked IJG-style
  libjpeg *encoder*, reachable only from `Bitmap::SaveJPG`; it needs no reimplementation (use libjpeg-turbo in x64).
  JPEG *decoding* goes through ijl10.dll (Intel, 32-bit only, ordinals 2/3/4 = Init/Free/Read).
* **State is concentrated and mostly not exported.** The "foundation" (Alloc, the `#StringData` heap, PStringCache with its
  global lock, the AnsiString null buffer, Log, ThreadLocalStorage) is called from orig-internal code by direct address.
  Consequence for the incremental method: replace **consumers before foundations** (top-down). The foundation moves last, as
  one closed unit, at which point nothing of the original module is left (section 4).
* Stateless leaf classes (math, CRC, sync wrappers, Rand, colour, list nodes) can be replaced first, one at a time, with exact
  differential tests.
* **Recommended first unit: U1 "Math-1" (Crc, StdMath incl. the 2 KB `s_sqrtLUT` table that 10 modules read inline, Vector2/3,
  integer-log helpers, OptimalRects, Eigen3), preceded by U0 (pure-forwarding KernelStandard proxy).** 35 exports, 4.3 KB.

## 1. Export inventory

### 1.1 Per group (grouping is mine; classes in section 1.3)

Counts: "exports" = names in the export table; code size = Ghidra function bytes attributed to the group: by namespace, plus
unnamed helper functions attributed by single-caller propagation and address neighbourhood (approximate for the 87 KB of
unnamed `Global` functions: 32.7 KB JPEG, 7 KB import thunks/CRT glue, 2.3 KB unattributed).

| Group | exports (code/data) | imported by anyone | code fns / bytes (incl. unexported + attributed helpers) | Jet modules importing | exes importing |
|---|---|---|---|---|---|
| 01 Memory (Alloc, PackedHeap, PoolBlockAlloc) | 42 (41/1) | 18 | 86 / 12,264 | 21 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 02 String core (PString/PStringCache) | 46 (44/2) | 30 | 52 / 6,019 | 21 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 03 AnsiString/UnicodeString/MultiByteString/String | 269 (269/0) | 80 | 246 / 19,737 | 17 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 04 StringTables (localisation tables) | 41 (34/7) | 0 | 37 / 6,669 | 0 | 0: - |
| 05 Threads and sync | 127 (114/13) | 59 | 105 / 7,419 | 8 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 06 Time, Rand, Crc, Date | 84 (70/14) | 20 | 56 / 4,838 | 9 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 07 Math (Vector/Matrix/Quaternion/Eigen3/StdMath) | 63 (62/1) | 29 | 84 / 20,243 | 8 | 6: CCP, ContentManager, Launcher, TADDaemon, TrainzUtil, trainz |
| 08 Intrusive lists/trees | 28 (27/1) | 3 | 16 / 387 | 1 | 1: trainz |
| 09 File and IOSlave streams | 405 (395/10) | 189 | 463 / 62,438 | 13 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 10 JIRF chunk files | 34 (33/1) | 15 | 32 / 3,416 | 1 | 2: TrainzUtil, trainz |
| 11 ConfigData/ConfigParser/Options | 232 (229/3) | 86 | 257 / 48,372 | 14 | 5: CCP, ContentManager, TADDaemon, TrainzUtil, trainz |
| 12 Diagnostics (Log, Console, Profile, Exceptions) | 160 (122/38) | 53 | 143 / 18,669 | 19 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 13 Platform (OS, Window, DynamicLibrary, Events) | 110 (101/9) | 45 | 102 / 8,311 | 10 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 14 Bitmap/Color/ImageProcessing (+JPEG) | 121 (108/13) | 39 | 111 / 31,542 | 12 | 7: CCP, ContentManager, Launcher, TADDaemon, TrainzDiag, TrainzUtil, trainz |
| 15 Embedded JPEG encoder (IJG-style, SaveJPG only) | - | - | 84 / 32,700 | - | - |
| 99 STL/CRT glue | - | - | 348 / 7,033 | - | - |
| ?? unowned | - | - | 28 / 2,312 | - | - |

Notes: group 04 (StringTables) has **no importer and no internal caller**: dead in the shipped game (the localisation tables are
read elsewhere), but its vftable and 41 exports exist. Group 08 is tiny. Groups 09 + 11 + 14 hold about 49% of the named code.

### 1.2 Who imports the module

| Importer | KernelStandard exports used |
|---|---|
| trainz.exe | 447 |
| TrainzUtil.exe | 311 |
| ContentManager.exe | 283 |
| Launcher.exe | 281 |
| TADDaemon.exe | 280 |
| CCP.exe | 259 |
| TrainzDiag.exe | 191 |
| RenderSystem_7c.dll | 168 |
| InterfaceSystem_7c.dll | 142 |
| Kernel_7c.dll | 121 |
| Render3DFoundation_7c.dll | 95 |
| Render3DCore_7c.dll | 90 |
| KernelResource_7c.dll | 83 |
| RenderOpenGL_7c.dll | 60 |
| InterfaceResources_7c.dll | 57 |
| InputSystem_7c.dll | 54 |
| WaterSystem_7c.dll | 54 |
| Render3DExtension_7c.dll | 46 |
| RenderDirectX9_7c.dll | 42 |
| DSMoviePlayer_7c.dll | 30 |
| Lexer_7c.dll | 30 |
| Render2DCore_7c.dll | 30 |
| KernelResources_7c.dll | 27 |
| DebugSupport_7c.dll | 23 |
| InputWin32_7c.dll | 21 |
| VFWMoviePlayer_7c.dll | 20 |
| InputDirectX_7c.dll | 15 |
| DebugInterface_7c.dll | 6 |

Every module imports by name (mangled C++ names); no ordinal imports. No other PE in `D:\games\trainz 2012 x64` imports
KernelStandard (84 PEs scanned; `trainznativeinterface.dll`, `TNISpeedTree*.dll`, JAConverter, XCrashReport, jpatchl do not).
`Launcher/ContentManager/CCP/TADDaemon/TrainzUtil/TrainzDiag` are native and use 191-311 exports each: they are consumers for
the differential harness too (TrainzUtil is the best one for JIRF/Config, TADDaemon for sync primitives).

Vftable exports (22) imported by others: `DynamicLibrary` (Kernel, 4 tools, trainz.exe), `Exception` (7 modules incl. Lexer,
trainz.exe), `IOSlave` (trainz.exe + all tools), `Mutex`, `Semaphore`, `WEvent` (TADDaemon: those derive/construct them).
Data exports imported (24): `PString::cache`, `StdMath::s_sqrtLUT` (10 modules), `g_jetSTLAllocHeap`, `Time::*`, the Profile
and ExceptionHandler statics, etc. The remaining data exports are never imported.

### 1.3 Per class

(`fns/bytes` = Ghidra functions in that namespace, exported or not; `globals written` = distinct `.data/.bss` addresses the
class's own code writes, from `datarefs.tsv`; this is what makes a class stateful.)

| grp | class | exports (data) | imported | fns | bytes | Jet mods | trainz.exe | tool exes | globals written |
|---|---|---|---|---|---|---|---|---|---|
| 01 | Alloc | 6 (0) | 3 | 5 | 868 | 21 | yes | 6 | 12 |
| 01 | DynamicPtr | 5 (0) | 1 | 3 | 47 | 0 | yes | 0 | 0 |
| 01 | PackedHeap | 14 (0) | 5 | 14 | 2,306 | 0 | yes | 5 | 2 |
| 01 | PackedHeapMP | 5 (0) | 4 | 5 | 570 | 0 | yes | 6 | 0 |
| 01 | PoolBlockAlloc | 7 (0) | 0 | 7 | 831 | 0 | - | 0 | 0 |
| 01 | ReferenceCount | 4 (0) | 4 | 5 | 158 | 17 | yes | 6 | 0 |
| 01 | g_jetSTLAllocHeap | 1 (1) | 1 | 0 | 0 | 0 | yes | 6 | 0 |
| 02 | PString | 15 (2) | 11 | 13 | 1,360 | 21 | yes | 6 | 0 |
| 02 | PStringCache | 31 (0) | 19 | 31 | 3,982 | 21 | yes | 6 | 0 |
| 03 | AnsiString | 92 (0) | 59 | 87 | 6,990 | 14 | yes | 6 | 5 |
| 03 | MultiByteString | 78 (0) | 0 | 72 | 6,109 | 0 | - | 0 | 0 |
| 03 | String | 16 (0) | 15 | 10 | 423 | 17 | yes | 6 | 5 |
| 03 | UnicodeString | 83 (0) | 6 | 75 | 5,768 | 0 | yes | 0 | 0 |
| 04 | StringTablesBase | 31 (7) | 0 | 23 | 6,296 | 0 | - | 0 | 5 |
| 04 | StringTablesDefaultTraits | 10 (0) | 0 | 9 | 285 | 0 | - | 0 | 0 |
| 05 | CriticalSection | 6 (1) | 4 | 6 | 330 | 7 | yes | 6 | 0 |
| 05 | Job | 16 (1) | 0 | 14 | 461 | 0 | - | 0 | 0 |
| 05 | Jobs | 6 (0) | 1 | 6 | 901 | 1 | - | 0 | 0 |
| 05 | MultiLock | 7 (0) | 3 | 6 | 451 | 1 | yes | 3 | 0 |
| 05 | Mutex | 6 (1) | 5 | 4 | 129 | 4 | yes | 6 | 0 |
| 05 | ProtectedLong | 10 (0) | 5 | 3 | 37 | 0 | yes | 6 | 0 |
| 05 | SWMRGuard | 6 (0) | 0 | 6 | 578 | 0 | - | 0 | 0 |
| 05 | Semaphore | 6 (1) | 4 | 4 | 134 | 2 | yes | 4 | 0 |
| 05 | SingleLock | 6 (0) | 4 | 5 | 161 | 6 | yes | 6 | 0 |
| 05 | SyncObject | 12 (1) | 8 | 8 | 241 | 5 | yes | 6 | 0 |
| 05 | Thread | 24 (2) | 18 | 23 | 866 | 5 | yes | 6 | 1 |
| 05 | ThreadLocalStorage | 14 (5) | 0 | 9 | 1,504 | 0 | - | 0 | 5 |
| 05 | WEvent | 8 (1) | 7 | 5 | 130 | 4 | yes | 6 | 0 |
| 06 | Crc | 1 (0) | 0 | 1 | 53 | 0 | - | 0 | 0 |
| 06 | Date | 3 (0) | 2 | 2 | 52 | 0 | yes | 3 | 0 |
| 06 | LCRand | 18 (0) | 2 | 13 | 1,033 | 1 | - | 0 | 0 |
| 06 | QuickTimer | 14 (9) | 0 | 5 | 692 | 0 | - | 0 | 6 |
| 06 | Rand | 27 (0) | 7 | 20 | 2,504 | 1 | yes | 6 | 1 |
| 06 | Time | 21 (5) | 9 | 15 | 504 | 9 | yes | 6 | 4 |
| 07 | Eigen3 | 10 (0) | 1 | 9 | 1,549 | 1 | - | 0 | 0 |
| 07 | Jet | 7 (0) | 2 | 7 | 781 | 2 | - | 0 | 0 |
| 07 | Matrix3 | 7 (0) | 4 | 7 | 2,037 | 0 | yes | 0 | 0 |
| 07 | Matrix4 | 12 (0) | 3 | 12 | 6,046 | 6 | yes | 3 | 0 |
| 07 | OptimalRects | 4 (0) | 3 | 3 | 1,170 | 1 | - | 0 | 0 |
| 07 | Quaternion | 10 (0) | 8 | 10 | 4,255 | 3 | yes | 5 | 0 |
| 07 | StdMath | 9 (1) | 5 | 7 | 628 | 5 | yes | 4 | 0 |
| 07 | Vector2 | 2 (0) | 1 | 1 | 19 | 1 | - | 0 | 0 |
| 07 | Vector3 | 2 (0) | 2 | 2 | 52 | 1 | yes | 0 | 0 |
| 08 | DLListNode | 4 (0) | 0 | 2 | 35 | 0 | - | 0 | 0 |
| 08 | DLTreeNode | 13 (0) | 2 | 8 | 169 | 1 | - | 0 | 0 |
| 08 | IListNode | 2 (0) | 1 | 0 | 0 | 0 | yes | 0 | 0 |
| 08 | ISingleListNode | 2 (0) | 0 | 1 | 9 | 0 | - | 0 | 0 |
| 08 | RBTreeNode | 7 (1) | 0 | 5 | 174 | 0 | - | 0 | 0 |
| 09 | BPE | 2 (0) | 0 | 2 | 1,472 | 0 | - | 0 | 0 |
| 09 | BlockReader | 27 (0) | 0 | 27 | 2,909 | 0 | - | 0 | 0 |
| 09 | DynamicBuffer | 51 (0) | 26 | 33 | 2,299 | 1 | yes | 6 | 0 |
| 09 | File | 93 (2) | 53 | 74 | 9,903 | 10 | yes | 6 | 3 |
| 09 | File::Entry | 7 (0) | 5 | 5 | 346 | 1 | yes | 2 | 0 |
| 09 | File::Find | 17 (1) | 7 | 16 | 3,057 | 3 | yes | 6 | 0 |
| 09 | File::Path | 53 (0) | 35 | 48 | 6,714 | 9 | yes | 6 | 6 |
| 09 | File::Time | 7 (0) | 0 | 4 | 72 | 0 | - | 0 | 0 |
| 09 | FileEx | 7 (1) | 0 | 5 | 119 | 0 | - | 0 | 0 |
| 09 | FileSubArchive | 2 (0) | 0 | 18 | 3,462 | 0 | - | 0 | 3 |
| 09 | IOSlave | 12 (1) | 4 | 10 | 327 | 1 | yes | 6 | 0 |
| 09 | IOSlaveFile | 10 (1) | 7 | 8 | 460 | 0 | yes | 6 | 0 |
| 09 | IOSlaveMemory | 10 (1) | 3 | 7 | 316 | 1 | yes | 2 | 0 |
| 09 | IOSlaveMemoryDynamic | 11 (1) | 0 | 10 | 744 | 0 | - | 0 | 0 |
| 09 | IOSlaveReadCursor | 16 (0) | 11 | 14 | 779 | 1 | - | 1 | 0 |
| 09 | IOSlaveResource | 12 (1) | 7 | 12 | 2,537 | 0 | yes | 6 | 0 |
| 09 | IOSlaveWriteCursor | 11 (1) | 3 | 8 | 1,033 | 1 | - | 1 | 0 |
| 09 | MemoryFile | 49 (0) | 23 | 41 | 3,961 | 4 | yes | 3 | 5 |
| 09 | SharedMemory | 8 (0) | 5 | 4 | 204 | 0 | yes | 5 | 0 |
| 10 | JIRFFile | 27 (1) | 11 | 25 | 2,863 | 1 | yes | 1 | 0 |
| 10 | JIRFWriteBlock | 2 (0) | 2 | 2 | 201 | 1 | - | 0 | 0 |
| 10 | JIRFWriteHeader | 2 (0) | 2 | 1 | 92 | 1 | yes | 1 | 0 |
| 10 | JIRFWriteSize | 3 (0) | 0 | 2 | 16 | 0 | - | 0 | 0 |
| 11 | ConfigData | 166 (1) | 59 | 139 | 20,962 | 10 | yes | 2 | 5 |
| 11 | ConfigParser | 25 (2) | 9 | 22 | 3,375 | 10 | yes | 3 | 5 |
| 11 | Options | 41 (0) | 18 | 36 | 6,702 | 9 | yes | 3 | 5 |
| 12 | BuildVersion | 5 (0) | 0 | 4 | 25 | 0 | - | 0 | 0 |
| 12 | Console | 18 (1) | 9 | 17 | 2,410 | 3 | - | 0 | 5 |
| 12 | Debugger | 6 (0) | 2 | 3 | 51 | 1 | yes | 6 | 0 |
| 12 | Exception | 22 (1) | 12 | 20 | 803 | 2 | yes | 5 | 0 |
| 12 | ExceptionHandler | 30 (11) | 10 | 17 | 2,841 | 18 | yes | 6 | 17 |
| 12 | HardwareException | 9 (3) | 0 | 6 | 962 | 0 | - | 0 | 4 |
| 12 | KernelStandardInit | 2 (0) | 0 | 1 | 147 | 0 | - | 0 | 3 |
| 12 | Log | 17 (7) | 2 | 9 | 2,677 | 1 | yes | 6 | 12 |
| 12 | LogClient | 4 (1) | 1 | 3 | 877 | 0 | yes | 6 | 5 |
| 12 | Profile | 47 (14) | 17 | 32 | 1,750 | 8 | yes | 0 | 16 |
| 13 | DynamicLibrary | 9 (1) | 6 | 8 | 298 | 1 | yes | 4 | 0 |
| 13 | Events | 10 (0) | 2 | 10 | 879 | 1 | - | 0 | 0 |
| 13 | Events::Event | 10 (1) | 0 | 8 | 191 | 0 | - | 0 | 0 |
| 13 | MousePointer | 3 (0) | 0 | 2 | 18 | 0 | - | 0 | 0 |
| 13 | OS | 19 (1) | 9 | 15 | 641 | 10 | yes | 6 | 5 |
| 13 | SharedScratchPad | 7 (3) | 2 | 5 | 794 | 2 | - | 0 | 0 |
| 13 | Window | 50 (3) | 26 | 44 | 4,196 | 6 | yes | 1 | 4 |
| 13 | WindowPlacement | 2 (0) | 0 | 2 | 106 | 0 | - | 0 | 1 |
| 14 | Bitmap | 98 (0) | 38 | 92 | 29,234 | 12 | yes | 6 | 5 |
| 14 | Color | 1 (0) | 1 | 1 | 318 | 0 | yes | 1 | 0 |
| 14 | HSVColor | 1 (0) | 0 | 1 | 276 | 0 | - | 0 | 0 |
| 14 | ImageProcessing | 8 (0) | 0 | 8 | 644 | 0 | - | 0 | 0 |
| 14 | PackedColor | 13 (13) | 0 | 0 | 0 | 0 | - | 0 | 0 |

### 1.4 Win32 / third-party dependencies of the module

| Dependency | Used by | x64 note |
|---|---|---|
| KERNEL32 (CRITICAL_SECTION, Interlocked*, Tls*, VirtualAlloc/Query, CreateThread/Event/Mutex/Semaphore, file mapping, QueryPerformanceCounter, GlobalMemoryStatus, GetVersionExA...) | everywhere | `GlobalMemoryStatus` -> `GlobalMemoryStatusEx` (>4 GB RAM), `GetVersionExA` lies on modern Windows (manifest) |
| USER32 (49 functions: Window class, message pump, MessageBoxA, SendMessage...) | Window, ExceptionHandler dialogs | `GetWindowLongA/SetWindowLongA` with index 0 (window extra bytes holds `this`): must become `...LongPtr` + `cbWndExtra = sizeof(void*)` |
| GDI32 (GetDeviceCaps, GetStockObject, GetTextExtentPoint32A, SelectObject), ADVAPI32 `GetUserNameA`, VERSION (3 funcs, BuildVersion/exception report), WINMM `timeGetTime` | Window, Console, ExceptionHandler, Time/Thread/Rand | `timeGetTime` wraps at 49.7 days; keep it (Rand seeds, frame times) or use QPC |
| DBGHELP `MiniDumpWriteDump` | ExceptionHandler (CRASH.DMP, writes then execs `XCrashReport.exe`) | x64 CONTEXT; needs `EXCEPTION_POINTERS` |
| **ijl10.dll** ordinals 2,3,4 | `Bitmap::GetInfoJPG`, `Bitmap::LoadJPG` (JPEG decode); 17 816 .jpg in the install | 32-bit only; replace with libjpeg-turbo. Pixel output may differ by IDCT/upsampling rounding: needs a tolerance test, not byte equality |
| embedded IJG libjpeg (41.6 KB) | `Bitmap::SaveJPG` | replace with libjpeg-turbo |
| MSVCR80 (129 imports), MSVCP80 (5, `std::string`/`allocator<char>`) | CRT: `_open/_read/_write/_sopen/_findfirst64i32/_stat64i32/_utime64`, `sscanf`, `qsort`, `atof`... | CRT file handles are plain ints in `File` (+0); keep semantics (text vs binary, `_sopen` sharing) |

## 2. Per-group analysis

Legend: **S** = owns global state; **L** = stateless leaf; "moves with" = must be replaced in the same step as, because
internal calls and shared statics bypass the export table.

### G01 Memory (`Alloc`, `PackedHeap`, `PackedHeapMP`, `PoolBlockAlloc`, `ReferenceCount`, `DynamicPtr`, `g_jetSTLAllocHeap`)  **S**
* `Alloc::New/Delete` are static members; state is a 128-entry size-class free-list table (8 bytes/entry at 0x100add5c), per-class
  caps (0x10074ba8) and counts (0x10074da8), **16 striped CRITICAL_SECTIONs** (0x10074fa8-0x10075128, selected by `class & 15`),
  statistics (0x100adb78-8c) and a shutdown flag (0x100728d6). `Alloc::Alloc()` (exported ctor, a global instance) inits the
  sections and caps; no lazy path. Out of memory: `OutputDebugString("Alloc::New> out of memory!")`, then throws `std::bad_alloc`.
* Every block = `malloc((n + 0xb) & ~7)` (>= n+4) from **msvcr80** with a 4-byte tag before the returned pointer:
  `tag = ((cls ^ 0xffffff55) & 0x1ff) | (cls << 9)`, `cls = (n + 0xb) >> 3`; blocks with cls >= 0x80 (n > ~1 KB) go straight to
  malloc/free. `Delete` reads `tag >> 9`. So **allocations made by ours and freed by orig (or the reverse) only work if tag
  format, size-class rules and the underlying CRT heap are identical**; the module's returned pointers are 4 mod 8.
  64-bit: this breaks 16-byte alignment (Matrix4 / SSE members); the x64 allocator needs a 16-byte aligned header.
* `PackedHeap` (0x20) / `PackedHeapMP` (0x2c, adds a `CriticalSection` at +0x20): 1 MB blocks from `VirtualAlloc(0x103000, PAGE_READWRITE)`,
  allocations located by an ordered tree keyed by block address (`FindBlockForAllocation`). The tree and the heap header are
  MSVC8 `std::set` internals (checked-iterator build, `_invalid_parameter_noinfo` calls): do not use the VS2026 STL for any
  member of these classes, **sizeof must stay 0x20/0x2c because trainz.exe and tools allocate them** (PackedHeap 5 imports,
  PackedHeapMP 6, `g_jetSTLAllocHeap` is an exported *object* of PackedHeapMP, 0x100ae1ac).
* Hidden state: `DAT_100adc18` = the string heap, a `PackedHeapMP("#StringData")` created lazily by the (unexported) helper at
  0x10039780, which `PString`, `AnsiString` and `PStringCache` use for every buffer up to 0xa0 bytes. Its summary ("Packed Heap
  Summary" at exit) changes between identical runs (MODLOG): ignore it in log diffs.
* Dependencies: Win32 only (VirtualAlloc, CRITICAL_SECTION, OutputDebugString) + `Log`/`File` for the debug print paths.
* Moves with: all of G02/G03 and everything that frees memory: i.e. **last** (see 4).

### G02 PString / PStringCache  **S**
* `PString` is one pointer to a `PStringCache::Node` (null = empty); its copy/dtor/compare are inlined in every importer
  (MODLOG). The cache is a hash table (default 0x3e5 = 997 buckets, 12 bytes each) created by `KernelStandardInit`
  (`FUN_100390b0`) into the exported static `PString::cache`; destroyed in the module shutdown function (sets
  `PString::isFinished`).
* **Global lock**: one static `CRITICAL_SECTION` at 0x100acb58, initialised *by the PStringCache constructor* and taken inline
  by importers through exported `PStringCache::Lock/Unlock`. Ref counting is done under it (node.refs, cache.liveRefs,
  cache.uniqueCount).
* Dependencies: Alloc/string heap; `GetANSI/GetUnicode` bridge to the string classes; `PString::Read/Write` use `BinReader/BinWriter`
  (virtual `IOSlave`-style objects).
* Node pointers are *compared as integers* in `SortPtr` (identity sort): ordering depends on addresses, never persisted
  (check: no file format stores it), but x64 pointers are 8 bytes in any `FastDynamicArray<PString>` etc.

### G03 AnsiString / String / UnicodeString / MultiByteString  **S** (shared null buffer + string heap)
* `AnsiString` and its alias class `String` are **one pointer** to a ref-counted block `{refs @-4, capacity @0, length @4, chars @8}`;
  refcount via `InterlockedIncrement/Decrement`; a *static null buffer* at 0x100ae254 (+ guard 0x100ae264, function-local
  static emitted into many functions) is never counted/freed, compared by identity (`lpAddend != &DAT_100ae254`). Buffers up to
  0xa0 bytes come from the `#StringData` heap, larger from `Alloc`. `String`'s 16 exports are thin wrappers over AnsiString.
* `UnicodeString` (0xc) is a plain owned buffer `{wchar_t* data, cap, len}`, no sharing. `MultiByteString` (0x10) is MBCS-aware
  `{data, cap, byteLen, charCount}` and has no importer.
* Dependencies: CRT (`_mbs*`, `_vsnprintf`, `wcstombs`, `strtol/wcstod`), `PStringCache::GetANSI`. 64-bit: lengths/caps are `uint`;
  the inline x87/SSE in `GetFloat` (2 functions) is trivial.
* Orig-internal code (File, Options, ConfigData, Log, ...) manipulates AnsiString buffers directly, including the
  null-buffer identity check and `PackedHeapMP::Free(DAT_100adc18, p)`. **A replaced AnsiString and an orig consumer cannot
  coexist unless representation, heap and null buffer are shared.** Moves with G01, G02.

### G04 StringTables (`StringTablesBase`, `StringTablesDefaultTraits`)  **S** (5 PString token statics)
* Reads/writes localisation string tables, text and binary (`AddBin/SaveBin`, file id tokens `fileTblId`, TABLE/ID/ENTRY/VALUE
  tokens as exported static PStrings). No importer, no internal caller in this install. Uses File, ConfigData, Exception.
* Cheapest correct choice: implement last or only as a literal port; test with synthetic tables.

### G05 Threads and sync
* `SyncObject {vptr, HANDLE}` (8) with `Mutex`, `Semaphore`, `WEvent` (8 each) derived; virtual slots (as used inline
  by other classes): +4 handle, +8 `Lock(timeout)`, +0x10 `Unlock`. `CriticalSection {vptr, unused HANDLE, CRITICAL_SECTION*}` (0xc;
  the section (0x18) is `Alloc::New`'d). `SingleLock {SyncObject*, bool locked}`, `MultiLock {HANDLE*, bool*, count, SyncObject**}`
  (0x10, calls `WaitForMultipleObjects`). `SWMRGuard` (0x20, single-writer multi-reader; no importer), `ProtectedLong` (interlocked
  long). **L**: all pure wrappers, no globals. Vftables of Mutex/Semaphore/WEvent are imported by TADDaemon.
* `Thread` (0x20): `{vptr, HANDLE, WEvent{vptr, HANDLE}, bool, entry, param, id}`, `Start` thunk runs the entry under
  `ExceptionHandler::Main`; `Thread::mainThreadId` (exported static, set by `KernelStandardInit`). **S** (one static).
* `ThreadLocalStorage` (singleton `0x100add44` created in `KernelStandardInit`, 4 exported TLS index statics): Profile/render
  trace pointers, per-thread data; 1.5 KB; moves with Profile (G12) and Thread.
* `Job`/`Jobs` (0x28 / 0x9c): job queue with a worker thread and console progress (`Console::Printf`); `Jobs` has one importer
  (Jet module), `Job` none.
* 64-bit: handles stored in `int` slots; `WaitForMultipleObjects` arrays; `TlsAlloc` ok.

### G06 Time, Rand, Crc, Date
* `Time` (21 exports, 5 data): QPC frequency (0x100ae160, written by an unnamed init function), frame-lock state (exported:
  `frameLock`, `frameLockCount/Time/Delta`) which `Rand::Randomise`/`LCRand` read. Time is **S** but tiny.
* `Rand` (0x20, lagged-Fibonacci XOR generator: `{idx +0, lag +4, size +8, ..., table* +0x1c}`) and `LCRand`
  (linear congruential, 18 exports). The generator itself is integer-exact; the **scaling to float/double/int range uses
  x87 with the CRT's 53-bit precision control and `fistp` rounding** (`Rand::operator()(uint)` etc.): bit-exact output in an
  SSE2 build needs care (sections 4 and 5). Seeds come from `timeGetTime`+10 or the frame-lock time.
* `Crc::Calc(buf, len, seed)`: table-driven MSB-first CRC (`crc = crc<<8 ^ T[crc>>24 ^ b]`, input/output complemented), 256-entry table at 0x10072998 (generate it; check polynomial and equality with the original table).
* `QuickTimer` (14 exports, 9 data, 6 written globals) is the lightweight profiler: **S**, belongs with Profile (G12).

### G07 Math: Vector2/3, Matrix3/4, Quaternion, Eigen3, StdMath, OptimalRects, free helpers  **L**
* All value types with no globals except `StdMath::s_sqrtLUT` (static 512-entry table, 2 KB at 0x10072078, **read inline by 10
  modules**: must stay an exported array with identical contents). `Sqrt/InvSqrt/Sqrt16/InvSqrt16/InvSqrtApprox` are
  table-driven Newton steps (bit tricks on float, `0x5f3759df` for the approx one).
* Code is SSE2 scalar (not packed) plus some x87 (`_CIacos/_CIasin`, `ROUND` conversions). 2632 SSE and 507 x87 instructions in
  62 functions. `/arch:SSE2 /fp:precise` in the new build should reproduce results; verify by bit comparison (not tolerance).
  Layouts: Vector2 8, Vector3 0xc, Matrix3 0x24, Matrix4 0x40, Quaternion 0x10.
* `OptimalRects` (4 exports, 1.2 KB) uses a `FastDynamicArray`-like list via `Alloc`; `Eigen3` (3x3 symmetric eigen solver, 1.5 KB)
  has an object of at least 0x3c bytes (3x3 matrix + eigen data).

### G08 Intrusive lists/trees (`DLListNode`, `DLTreeNode`, `ISingleListNode`, `IListNode`, `RBTreeNode`)  **L**
* Node bases: DLTreeNode >= 0x18, RBTreeNode >= 0xc with an exported vftable (no importer), `DLListNode` 8. 28 exports, 383 bytes, 3 imported
  (trainz.exe + one Jet module): exact layouts to be read from the constructors when the unit is done.

### G09 File, Path, Find, IOSlave*, MemoryFile, DynamicBuffer, BlockReader, BPE, SharedMemory, FileSub*  **S**
* `File` (no vtable; at least 0x39 bytes, probably 0x3c) = `{CRT fd +0, 16 KB buffer ptr +4, cursor +8, ..., position +0x1c, flags +0x28, ...}` over the CRT low-level I/O
  (`_open/_sopen/_read/_write/_lseek/_commit`), plus `File::callback` (exported static, progress hook) and the static `File::Sep`
  PString (exported).
* `File::Path` is a fixed `char[0x104]`; constructing/assigning from more than 0x102 chars leaves the path empty and sets a global error
  byte at 0x100adb92 (behaviour to keep). `Entry` 0x120 = `{Path, size/time fields...}`; `Find` 0x440 = Entry + CRT
  find handle + 3 Paths (0x124, 0x228, 0x32c) + an **MSVC8 `std::list/set` header** at +0x430 (self-referential sentinel at
  +0x434): size and layout depend on the old STL.
* `FileSubArchive`, `FileSubPath`, `FileSubLink`, `FileSub` (+ `FileSubstList.cpp`; assert strings give the source names `File.cpp`, `FileSub.cpp`,
  `FileSubArchive.cpp`, `FileSubstList.cpp`, `StringTables.cpp`): the **virtual file system ("subst")**. This
  is almost certainly the `.ja` archive reader (to confirm when U10 starts) (Base.ja, Data.ja ... 10 .ja files hold most of the 133 417-file install), directory
  substitution and links; temp names `%08X.T%08X`; the on-disk archive format is parsed here (`SetDefaultBuildVersion`,
  `SetDefaultProductCode`), exported only via two entries but called from `File::Open`. It owns the substitution list and
  archive state (globals 0x100adbc4-0x100adbd4). **The most compatibility-critical code in the module.**
* `IOSlave` family: `IOSlave` (4, vptr only; slots 0 dtor, +0x10 ReadData, +0x14 WriteData, the rest to be read from the vftable), `IOSlaveMemory` 0x10 `{vptr,data,pos,size}`,
  `IOSlaveFile` 0xc `{vptr, HANDLE, ...}`, `IOSlaveMemoryDynamic` 0x18 (embeds an **MSVC8 `std::vector<char>`** at +4..+0x10),
  `IOSlaveReadCursor` 0x18, `IOSlaveWriteCursor` 0x20 (buffered, 0x1000 chunks), `IOSlaveResource` 0x24. `DynamicBuffer` 0x18
  `{data +0, size +4, capacity +8, cursor +0xc, growth +0x10 (0x200 default), bool +0x14}`. `MemoryFile` 0x14 `{+0, +4, +8, bool +0xc, AnsiString name +0x10}`.
* Known behaviour bugs worth keeping out of the spec: LZSS-through-IOSlaveMemory short-write (MODLOG).
* Dependencies: G01-G03, Exception (G12), PackedHeap debug printing calls a helper in this region (0x10028530).
* Moves with: **Log** (opens JetLog.txt through File), Bitmap/Config (callers).
* 64-bit: `ulong` sizes (4 GB files), `_stat64i32/_utime64` are already 64-bit time; `_findfirst64i32` ok; the `std::list`/`vector`
  members; mapped files (`CreateFileMapping`) are fine.

### G10 JIRF chunk files (`JIRFFile`, `JIRFWriteBlock/Header/Size`)  **L-ish**
* Chunked container ("Not a JIRF file"; write side `JIRFWriteHeader/Block/Size`): little-endian u32 sizes, chunk ids. JIRFFile >= 0x144 (member-access lower bound). Used by trainz.exe and TrainzUtil: **an on-disk format that add-ons (compiled assets)
  depend on**. No pointers in the file; offsets are 32-bit.

### G11 ConfigData, ConfigParser(+flex scanner), Options  **S** (statics: `unknownItem`, parser tables)
* `ConfigData` (0x14, 166 exports!): tree of named/typed values (the config.txt/kin/etc. data model), conversions to
  Vector2/3/4, Rect, Color, Quaternion (x87+SSE), text writer (`ConfigData::Writer`, encodings) and **binary format**
  (`ReadBinFile/WriteBin`). `unknownItem` (exported static object, 0x100ae168).
* `ConfigParser` (0x18) wraps `ConfigParserCore` + `scfgFlexLexer`: a **flex-generated C++ scanner** (6.9 KB `yylex` + tables in
  `.rdata`; Flex 2.5 C++ skeleton: vtable with 9 slots). Reproduce by regenerating from a reconstructed .l (actions from the code,
  tables from flex) and diffing token streams on the corpus; do not hand-translate the DFA.
* `Options` (0xc, 41 exports): command-line/ini/env option list; `Options::Get` has many overloads that parse numbers
  (SSE/x87), `GetNext`, `ProcessCmdLine/File/Env/Raw`. Used by 9 Jet modules (`Kernel_7c` too) and trainz.exe.
  Read so far (0x10035a40 ProcessRaw, 0x10035fc0, 0x100358a0 ProcessFile, 0x10036140 SetSeparators, 0x10036180 GetNext):
  **one entry per key** (0x18: next, prev, key, value, separators, cursor), found by FUN_10036040. Each token has an
  optional `/ - +` switch, then a key up to whitespace, `=`, `-` or `+`. A value is quoted, or runs up to the whitespace
  before the next switch character, so it may span spaces and newlines. `+key=v` on a key with a non-empty value
  appends `","` + v (separator byte at 0x10053d2c); anything else replaces the value. `key-`/`key+` without `=` stores
  the sign character as the value. SetSeparators stores the separators and resets the cursor to the value start.
  GetNext returns the next run up to a separator (trailing spaces trimmed), skips the following whitespace and
  separators, and keeps the cursor in the entry (mutable state behind a `const` method). ProcessFile reads the file
  through `File` and copies it as is when `DetectEncoding` reports 1, otherwise converts it with `wcstombs` (so 1
  presumably means ANSI). Then it runs ProcessRaw.
* Dependencies: G01-G03, G09 (File/MemoryFile/IOSlave), math (typed getters), `Crc` (binary files).
* Real corpus: 20 286 .txt and 723 .kin in the install, but only 8 files named `config.txt` are loose; most content lives in the .ja archives, so
  test data has to be extracted through the FileSub layer (original) or dumped with the game's own loader.

### G12 Diagnostics: Log, LogClient, Console, Profile, QuickTimer, Exception*, ExceptionHandler, HardwareException, Debugger, BuildVersion, KernelStandardInit  **S**
* `Log` statics: client list, `logFile` (a `File*`), stderr, `writeLock`, `startTime`, `initialized`, `displayModuleName`;
  JetLog.txt is written at exit (MODLOG). `LogClient::OutputString(V)` formats with `vsprintf` and timestamps (`Time`).
* `Exception` (0x1110): `{vptr, char msg[0x1000] @4, char callstack[0x100] @0x1004, bool @0x1104, int line @0x1108,
  const char* file @0x110c}`; `FileEx` derives; `HardwareException` (0x1114) maps SEH codes; vftable of `Exception` imported
  by 7 modules (they derive custom exceptions): **layout and vtable slot order are ABI**. 64-bit: the file pointer field
  and the 0x1000/0x100 buffers are fine; the stack-dump code walks x86 EBP frames, `CONTEXT.Eip/Esp/Ebp`,
  `unaff_FS_OFFSET` SEH chain manipulation (`ExceptionHandler::Main` installs an `__try`-style handler with
  `FS:[0]`-linked frames): **must be rewritten for x64** (RtlVirtualUnwind/StackWalk64) and is not byte-comparable.
* `ExceptionHandler` writes `CRASH.DMP` (`MiniDumpWriteDump`), `ERRORLOG.TXT`, runs `XCrashReport.exe`; hardware exception names
  (`HardwareException::Type` table), stack overflow / FPU stack check detection. 17 globals written: it is a big state machine.
* `Profile`/`QuickTimer` (47+14 exports, 14+9 data): per-thread trace ring buffers (`FrameTrace` region of ~128 KB at 0x88758-0xa8958,
  `TraceInfo`, `Info**` function tables at 0xa8b58), timing via QPC. Used by 8 Jet modules (profile macros) but only as
  counters.
* `Console` (>= 0x1c, debug text console with a built-in bitmap font `f50_font`), no tool importers.
* `KernelStandardInit()` is **not imported by anyone**: it is a static object inside KernelStandard that sets locale `".ACP"`,
  `Thread::mainThreadId`, creates the PStringCache (997 buckets) and TLS singleton, and appends a shutdown callback to a table at 0x100acb70 (up to 0x400 entries); the shutdown routine at 0x1002ec50 destroys the cache, calls `Log::Done` and destroys the TLS singleton. DllMain stores only `hInstance` (`0x100adbd8`).
* Dependencies: Log needs File + strings + Time; Alloc needs Log for diagnostics: **G01 and G12 are mutually dependent** (see 4).

### G13 Platform: OS, Window, WindowPlacement, DynamicLibrary, Events, SharedScratchPad, MousePointer
* `OS` 0x18 `{HINSTANCE +0, flag +4, appName* +8, listHead* +0xc (0xc-byte list head), HWND +0x10, AnsiString +0x14}`; `OS::Init`
  calls `ExceptionHandler::Main`, `Options::Init/ProcessRaw` (default options string `OS::defaultOptions`, 0x72898).
* `Window` 0x134 (50 exports, `mainWindow` static): Win32 window class, message pump (`HandleMessages`), full-screen/mode
  switching, `WM_END_OF_MESSAGES/WM_MODE_CHANGED` custom messages (exported consts), `ExceptionHandler::criticalShutdown` hooks.
  **64-bit hazards**: window extra bytes `this` pointer via `Get/SetWindowLongA(…,0)`, `GWL_STYLE/GWL_EXSTYLE` (fine),
  `WndProc` casts.
* `DynamicLibrary` 8 `{vptr, HMODULE}`, slots dtor, +4 Open (`LoadLibraryExA`), +8 Close, +0xc Import (`GetProcAddress`);
  vftable imported by 6 modules. `Events` 0x2c `{bool manual?=1 @0, ?@4, 3 lists (head/tail/count) @8, @0x14, @0x20}`;
  `Events::Event` 0x18 `{vptr, links @4/@8, id @0xc, data @0x10, time? @0x14}`; `SharedScratchPad` (0x14, static list + lock,
  per-thread scratch buffers), `SharedMemory` 0x10 (file mapping).
* Dependencies: Options, ExceptionHandler, Alloc, Window <-> HardwareException (modal dialogs).

### G14 Bitmap, Color, HSVColor, PackedColor, ImageProcessing (+ embedded JPEG)  **S**-lite
* `Bitmap` (0x44, 98 exports, 29 KB): load/info for **TGA, BMP, PCX, JPG** (`GetInfoTGA/BMP/PCX/JPG`, `Load*`, `Save`, `SaveJPG`),
  alpha merge (`AddAlpha`), blitters (a format-pair dispatch via `GetBlitter` returning function pointers), `MapPolygon`, `Circle`,
  `ClipLine`, `ScaleArbitrary` (SSE+x87, 172 instr in MapPolygon alone), pixel formats. Corpus: 8 443 .tga, 2 651 .bmp, 17 816 .jpg,
  0 .pcx, 1 .png in the install (.texture/.im are other modules).
* `Color`/`HSVColor` convert (SSE), `PackedColor` has 13 constant colours as exported data, `ImageProcessing` (brightness/contrast LUTs).
* Statics: only `PString::cache` use. **IJL decode struct** (`JPEG_CORE_PROPERTIES`, ~0x4e00-byte stack object in the
  caller) contains pointers: whole decode path is rewritten for libjpeg-turbo.
* Dependencies: G09 (File/MemoryFile heavy), G01/G03.

## 3. Layouts other modules read or construct inline

Confirmed from the code (offsets in bytes, 32-bit). Everything marked **ABI** is used by importers either by inline field
access or because they allocate/embed the object (so sizeof matters). "SDK" notes where Auran's SDK header agrees.

| Class | sizeof | Layout (confirmed) | ABI use |
|---|---|---|---|
| `PString` | 4 | `Node*` (null=empty). Node: `{next +0, prev +4, len +8, cap +0xc, refs +0x10, hash? +0x14, chars +0x18}`; alloc rounds `(len+0x29)&~0xf` | inline ctor/copy/dtor/compare in every importer; `PString::cache` (data export) + `Lock/Unlock` |
| `PStringCache` | 0x30 | `{uniqueRefd +0, liveRefs +4, +8, +0xc, +0x10, +0x14, +0x18, bucketCount +0x1c, +0x20, buckets* +0x24 (12-byte bucket {head, tail, count}), +0x28, +0x2c}`; static lock at 0x100acb58 | importers touch refs/liveRefs inline under the lock |
| `FastDynamicArray<T,13>` | 4 | ring of 0x40-byte chunks `{next, prev, count, items[13]}`; head pointer is the object (MODLOG) | inline |
| `String`/`AnsiString` | 4 | `char*` block `{refs @-4, cap @0, len @4, chars @8}`; shared null block (0x100ae254) | `operator const char*` exported (reads +8 or null); copy ctor/dtor exported |
| `UnicodeString` | 0xc | `{wchar_t* data, cap, len}`, owned | exported ops |
| `MultiByteString` | 0x10 | `{data, cap, byteLen, charCount}` | internal |
| `ReferenceCount` | 8 | `{vptr, long refs}`; vtable `{DeleteThis(cdecl,this on stack), scalar deleting dtor}`; `AddReference` = `InterlockedIncrement`, `ReleaseReference` calls slot 0 at 0 | virtual base of PluginInstance etc. |
| `Options` | 0xc | `{head +0, tail +4, count +8}` of 0x18-byte entries `{next, prev, String key +8, value +0xc, separators +0x10, const char* cursor +0x14}` (`Alloc::New(0x18)` in ProcessRaw) | embedded by value in importers; ctor zeroes 3 words; the destructor is inlined in every importer (Kernel_7c 0x1000c280) |
| `File::Path` | 0x104 | `char[0x104]`; overflow (>0x102 chars) clears/aborts and sets error byte 0x100adb92 | embedded by value, 3-up in `Find` |
| `File::Entry` | 0x120 | `{Path +0, +0x104.. 6 dwords, 2 words +0x11c}` | by value |
| `File::Find` | 0x440 | `{Entry, handle +0x120 (-1), Path +0x124, +0x228, +0x32c, +0x430.. MSVC8 list: head +0x434 (sentinel), size +0x438, +0x43c}` | by value |
| `File` | >= 0x39 (0x3c?) | `{fd +0 (-1), buf +4, cur +8, +0xc, +0x10, +0x14=-1, +0x18, pos +0x1c, ...., flags +0x28, +0x2c, enc? +0x34=1, bool +0x38}`; buffer 0x4000 from `Alloc` | by value / `new File` in importers |
| `IOSlave` | 4 | `{vptr}`; slots: dtor +0, Seek +4, Tell +8, GetSize +0xc, ReadData(void*,ulong) +0x10, WriteData(const void*,ulong) +0x14 (the order our Kernel_7c's LZSS calls through, proven against the real IOSlaveMemory in the plugin probe) | vftable imported by trainz.exe and tools (they derive IOSlave) |
| `IOSlaveMemory` / `IOSlaveFile` / `IOSlaveMemoryDynamic` | 0x10 / 0xc / 0x18 | `{vptr, data* +4, pos +8, size +0xc}` / `{vptr, +4, +8}` / `{vptr, alloc +4, first +8, last +0xc, end +0x10, readPos +0x14}` (MSVC8 vector) | by value |
| `IOSlaveReadCursor` / `WriteCursor` / `Resource` | 0x18 / 0x20 / 0x24 | chunked 0x1000-byte buffered cursors | by value |
| `DynamicBuffer` | 0x18 | `{data* +0, size +4, capacity +8, cursor +0xc, growth +0x10 (0x200 default), bool +0x14}` | by value |
| `MemoryFile` | 0x14 | `{+0, +4, +8, bool +0xc, AnsiString name +0x10}` | by value |
| `DynamicLibrary` | 8 | `{vptr, HMODULE}`; slots dtor, Open +4, Close +8, Import +0xc | vftable imported |
| `OS` | 0x18 | `{instance? +0, +4, const char* +8, list head* +0xc (0xc-byte object), HWND +0x10, AnsiString +0x14}` | by value (App/Kernel_7c create it) |
| `Events` | 0x2c | `{bool +0 (=1), +4, list A {+8,+0xc,+0x10}, list B {+0x14,+0x18,+0x1c}, list C {+0x20,+0x24,+0x28}}` | by value |
| `Events::Event` | 0x18 | `{vptr, link +4, link +8, id +0xc, data +0x10, +0x14}` | derived in importers |
| `SyncObject`/`Mutex`/`Semaphore`/`WEvent` | 8 | `{vptr, HANDLE}` | vftables imported by TADDaemon |
| `CriticalSection` | 0xc | `{vptr, HANDLE +4 (unused), CRITICAL_SECTION* +8 (0x18-byte `Alloc::New`)}` | embedded in many classes |
| `SingleLock` / `MultiLock` | 8 / 0x10 | `{SyncObject*, bool}` / `{HANDLE*, bool*, count, SyncObject**}` | by value |
| `Thread` | 0x20 | `{vptr, HANDLE +4, WEvent{vptr +8, HANDLE +0xc}, bool +0x10, entry +0x14, param +0x18, threadId +0x1c}` | derived/embedded |
| `Exception` | 0x1110 | `{vptr, msg[0x1000] @4, stack[0x100] @0x1004, bool @0x1104, line @0x1108, file* @0x110c}`; `FileEx` +0; `HardwareException` 0x1114 | vftable imported by 7 modules |
| `PackedHeap` / `PackedHeapMP` | 0x20 / 0x2c | `{..., tree (MSVC8 set) @4, ...}`, MP adds `CriticalSection @0x20` | allocated by importers (exports used) |
| `PoolBlockAlloc` | 0x30 | `{head, ..., align +8, blockSize +0xc, count +0x10, +0x14 total, CriticalSection +0x24}` | no importers |
| `Rand` | 0x20 | `{idx +0, lag +4, size +8, ... table* +0x1c}`, state table `Alloc`ed | by value |
| `Vector2/3`, `Matrix3/4`, `Quaternion` | 8, 0xc, 0x24, 0x40, 0x10 | plain floats | by value everywhere |
| `PluginSystem` objects | | see MODLOG (Kernel_7c) | |

Not confirmed (lower bounds from member access only; confirm before the unit that owns them): `Window` >= 0x134, `JIRFFile`
>= 0x144, `Console` >= 0x1c, `Profile` >= 0x10, `Bitmap` >= 0x44, `ConfigData` >= 0x14, `ConfigParser` >= 0x18, `StringTablesBase`,
`SWMRGuard` 0x20, `Job` 0x28, `Jobs` >= 0x9c, `SharedMemory` 0x10, `SharedScratchPad` 0x14. Method: for each, find the
`Alloc::New(size)` that precedes its constructor in trainz.exe (xrefs) and check against the stride of a `new X[]`.

## 4. How to replace this module (strategy)

### 4.1 Why the Kernel_7c method needs a twist here
* Kernel_7c called into KernelStandard through exports only. KernelStandard's own foundation classes call each other by
  address, and the allocator/string/lock state is **not exported** (statics at 0x100acb58, 0x100ae254, 0x100adc18, 0x10074fa8..,
  `Alloc` free lists). Replacing a foundation class while orig consumers still run puts two implementations on one set of objects
  and breaks pairing: orig frees a string buffer with `PackedHeapMP::Free(DAT_100adc18, p)` and ignores unknown pointers (silent
  leak) or uses a different size-class tag (crash).
* Therefore: **replace consumers first, foundations last** ("top-down"). While orig foundations are still the ones running, our
  consumers reach them through the same exported API the importers use (via `implib\KernelStandard_7c.lib` built against the
  `_orig` DLL), exactly like Kernel_7c. Object layouts and vtables of ours and orig are byte-identical, so objects cross freely.
* A consumer is safe to replace on its own only if (1) layout/vtable/virtual-slot order is identical, (2) every global it writes is
  exported or private to it, (3) its **callers inside orig** (not just importers) don't hold separate copies of its state. G09's
  subst list (FileSub*) fails (3) for any orig caller of `File::Open` until those callers are ours: so File moves after Config,
  Bitmap, JIRF and Log.
* The foundation unit U14 (+U12/U13, which Alloc depends on) is replaced at once and brings the module to "no orig left".
  For it we need a different check than in-game incrementality: the diff harness (below) loads ours and orig in one process
  (both initialise their own statics) and runs identical operation scripts on both; the in-game proof is the switch that deletes
  `KernelStandard_7c_orig.dll`.
* **CRT choice matters here, unlike in Kernel_7c.** (a) `Alloc` must use the same `malloc/free` as orig's code while any orig code
  remains: build U14 against msvcr80 (or `HeapAlloc(GetProcessHeap())` after confirming msvcr80 uses the process heap).
  (b) **`File` stores a CRT low-level file descriptor (`_open`/`_read`) at +0, and fds are per-CRT-instance**: `File` objects
  are created by importers (our constructor) and read by orig code or vice versa, so U10 and everything that touches `File`, `FILE*`
  (`Log` uses `stderr`/`vfprintf`) or `errno` must link the same msvcr80.dll (the game already ships it; make an import lib
  from its exports with `tools\gen_implib.py`). (c) Units that only exchange `Alloc` memory and plain data with orig (U1-U9)
  can use a static CRT as Kernel_7c does, provided they never `free` orig-`malloc`ed memory (the flex scanner in U7 uses
  `malloc/realloc/free` internally: keep all of it inside the unit).

### 4.2 Dependency order among groups (calls that bypass exports)
```
Math, Crc, nodes, colour, sync, Rand          (leaves, no shared state)           U1-U5
   ^
ConfigData/Parser/Options, StringTables, Bitmap, JIRF, stream helpers             U7-U9     (consume File + strings)
   ^
File family (File/Path/Find/Entry/FileSub*/MemoryFile) + IOSlave                   U10       (subst state; Log uses File)
   ^
OS/Window, Diagnostics (Log, ExceptionHandler, Profile, Console), Thread/TLS/Time U11-U13   (Alloc calls Log; Log calls File/Time/CS)
   ^
Alloc, PackedHeap, PString/PStringCache, AnsiString/UnicodeString/MultiByteString/String, DllMain/startup   U14  (foundation)
```
A layer calls the layers below it, so replace from the top down. Cycles found (cannot be split into separate in-game steps): Alloc/PackedHeap <-> Log/File (debug dumps, "out of memory");
Thread -> ExceptionHandler::Main -> Log/Window; PString::Read -> Config glue; Time <-> Log (timestamps). The cycle is why
U12-U14 are the last block, replaced in the order **U13 -> U12 -> U14** only if each step keeps working with the
remaining orig pieces through exports (Thread/TLS/Time/Log first, then Alloc+strings), otherwise as one cut-over.

### 4.3 Units (small, verifiable steps)

Sizes: exports total / imported by others / class-attributed code bytes (named functions in the class namespaces; add roughly
30% for unnamed helpers).

| Unit | Contents | exports / imported / bytes | Shared state touched | In-game step? |
|---|---|---|---|---|
| **U0** | Pure forwarding proxy: all 1762 names, ordinals unchanged, data exports/vftables forwarded to `KernelStandard_7c_orig.dll` | 1762 / 666 / 0 | none | yes (Kernel_7c did it) |
| **U1** Math-1 | `Crc`, `StdMath` (+`s_sqrtLUT`), `Vector2/3`, `Jet::{ILog2 x2, IntLog2, CeilPow2, FloorPow2, GetBaryCentricWeights}`, `OptimalRects`, `Eigen3` | 35 / 14 / 4 252 | `s_sqrtLUT` (we export it), `Crc` table | yes |
| **U2** Math-2 | `Matrix3`, `Matrix4`, `Quaternion` | 29 / 15 / 12 338 | none | yes |
| **U3** small PODs | `Color`, `HSVColor`, `PackedColor` (13 data), `ImageProcessing`, `Date`, `BuildVersion`, `DLListNode`, `DLTreeNode`, `IListNode`, `ISingleListNode`, `RBTreeNode`, `ProtectedLong`, `DynamicPtr`, `MousePointer` | 77 / 12 / 1 804 | none | yes |
| **U4** sync | `SyncObject`, `Mutex`, `Semaphore`, `WEvent`, `CriticalSection`, `SingleLock`, `MultiLock`, `SWMRGuard` | 57 / 35 / 2 154 | none (CriticalSection uses `Alloc`) | yes |
| **U5** Rand | `Rand`, `LCRand` | 45 / 9 / 3 537 | reads `Time::frameLock*` (exported) | yes |
| **U6** small platform | `DynamicLibrary`, `Events`, `Events::Event`, `SharedMemory`, `SharedScratchPad`, `Job`, `Jobs` | 66 / 16 / 3 728 | scratchpad list+lock (private) | yes |
| **U7** Config | `ConfigData`, `ConfigParser`+flex core, `Options`, `StringTables*` | 273 / 86 / 45 165 | `ConfigData::unknownItem`, parser tables, token PStrings | yes (needs File/AnsiString via orig exports) |
| **U8** Bitmap | `Bitmap` + replacement of ijl10 / embedded JPEG | 98 / 38 / 29 234 (+32.7 KB JPEG not ported) | none | yes |
| **U9** JIRF + streams | `JIRF*`, `BlockReader`, `BPE`, `DynamicBuffer`, `IOSlave*` | 196 / 76 / 16 048 | none | yes |
| **U10** File family | `File`, `Path`, `Find`, `Entry`, `Time`(File::Time), `FileEx`, `MemoryFile`, `FileSub*` | 235 / 123 / 30 347 | subst/archive list (0x100adbc4-d4), `File::callback`, `Sep` | yes, after U7-U9 |
| **U11** platform core | `OS`, `Window`, `WindowPlacement` | 71 / 35 / 4 943 | `mainWindow`, placement statics | yes |
| **U12** diagnostics | `Exception`, `HardwareException`, `ExceptionHandler`, `Debugger`, `Log`, `LogClient`, `Console`, `Profile`, `QuickTimer`, `KernelStandardInit`, `AssertEx` | 169 / 53 / 13 247 | 40+ globals (Log, handler, profile) | yes (cycle with U13/U14) |
| **U13** thread/time | `Thread`, `ThreadLocalStorage`, `Time` | 59 / 27 / 2 874 | TLS singleton, `mainThreadId`, QPC freq | with U12 |
| **U14** foundation | `Alloc`, `PackedHeap(MP)`, `PoolBlockAlloc`, `ReferenceCount`, `PString`, `PStringCache`, `AnsiString`, `UnicodeString`, `MultiByteString`, `String`, DllMain/CRT glue | 352 / 127 / 29 365 | everything in G01-G03 | split (2026-10-26): **U14a** the string classes (269 exports, done; the empty block 0x100ae254 and the string heap 0x100adc18 used by address), **U14b** the rest + all forwarded data, one cut-over |

(Class-attributed bytes of all units: 199 036 of 292 521; the rest: JPEG 32.7 KB, unnamed helpers and CRT.)

### 4.4 Differential test per unit

Common harness (two patterns already in `src\jet\tests`): **(a)** in-process: load `KernelStandard_7c_orig.dll` and ours, call each export in
both through raw exports (thiscall emulated with `__fastcall`, hidden result pointer for `__cdecl` members returning by value; MODLOG
"Calling into the original modules") and compare results and object bytes; **(b)** same exe linked by name to
`KernelStandard_7c.dll`, run once in a directory with the original and once with ours, logs diffed (plugin_probe/task_probe).
Internal functions of orig can be reached by RVA (`diff_blowfish.cpp`).

* **U0**: `tools\menu_check.ps1` on the main menu with identical JetLog (ignoring "Packed Heap Summary" lines); verify the 22 vftables
  and 24 data exports resolve through forwarders (`dumpbin /imports` of Kernel_7c + loaded-module list).
* **U1**: bit-exact comparison of every function over random + edge floats (denormals, +-0, inf, NaN payloads, powers of two
  boundaries) 10^6 rounds each; `s_sqrtLUT` byte-equal (all 512 entries; regenerate procedurally and compare, don't copy);
  Crc over random buffers/sizes/seeds; Eigen3 on random symmetric matrices incl. repeated eigenvalues; OptimalRects: random
  invalidate/validate/process sequences comparing the returned `Rect` lists; layout static_asserts.
* **U2**: all 29 members vs orig on random matrices/quaternions/angles; compare as raw bits (expect exact because SSE2 scalar
  on both sides; the 507 x87 instructions (`_CIacos`, `fistp` conversions) are the places that may differ in the last bit);
  chained operations (Rotate*, Invert, Slerp, GetEulerAngles) and the ConfigData typed readers that call them.
* **U3**: `Color::Convert`/`HSVColor::Convert` exhaustively over all 2^24 RGB values; `ImageProcessing` LUTs over the whole
  parameter grid; node containers by operation scripts comparing pointer graphs; `Date` formatted output in en-US.
* **U4**: scripted multithreaded scenarios (handshakes via events so output is deterministic): Wait/Lock timeouts, return codes,
  abandoned mutex, semaphore counts, SWMRGuard reader/writer mixes, MultiLock WaitAll/WaitAny index results; **vftable compared slot by slot** as in task_probe; run TADDaemon-style construction (it derives these classes).
* **U5**: for 1000 seeds, 10^6 draws of every overload (`operator()` uint/int/int,int/float/float,float; `NextInt`, `NextFloat`, `Gaussian`)
  with `Time::frameLock` on and off; compare bit patterns. `Randomise` seeds from `timeGetTime`: compare via explicit `SetSeed`.
* **U6**: `DynamicLibrary` open/import real and bogus DLLs; Events add/remove/dispatch order with scripted observers; Job queue progress strings.
* **U7**: the corpus test. Extract (once, with the game's own loader) every config/kin/txt from the .ja archives; parse with orig and
  ours; dump the whole tree (names, types, values, order, line info) and diff; round-trip `Write` and `WriteBin/ReadBinFile`
  byte-for-byte; scanner level: token stream comparison on mutated/fuzzed inputs, malformed ones included (orig errors must be
  reproduced: "Syntax error at %s"); `Options`: scripted command lines/ini/env incl. quotes and `+/-` switches; all `Get` overloads.
* **U8**: the install's 8 443 .tga, 2 651 .bmp, 17 816 .jpg (+ synthetic .pcx): `GetInfo*` and `Load` results pixel-compared; JPEG decode
  allowed a small per-channel tolerance (IJL vs libjpeg-turbo) with exact match demanded for TGA/BMP/PCX; `Save/SaveJPG` -> reload; the blitter
  matrix over all format pairs x flags x random sizes; polygon/line/circle raster ops pixel-exact.
* **U9**: `JIRFFile` read the real chunk files used by trainz.exe/TrainzUtil, write-read-compare byte-for-byte; `DynamicBuffer`
  scripted read/write/seek; IOSlave family (note the original's LZSS-through-memory short-read bug; compare with and without).
* **U10**: path ops (`Make/SetExt/SplitPath/IsMatch...`) exhaustive on a generated path corpus; `Find` on a scratch tree (names, attributes,
  times, 64-bit sizes); `File` open modes/sharing/seek/commit; **FileSub**: mount the real Base.ja/Data.ja and compare
  directory listings, file sizes and full-content hashes of every entry (133 417 files in the install) read through both; subst and
  link operations in scripted sequences; `File::callback` progress calls.
* **U11**: Window creation/messages are hard to diff; use scripted message sequences to a hidden window on both builds
  (PostMessage log of `WndProc` callbacks, style/placement getters), and the in-game main menu run.
* **U12**: Log: identical JetLog.txt lines/prefixes for scripted `Log::Printf` calls under several flags; `Exception` objects
  (copy semantics: whole 0x1110 bytes compared); ExceptionHandler: deliberately raise access violation / divide by zero /
  stack overflow in a tiny probe exe under both builds and diff the crash log text (dump path excluded);
  `Profile` trace/readout formatting.
* **U13**: Thread start/priority/suspend/exit codes, TLS slot values per thread, `Time` frame lock arithmetic with scripted
  `IncFrame`.
* **U14**: stress test against orig: random alloc/free workloads (sizes around class boundaries 0xa0, 1 KB, tag bit patterns) under
  threads, byte-compare string operations over a corpus (all strings from the config corpus), PString interning (hash chains,
  refcount, `Unique`, `Read/Write` streams) comparing the full cache state; then the final cut-over: delete `_orig`, game
  menu, JetLog identical, `PackedHeap` summary ignored.

## 5. Risks and open questions

1. **Float exactness (Rand/LCRand/StdMath/Time::FrameLockMs/ConfigData numeric readers)**: MSVC8 x87 code evaluates expression
   chains in the CRT's 53-bit precision and rounds only when stored; SSE2 float ops round at each step. Where the original is x87
   (`Rand::operator()` forms, `Gaussian`, `FrameLockMs`, `ConfigData::operator uint`), reproduce with `double` arithmetic
   explicitly and verify bit-exact; if a mismatch appears the original is the spec (gameplay RNG sequences, replays).
2. **STL members in exported objects** (`PackedHeap`/`MP`, `File::Find`, `IOSlaveMemoryDynamic`, probably others in
   `FileSub*`): MSVC8 checked-iterator layouts; VS2026's STL differs. Rebuild with explicit hand-rolled containers of the same
   size; confirm each sizeof in the units that own them. The module also pulls 5 `msvcp80` symbols (`std::string`).
3. **Allocator and CRT-handle contract**: tag format, 4-byte offset and size classes are an ABI between ours and orig as long as both are
   loaded, and an alignment problem for x64 (blocks are 4 mod 8). Whether msvcr80 `malloc` equals `GetProcessHeap` must be checked before U14; CRT fds force msvcr80 for U10 (4.1).
4. **Hidden static state discovered only by data xrefs**: the null AnsiString buffer (0x100ae254 and its guard 0x100ae264), the
   string heap (0x100adc18), the PStringCache lock (0x100acb58), the `FileSub*` subst/archive list (0x100adbc4-d4), 0x100ae1e0 (used as a
   `LogClient*` by the helper at 0x10036cd0), 0x100ae250 (written only by `Rand`). Each unit's checklist must list the
   globals it reads/writes (the table is in `data\globals_touched.txt`).
5. **Flex scanner and parser tables** (G11): tables live in `.rdata`; regenerate from a reconstructed `.l`; any difference in
   error recovery or line counting changes parse results for slightly malformed add-ons (which ship). Corpus testing must
   include the install's *invalid* files if any (the game tolerates them).
6. **.ja archive format** (`FileSubArchive`): decode and document it before U10; it gates all test data and all add-on loading. Check for
   offset/size fields > 32 bits and for any stored pointers (none seen in the export surface, but not yet read in detail).
7. **ExceptionHandler is x86-specific** (EBP-chain stack dump, `CONTEXT.Eip`, `FS:[0]` frames, FPU stack check); the x64 version cannot be
   verified by diffing, only by behaviour tests (crash log contents, XCrashReport launch). Keep it in U12 but port last.
8. **`Window` uses a 32-bit pointer in extra window bytes** (`SetWindowLongA(h,0,this)`): hazard only on x64 but trivially testable
   by compiling the x64 build early.
9. **ijl10.dll replacement**: libjpeg-turbo output may not be bit-identical (IJL has its own IDCT/upsampler); needs per-channel
   tolerance and a visual check of textures (17 816 JPEGs). Progressive/CMYK/restart-marker behaviour of IJL vs libjpeg should be
   probed with the corpus; the module also accepts `.jpg` through `MemoryFile` and `GetInfoJPG`.
10. **`.jet` sections**: KernelStandard has two sections named `.jet` (RW 8 bytes `01 00 00 00 01 00 00 00`, RO 4 bytes holding the address of a stub
    at 0x100509a0) which no other Jet module has; no code reference found. Unknown purpose (a marker/tool data?); keep them in
    the link (`#pragma section`) until proven irrelevant.
11. **Export surface policy**: 1096 exports have no importer. Decide whether the final x64 module still exports them (cheap if
    source exists; needed only for code in the module itself). `StringTables*`, `MultiByteString`, `BlockReader`, `BPE`,
    `PoolBlockAlloc`, `SWMRGuard` look dead in this install: schedule them last or port literally without tests beyond layout.
12. **Name-decoration drift in x64**: all C++ exports change mangling on x64 (no `__thiscall/__cdecl`); the 32-bit drop-in keeps names,
    the final x64 stage rebuilds trainz.exe and all modules against the new headers, so it is a rebuild, not a drop-in.
13. **Class attribution is approximate**: ~87 KB of functions in namespace `Global` were attributed to classes by call/address heuristics; the
    "bytes" columns are estimates (see addrmap `notes\data\addrmap.txt`: code is laid out in per-class translation-unit order, so
    neighbours are reliable except across template instantiations). Several `Global` helpers are templates (`std::list`/`set`
    node allocators at 0x10028d20, 0x1000d200...) shared by classes.
14. Open: who inside trainz.exe reads `PackedColor` constants / `Console` (checked imports only; no behavioural trace);
    whether `Jobs` runs in the shipped game (one Jet module imports it); whether StringTables is loaded by any script path.

## 6. Recommended first unit

**U0 + U1.**
* U0: generate the KernelStandard proxy (all 1762 names, forwarders for the 113 data/vftable exports, `_orig` rename) and prove the menu
  and JetLog (the harness already exists). It also settles the `.jet` sections and data-forwarder question before any real code.
* U1 = `Crc`, `StdMath` (9 exports + `s_sqrtLUT` table), `Vector2`, `Vector3`, `Jet::{ILog2(int/float), IntLog2, CeilPow2, FloorPow2,
  GetBaryCentricWeights}`, `OptimalRects`, `Eigen3`: 35 exports (14 imported: 6 Jet modules + trainz.exe + 4 tools; the LUT itself by 10 modules), ~4.3 KB, no globals except the
  2 KB table, a tiny public surface, everything bit-testable against the original in one process. It exercises the new pipeline
  (class layouts via `static_assert`, mangled-name linking against `implib\KernelStandard_7c.lib`, forwarding of the rest), and the
  riskiest cross-cutting point (inline readers of `s_sqrtLUT` in 10 modules) with a 2 KB equality check.
* Then U2, U3, U4, U5 in any order (all leaf), then U7-U9, then U10, then U11-U14.
* x64 side: `build64` already links Kernel_7c against a stand-in for the 95 KernelStandard functions it imports
  (MODLOG "Kernel_7c on x64"). Each reconstructed unit can replace its part of the stand-in, and the x64 task and
  EditItem probes plus `golden_kernel` must keep matching their 32-bit references: a free x64 regression test for
  PString, Options, sync, File::Path and ReferenceCount. The stand-in's provisional `JET_OPAQUE_SIZE` x64 sizes
  (OS, Events, File::Find, IOSlaveMemory) get replaced by the real layouts as their units are done.

## U6 done (2026-10-21): Platform.cpp, 66 exports
- DynamicLibrary vftable: deleting dtor, Open, **Close, Import** (the header had Import before Close; Kernel_7c
  called by name, so it never mattered). GetModuleNameAndAddress and SharedMemory::TryExclusiveLock are one folded
  `return false` stub (0x10014fe0); SharedMemory::Unlock and Job::Process share a bare `ret` (0x1002abe0).
- Events (0x2c): {bool owns, lastId, removed, queue, requeue}, each list {head, tail, count} of Event* linked
  through Event+4 (links point at links). Update: deletes `removed`; then while the queue head is due (nextTime
  <= time): virtual Update(); if the event is still queued it leaves the queue: finished -> deleted when owned
  (else dropped), not finished -> `requeue`; after the pass `requeue` is sorted back (before the first with
  nextTime >= its own). Remove(id) searches queue then requeue, owned ones go to `removed` (deleted next Update).
  Event::Update (base) adds the interval and returns true (finished). Copies of Event and Job copy the links too.
- Jobs (0xa8): worker thread built inline (= Thread(fn, this, true)), priority 1; waits on {queued semaphore,
  thread} through an inlined MultiLock; FIFO; a processed job whose children are not all dispatched goes to a
  pending list that is only rescanned after the next processed job. Update waits 1 ms for the finished
  semaphore and 5 ms for the lock, then dispatches and deletes every finished job. "JobsConsole" draws three
  text consoles into 0xb0000 (DOS mono text memory: debug only).
- SharedScratchPad: per thread, ReferenceCount base (DeleteThis still the original's), array append on create,
  find+remove on destroy, both under the static CriticalSection.

## U7a done (2026-10-21): Options.cpp, 41 exports
- Entry (0x18): {next, prev, key (lower case), value, separators, cursor}. Find lower-cases the query on the
  stack and strcmp's; keys are lower-cased in place when the entry is made.
- ProcessRaw per token: optional switch / - +, key up to space = - +, then `= value` (quoted to the next quote
  of either kind; unquoted to the whitespace before the next switch, trailing whitespace dropped), or `-` / `+`
  right after the key (no space). Key and value are cleared for every token. A token without a key is dropped.
  `+key...` on a non-empty value appends "," and the value (a missing value appends just ","), else replaced.
- Get(bool): missing -> fallback (false); no value -> !fallback; "-..."/false/off/0 false; "+..."/true/on/1
  true; else !fallback. GetNext(bool): the run lower-cased; false/off/true/on, else strtol != 0.
- Typed Gets copy fallbacks through fld/fstp (SNaN quieted). GetNext(Quaternion) reads three angles through
  Quaternion::Set. Range "a:b" needs a non-empty b; strchr on a key with no value crashes (both).
- GetNext's cursor points into the value it was set on: after a ProcessX that replaces the value it dangles.
  ProcessFile terminates its buffer with one zero byte, so wcstombs (UTF-16 files) reads one wide char of heap
  garbage past the end unless the file ends with a wide NUL.
- CRT: atof/atoi/strtol/strtoul/isspace/_strlwr/_stricmp/wcstombs/getenv/printf are msvcr80's
  (src/jet/KernelStandard_7c/Crt80.h). msvcr80 is loaded side-by-side, so GetModuleHandle("msvcr80.dll") fails:
  Crt80 finds it by enumerating modules (Math2's _CIacos lookup had the same bug and fell back to std::acos).
- File is 0x13c bytes (+0x38 path, MAX_PATH): the header said 0x3c, so File locals (ours in InputSystem's
  Force.cpp too) were overrun by the original's Open/Close.
- Declarations must mangle exactly: `char* argv[]` is QAPAD; `char* const*` resolved silently to the original's
  thunk through the import library. tools/check_thunks.py finds such exports.

## U7b1 done (2026-10-21): ConfigData.cpp, 139 exports (the data model)
- Node (0x10, from the original's PoolBlockAlloc: its ??2 / ??3 stay forwarded, and so does `unknownItem`):
  {PString name, int type, void* data, FastDynamicArray<ConfigData*,13> children}. Types: 0 none, 1 int,
  2 float (bits in data), 3 PString (the node pointer is the data), 4 bytes, 5 ints, 6 floats, 7 PStrings; the
  arrays are one Alloc block {count, items}. A type-3 node may have a null data (an empty string).
- Copy constructor, operator= and CopyData copy ints and floats raw (an earlier reading said the copy
  constructor left them 0: wrong, eax is loaded before the type-7 jne).
- Find / Count / Remove match names by node pointer (interned). Find(key, value) and Remove(key, value) want
  type 3 and `data == value.node`. Remove(child, n) matches type, name and the data word, not the contents.
  Replace(key, n) makes the missing nodes up to n and returns the last.
- CompareData: type 0 counts as 3; strings by node; arrays by count and contents (type 7 item nodes).
- CalcCRC: name with its NUL, then the value (strings with NUL, array blocks with their count, floats as their
  FloatText, ints as the 4 data bytes), then the children except those named `skip` (when skip has text).
- FloatText: msvcr80 `_gcvt(f, 8)` into a static buffer, "5." -> "5.0".
- AddPStringArray copies the old strings with a reference each and frees the old block without releasing
  them: one reference per old string leaks (kept).
- Conversions to Vector/Point/Size/Rect/Color/Quaternion and operator>> into Range read the block without a
  check: garbage (or a crash) for a node without an array. Ours returns zeros there; the test does not draw it.
- operator Quaternion: three items go through Quaternion::Set (Euler); otherwise the x87 sums the squares
  (fld/fmul/faddp, so the NaN of two NaNs is the larger payload), fsqrt, then SSE divss 1/len and mulss with
  the reciprocal first. Done with inline asm + intrinsics to match NaNs bit for bit. Quaternion::Set (U1)
  still differs in NaN payloads only (as accepted in diff_ks_math2).
- operator>>(bool): int != 0; none false; string "1" or "true" in any case (tolower: ASCII by hand, others
  through msvcr80); floats and arrays leave the bool alone.
- Header fixes: AnsiString() is the original's export (data = the shared empty buffer at 0x100ae258, its
  refcount at -4), not an inline null; UnicodeString {text, capacity, length} declared; the original's
  wide-string exports use native wchar_t (`_W`) while the tree builds with /Zc:wchar_t-, so those
  declarations take `__wchar_t`.
- Test: tests/diff_ks_configdata.cpp (trees built by each side through raw exports, every op followed by a
  tree dump and Crc, reference counts of the test's strings checked at the end).

## U7b2 done (2026-10-21): ConfigWrite.cpp, 22 exports (text and binary)
- ConfigData::Writer: {vftable}; slots: deleting destructor (base vftable 0x10053d60), Write(data, size, count)
  -> items written, Encoding() (base 0; 2 = UTF-16). The module's writers: file {vftable 0x10053dd0, File*}
  (File::Write; Encoding = the File's +0x34 field) and string {0x10053de0, PString*} (PStringCache::Ins of the
  data at the string's end, up to its NUL whatever the size; Encoding 1). An encoding argument of 0 means "ask
  the writer".
- Text: WriteChildList writes each child and "\r\n", with a blank line before a block and after one. An item is
  "name = value;" (tabs: two spaces a level, at most 16 per call); no name and no value writes nothing; no name
  drops " = " for arrays only. Arrays: "(x)" for one item, "a,b,c" for more, "//count" after ";" from ten on;
  bytes as ": " then 32 upper-case hex pairs a line, each line "\r\n" + 4 spaces. WriteWrap breaks the line when
  the column passes 100 and is 50 past the indent (column after " = " minus 1). A block is "name(value)\r\n{",
  the children one level deeper, "}". Floats print as FloatText (strncpy'd, 0x30 max).
- FormatDataString: ` \ \b \t \r \n " get a backslash (b t r n for the controls) and the whole is quoted;
  otherwise it is quoted when it starts with a digit or holds one of # / space , ; = ( ) { }.
- WriteChars writes nothing and fails for a length <= 0 (an empty name or string item fails the whole Write);
  UTF-16 through msvcr80 mbstowcs (a null text with length -1, i.e. a null PString-array item, ends the
  process in the CRT's invalid-parameter handler: both modules).
- Binary: "CD" (0x4443), version 0x101, then per node: name (WritePString: u32 length | 0x40000000 when
  UTF-16, then the bytes), one type byte, the value (int/float 4 bytes, string, count + strings, or the array
  block with its count), u32 child count, the children. ReadBin(File&) checks both words, ClearData()s (the
  children stay: new ones are appended) and reads; a type-3 string read as null becomes "" ; children count is
  read as Read(p, 1, 4); a failure in a PString array frees the items read so far.
- WriteBin(path, flag) opens with (flag ? 2 : 1) | 0x100; ReadBin(path) with 0x10. Open(base, path) takes a
  subst drive name as base (File::Subst), not a directory.
- Original File/PString behaviour met by the test (not ConfigData's): a field cut part way counts as read
  (heap or stack bytes stay in it), and a string whose text is cut overruns the heap; WriteBin to a PString
  reads past each binary field into the stack (the string writer copies up to a NUL).
- Header: AnsiString(const char*) (null makes the empty buffer, Size() 0) declared; the code builds
  AnsiStrings where the original does and passes them as String&.

## U7c1 done (2026-10-21): ConfigParser.cpp + ConfigLexer.cpp, 23 exports
- ConfigParser (0x18): {vftable, ConfigLexer* core +4, PString fileName +8, ConfigData* root +0xc, float version
  +0x10 (left unset by the default constructor; 1.0 with a given tree), AnsiString key +0x14 (MemoryFile::Open's
  resource key)}. Copy and assignment are shallow (core and root shared). The destructor deletes both.
  `results` and the vftable stay the original's exports: nobody imports them.
- Parse(path) / Parse(base, path) (a subst drive; the scanner's name becomes "base:path") / Parse(MemoryFile&) /
  Parse(name, data, size): the file name kept, File opened (2 if not), MemoryFile::Open (3 if not), then the
  protected Parse: a new scanner, the root replaced by a node named after the file, the parse: 0 or 3.
- Save(base?, path, flag, enc): (flag ? 2 : 1) | 0x100; WriteData writes "version %1.2f\r\n" (UTF-16 with a BOM
  for encoding 2, through AnsiString::GetUnicode into 63 wide chars) then ConfigData::Write. SaveString is
  ConfigData::Write to the PString alone (no version line).
- Scanner: flex 2.5.4 C++ skeleton (REJECT-style state buffer, start-condition stack, yylineno), 81 rules. Start
  conditions: 0 header (comments, "ver..."), 1 block comment, 2 the body, 3 a bare string (until = ; { ( ) or a
  newline), 4 a quoted string (escapes \" \n \r \t \b, any other \x is x), 5 hex bytes. Token values are
  Jet::Alloc copies of the text; a quoted string's quotes are cut. Errors: -1 (an unfinished string) and -2 (any
  other character); LexerError prints to msvcr80's stderr and exit(2)s.
- Parser: old Lemon (2000-era lempar.c: hash-table states, yy_find_parser_action / yy_reduce / error recovery
  with the `error` symbol). Tables extracted at build time: 261 action entries (0x100735a0, links resolved to
  indices), 65 states (0x10073dc8), 34 rules {lhs, nrhs} (0x10074148), hex digit tables (0x10074444 lo,
  0x1007447c hi). Error messages go to a vsprintf buffer that nobody reads (logging compiled out).
- Rule actions build ConfigData nodes from the pool: name = value, name(value) { ... }, single numbers (int via
  msvcr80 strtol base 0, float via atof), the special floats FLOATIND/INF/QNAN (0x7fe00000/0x7f800000/0x7fc00000,
  negative forms when atof < 0 or NaN), hex bytes, joined string pieces, and data arrays. An array's kind is its
  first item's unless the first item of another kind is a string (strings: numbers become "%d" / "%f") or a float
  (floats); the scan stops at that item.
- Original behaviour kept: a backslash outside quotes in the body loops forever (rule 29 puts it back and enters
  the bare-string state, rule 43 puts it back again and returns an empty STRING, which the grammar joins without
  end), in both modules. The test does not draw such texts.
- Found on the way: ConfigData::Write (U7b2) wrapped a string array's items one column short (the item wraps to
  the column, only the comma before it to one less). Fixed; caught by this test's SaveString/Save comparison.
- Test: tests/diff_ks_configparser.cpp (trees written by the original's Write behind a version line, damaged
  copies, token soup; result, version, tree, file name, SaveString, Save in both encodings, re-parse through every
  overload, copies). KSP_WATCH=<s> prints where a scenario hangs.
- Scanner tables (extracted at build time): yy_accept 0x100526c0, yy_acclist 0x10052360, yy_ec 0x10052860 (chars
  >= 0x7f read as 0x7e), yy_meta 0x100528e0 (ints), yy_base 0x10052990, yy_def 0x10052b58, yy_nxt 0x10052d20,
  yy_chk 0x10053510; jam state 0xcb. Original yylex 0x10017530, driver 0x1001a670, ConfigParserCore vftable
  0x100542a4, scfgFlexLexer vftable 0x1005416c.

## U7c2 done (2026-10-21): StringTables.cpp, 35 exports (StringTablesBase, StringTablesDefaultTraits)
- StringTablesBase (0x20): {vftable, keyCharSize +4, bool namesResolving +8, AnsiString +0xc, UnicodeString +0x10
  {text, capacity, length}, AnsiString +0x1c}. The constructor leaves +4 and +8 unset (the derived class sets them).
  Copy / assignment copy all members (the UnicodeString through Set(text, length, length)). The deleting
  destructor frees with Jet::Alloc. Header: include/jet/StringTables.h.
- vftable 0x1005607c, 20 slots, overloads in reverse declaration order: 0 deleting dtor; 1-4 AddConfigData
  (ConfigData&, bool) / (void*, size, bool) / (path, bool) / (PString base, path, bool); 5-8 AddBin (DynamicBuffer&)
  / (File&) / (path) / (base, path); 9 pure SaveConfigData(ConfigData& root, flags); 10-11 SaveConfigData (path) /
  (base, path); 12 pure SaveBin(DynamicBuffer&, flags); 13-15 SaveBin (File&) / (path) / (base, path); 16 pure
  AddTable(const char* name, long id) -> void* table (binary); 17 pure AddTable(const String& name, unsigned id)
  (config); 18 pure AddEntry(table, const char* name, long id, DynamicBuffer& data, bool) (binary); 19 pure
  AddEntry(table, const String& name, unsigned id, const ConfigData& node, bool) (config; its result is ignored).
  The pure slots are named from how the base calls them (nobody in this install derives from the class).
- The base + path forms make "base:path" with strncpy / strncat (260, as the original: a long base overflows) when
  the base has text, else use the path. SaveConfigData needs namesResolving (else 2).
- Config form: AddConfigData(path) parses with a ConfigParser (Parse 2 -> 3, other errors -> 8, no root -> 0);
  (data, size) parses the buffer named "StringTableBuffer" (any error -> 8). ScanConfigData walks the top-level
  nodes with an empty AnsiString path: a node's name is appended; a valueless node is a group ('.' appended, its
  children walked); a node whose value is TABLE_TOKEN ("Table") with a non-zero ID_TOKEN ("Id") child is a table:
  AddTable(path or an empty name when names are not resolved, id), null -> 8, then each child through
  ScanTableConfigData with a fresh entry path, where ENTRY_TOKEN ("Entry") nodes with a non-zero Id are entries
  (AddEntry with the entry path relative to the table, or an empty name). The path is cut back (MakeBufferUnique
  to (len + 0x11) & ~0xf, length, NUL) only when the node succeeds. A missing / zero Id is 8. VALUE_TOKEN
  ("Value") is the derived class's business.
- Binary form: header 8 bytes {'S', 'T', u16 0x101, names flag, key char size, 2 unused}, copied inline (a buffer
  shorter than 8 leaves the rest as it was on the stack). With names, namesResolving needs the same char size;
  without names, namesResolving fails (8). Then while 12 bytes are left: "TABL" (fileTblId), long id, [name],
  long count, per entry long id, [name], then AddEntry reads the data. A name is a long length (more than what is
  left -> 8) and the bytes: resolved into one growing buffer (a longer name allocates length + 0x200 without
  freeing the old one, as the original), followed by keyCharSize NULs; skipped with Seek when not resolved.
- AddBin(path): File::Open(path, 0x10) (2 -> 4, 7 -> 9, other -> 3), AddBin(File&), Close; a failed Close throws
  FileEx(StringTables.cpp, 0xa8) which is caught at once (the result stays). AddBin(File&): the whole file into a
  DynamicBuffer {data, pos, capacity = size, size, 0x200, external} (a short read throws FileEx 0xc2, caught: 6).
  SaveBin(path): Open(path, 0x101) (9 -> 10, other -> 5), SaveBin(File&), Close (failure thrown, caught: 7).
  SaveBin(File&): a 32 KB DynamicBuffer filled by the pure SaveBin, one File::Write (short: FileEx 0x1e0, 7).
  SaveConfigData(path): a pool node named by the path, the pure SaveConfigData (failure: returned, the node left
  allocated), then ConfigParser(root).Save(0, path, false, 0): 0 or 7.
- CreateNode(parent, "a.b.c"): each part (strchr '.', the PString cut to the part) the first child of that name, a
  new pool node appended when there is none (or the first match is the missing-key node).
- Traits: NewKey / NewKeyFromCharPtr Jet::Alloc strdup, DeleteKey, GetHashVal h * 31 + (signed char)c, Compare
  -1/0/1 over unsigned bytes, DataToConfig = ConfigData::SetUnicode (inlined in the original), DataFromConfig =
  GetUnicode, DataTo/FromDynamicBuffer = DynamicBuffer::Write/Read(UnicodeString) (u32 bytes | 0x40000000 for
  UTF-16, then the bytes). LogErr / LogWarn are empty (one shared 0x1002abe0 in the original).
- Data exports stay the original's: fileTblId "TABL", results, the four token PStrings (its static initialiser
  fills them). FileEx is the original's class (thrown with our ThrowInfo; matched by name).
- Original-only garbage, not compared by the test: DynamicBuffer::Read(long&) succeeds when only 1-3 bytes are
  left and leaves the rest of the value as it was (the stack): a binary table whose count, entry id or name length
  is cut mid-field. The test emulates the reader (PartialAt) and does not compare what follows such a read.
- Test: tests/diff_ks_stringtables.cpp: per side a table object with that side's vftable and the pure slots
  replaced by logging test functions (SaveConfigData / SaveBin build the model with the side's CreateNode and
  traits). Inputs: a model of tables / entries (dotted names, special ids, UTF-16 texts), its config tree with noise
  (groups, wrong value kinds, missing / zero / string ids, the missing-key node as a child), its text (the
  original's ConfigParser::Save, sometimes cut or prefixed with junk), its binary image (cut at >= 8 bytes, a byte
  flipped, a word overwritten, junk appended). Every overload, the saved outputs read back, the Scan functions with
  a non-empty path, copy / assignment / deleting destructor, CreateNode, the traits. KST_SWAP, KST_DUMP,
  KST_TRACE, KST_ONLY=<n>.
- Test-only find: MSVC evaluates the operands of `a + f(x) + g(x)` in no fixed order (it read the buffer before the
  call that filled it); calls are sequenced before the log line is built.

## U9 done (2026-10-22): Streams.cpp, IOSlave.cpp, BlockReader.cpp, JIRF.cpp, 195 exports (763/1762 ours)
- U9a DynamicBuffer (51 exports, 0x1002e120..0x1002ebaf): {data, position, capacity, size, increment (0x200 when
  0), bool external}, 0x18. Writes at the position (AddLength grows the capacity to a multiple of the increment past
  position + n, copying the old capacity's bytes; external buffers refuse writes) and move the size along; reads take
  what is left of the size and succeed when any byte came (a 4-byte read of 1-3 bytes leaves the rest of the value);
  Read32 zeroes first, Read(bool&) reads a long. Write(ptr, size, count) returns `size`, Seek and SetSize clip to the
  capacity, GetLength is the capacity. The strings' binary Read / Write go through a BinReader / BinWriter
  {vftable, buffer} whose slot 1 moves bytes (format: u32 byte count | 0x40000000 for UTF-16, then the bytes).
  The default constructor closure is exported through Jet_DynamicBufferDefaultCtor. StringTables now uses the class.
- U9b IOSlave family (vftable: deleting dtor, Seek, Tell, GetSize, ReadData, WriteData; Seek and Tell pure in the
  base): IOSlaveMemory {data, position, size}; IOSlaveMemoryDynamic over an MSVC8 std::vector<char> (msvcr80's
  operator new, growth capacity + capacity / 2; an empty write at the end is an invalid parameter, as the checked
  vector makes it); IOSlaveFile {File* (a closed file counts as none), size grown by writes}; IOSlaveReadCursor
  {slave, stream position, buffer, offset, filled, capacity} (Access moves the unread bytes to the front or into a
  buffer of exactly offset + n, then fills from the slave; a failure drops the slave; operator=(IOSlave*) leaks the
  old buffer); IOSlaveWriteCursor {slave, two history buffers of `chunk`, fills, current} (ReadData from the
  history or around the slave's position; WriteCopy writes an overlapping copy from one read); IOSlaveResource
  {slave, base, AnsiString key, write, version, key word, position, size}: a 12-byte header {1, key word ^ crc ^
  size, size} (the key word from QueryPerformanceCounter, the CRC of the key text), every byte scrambled by its
  position (CheckWrite / CheckRead: rotations by signed amounts, pos / 3, 5, 11, 23, 509, 7919), 4 KB padding of
  timeGetTime-based bytes when a written stream goes; an empty key passes through.
- BlockReader {File*, line, token, cursor, buffer size 0x200, PString name, parent, child, ended, pending}: "name
  ... { ... }" text through File::ReadLine (which puts no NUL after the line: scans run into what earlier lines
  left). Next skips blank / comment lines and whole blocks, ends at a "}" line, leaves a "{" pending; Begin makes a
  child sharing the buffers, End skips the rest of its block. Reads use msvcr80's sscanf "%s", strtol base 0, strtod.
- BPE: Philip Gage's byte pair encoding with a 0x3a04-byte workspace (blocks of 0x800 bytes / 210 distinct values,
  hash (right << 5 ^ left) & 0xfff); codes are taken from 254 down (255 is never used); output past the input size
  falls back to a copy. Decomp keeps the original frame layout (pair stack next to the identity table).
- U9c JIRFFile : File {+0x13c IOSlaveFile*, +0x140 IOSlaveResource*} (OpenJIRF: write when a bit of mode & 0xf is
  set; earlier slaves are not freed), header {"JIRF", size, id}, blocks {id, size, data}, padded strings {length + 1
  (-1 empty, 0 none), text, zeros to 4}; JIRFWriteHeader / JIRFWriteBlock patch the size when they go. The copy
  copies the File as MSVC8 does (fields, two flag bits of +0x28, the path through Path::operator=). Reads of a short
  stream leave stack garbage (counts, ids, text) in both.
- Tests: diff_ks_dynbuf (random operation sequences on every constructor; bytes no write set are tracked and not
  compared), diff_ks_ioslave (every slave, the cursors, the resource stream read from the original's output and
  written with the key word set by the test, BlockReader over random block text with its buffers zeroed first,
  BPE), diff_ks_jirf (a chunk tree written by each side and by the original, read back along the tree).
  Mutations: 10/10, 12/15 + 3 equivalent, 7/8 + 1 equivalent.

## U8 done (2026-10-22): Bitmap.cpp, BitmapIO.cpp, 98 exports (861/1762 ours)
- Layout = the Jet SDK's Bitmap: {Size, palette, data, pitch, bytes per pixel, format (INDEX, RGB, ARGB, XRGB,
  RGB565, UNSPECIFIED), owned, 8 primitives, data size}, 0x44. The primitives {GetPixel (to 32-bit), SetPixel,
  BlendPixel, BlitSelf, BlitRGB565, BlitRGB888, BlitARGB888, BlendARGB888} are set by Init per format (Done: all a
  `ret`, format 5; Done does not reset the bytes per pixel). Init refuses sizes over 0x1000, h * pitch or numBytes
  over 16 MB; the loaders inline a copy of Init for XRGB/ARGB without the 16 MB check.
- The primitives (0x100079a0..0x10007ff0) are hand-written assembly, rewritten register by register where it
  matters: the 32/24-bit blends add the high bytes of c*w and d*(-w) in 8 bits (w = a + 1, or (a + 1 + 255 - da)
  / 2 with a new dest alpha of byte 1 of -((255 - a) * (255 - da)) when the destination is not opaque); the 24-bit
  fill blend sums the three channels in one 32-bit add (carries cross channels); the 565 blends keep w (or w ^ 0xff)
  in the low word of the sum. Counts are dec / jne loops (0 = 2^32) except the rep forms. The copies are rep movs
  element by element: Image's tiling relies on an overlapping copy repeating the pattern (memmove does not).
- Bugs kept: BlendPixel is Pixel (no blending); Box/Line/Circle/Triangle/Pixel trust the clip rectangle to lie
  inside the bitmap; Image reads the source past its area when the destination is clipped on the left or top;
  CopyImage with a negative source x/y moves the destination right/down without shrinking the width; the tiled
  Image draws whole tiles only up to clip.x + srcArea.w and copies the rest of each row from one tile to the left;
  Triangle's scan table (alloca, height + 1 rows) overflows when the clip cuts the top vertex off and when an
  antialiased triangle has a flat bottom; MapPolygon draws a row at the left end's rounded y (float drift can reach
  the row past the bitmap) and the span's right end inclusively; Crop finds source rows by width (not pitch) and
  packs its rows; the 24-bit ScaleArbitrary writes rows pitch / 3 * 3 bytes apart; ScaleArbitrary shrinking a 24-bit
  bitmap returns it unwritten; GrayScale with an empty width reads 0 pixels per row (2^32 in the 16/24-bit
  primitives); Colorize uses (a * c + 127) / 255 (a static 256-aligned table).
- Floating point: SSE single precision as in the original (ClipLine, MapPolygon, ScaleArbitrary, SetTransparency;
  MapPolygon's edge ends floor / ceil in double); Circle's antialiasing is x87 double (sqrt(x^2 + y^2) + 0.5 - r,
  truncated): equal at the default 53-bit precision control.
- Files (through the original's File / MemoryFile, U10): TGA types 1 (256 x 24-bit map, 8-bit) and 2 (16 as 555,
  24, 32 = ARGB), descriptor bit 4 refused, bit 5 top-down (kept as a negative height while reading); BMP 8/24/32,
  width a multiple of 4, rows always bottom-up (the sign of the height is lost); PCX 8-bit RLE with the end palette
  (as colours the run length is the whole byte, 192..255: kept); JPEG through ijl10.dll (ordinals 2/3/4,
  JPEG_PROPERTIES by offset, RGBA_FPX then swapped to 0xffRRGGBB). Short files: the original reads headers and
  palettes into uninitialised buffers and keeps the last byte on a short read (ours: zeros); kept-index TGAs
  stored bottom-up all land on row 0. GetInfo PCX gives xmax - xmin (no + 1).
- Save: TGA (top-down, 24 or 32 bits), BMP (bottom-up; XRGB as 24-bit), JPEG: the original's static IJG 6b encoder
  with both quantisation tables scaled by 0 (every quantiser 1), RGB rows. Ours: libjpeg-turbo 3.2.0 (vcpkg
  x86-windows-static, jpeg_set_linear_quality(0)) gives the same bytes.
- Tests: diff_ks_bitmap_draw (random formats, sizes, pixels and operation lists; every header and pixel row after
  each operation; KBD_TRACE / KBD_SKIP / KBD_HEAPCHECK for bisecting) and diff_ks_bitmap_io (synthetic TGA / BMP /
  PCX / garbage / cut files, alpha files, save and reload, and the install's images: KBI_ALL for all of them).

## U10a done (2026-10-25): FilePath.cpp, File.cpp, MemoryFile.cpp, 187 exports (1048/1762 ours)
- U10 was split: U10a is everything a File object and a path need (Path 50 of 52, Time, Entry, FileEx, File,
  MemoryFile); U10b keeps the substitute system: Path::GetSubst, the path statics that consult it (Exists,
  ExistsIgnoreCase, IsDirectory, Unlink, Remove, Rename, Md, Prune, TrueName, SetReadonly, CheckWriteMode,
  GetStamp(path), Subst, SubstExists, SubstLink, GetSubstBuildVersion / ProductCode), Find, FileSub*, File::Sep,
  File::callback and SetCallBack (only the original's FileSubArchive reads the callback).
- Of the original's own code that is left, only Log (JetLog.txt, a plain path) still calls File internally, so
  nothing in the original needed patching.
- File {fd, cur, buffer (16 KB), count, mode, size (-1 unknown), dataStart, position, rawPos, dirty, flags (bit 0
  own buffer, bit 1 descriptor owned by a FileSub), FileSubstitute*, FileSub*, encoding, Path}, 0x13c. Descriptors
  are msvcr80's (Crt80.h: _open/_sopen are exported as C++ overloads), because the original's FileSubs open files
  into our objects. Mode: 0x0f write (2 = BPE "CFIL" output), 0xf0 read (0x10 auto, 0x20 plain, 0x30 compressed),
  0xf00 create (0x100 truncate, 0x200 append, 0x300 new), 0x1000 refused. Results: 1 read error, 2 cannot open,
  3 already open, 4 bad mode, 5 compressed for writing, 6 not compressed, 8 exists.
- "CFIL" files: {"CFIL", version 0x101, size} then chunks of 16 KB, each a u16 header (0x8000 | n raw, else the
  BPE size) and the data; version 0x101 archives store only the 4-byte magic. Writing goes plain to the file and
  Close packs it into a hidden "%08X.T%08X" temp file (pid, counter) that replaces it. The counter is the
  original's (0x100adbc4, shared with its FileSub temp files until U10b).
- Open "name:path" with a name of two or more characters looks the name up in the original's subst list (head
  0x100741c8, lock 0x100741d8, sorted by PString node) and lets 0x10029b60 / 0x10029c00 (__stdcall, ret 14h) open
  the file through the substitute's FileSubs; FileSub virtuals are __cdecl with the object first (+0x10 archive
  version, +0x20 close, +0x4c / +0x50 lock / unlock the shared descriptor).
- Bugs kept: SafeRead / SafeWrite compare the items moved with the item size (not the count); a failed read
  sets size to 0; Open returning 5 or 6 closes the descriptor but leaves it in the object; the typed Read / Write
  move sizeof bytes as one item of size 1 (a short read is false); Seek of a compressed file with nothing buffered
  takes the old position as the buffered chunk's start; ReadBlock / WriteBlock / Size / GetStamp on a closed file
  pass -1 to msvcr80 (its invalid-parameter handler ends the process); Path::Normalize("") and some
  MakeRelativeTo inputs (same directory, a base with no name) read address 0 in the original (ours returns);
  Path::DelPath's memmove copies len + 1 bytes from the name (reads past the string); DelName(true) also cuts the
  separator; DelFileName keeps the directory and the extension; SetExt has no length check.
- MemoryFile {cur, start, size, owned, AnsiString name}, 0x14. Open(data, size, unicode, encoding) meant to
  convert UTF-16 but clears the first character before wcstombs (it indexes with the size it has just zeroed), so
  it always ends up plain over the caller's data (the size without a byte-order mark) and leaks the conversion
  buffer. ReadData with no mark seeks the file to 0, not back where it was. Read(PString&) is {u32 length,
  bit 30 UTF-16} and releases the old node the inlined way (no Destroy).
- Exception is 0x1110 bytes (the header said 0x110c: StringTables' throw FileEx wrote 4 bytes past the object);
  FileEx vftable {deleting destructor, Class "File", IsHardwareException, Description, Reason}, the last three the
  original's.
- Test: diff_ks_file (paths over a random corpus with the whole 0x104-byte buffer compared, Time, Entry, File
  scripts over every mode around the 16 KB edges, compressed files written and sought, a directory and a new .ja
  archive through the original's substitutes, MemoryFile). KFD_TRACE / KFD_CONTEXT / KFD_SWAP / KFD_DUMP.

## U10b preparation: the .ja archive ("ARCH") as far as read (2026-10-25)
- All ten .ja files of the install (Base.ja, Data.ja, Builtin\ts12a..h.ja, up to 1.68 GB: every offset fits 31
  bits) are FileSubArchive containers: "ARCH", then 8-byte chunks {id, u32 size}: "INFO" (0x20 bytes: 'ARC0'
  0x41524330, version 0x101 or 0x102 (others refused), then u32 fields, Base.ja: 0x8d6, 0x03bf9a6c, 0xefcc, 0...),
  one "FIL " per file {u32 size = 0x120 + data, File::Entry (0x120: name, sizes, crc, attrib 0x80 compressed),
  data (a "CFIL" stream when compressed)} and an index ("IDX " string at 0x100554b8) read by 0x10027010 into an
  STLport-style tree in PackedHeapMP 0x100ae180 (not exported: ours can keep its own).
- FileSubArchive (0x278 bytes) also writes: new archives get "ARCH" + INFO; a file opened for writing goes to a
  hidden temp file and Close (vf8) appends a "FIL " record (BPE chunks for mode 2, the CRC of the plain data,
  File::callback progress), the directory being rewritten at destruction (0x10027330). Per-thread read cursors
  (vf7, 0x100268c0) share the archive's descriptor under two CriticalSections.

## U10b part 1 done (2026-10-25): FileSub.cpp, FileFind.cpp, 42 exports (1090/1762 ours)
- The substitute list is ours: substitutes {next, FileSubs, count, PString name} sorted by the name's node pointer,
  one lock. Subst(name, path) puts a FileSubPath (path ends in '\' or '/') or an archive first; SubstLink puts a
  FileSubLink last; Subst(name, null) clears every link to it (FileSub slot 1 in every substitute) and deletes the
  substitute with its FileSubs. At exit everything is deleted (an archive written to rewrites its directory).
  Subst with "" reads path[-1] in the original (crash): not tested.
- The archives are still the original's objects: made with its constructor 0x100252f0 (__thiscall on the path,
  object and flag on the stack, ret 8; called as __fastcall), used through their vftable, and File::Find calls
  their read cursors directly: 0x100268c0 (archive in esi, returns the cursor), 0x100269f0 (archive edi, cursor
  edx), 0x10025b10 (pattern ecx, archive edx, then cursor, Entry*, 1, 0 on the stack, cdecl). Those, File::Sep,
  File::callback, SetCallBack, SetDefaultBuildVersion / ProductCode, Find::ClosedHandle and the temp-name counter
  0x100adbc4 stay the original's until FileSubArchive is ported (U10b part 2).
- FileSub vftable (slot: base / directory / link / archive): 0 deleting dtor (thiscall), then __cdecl with the object
  first: 1 Forget(name) (link: drop a target of that name), 2 IsReadOnly (true / bit 0 / false / bit 0 of +0x25c),
  3 IsReadable (false / bit 1 "exists" / target set / fd != -1), 4 Version (0 / 0x102 / 0 / +0x238), 5 IsType(t)
  (archive 0, directory 1, link 2), 6 OpenWrite, 7 OpenRead, 8 Close (base throws "CloseFile() Logic Error"),
  9 Exists(PString* base, path), 10 IsDirectory, 11 ExistsAs (base: *found = path, then slot 9), 12 Unlink,
  13 Rename, 14 Md (base true), 15 TrueName, 16 GetStamp, 17 SetReadonly, 18 Collect (directory and archive add
  themselves, a link its target's FileSubs), 19/20 Lock/Unlock, 21 IsPath (strcmp with the directory / archive path).
- The list walks: write = first FileSub not read-only that opens it; read = first readable one; Exists = first
  readable that has it, the base PString set to the substitute's name on the way; IsDirectory asks only while the
  base equals the substitute's own name, so File::IsDirectory("name:x") (null base) is always false, and through a
  link too (kept); Unlink = every writable one; Rename / Md = first writable; TrueName = any (readable or not);
  GetStamp = first readable; SetReadonly = every readable, and SetReadonly(base, path) then chmods the path as a
  plain one as well. GetSubstBuildVersion / ProductCode: the first collected archive with +0x244 / +0x248 set.
- File::Find (0x440): Entry, handle +0x120, dir +0x124, current +0x228, wildcard +0x32c, collected FileSubs +0x430
  (chunks of 13, Jet::Alloc), names given +0x434 (a sorted ring of {next, lower-case name}, 0x108 each) and their
  count, archive +0x43c. Open on "name:" collects the FileSubs, takes the name and any separators off `dir`, and
  NextSearch takes the FileSubs from the end (last collected first) after setting `dir` back from `current` (so the
  name is there again: GetNameAndPath gives "name:/file"). Plain Open gives the first entry without the name check.
  Next strips directories from names and skips names already given (case-insensitive). The copy constructor and
  operator= share the names ring (a copy of a search with names given double-frees); Close leaves the archive set.
- Original bug kept: FileSubPath::Close packs a compressed write into a temp file named in a 16-byte buffer that the
  19-byte "%08X.T%08X" overflows into the read buffer; the first chunk read garbles the name, the rename fails, the
  file is deleted and its packed copy stays as a hidden temp file in the current directory. So a compressed file
  written through a directory substitute is lost (an empty one survives). File::Close's own packing is not affected.
- Prune works on plain paths only (no substitute), stops at a name too long for MAX_PATH, and after a failure goes
  on making files writable but deletes nothing more. Md walks the components with ExtractFirstPath from RootDir.
- Test: diff_ks_filesub (each side its own substitutes over the same files: a directory, an archive written through
  the original, a link; every static, File::Find over drawn patterns with the whole Entry; writes, renames, deletes,
  read-only bits through writable substitutes and links, plain Md / Prune, then the whole tree; first the real
  Base.ja and Data.ja listed and read completely, KFS_BUILTIN=1 for the 8 builtin archives). diff_ks_file now gives
  each side its own File::Subst.

## U10b part 2 done (2026-10-25): FileSubArchive.cpp, 6 exports (1096/1762 ours)
- The archives are ours (FileSubArchive deriving our FileSub; the original's 0x278-byte layout is not kept, only
  our code touches them now), with SetDefaultBuildVersion / ProductCode (0x100adbd0 / d4), File::Sep (PString "/"
  made at load), File::callback / SetCallBack, Find::ClosedHandle (-1, unused) and the temp-name counter
  (0x100adbc4). Nothing in KernelStandard calls the original by address any more.
- Original fields: +0xc record ring (chunks of 61 record pointers, 0x100 bytes), +0x10 name hash (401 buckets of
  rings; first record of the exact name wins), +0x1c directory set (PStrings by text), +0x28 the last directory
  found, +0x12c cursor pool (chunks of 4 cursors {pos, item, ring head, chunk}, never shrunk), +0x130 path, +0x234
  INFO {'ARC0', version, count, directory offset, build, product, 0, 0}, +0x254 bit 0 directory known, bit 1 changed,
  +0x258 fd, +0x25c read-only, +0x260 descriptor lock (FileSub Lock / Unlock), +0x26c list lock. Records (PackedHeapMP)
  {data offset, stored size, Entry size, length, crc, attrib, lower-case name}: times are not kept, so an entry made
  from a record has time 0 (GetStamp of an indexed file is 0; a file met while scanning keeps its header's time).
- Open: read-only 0x8000 / writable 0x8002, SH_DENYNO. Missing and writable: created (0x8102) with "ARCH" + INFO
  (0x102, the defaults), bits 0 and 1 set (so an unused new archive still gets an empty "IDX " at exit). Existing:
  fewer than 4 bytes read: kept open with nothing in it; "ARCH" and an "INFO" chunk (0x100218b0: the first chunk of
  that id; a different id makes it read the next 8 bytes as a header, without skipping) up to 0x20 bytes: the
  directory is read even when the magic or version (0x101..0x102) is wrong, and then the file is closed (fd -1,
  version 0). A directory read fails without count or offset, without "IDX " there, or on a short block (blocks of up
  to 0x1000 entries {offset, size, Entry}); a name without a NUL ends it as a success. Each entry: a record (none
  for an empty name) and its directories (normalized: '\' to '/', A-Z lowered, one trailing '/' cut).
- Next (0x10025b10, cursor, entry, advance, by name): by name (when there are records) the key is the path lower-cased
  AND Path::Normalize'd ('\' becomes '/'), the records only lower-cased: a file stored with '\' in its name is never
  found by name in an archive with a directory (kept; the game's files use '/'). A miss puts the cursor after the
  last record. Otherwise the records from the cursor's place, wildcard-matched (a directory match: attrib |= 0x11,
  the name cut after it). Not found and not (read-only with a directory): the file is scanned from the cursor's
  offset; every "FIL " met becomes a record (the first one also moves every other cursor that was at the ring's end
  onto it) and is matched; other chunks are skipped by their size. The entry buffer holds the last header read.
- OpenWrite: the path, then the temp name "%08X.T%08X" right after its NUL in the File's path buffer (a long path
  writes past the File), opened 0x8301 SH_DENYRW, hidden. Close (modes 1 / 2): an old file of that name is unlinked
  first, then "FIL " {size, Entry} at the directory's offset when there is one (overwriting it; it is rewritten at
  exit) or at the end; data plain, or for mode 2 "CFIL" 0x101 size + BPE chunks (u16 0x8000|n when stored raw);
  CRC (Crc::Calc) of the plain data; File::callback(done, stored) per chunk (stored 0 for plain), its result ignored;
  with a directory the record is added and the directory's offset moves to the end. The Entry written has stack
  leftovers after the name in the original (ours: zeros).
- OpenRead shares the archive's descriptor: size = Entry length for mode 0x20, else Entry size; data start and raw
  position = the cursor's offset; the cursor is freed. Exists: by name, else a name without '.', '*', '?' in its last
  part that is a directory. IsDirectory compares with the last directory found first (initially "": IsDirectory("")
  is true). Unlink (writable only): the chunk becomes "FIL!", the name (lower-cased as given, not normalized) leaves
  the hash and the first record wildcard-matching the path leaves the ring (a hash entry left behind keeps pointing
  at the freed record in the original; ours keeps such records until the archive goes). That happens whenever a
  path with '\' is unlinked (Close unlinks an old file of the same name too): a later lookup of the name reads
  freed PackedHeapMP memory, so the original's answer depends on whether it was reused (seen once in 1000 test
  scenarios: the original 0, ours 1). The test writes and unlinks with '/' only, as the game does.
- Directory rewrite (0x10027330, at destruction when writable with bits 0 and 1): "IDX " at the old offset or the end
  (a shorter one leaves the old tail after it; the file is never truncated), the entries from Alloc memory (heap
  leftovers after names in the original), then "ARCH" + INFO again at 0.
- Kept original crashes (the test avoids them): a writable archive whose file was turned away (no INFO, a foreign
  magic or version) still takes writes, which go to fd -1 (msvcr80 ends the process); a compressed file written to
  a version 0x101 archive gets the 12-byte "CFIL" header but is read with the 4-byte one (BPE::Decomp runs off);
  a record with a garbage offset (from a directory over stale data) makes Unlink write "FIL!" hundreds of MB out.
- Test: diff_ks_filesub now also runs "archive" scenarios (each side its own .ja, built or new, written / read /
  searched / unlinked / remounted, the file compared chunk by chunk with names masked after their NUL; each side its
  own current directory, because the modules now count temp names separately and FileSubPath's lost temp files of
  one side would collide with the other's). KFS_APROBE shows the '\' name and "name:pattern" search behaviour.

## U11 done (2026-10-25): Window.cpp, 71 exports (1167/1762 ours)
- OS 0x18 {instance, Window*, commandLine (the caller's pointer, not copied), Options* (new 0xc), parent HWND,
  AnsiString appName}. The header keeps OS opaque (the x64 stand-in has its own state); Window.cpp overlays it.
  The constructor takes (appName, commandLine, instance, window); GetAppName is null for an empty name. Init runs
  InitCallBack under ExceptionHandler::Main: Options::ProcessRaw(defaultOptions) then Options::Init(appName,
  commandLine). The copy constructor and operator= share the Options pointer (both destructors free it). GetName
  "Windows"; CreateNewWindow = Window(parent) in Alloc memory; CreateDynamicLibrary = our U6 DynamicLibrary.
  `defaultOptions` is `char const * const`, which VS2005 mangled `0PBDB` and VS2026 mangles `0QBDB`: exported
  through an alias in implemented.txt. Five getters are folded into other functions in the original (GetInstance =
  Window::GetHandle 0x1870, GetOptions, GetCmdLine, GetWindow, Window::GetTitle, Window::SetHandle): ours are
  separate functions.
- Window 0x134 {HWND, title[0x101], active +0x105 (bit 0 WM_ACTIVATEAPP, bit 1 WM_ACTIVATE), hooks ring +0x108,
  dispatchers ring +0x110 (rings {head, count}, singly linked, the ring itself is the sentinel, appended at the
  end, cleared by context: the first match), parent +0x118, flags +0x11c (1 ShowFPS, 2 visible, 4 full-screen
  host, 8 handles messages, 0x10 context only, 0x20 class registered), FPS frames / float / time +0x120..0x128,
  desktop bpp +0x12c (read once), critical-shutdown entry +0x130}. The constructor leaves bits 6-7 of the flags
  and bits 2-7 of `active` as the memory held; operator= copies the 0x134 bytes (the rings still point at the
  source). mainWindow = the first one constructed, cleared by its destructor.
- Open: ExceptionHandler::AddCriticalShutdown(CriticalShutdown, this) (the original builds the same node inline
  on the private list head; the exports do exactly that), RegisterClassExA "JET" (CS_OWNDC, 4 extra bytes for
  the object, IDI_WINLOGO, black brush; failure: false, the entry stays), SetTitle, then the options HideWindow
  (present at all: not visible) and ShowFPS. Close: RemoveCriticalShutdown, DestroyWindow, UnregisterClass when
  flag 0x20 (never cleared: a second Close fails to unregister, debug output only).
- Styles: windowed 0x00c80000 (child of a parent: 0x44000000), full screen 0x81000000 (0xc1000000) with
  WS_EX_TOPMOST; WS_VISIBLE with flag 2. CreateWindowedHost sizes the window from the client size (or covers the
  parent's client area); CreateDefault = CreateWindowedHost(CW_USEDEFAULT x4, param = parent); Create2DWindow is a
  jmp to CreateWindowedHost. Each create sets the "creating" static (WindowProc's object until the window's extra
  bytes hold it, never cleared) and ends with HandleMessages. SetHostState restyles (no SWP_FRAMECHANGED: the frame
  follows on the next frame change) and sends WM_MODE_CHANGED (0x402, wParam 1 = windowed).
- HandleMessages (false only on WM_QUIT): FPS title "%s  (%0.1f)" every > 250 ms (fps = (frames * 1000 / ms +
  fps) / 2, in float); PeekMessage loop over the whole thread, dispatchers (fastcall (context, MSG*)) until one
  takes it, else Translate / Dispatch; then posts WM_END_OF_MESSAGES (0x401) to the window and dispatches one more
  message (no Translate). A dispatcher that clears itself or the next one makes the walk read a freed node (both).
- WindowProc: hooks (fastcall (context, {hwnd, msg, wParam, lParam}*, int& result), result starts 0; a hook that
  returns true returns its result); a hook added or cleared (a module-wide counter) ends the walk. Then WM_PAINT
  ValidateRect, WM_DESTROY PostQuitMessage unless a parent or context only, WM_SIZE with a parent: SetWindowPos to
  the size, WM_ACTIVATE / WM_ACTIVATEAPP bits; WM_SYSCOMMAND without a parent: SC_SCREENSAVE / SC_KEYMENU /
  SC_MONITORPOWER (exact values) dropped, SC_CLOSE sends WM_CLOSE; with a parent, keyboard (0x100..0x109) and mouse
  (0x200..0x20d) messages are posted to the parent. The rest: DefWindowProcA. The x64 build uses
  Get/SetWindowLongPtr and 8 extra bytes.
- Smaller things: Error ignores its text and sends "GetLastError(%d) returned: %s" to the debugger; Move takes the
  client rectangle; Resize keeps the window's top-left (GetWindowRect unchecked); CalculateWindowArea uses the
  current style or the windowed one; GetClientArea is all -1 and GetParentArea all 0 on failure; SetTitle refuses
  more than 0x100 characters; Message / MessageConfirm are MessageBoxA(0, text, "JET", MB_SETFOREGROUND |
  MB_TOPMOST [| MB_YESNO]).
- WindowPlacement: SaveWindowPositions appends {HWND, rect, WINDOWPLACEMENT} of every visible top-level window to a
  module ring (never emptied before exit, so a second save adds duplicates); Restore applies them all in order.
- Test: diff_ks_window, on a private desktop (CreateDesktop + SetThreadDesktop: nothing shows on the user's screen,
  and WindowPlacement only sees the test's windows). MessageBoxA is patched in both modules' import tables. Only the
  messages the test sends (lParam 0x5aNN) and a few the window makes from them are logged with parameters: system
  activation / paint / input traffic depends on the desktop's history, and so does WM_END_OF_MESSAGES (HandleMessages
  dispatches one message after posting it: which one depends on the queue), so it is not logged. After SetHostState the test forces
  SWP_FRAMECHANGED, otherwise the client area depends on when the system recomputes the frame. No SC_SIZE / SC_MOVE
  (modal loops) and no SC_SCREENSAVE / SC_MONITORPOWER / SC_KEYMENU to a child (DefWindowProc would act on them).
  It is slow (about 3 s a scenario, waiting in window calls, not CPU). KSW_ONLY=n replays one scenario,
  KSW_LIVE / KSW_ALL print the log / every hooked message as they come. 300 scenarios: 77 173 checks, 0
  mismatches; swapped (KSW_SWAP) 100 scenarios: 25 636 checks, 0 mismatches.
- diff_ks_configdata / diff_ks_configparser had been crashing (0xC000000D in the original) since U10b part 2: the
  modules now count temp names separately and FileSubPath's lost temp file of one side collided with the other's
  next name. They now give each side its own current directory, like diff_ks_filesub, and pass again.

## U13 done (2026-10-26): Thread.cpp + Time.cpp, 49 exports (1216/1762 ours)
- The state stays the original's: `Time::freq` / `frameLock*`, `Thread::mainThreadId`, `ThreadLocalStorage::singleton`
  and `ThreadTlsIndex` / `ProfilePt|Trace|RenderTlsIndex` are imported (JET_KS_FORWARDED). The original's
  KernelStandardInit (U12) sets mainThreadId and creates the singleton, its shutdown routine 0x1002ec50 destroys
  it with the PString cache, its Profile reads the indices and its Log reads the clock. The data moves at U14.
- Time: GetSeconds = counter / freq, both as unsigned 64-bit doubles. FrameLockMs = low half of
  `(long long)(frameLockTime * 0.001)` (fistp with RC = chop, so out of range gives 0). IncFrame: `time = count *
  delta; ++count` (old count; count read as unsigned). SetFrameLockTime only sets the flag and delta. HHMM picks
  characters 0, 1, 3, 4 of `_strtime`. Us is the counter's low half. Usq and GetCounter are QPC, TimeStamp is
  `_time64`, Str is `_strtime`.
- Thread 0x20: `{vptr, handle @4, WEvent done @8 (auto reset), alive @0x10 (1 from the ctor, 0 when Start returns),
  fn @0x14 (__stdcall), arg @0x18, id @0x1c}`.
  - The vftable is SyncObject's, except slot 1 (`operator void*` = done's handle).
  - CreateThread uses `start ? 0 : CREATE_SUSPENDED`.
  - The destructor (and Terminate): SetEvent(done), ResumeThread, Lock(INFINITE), GetExitCodeThread. The destructor
    then sets the handle to 0 without closing it. Terminate returns the exit code, or -1 when there is no handle.
  - IsDone = done.Lock(0) then done.Unlock() (slot 4, WEvent's no-op).
  - GetThreadID shares its code with File::Tell (+0x1c).
  - IsProcessAlive: OpenProcess(QUERY_INFORMATION), exit code == STILL_ACTIVE. TerminateProcess(0) does nothing.
  - GetProcessName: GetModuleFileNameA(0, out, 0x100).
- ThreadLocalStorage 0x10: `{Mutex*, count, capacity, entries}`, each entry `{PStringCache::Node*, index}`.
  - The ctor makes a Mutex and registers "ThreadIndex", "ProfilePtIndex", "ProfileTraceIndex" and
    "ProfileRenderIndex" (Create(text, length)), storing the four statics.
  - GetIndex: Lock(INFINITE); a linear search by node pointer; else TlsAlloc and an append. Growth gives
    `((count + 0x80) >> 7) * 128` entries. The name is AddRef'd twice and released once, the original's
    temporary copy. Unlock only when the lock succeeded.
  - The dtor: Lock, TlsFree every index, Unlock, delete the mutex, release the names, free the array.
  - The copy ctor and operator= share the mutex. Copying (0x1002ecf0) clears the target and returns at once if the
    source has no array. It reallocates only when its capacity is smaller, then copies count and entries with an
    AddRef each.
  - Releases are the inlined form (`refs == 1` decrements cache word 0, then liveRefs and refs; never Destroy).
- Test: diff_ks_thread (2731 results per side, 0 mismatches, 4 runs; 12 of 12 mutations caught). Both sides share
  the original's statics, so the scenario saves and restores them. TLS growth runs on a fresh instance of each
  side, and each side uses its own names.

## U12a done (2026-10-26): Exception.cpp + Log.cpp, 41 exports (1257/1762 ours)
- U12 is split: U12a Exception / Debugger / Log / LogClient, U12b Profile / QuickTimer / Console, U12c
  ExceptionHandler / HardwareException (x86 stack walking, done last).
- Exception 0x1110: `{vptr, description[0x1000] @4 (call stack text), reason[0x100] @0x1004, hardware @0x1104,
  line @0x1108, file @0x110c}`. Class "Unknown". The file/line constructors put "No call stack available\r\n" in
  description. Copies take every byte. Formatters use msvcr80 vsprintf. AddReasonf = strcat "  -  " + vsprintf at
  the end. The vftable is `{deleting dtor, Class, IsHardwareException, Description, Reason}`. Exception::Log,
  Debugger::Break and Debugger::LogCallStack are the shared empty function 0x1002abe0.
- Debugger: Output = OutputDebugStringA, IsReadValid = !IsBadReadPtr, IsDebuggerAttached = (IsDebuggerPresent() == 1).
- LogClient 0x24 `{next, prev, AnsiString name, AnsiString unused, level, kind, busy, outputs, format}` (offsets in
  MODLOG). Log statics are imported. The private LogFlush byte (0x1007287b) is used in the original module by
  address (base + 0x7287b).
- GetClient: `_stricmp` over the ring (it creates the ring on first use). SearchClientList: the same length, then
  `_strnicmp`. CreateClient appends at the tail. Process tokenises the option line by ':'. A non-level token is
  split into key = Token('=') and value = Token(':'); value "=off" clears the switch.
- Init / Done / Shutdown: see MODLOG "U12a". The File objects are freed the original's inline way (~File, then
  Alloc::Delete).
- Test: diff_ks_log (1245 results per side, 0 mismatches; 14 of 14 mutations caught).

## U12b done (2026-10-26): Profile.cpp + Console.cpp, 55 exports (1312/1762 ours)
- Profile::Info `{name, file, count @8, time @0xc, id @0x10, mask @0x14, peek @0x18}`. The Profile object is
  `{info, parent, childTime, start}` (QPC low halves). The constructor registers into funcs (at most 0x1000) on the
  first enabled call. The destructor adds `now - start - childTime`, and adds `now - start` to the parent's
  childTime when childtime is set.
- Statics are imported. Display(float)'s accumulators shown/sorted/peeks are used in the original by address (base +
  0xae210/0xae214/0xae218). GetDisplayFrequency is exported as `?Profile_GetDisplayFrequency@Jet@@YANXZ` (double).
- The sort functions return differences: SortTotal and SortCount `b - a`, SortAvg `b.time/b.count - a.time/a.count`
  (0 for no calls). The sort itself is msvcr80 qsort.
- RegisterTracePoint: "START" takes slot 0 (and makes the count 1 if it was 0). The others go to `count++`, and the
  first one (count 0) goes to slot 1 with count 2. At 0x80 the result is -0x151. TracePoint(0) starts frame
  `(f+1)&0x7f`. Each FrameTrace (0x404) holds 128 `{id, QPC low}` points.
- QuickTimer 0x20: `{info, parent, childTime u64, start u64, frame}`. Info `{name, file, id, Stats[4] @0xc}` with
  Stats `{count, min, max, total}`. The id comes from InterlockedIncrement(nextid)-1; at 0x40 or more it is -2.
  threadInfo points to `{current, depth}`.
- Console: see MODLOG "U12b". The span ring is `Chunk{next, prev, count, Span[13]}` with Span
  `{u8 type, int pos, u32 rgb}`. The copy appends chunk by chunk (count and spans memcpy'd).
- Test: diff_ks_profile (127 728 results per side, 0 mismatches; 20 of 20 mutations caught).

## U12c done (2026-10-26): ExceptionHandler.cpp, 25 exports (1337/1762 ours)
- Ours: the exported ExceptionHandler and HardwareException members and an AssertEx class. Still the original's,
  called by address: the crash report writers (base + 0x1e920 for Main's fault filter, cdecl; base + 0x1ee20 for the
  assertion report, fastcall with the Exception in ecx) and HardwareException::GetCallStack (its export).
- Private statics are used in the original by address: report lock (base + 0x7663c), Main nesting (0xae208), list
  re-entry flags (0xae20c / 0xae20d), the "Exception" log channel (0xae178). `file` / `reason` are looked up with
  GetProcAddress (VS2005 mangled them 0PBDB).
- MainTryCatch catches `AssertEx&` (critical shutdown only) before `Exception&` (report, then critical shutdown).
- The lists are `{arg, fn, next}` nodes, newest first, run front to back. A run is allowed again while the list is
  not empty. A failing log message closes the log (Log::Shutdown(true)), and a failing callback logs "... failure!".
- Test: diff_ks_eh (ordinary paths only; 131 results per side, 0 mismatches).

## U14b done (2026-10-26): Memory.cpp + PString.cpp and every data export (1761/1762 ours)
- The foundation cut-over: Alloc, PackedHeap / PackedHeapMP, PoolBlockAlloc, ReferenceCount, ConfigData's pool
  operators, PStringCache / PString, KernelStandardInit, g_jetSTLAllocHeap, and all 68 data exports (defined in the
  unit that uses them). Only HardwareException::GetCallStack is still forwarded; the crash report writers (base +
  0x1e920 / +0x1ee20) are still called by address. Both keep their own state (a report buffer at 0x10076658,
  ERRORLOG.TXT), so they need none of ours. The original module stays loaded for them (and msvcr80 with it).
- Static initialisers: the original's .CRT$XCU table (0x10051478) runs Alloc's constructor, the free-list heads
  {0x155, head}, the shutdown table runner (0x10050e20), KernelStandardInit, the counter frequency, ConfigData's pool,
  the "ExceptionHandler" and "PackedHeap" log clients, File::Sep, the StringTables tokens and g_jetSTLAllocHeap
  ("#jetstd::"). Ours: Memory.cpp and Time.cpp are `#pragma init_seg(lib)`, so they come before every other static
  and go after them at exit. KernelStandardInit (one instance; a static flag as in the original) sets the locale
  (msvcr80 setlocale(LC_CTYPE, ".ACP")), Thread::mainThreadId, the cache (997 buckets) and the TLS singleton, and puts
  the shutdown routine (0x1002ec50: cache, isFinished, Log::Done, TLS) in the 0x400-entry table run at exit.
- Alloc: class c = (size + 0xb) >> 3 (size 0: class 1, 8 bytes); classes above 0x7f go straight to msvcr80 malloc.
  The tag is ((c ^ 0x155) & 0x1ff) | c << 9; Delete reads only c. Free lists keep caps[c] = 0x2000 / 8c + 1 blocks
  (class 0: 1; classes 2/3/4: 0x2000 / 0x1555 / 0x1000) and push the block header; 16 locks by c & 0xf. Until the
  constructor and after the destructor (flag 0x100728d6, initially 1) nothing is locked. Out of memory: debug string,
  then std::bad_alloc. Dump prints hits / misses / large / bytes (Search and Maximum are never counted).
- PackedHeap {name, block set, size map, unused}: 1 MB blocks (VirtualAlloc, MEM_TOP_DOWN) {used, offset, free
  chain}, allocations {size, data} from +0xc. Sizes: < 0x10 become 0x10, rounded to 4, at most 0xffff0. Alloc tries
  the freed allocation of that exact size first (a map size -> chain), then bumps in the blocks **from the highest
  address down** (a reverse walk of the original's set), then a new block. A freed allocation goes on its block's
  chain and its size's chain {size, prev link, next, prev link, next}. A block that empties is released when another
  empty one exists, else reset (its chunks unlinked, offset 0); the unused count is not lowered on release (as in the
  original). Free ignores pointers outside a block, misaligned ones, and sizes past the block's offset. Copies (copy
  constructor and operator=) take the same blocks and chains, are not in the list of heaps, and destroying two of
  them frees the blocks twice (as in the original). DebugPrint logs "%s - %luk unused, %luk used, %luk free" with
  the values in the order free, used, unused. The two MSVC8 trees are our own containers behind the same 12 bytes
  (a sorted block array; a std::map, whose slots stay put as the chains need).
- PoolBlockAlloc 0x30 {full, partial, align, item size, count, block bytes, free items, carving, uncarved,
  CriticalSection}: align not a power of 2 becomes 8; item = (align + 3 + size) & ~(align - 1). Blocks (one
  Jet::Alloc each) are {next, prev, offset to the raw block, free chain, free items} 0x14 bytes before the first item
  {block, data}; free items are used before a new block is carved; a block whose items are all free is released.
  ConfigData's nodes come from one (0x10-byte nodes, 0x200 a block, 4-aligned); the placement delete frees into it too.
- PStringCache 0x30 {usedNodes, liveRefs, hits, nodes, probes, -, -, bucketCount, gcBucket, buckets, walkBucket,
  walkNode}: buckets {head, tail, count}, an Alloc'd array with its count at -4. The hash (also GetHash) is
  h = h * 8 ^ byte over the last 16 bytes at most; a lookup moves a found node to the front and counts hits and
  probes. Nodes are (length + 0x29) & ~0xf bytes from the "#StringData" heap (above 0xa0 from Alloc) {next, prev,
  length, capacity, refs, id, text}. Create / Insert count usedNodes when a node gets its first reference; Destroy and
  the inline release drop it at the last. GetUnique takes the node out of its chain when it has at most one reference
  and room, else copies it (one reference dropped); Ins / Del / ToLower / ToUpper edit that and Insert it back
  (merged with an equal node). CollectGarbage sweeps one bucket a call (round robin) from the tail. CreateUnicode
  makes a node of the bytes msvcr80 wcstombs wrote. The static lock (0x100acb58) is made by the constructor.
- PString::Read: header bit 30 = a UTF-16 byte count (converted with CreateUnicode), else the length. An ANSI text
  cut short frees its node with Jet::Alloc::Delete whatever heap it came from (the original's bug: a string-heap node,
  length below 0x87, corrupts the msvcr80 heap). Write: {length} then the text.
- The string heap and AnsiString's empty block (a constant {1, 0, 0, ""} now) are in AnsiString.cpp
  (StringHeap.h); Log's LogFlush, Profile::Display's accumulators (one array, displayClock), ExceptionHandler's
  report lock, Main nesting and list flags are ours. `file` / `reason` / the three `results` tables mangle
  differently in VS2005: they are exported from our own symbols through implemented.txt aliases. Console's 8x8 font
  is read from the installed game's module at build time (ConsoleFont.h, like StdMath's table).
- Tests: diff_ks_alloc (Alloc reuse and Dump counters, PackedHeap / MP layout and block headers, copies,
  PoolBlockAlloc with four shapes, ConfigData's pool, ReferenceCount slots, a 6-thread stress per side) and
  diff_ks_pstring (every PStringCache and PString export over a corpus, node identity by the test's slots, chain
  places, counter changes, Read / Write). The older tests now make every input through the side under test: each
  module has its own heap, cache and statics (helpers per side; LogFlush and the Display accumulators of ours from
  KernelStandard_7c.map next to the DLL, tests/ks_private.h; diff_ks_filesub interns its substitute names on both
  sides in one address order, since that list is sorted by node address).
- Also fixed on the way: LCRand::NextDouble(lo, hi) is computed on the x87 stack (with two NaN arguments the x87
  keeps the larger significand, SSE the first operand): the U5 NaN-only difference is gone.
- Two static heaps besides "#StringData": "FileSubArchive" (the archive records; ArcRecord's operator new) and
  g_jetSTLAllocHeap "#jetstd::". DebugPrintAll lists heaps by address and the original's archive heap comes first, so
  both sit in one section in that order (`.jheap$a` / `$b`; plain definitions came out in either order). The game's
  exit summary in JetLog then matches the baseline line for line, except #StringData's fill (486k used against
  507k): the archive directories and file searches keep their strings in std containers since U10b.
- Results: diff_ks_alloc 1.6 million results and diff_ks_pstring 1.5 million, 0 mismatches; 24 of 27 mutations
  caught, 3 equivalent. All 25 diff_ks_* tests pass (one diff_ks_file mismatch once, while the game ran at the same
  time; not repeated in 2 reruns). x64 harness: 0 differences. In game: 3 menu checks with JetLog identical to the
  baseline; an input run (hover, SELECT ROUTE with Cyrillic route names, Esc, the Main menu dropdown) shows the same
  screens as the U14a build; the Launcher shows no "faulty" box.
