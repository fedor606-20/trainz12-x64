# Trainz Simulator 2012: 64-bit port, working log

Original install (read-only, never modified): `D:\games\trainz 2012 VDK`
Working copy for experiments: `D:\games\trainz 2012 x64`
Decompiler projects and decompiled output (private, never publish): `D:\trainz12-decomp`
Tools: `D:\tools` (Ghidra), scripts: `trainz12-x64/tools/`

## Facts confirmed (2026-10-18)

### Binaries
- Game process: `bin\trainz.exe`, version 1.5.0.61388. Top-level `Trainz.exe` (77 KB) is only a stub launcher.
- x86 (machine 0x14c), MSVC 2005 (linker 8.0, Rich build 50727), CRT `msvcr80/msvcp80`.
- **Not packed, no DRM wrapper**: standard sections, `.text` entropy 6.47. Has `.reloc` (relocatable).
- **LargeAddressAware is already set** on trainz.exe and the Jet DLLs, so the game already gets 4 GB on x64 Windows.
- trainz.exe: 6.69 MB `.text`, 1309 RTTI classes, 1 export. Imports ~1600 functions from the Jet DLLs by name.
- Extra section `.shr` (4 bytes, shared): likely a single-instance flag.
- Census of all native modules: `census.json` (from `tools/census.py`). Native code across bin/ = 23.5 MB,
  12 431 mangled C++ exports, 4186 RTTI classes.

### Engine ("Jet", Auran)
- Modular: `*_7c.dll` plugins registered by GUID in `bin\plugins.ini` (resource factories per file extension:
  `texture`, `kin`, `msh`, `fx`, `wav`, `mp3`, `terrain.txt`...). Modules can be swapped one at a time.
- Module sizes (code): RenderSystem 396 KB, Render3DCore 504 KB, Render3DFoundation 221 KB,
  InterfaceSystem 368 KB, KernelStandard 328 KB, WaterSystem 112 KB, InputSystem 81 KB, Kernel 60 KB,
  RenderOpenGL 152 KB, RenderDirectX9 78 KB, Lexer 22 KB, plus small ones.
- Jet DLL exports are **full C++ signatures** (e.g. RenderSystem 2686 of them): the module APIs are named for free.
- Renderers: `RenderDirectX9_7c.dll` (uses `d3dx9_42.dll`) and `RenderOpenGL_7c.dll`; shaders are NVIDIA Cg 2.2.
- TrainzScript runtime lives in trainz.exe (`GSRuntime`, `GSScriptInstance`, `GSMessage` RTTI).
- `JetLog.txt` lines are prefixed with `Class::Method>`: these strings are in the exe and name functions.

### Ghidra project (D:\trainz12-decomp\project\trainz12, Ghidra 12.1.4, JDK 21)
- Run with `tools/ghidra_analyze.cmd <log> <binaries...>`; per-module function lists land in
  `D:\trainz12-decomp\out\*.functions.tsv` (script `ghidra_scripts/ExportFunctionStats.java`).
- 21 Jet modules analysed: **21 223 functions, 14 348 named (68%)** straight from exports/RTTI.
  Best: Lexer 86%, RenderSystem 83%, KernelStandard 77%, InterfaceSystem 74%. Worst are the plugin modules
  with one export (RenderDirectX9 21%, InputDirectX 25%, DSMoviePlayer 26%): named via vtables/RTTI later.
- Gotcha: do not touch the JDK while Ghidra runs. A JDK upgrade replaced `jdk-21.0.11` mid-analysis and
  killed the first run of trainz.exe (2026-10-18 22:47); finished modules were already saved.
- No PDB exists for any module.

### Module replacement harness (confirmed in game 2026-10-18 23:25)
- Source of replacements: `D:\trainz12-decomp\src\jet` (private), CMake + VS 2026 (MSVC 14.51), `-A Win32`,
  shared CRT (/MD) since 2026-10-27 (static before; vcruntime140.dll + msvcp140.dll x86 sit next to the game).
  Build: `cmake -S D:\trainz12-decomp\src\jet -B D:\trainz12-decomp\build32 -G "Visual Studio 18 2026" -A Win32`.
- `tools/gen_def.py` makes the .def: every original export keeps name + ordinal; names in
  `<module>/implemented.txt` come from our code, the rest forward to `<module>_orig.dll`.
  link.exe (x86) rejects a .def forwarder for an undecorated C name (`JetGetPluginsKernel`): those go through
  `#pragma comment(linker, "/export:...")` in a generated `<module>_forwards.cpp`.
- Kernel_7c ABI: no STL types in any export; 14 vftables + 5 vbtables (virtual base PluginInstance) and
  9 static data exports. No malloc/new from msvcr80 (objects use Jet::Alloc from KernelStandard), so a
  different CRT is safe for this module.
- **Pure-forwarding Kernel_7c.dll proxy works in game**: copy reaches the main menu, both `Kernel_7c.dll`
  (ours) and `Kernel_7c_orig.DLL` are loaded (checked with 32-bit PowerShell; 64-bit can't list a WOW64
  process's modules), JetLog identical to the baseline line for line, clean exit.
- Test launch: `Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64'
  -ArgumentList '-windowed','-Width=1280','-Height=800'` (args taken from Launcher.exe). Main menu in ~20 s.
  Close with CloseMainWindow(): JetLog is only written on exit. The white "Forbidden" panel is the dead
  N3V news feed, same in the original.

### First reconstructed code: Jet::Version and Jet::UId (2026-10-18)
- `src/jet/include/jet/{Version,UId}.h`, `src/jet/core/Kernel_7c/{Version,UId}.cpp` (private tree). 25 of 256 Kernel_7c
  exports now ours; Version default-ctor closure and UId Read/Write/GetString/SetRnd/UIdNull still forwarded
  (they need DynamicBuffer/PString/Rand from KernelStandard_7c).
- ABI gotcha: most UId members are **`__cdecl` member functions** (mangled `QBA`/`QAA`, `this` on the stack);
  Ghidra shows them as thiscall with `in_stack_` vars. Constructors, operator= and Zero are `__thiscall`.
- Version = u16 build | u8 minor | u8 major, compared as one u32. UId = GUID layout; `<`/`>` compare
  data1, data2, data3 unsigned, then data4 bytewise. `UId(const char*)` copies 37 chars but zeroes index 36.
- **Differential test** `src/jet/tests/diff_kernel_uid_version.cpp` loads ours and Kernel_7c_orig.dll in one
  32-bit process and calls both through the raw exports (thiscall emulated with __fastcall): 600 000 rounds of
  random, near-equal, zero and malformed-string inputs, **0 mismatches**.
- In game: main menu reached, JetLog identical to baseline, clean exit.
- **UId complete** (30/256 Kernel_7c exports ours): Read/Write/GetString/SetRnd and UIdNull added. Only the
  Version default-ctor closure is still forwarded in these two classes.
  Differential test extended with real KernelStandard objects (DynamicBuffer incl. truncated buffers, PString
  through the live PStringCache, SetRnd shape): 621 000 rounds, 0 mismatches. In game: identical JetLog.

### Calling into the original modules
- `tools/gen_implib.py` builds x86 import libs for every Jet module from its export dump
  (`D:\trainz12-decomp\implib\*.lib`; data exports marked DATA → declare `__declspec(dllimport)`).
  If our header declarations link against these, their mangled names are right.
- `tools/dump_imports.py <dll> <from dll> <undname>` lists what a module uses from another, demangled.
- `Jet::PString` = one `PStringCache::Node*`. Its ctor/copy/dtor are inlined everywhere (not exported);
  reconstructed: copy = Lock, ++node->refs (+0x10), ++cache->liveRefs (+4), Unlock; dtor = cache->Destroy(node)
  if both non-null. `PString::cache` is a private static, imported as DATA.
- A `__cdecl` member returning a class by value takes (this, hidden result*) on the stack in that order.

### What actually loads (main menu, 2026-10-18, 32-bit module list of the running game)
- Jet: Kernel, KernelStandard, KernelResource, KernelResources, Input{System,DirectX,Win32}, Interface{System,Resources},
  Render2DCore, Render3DCore, Render3DFoundation, **RenderOpenGL**, RenderSystem, WaterSystem, DebugInterface.
  `out\loaded_modules_menu.txt`.
- **The renderer in use is OpenGL** (RenderOpenGL_7c), not DirectX 9.
- **Not loaded at the menu**: Lexer_7c (nothing references it; not in plugins.ini, so probably dead),
  DebugSupport, DSMoviePlayer, VFWMoviePlayer, Render3DExtension, RenderDirectX9. Re-check inside a driving session.
  - Correction 2026-10-26: RenderDirectX9 is not dead. plugins.ini registers it in the same renderer group as
    RenderOpenGL (`RenderDirectX9:DirectX9:052FEC1E-...`); the launcher's renderer choice loads one of them.
- **Embedded Mozilla XULRunner 1.9pre** (`bin\mozilla\`: xul, xpcom, js3250, nspr4, nss3/ssl3/smime3/softokn3, sqlite3):
  the HTML panels of the menus. No x64 build of that version exists: the x64 port must replace it (e.g. WebView2).
  Also loaded: cg/cgGL, cudart, nvtt, ijl10, OpenAL32 + wrap_oal, LIBEAY32, MSVCR71, UnicoWS.

### Replacement granularity (lesson from Task/TaskManager)
- Inside a module, functions call each other directly, not through the export table. So replacing one class
  of a module is only safe when that class holds no state shared with the rest of the module (UId: fine).
  Stateful subsystems (App, Task/TaskManager + the internal LogTask, plugin system, EditItemManager) must move
  together: Kernel_7c's core gets replaced as one unit.

### Naming pass via RTTI vftables (`ghidra_scripts/NameVtableFunctions.java`, all programs via `tools/ghidra_all.ps1`)
- Unnamed functions referenced from a labelled vftable become `Class::vfN`. +~9 900 names; trainz.exe alone
  +6 804 (5 268 vftables). trainz.exe now 65.6% named (was 51.0%); Jet modules 70–91%.
- Kernel_7c internal classes revealed: BFEncrypt (Blowfish), LZSSCompress, PluginSystem, PluginLibrary,
  PluginGroup, PluginFactoryOf<...>, LogTask, Mutex, ReferenceCount.

### Auran Jet SDK 1.0 (2001–2003) as a reference (found by the user, 2026-10-19)
- Source: archive.org item `auran-jet-demo` (public downloads from auran.com/jet), saved in
  `D:\download\auran-jet-demo`. Licence: "AURAN Jet Free Access" (copyright Auran): private reference only,
  never commit or publish SDK files.
- Unpacked (without running installers) to `D:\trainz12-decomp\jet-sdk\{RC1,Beta2,Demo}\out`. The installers
  are InstallShield 6 cabs: 7-Zip can't open them; `unshield` 1.6.2 built from source with zlib 1.3.2
  (`D:\tools\src\unshield-build\src\Release\unshield.exe -d <out> x data1.cab`). The separate
  `Auran Jet Documentation.exe` uses an older format unshield rejects, but RC1 already contains the same
  `Doc\doc\JetAPI.chm`, extracted to `jet-sdk\RC1\chm` (10 003 HTML pages).
- RC1: 517 engine headers (`Inc\inc\*.h`), 111 .cpp (examples, extensions, JIVE), 51 import libs, 98 DLLs
  (`Kernel.dll`, `KernelStandard.dll`, ... the same modules without `_7c`, plus `.Dev` builds), docs
  (JetKernelOverview.doc, RenderOverview.doc, whitepapers incl. "Jet DLLs & The Plugin Management System",
  "Jet Program Flow"). No PDBs.
- **Checked against TS12**: `UId.h` matches our reconstruction (union data[4] / data1..data4, same compare,
  same Read/Write). `Version.h`: same packing, fields named `minor`(u16) `major`(u8) `main`(u8).
  `Task.h` matches the TS12 binary to the byte: taskName +0x10, updatedTime (double) +0x18, priority +0x20,
  bitfields isUpdating/isOnUpdateList/isInitialized at +0x24; virtuals ~Task, GetName, Init, Done, Update.
- Differences seen so far (SDK is ~10 years older than TS12): many members became `__cdecl` in TS12;
  `DynamicBuffer::Write(const void*, ulong, ulong)` gained a third argument. The SDK guides names and intent;
  the TS12 binaries remain the authority.

### Jet::BFEncrypt = a modified Blowfish (reconstructed and verified 2026-10-19)
- `src/jet/core/Kernel_7c/AuranBlowfish.{h,cpp}`; tables generated from pi by `tools/gen_pi_tables.py` (Machin formula),
  checked equal to the DLL's tables (P at RVA 0x18478, S at 0x184c0). No data copied from the binary.
- Differences from standard Blowfish (all confirmed in the disassembly, `ghidra_scripts/DumpAsm.java`):
  key repeated/cut to 56 bytes read as little-endian words; blocks are two LE words (L first);
  F(x) = ((((S1[b]&1) + S0[a]) ^ 0x20) + (S3[d]&1) + S2[c]) with a = low byte; R ^= F(L) ^ 0x20;
  **the S-box fill rewinds every outer pass** so only S0[0], S1[0], S2[1], S3[1] change;
  a non-multiple-of-8 tail is one more block ending at the buffer end (decrypt does it first).
- Methods (thiscall, `RET 0xC`, hidden Result* first): GetCaps 0x21f0 {keyMin 1, keyMax 256, bufMin 8,
  bufMax 0xffffffff, align 0x40}, SetKey 0x2230, GetOptimalSize 0x1db0 (round up to 8), EncryptData 0x24d0,
  DecryptData 0x2550. Codes: 2 null, 3 key size 0, 4 key > 256, 5 buffer < 8.
- `src/jet/tests/diff_blowfish.cpp` calls the original methods by RVA on a zeroed 0x1060-byte object:
  75 008 checks (key-schedule state, encrypt, decrypt of random data, round trips, error codes), 0 mismatches.
- Technique: internal (non-exported) functions of `<module>_orig.dll` can be diff-tested by calling them at
  base + RVA with the right calling convention.

### Jet::LZSSCompress decompression (reconstructed and verified 2026-10-19)
- `src/jet/core/Kernel_7c/JetLzss.{h,cpp}`: header, mode 0, token mode (levels 1-6), adaptive Huffman (levels 7-9) with
  the original's model init (0x10005cf0), update (0x100060b0) and in-place rebuild (0x10005e60) carried over.
  Tree 1 has 0x111 symbols rounded to 0x112 leaves (256 literals, 16 length codes, 0x110 = end); tree 2 has
  16/64/64 symbols with 4/6/8 raw low distance bits for modes 0/1/2. Bits are read LSB first.
- `src/jet/tests/diff_lzss.cpp`: a real original LZSSCompress (ctor RVA 0x5a00, flag 1) compresses 5 kinds of data
  (random, config-like text, runs, zeros, low-entropy binary) at 16 sizes up to 200 KB, levels 0-9:
  **6 287 checks, 0 mismatches** — our decoder restores every input exactly.
- The original *decoder* is not a usable oracle from a test harness: driven through IOSlaveMemoryDynamic it
  fails reading back across that class's 256-byte blocks; through IOSlaveMemory a stream ending in a long
  back-reference comes out short (buffered IOSlaveWriteCursor + WriteCopy). Valid data in the game goes
  through other paths; our decoder is checked against the original *encoder's input* instead.
- Corrupted streams (informational, not replicated): the original treats distance 0 as "copy what the
  destination already holds at the cursor" (undefined), and it corrupts its heap on bad Huffman streams.
  Ours fails cleanly. Distance 0 cannot occur in valid streams (mode 0 is only used below 256 bytes).

### Jet::LZSSCompress compression (reconstructed and verified 2026-10-19)
- `JetLzss::Compress` produces **byte-identical** output to CompressBlock: 4 800 comparisons (5 data kinds,
  16 sizes up to 200 KB, levels 0-9), 0 differences; whole LZSS test now 11 087 checks, 0 mismatches.
- Level table 0x100194c0 (stride 0x28): {mode, nice length, emitMatch PMF, emitLiterals PMF}; the PMFs are
  zero in the file and filled by the static initialiser 0x1000f540: levels 1-6 use the token emitters
  0x10006410 / 0x10006600, levels 7-9 the Huffman emitters 0x100062c0 / 0x10006390 (Ghidra had no functions
  there: only the table points at them; `ghidra_scripts/CreateFunctionsAt.java`, `XrefsTo.java`).
- Match finder 0x100067d0: hash h = ((h << s) ^ b) & mask over 3 bytes, s = 3/4/5 for buffer < 2000 / < 24000 /
  larger, head + prev chains of u16 positions compared as **signed shorts** (mode 2 positions pass 32767),
  whole chain walked down to pos - window, stop early at the level's nice length; bytes 0, best, 1 checked
  first, byte 2 implied by the hash. Window slides when pos reaches 2*window (0x10006700).
- Main loop 0x10006940: pending literals flushed above 0x101; matches over 0x102 split into 0x102 chunks, a
  1-2 byte remainder goes back to the input. Literal runs of 6+ in modes 1-2 become raw runs.
- Huffman encoder: symbol bits gathered leaf->root as (node & 1), LSB first, 16-bit flushes with x86 shift
  semantics; length table 0x10018158 {code, extra value, extra bits} is derived from the decode table;
  end symbol 0x110 written without a model update, then zero padding to 16 bits.

### Kernel_7c core: inventory and exported layouts (2026-10-19)
- Function areas (non-thunk): unnamed 129 fn / 22.1 KB (mostly plugin system, TaskManager internals, Jet
  container templates), LZSS 37 / 8.9 KB (done), plugin system 85 / 6.9 KB, EditItem* 50 / 5.3 KB,
  CRT 132 / 3.1 KB, App/Task/EntryPoint/LogTask 52 / 2.8 KB, Blowfish 19 / 2.6 KB (done), UId/Version (done).
  `ghidra_scripts/NeighboursOfUnnamed.java` → `out\unnamed_Kernel_7c.tsv` (callers, callees, strings).
- Plugin system internals: PluginSystem (0x48 bytes: ReferenceCount + 5 hash tables at +8/+0x14/+0x20/+0x2c/
  +0x38 + a circular list at +0x44), plugins.ini loader 0x1000acd0 (keys Plugin, PluginGroup, PluginList,
  PluginRemap, AutoPlugin, PluginDir; looks next to Kernel_7c.dll and next to the process), library loader
  0x10009920 (name + "_7c.dll"/"_7c.exe", GetProcAddress "JetGetPlugins" + suffix). Internal only: may be
  rebuilt with our own containers as long as behaviour matches.
- Layouts other modules depend on (must match byte for byte; they match the SDK field order):
  PluginType 0x20 {vptr; UId id +4; PString name +0x14; PluginLibrary* lib +0x18; PluginGroup* parent +0x1c},
  vtable {AddReference, ReleaseReference, scalar deleting dtor}.
  PluginConfig 0x28 {PString name; UId id +4; parent +0x14; lib +0x18; type +0x1c; children array +0x20;
  bool isGroup +0x24; bool isMissing +0x25}.
  PluginInstance: `virtual` ReferenceCount base (vbptr +0, pluginFactory* +4, ReferenceCount {vptr, refs}
  placed last). Differs from the SDK, where the base is not virtual.
- JetGetPluginsKernel registers PluginFactoryOf<LZSSCompress,Compress> (uid 8DBDFEB1-52F4-4c1b-BA91-
  6A7B1CCC6D3E, group 8DBDFEB0-...) and PluginFactoryOf<BFEncrypt,Encrypt> "BlowFish" (AAF296E1-17A7-11d5-
  A9C2-00E01898A4A9, group AAF296E0-...).
- TaskManager options read in 0x1000cb80: "Frame End" task, vSync, FrameRate, lockFPS; WaveServer task
  (0x1000d160). The frame-rate cap lives here: relevant for the performance goals.
- Ghidra note: `FixCdeclMembers.java` sets every function's convention from its mangled name (and adds an
  explicit `this` to __cdecl members); in Kernel_7c they were already right, the remaining noise in calls
  like `Options::Get(this_04, ...)` is lost ECX tracking, so read the asm where arguments matter.

### Plugin system: behaviour spec (read from Kernel_7c, 2026-10-19)
Globals: PluginSystem* at 0x10019c24 (ref-counted; created on first use by 0x1000c460, which also loads the
ini files), one CriticalSection at 0x10019ccc guarding everything below.
- **Loading** (0x1000acd0): Options opts; process `<dir of Kernel_7c.dll>/plugins.ini` if File::Exists, then
  `<dir of process>/plugins.ini` if it is a different path and exists. If OS options have `AutoPlugin`:
  scan "" and every `PluginDir` value (0x1000afd0): Find "<dir>/*.dll" (or "*.dll"), DynamicLibrary::Open,
  Import "PluginList" -> call -> NULL-terminated `const char*` list, each line -> opts.ProcessRaw. Then:
  `PluginGroup` lines "Name:GUID[:ParentGUID]" (0x1000a3e0), `Plugin` lines "Dll:Name:GUID[:ParentGUID]"
  (0x1000a710), `PluginRemap` lines "FromGUID:ToGUID" (0x1000aad0). A line whose parent GUID is unknown is
  skipped. PluginConfig {name, id, parent, lib (Plugin only), isGroup}; added to the parent's children and to
  the id table; also to a name index where a new entry goes *before* existing ones with the same name.
  Libraries are found or created by name (list at +0x44), reused while consecutive lines name the same dll.
- **Library load** (0x10009920, lazy): name "*" -> GetModuleHandle(NULL); ext "exe" -> drop ext, add
  "_7c.exe"; otherwise drop "dll" ext if any, add "_7c.dll"; LoadLibrary. Entry = "JetGetPlugins" + file name
  (no suffix for "*"); missing -> FreeLibrary, mark failed, result 2; else call it, result 0. Load failure ->
  mark failed, result 1. PluginLibrary {vptr, PString name, refs +8, types +0xc, HMODULE +0x10,
  bool +0x14, bool failed +0x15}; refs 0 -> vf1: delete its types, FreeLibrary (unless the name is "*").
- **PluginType ctor**: copy id/name, AddRef system, lib = 0; parent GUID non-zero -> find type in types table
  or make a PluginGroup type (0x28: PluginType + refs +0x20 + member list +0x24) from the group config;
  add to the group's members; register (0x10009fb0): lib = config's lib (and add to lib's type list), add to
  the types-by-parent and types tables. **dtor**: lock; unlink from group (group refs--, deleted at 0);
  unregister; release system. **AddReference/ReleaseReference**: lock; lib->refs++/--, 0 -> unload lib.
- **Lookup by id** (0x1000a020): remap; types table hit -> lib==0 ? result 4 : AddReference, result 0. Else
  config (not missing, lib not failed and not loaded) -> load lib -> retry; still nothing -> config.missing=1,
  unload lib; result 1. By parent+name (0x1000a140/0x1000b290) and by list (Enumerate: non-group children,
  recursing into groups). PluginCollection(uid[, name]) = FastDynamicArray<PluginConfig*> filled the same way.
- **CreateInstance** (Compress/Encrypt/EditItemManager): lookup factory, factory vtable +0xc =
  CreateInstance, then Init (+8 for Compress, +0x24 for EditItemManager) and release on failure; the no-arg
  versions build LZSSCompress / BFEncrypt directly.
- **FastDynamicArray<T,13>** as other modules see it inline: a pointer to a ring of 0x40-byte chunks
  {next, prev, count, T items[13]}; Add appends to the last chunk, a full last chunk gets a new one.
  PluginCollection *is* that array, so the chunk format must stay exact.

### FastDynamicArray<PluginConfig*> reconstructed (2026-10-19): 63/256 Kernel_7c exports ours
- `src/jet/include/jet/FastDynamicArray.h` (own code; layout and behaviour of TS12, not a copy of the SDK header):
  array {head}, Chunk {next, prev, count, data[SIZ]} ring from Jet::Alloc, Iterator {node, head, chunk}.
  All 33 exported members link against the original mangled names via one explicit instantiation
  (`Kernel_7c/PluginConfigList.cpp`).
- TS12 differs from the SDK in: Insert/InsertUnique (first chunk whose last item is not less, then
  BinarySearch 0x10008ca0 inside it, which lands before the first equal item), Index(n) without a bounds
  check (past the end: {null, head, head}), Index(Iterator) computed as items-before-chunk +
  (node - chunk - 0xc) >> 2 (so -3 for End() of an empty array), RemovePack building its result after moving
  the head (the SDK returned the freed head), Pop() on empty reads address 0 (ours returns T()).
- `src/jet/tests/diff_fda.cpp`: 400 random runs of 23 operation kinds with many duplicates, whole chunk ring and
  returned iterator positions compared after every step: **100 306 checks, 0 mismatches**.
- In game: menu, clean exit, JetLog identical to the baseline.
- Method note: the exit "Packed Heap Summary" lines (e.g. `#StringData ... 507k/508k used`) change between
  identical runs (a control run with the old DLL showed it), so `tools/menu_check.ps1` ignores them.
  `tools/bisect_exports.ps1` builds a DLL with a slice of an export group, for hunting in-game differences.

### Plugin system unit reconstructed (2026-10-19): 140/256 Kernel_7c exports ours
- `Kernel_7c/PluginSystem.cpp` (PluginType/Config/Instance/Base/Collection + the internal system),
  `CompressEncrypt.cpp` (Compress/Encrypt bases, LZSSCompress and BFEncrypt over our codecs, the kernel's
  `JetGetPluginsKernel`, CreateInstance helpers incl. EditItemManager's), `KernelStandardRuntime.cpp`.
  Object sizes asserted against the original: PluginType 0x20, PluginInstance 0x14, PluginGroup 0x28,
  LZSSCompress 0x115c (trainz.exe and the tools construct it themselves), BFEncrypt 0x1060.
- Stays forwarded: the vbtables and vbase destructors of Compress/Encrypt (MSVC emits them only for complete
  objects of these abstract classes; the original's copies are fine for derived classes elsewhere).
- Corrections to the spec above: the library list is always searched by name (0x1000a260), and Unload
  calls FreeLibrary only when the name *is* "*" — plugin DLLs are never unloaded, a later lookup reloads them
  (LoadLibrary refcount) and calls JetGetPlugins again.
- Nothing left in the original module reaches the old state: the only forwarded caller of the internals is
  EditItemManager::CreateItem (0x100036f0, lookup by name through vtable slot 10's group id), and no module
  of the game imports any EditItem* symbol.
- `src/jet/tests/plugin_probe.cpp` (+ `D:\trainz12-decomp\probe\run.ps1`): one exe linked to "Kernel_7c.dll" by
  name, run in a directory where that is the original and in one where it is ours, on the real plugins.ini:
  collections by group and by group+name before and after, remaps, lookups by id and by name (these load the
  real plugin DLLs), LZSS and BlowFish through the plugin interface. **2 732 lines, identical** except one
  known original bug (below).
  - Method: the probe must hold a system reference for the whole run, as the game does. Without one the
    system is destroyed and rebuilt (ini re-read) on every call, and "missing"/"failed" flags vanish.
  - Original crashes (null library) on lookups of *group* ids by id or by name; ours returns 1 / 3. When it
    crashes it leaks a system reference, which made the first comparison look different.
  - LZSS DecompressBlock at levels 1 and 5 on 20 000 bytes: the original returns 0 on its own output (the
    IOSlaveMemory short-write bug noted above), ours restores the data. Compressed bytes are identical.

### Main loop reconstructed (2026-10-19): 195/256 Kernel_7c exports ours
- `Kernel_7c/Task.cpp`, `App.cpp`, `dllmain.cpp`, `include/jet/Task.h`: Task, TaskManager, App, EntryPoint,
  TimeEventSystem, GetTaskManager, the "Log" task and the module attach/detach (run under
  ExceptionHandler::Main like the original). SDK 1.0 names; TS12 layout checked by static_assert
  (Task 0x28: MSVC pads the vfptr to the class's 8-byte alignment, so the IListNode links sit at +8;
  TaskManager 0x38; App 0xc) and the vftables of ours and the original compared slot by slot.
- TaskManager (0x38): os +0, lastTime +8, frameEndTask +0x10, taskArray +0x14, taskChangeArray +0x18,
  taskUpdateList +0x1c (IList head, count +0x24), flags +0x28 (initialised, taskChanges), frameRate +0x2c,
  **lockFPS +0x30 (TS12)**, waveServer +0x34. Statics shutdown 0x10019c2c, running 0x10019c2d.
- Frame (0x1000d3c0): pending changes first (Init new tasks, insert updating tasks before the first task of
  equal or higher priority, unlink suspended ones); then the update list works as a queue: pop, Update,
  back of the queue (STOP leaves the list, EXIT ends the loop) until the "Frame End" task (priority FLT_MAX)
  comes round; it goes to the back too.
- Main loop (0x1000cf00): `FrameRate` option (ignored with `vSync`) paces frames with Time::Delay in whole ms
  and carries the error into the next frame, clamped to ±half a frame; `lockFPS` gives a fixed dt =
  1/lockFPS with Time::IncFrame and Time::SetFrameLockTime. This is where frame pacing can be improved later.
- Options read: vSync, FrameRate, lockFPS, WaveServer=On (loads WaveServer_7c.dll unless a task named
  WaveServerTask exists).
- Module attach also interns 21 names ("OnMinZoom" ... "UserInterface") kept by the original, so the string
  pool sees the same strings.
- `gen_def.py`: `name=symbol` lines export our symbol under an original name (used for
  `??_FVersion@Jet@@QAEXXZ`, the compiler's default constructor closure).
- `src/jet/tests/task_probe.cpp` (+ `probe/run_task.ps1`): scripted tasks through Start/Update/Stop on both
  modules (priorities incl. equal and negative, STOP, EXIT, failing Init, Suspend/Resume, delete while
  running, task added while running, UpdateTask, GetTask, restart): **logs identical**. Plugin probe and the
  older diff tests unchanged.
- In game: menu reached on our main loop, clean exit, JetLog identical to the baseline.

### Kernel_7c complete (2026-10-19): 256/256 exports ours, the game runs without the original module
- `Kernel_7c/EditItem.cpp`, `include/jet/EditItem.h`: EditItem, EditItemStack, EditItemManager (61 exports, layout
  below); Compress/Encrypt vbtables and vbase destructors in `CompressEncrypt.cpp`, exported under the original
  names through `name=symbol` lines. vbtable bytes match the original ({0, 0xc} at RVA 0x1051c, shared by both).
  Our `??_D` skips the original's re-store of the Compress/Encrypt vftable pointers on the dying object (not
  observable), then does the PluginInstance reset and `~ReferenceCount` like the original.
- `src/jet/tests/edititem_probe.cpp` (+ `probe/run_edititem.ps1`): items (caps, full caps growing in steps of 13,
  Save/Load flags), stacks (push/pop across chunk boundaries, paged items, failing Load/Undo/Redo, collapse,
  page out), managers made by the plugin system (Initialize with a named mutex held by another thread, temp
  dir clean-up, unique file names, GetItem/SubmitItem/BinItem incl. wrong handles, undo/redo to the ends,
  PageToDisk, CleanupThisTempDataOnDisk): **307 lines, identical**. The random per-process UUID is masked.
- No regressions: plugin probe (2 732 lines, only the known original LZSS bug differs), task probe identical,
  diff tests 0 mismatches (Blowfish 75 008, LZSS 11 087, FDA 100 306, UId/Version 621 000).
- Our DLL imports only KernelStandard_7c, ole32 and kernel32; the generated .def has no forwarders left.
- In game: menu reached, clean exit, JetLog identical to the baseline; **also with `Kernel_7c_orig.dll` moved
  out of bin\** (the 32-bit module list shows only our Kernel_7c).
- Gotcha for agents: the Bash tool turns `\\` into `\` inside heredocs, so a Python fix-up script passed that
  way corrupted every `"\n"` in a C++ file. Write such scripts with the file tool (or use `chr(92)`).

### EditItem block: analysis notes (2026-10-19)
- 0x10002c80-0x10004a20 (~7 KB, 61 exports). No module of the game imports any EditItem* symbol: dead code
  in TS12, but needed for a Kernel_7c without the original. Asm: `D:\trainz12-decomp` scratch dumps.
- EditItem (0x34): own vfptr +0, PluginInstance at +4 (vbptr +4, factory +8), manager +0xc, PString type
  +0x10, caps list (FDA<PString,13> chunks) +0x14, Array<FullCap,13> {count +0x18, capacity +0x1c, data +0x20}
  (FullCap = 2 PStrings, grown in steps of 13), bool paged +0x24, vtordisp +0x28, ReferenceCount +0x2c.
  Ctor takes the PString *by value* plus the vbase flag. Init stores the manager; Save/Load flip +0x24.
- EditItemManager (0x30): vfptr +0, vbptr +4, factory +8, item list (DynamicArray<PString>) +0xc,
  current item +0x10, undo stack +0x14 and redo stack +0x18 (EditItemStack, 0x18 bytes each, ctor(bool)),
  Mutex* +0x1c, PString name +0x20, vtordisp +0x24, ReferenceCount +0x28. Ctor registers
  CleanupThisTempDataOnDisk with atexit. Vtable slots used: +0x28 group id for CreateItem, +0x2c CreateItem,
  +0x30 GetSystemLevelIdentifier, +0x34 item default name, +0x38 GetApplicationUniqueTempPath.
- Temp files: tempFileDir (static PString) = app temp path + "\" + GetUUID() (UId::SetRnd → GetString,
  stripped), made with File::Md; names "Temp_%d.bin" from tempFileCounter under tempFileMutex; File::Prune
  cleans up. Initialize(name) takes a named Mutex and cleans old data when it gets the lock first.

### Kernel_7c on x64: standalone harness (2026-10-19)
- `build64` (`cmake -S src\jet -B build64 -G "Visual Studio 18 2026" -A x64`): Kernel_7c as an object library
  (`Kernel_7c_x64`, `JET_KS_STATIC`) linked with `tests/x64/KernelStandardStub.cpp`, a stand-in for the ~95
  KernelStandard functions Kernel_7c imports. **It is not a reconstruction**: just enough behaviour, following
  the 32-bit module where a probe's output depends on it (PString interning: Destroy only drops the reference;
  Options: one entry per key {key, value, separators, cursor}, `+Key=v` appends `,v` (separator byte read from
  the DLL), values run up to the whitespace before the next `/ - +` switch; ReferenceCount; Mutex).
  No DllMain when linked statically: `StaticModuleAttach/Detach` (dllmain.cpp) via `JET_STATIC_KERNEL` in main.
  Static mode finds plugins.ini next to the exe (`GetModuleHandle("Kernel_7c.dll")` is null).
- Results (`probe/run_x64.ps1`): **task probe 92 lines and EditItem probe 307 lines identical to the original
  32-bit module's output**; new `tests/golden_kernel.cpp`, built for both arches (LZSS all levels on 80 inputs +
  300 corrupted streams, Blowfish key schedule/encrypt/decrypt/error codes, 200 FastDynamicArray runs =
  50 126 steps with the chunk ring hashed after each, 2 000 UIds incl. GetString round trip, 2 000 Versions):
  **305 lines identical** on Win32 and x64. Its 50 failed LZSS round trips are the size-0 input on both (a
  header with no payload is a failure, as in DecompressBlock).
- Compiles clean on x64 at /W3 and with C4267/C4244/C4311/C4312/C4302 enabled: no inline asm, no truncating
  casts (earlier sessions cast explicitly everywhere).
- **64-bit hazards found and fixed** (the Win32 build is unchanged: probes, diff tests, menu check all as before):
  1. FastDynamicArray::Index(Iterator) hard-coded `(node - chunk - 0xc) >> 2`; on x64 the chunk header is 0x18
     and items 8 bytes → now `offsetof(Chunk, data)` / `sizeof(T)`.
  2. PString read the node at raw +8 (length) / +0x18 (text); node is now a struct {next, prev, length,
     capacity, refs, unknown, text[]} (KernelStandard 0x10039cc0); on x64 text is at +0x20.
  3. 17 size asserts are the 32-bit ABI → `JET_LAYOUT32` (include/jet/Platform.h); UId/Version stay exact.
  4. KernelStandard objects Kernel_7c embeds without seeing their members (OS 0x18, Events 0x2c, File::Find
     0x440, IOSlaveMemory 0xc) → `JET_OPAQUE_SIZE(32-bit, provisional x64)`. The real x64 KernelStandard must
     publish real layouts; File::Find holds MSVC8 STL members (see the KernelStandard hand-over notes).
  5. DestroyTimeEvents empties Events' lists at raw 32-bit offsets (+8/+0x14/+0x20, links at +4) → skipped on
     x64; an x64 KernelStandard must empty them in ~Events.
  6. KernelStandardRuntime binds File::Exists by its 32-bit mangled name with GetProcAddress → not built on x64.
  7. Export plumbing is x86-only: gen_def.py and implemented.txt use x86 mangled names, `name=symbol` aliases
     point at `__fastcall` helpers (vbase destructors, Version default-ctor closure). An x64 Kernel_7c.dll
     needs its own export list (x64 manglings differ: `QEAA` for `QAE`/`QAA`). Since trainz.exe will be
     rebuilt too, compatibility with the 32-bit manglings is not needed there.
  8. Only off Windows: UId::data1 and Version::Packed use `unsigned long` as 32 bits (LLP64).
- x64 sizes vs 32-bit: pointer 8/4, PString 8/4, FastDynamicArray<T*,13>::Chunk 0x80/0x40 (data at 0x18/0xc),
  Task 0x30/0x28, TaskManager 0x58/0x38, Mutex 0x10/8; UId 0x10 and Version 4 on both.
- Not portable by design: the differential tests against the original (diff_*, plugin_probe) need the 32-bit
  original DLLs and call them through emulated thiscall; on x64 the golden output and recorded probe logs act
  as the oracle instead.
- Menu-check note: from about 06:40 the exit line `#StringData` reads ~1 100-1 200k used instead of ~508k.
  The older verified DLL (02:24 build) gives the same number, so the cause is the game copy's environment, not
  our module (JetLog otherwise identical).

### KernelStandard_7c: inventory and unit order (2026-10-19)
Full plan in `D:\trainz12-decomp\notes\KernelStandard.md` (private; reproducible inputs in `notes\data`, `notes\ghidra`).
- 1 762 exports (1 649 code, 113 data/vftables), `.text` 327 509 bytes, 2 276 functions. **Only 666 exports are
  imported by anything** (trainz.exe 447, six tool exes 191-311, 21 Jet modules 6-168, Kernel_7c 121). The other
  1 096 are used only inside the module; StringTables has no caller at all.
- About 13% of `.text` is a statically linked IJG JPEG *encoder* (only `Bitmap::SaveJPG`). JPEG *decoding* goes
  through ijl10.dll (32-bit only). Both are to be replaced by libjpeg-turbo; decoding will need a tolerance test.
- The foundation state is **not exported** and orig's own code reaches it by address: the Alloc size-class free
  lists with 16 striped critical sections, the `#StringData` PackedHeapMP, the PStringCache lock, the shared null
  AnsiString buffer, the FileSub (.ja archive) list. So the Kernel_7c method is reversed here: **consumers first,
  foundation last**. Units: U0 proxy, U1-U6 leaves (math, PODs, sync, Rand, small platform), U7 Config (flex
  scanner), U8 Bitmap, U9 JIRF and streams, U10 File family incl. the .ja reader, U11 OS/Window, U12 diagnostics,
  U13 thread/time, and U14 foundation as one cut-over.
- CRT: Alloc blocks are msvcr80 `malloc` with a 4-byte size-class tag (pointers 4 mod 8, a 16-byte alignment
  problem for x64). `File` keeps a CRT file descriptor at +0, so the File family and Log must link the game's
  msvcr80, unlike Kernel_7c.
- Exported objects with MSVC8 STL members (PackedHeap/MP, File::Find, IOSlaveMemoryDynamic) keep their size only
  with hand-rolled containers.
- x64 hazards: Window keeps `this` in window extra bytes via `Set/GetWindowLongA(h, 0)`. ExceptionHandler walks
  EBP frames and uses `CONTEXT.Eip` and `FS:[0]`. Rand, LCRand and Time scale with x87 `fistp` (bit-exactness).
- Options semantics (one entry per key, `+key=v` appends `,v`, values run to the next switch) are recorded in the
  plan and implemented in the x64 stand-in.
- Recommended first: U0 + U1 (35 exports, ~4.3 KB, bit-testable; `StdMath::s_sqrtLUT` is read inline by 10 modules).

### KernelStandard_7c U0 + U1 (2026-10-19): 34/1762 exports ours, bit-exact
- **U0**: `jet_module(KernelStandard_7c)` with 1762 forwarders to `KernelStandard_7c_orig.dll` (all names are
  decorated, so a plain .def covers them, data and vftables included). In game both modules are loaded, menu
  reached, JetLog identical. Kernel probes (plugin, task, EditItem) unchanged through it.
- **Our code calls the not-yet-replaced foundation in the original**: `implib\KernelStandard_7c_orig.lib`
  (gen_implib.py with that module name). Our DLL imports only KERNEL32 and KernelStandard_7c_orig.dll.
- **U1** (34 exports: Crc, StdMath, Vector2/3 conversions, IntLog2/ILog2/CeilPow2/FloorPow2,
  GetBaryCentricWeights, Eigen3, OptimalRects): `src/jet/core/KernelStandard_7c/{Math,Eigen3,Barycentric,OptimalRects}.cpp`.
  The 35 in the plan was one too many.
- `tests/diff_ks_math.cpp` (ours vs `_orig` in one process, raw bits): **all 2^32 inputs** of the five float
  StdMath functions, IntLog2, ILog2(float), ILog2(int), CeilPow2 and FloorPow2; 4 M doubles for Sqrt/InvSqrt(double);
  Crc 200 k; the LUT; Vector ctors incl. signalling NaNs; 2 M Eigen3 matrices (every step separately, incl.
  QL giving up after 32 iterations on 12 231 of them); 3 M barycentric inputs (394 k degenerate); 3 000 random
  OptimalRects runs, 193 k steps with both lists compared after each, plus Process on hand-built pending lists.
  **About 43 billion checks, 0 mismatches.** For inputs that already hold NaNs, Eigen3 (111) and the barycentric
  weights (1 708) differ only in which NaN payload propagates (x87 keeps the larger significand, SSE the first
  operand). Matching that would need the original's SSE operand order, so it is accepted. In game: menu,
  clean exit, JetLog identical.
- Things that only showed up in the asm:
  - StdMath works in x87 at the CRT's 53-bit precision and **returns floats unrounded in st(0)**. We compute
    in double and return through a naked `jmp` thunk on Win32, so an MSVC8 x87 caller sees the same value.
  - `InvSqrt(double)` takes its seed from the **low** 32 bits of the double (a bug, kept). `Sqrt(double)`
    takes it from the value rounded to float.
  - Constants: 0.5f, 1.5f, 1.5, 0.5, 1.0f, 2.0f, ±1e-6 (doubles).
  - `s_sqrtLUT` (512 mantissas with 8 significant bits): the best simple formula (1/sqrt of the bucket
    midpoint, rounded) matches only 474 of 512 entries, so the table looks tuned. It is read at build time from
    the installed game's original DLL by `tools/extract_data.py` into a generated header; the source has no
    copy of it.
  - Copies through x87 `fld`/`fstp` quiet signalling NaNs (Vector ctors, Eigen3 diag[0] and the sort swaps):
    `X87Copy` does the same on Win32.
  - Eigen3 mixes SSE single precision with x87 double precision step by step: the QL convergence test sums
    in double, the rotation's new off-diagonal uses the unrounded double square root, c and s are floats. Its
    4x unrolled scan loops only run for counts above 3, so they are dead for 3x3. `operator[](i)` indexes the
    object as a flat float array from offset 0. Layout: m[3][3], diag[3], subd[3] (0x3c).
  - OptimalRects ({set head, tail, count, pending head, tail, count}, 0x18-byte nodes {Rect{x,y,w,h},
    next link, prev link}, links pointing at link fields) uses custom register conventions in its 15 split
    helpers. Kept off-by-ones: the strips cut in cases 3, 6, 10 start at the member's last column, and in
    case 12 at its last row, ending one row short. Validate is an empty function (ICF-folded with an empty
    destructor).
- Win32 build note: `/arch` default (SSE2) plus /fp:precise gives the original's single-precision SSE steps
  exactly. The `double` expressions stand in for its x87 code. The game uses OpenGL, which leaves the x87
  precision at 53 bits; D3D9 without FPU_PRESERVE would set 24 bits and change the original's results.

### KernelStandard_7c U2-U5 (2026-10-19): 242/1762 exports ours, bit-exact
- **U3** small PODs (`SmallTypes.cpp`): 11.15 M checks, 0 mismatches.
- **U4** sync objects (`SyncObjects.cpp`: SyncObject, Mutex, Semaphore, WEvent, CriticalSection, SingleLock,
  MultiLock, SWMRGuard): `tests/diff_ks_sync.cpp` runs a 319-step two-thread scenario on both modules,
  identical results over 3 runs. `SingleLock::operator=` copies the whole object, padding bytes included. The
  default-constructor closures (`??_F`) are exported as aliases of `__fastcall` workers that use `::new`
  (the class `operator new` hides placement new). Data still owned by the original is declared
  `JET_KS_FORWARDED` (dllimport) in our headers.
- **U5** Rand / LCRand (`Rand.cpp`): Rand 16 M checks, 0 mismatches. LCRand 8 M checks, 1 difference, which is
  NaN-only (NextDouble with two NaN arguments). Details from the asm:
  - Scale 1/(2^32-1) (bits 0x3DF0000000100000).
  - Schrage step for 16807 x mod (2^32-1); t <= 0 becomes t - 1.
  - Integer results go through `_ftol2_sse`, which is a 32-bit `cvttsd2si`, so out-of-range values give 0x80000000.
  - The Gaussian's spare value lives in one global for all generators.
  - Randomise reads `Time::frameLock`/`frameLockTime`. The test sets these to make it deterministic.
  - Float results come back unrounded in st(0). Their exports are aliases of `__fastcall(this, edx, ...)`
    workers returning double, which has the thiscall ABI.
- **U2** Matrix3 / Matrix4 / Quaternion (`Math2.cpp`, 29 exports):
  - `tests/diff_ks_math2.cpp` makes 200 k random cases per member. Entries come from four distributions, 2 %
    of them are special values, and some inputs are aimed at each branch (rotation matrices, equal/close/
    negated quaternions for Slerp, near-gimbal quaternions, up parallel to the axis).
  - The whole object is compared as raw bits, and GetDeterminant as the st(0) double.
  - Result: **5.8 M checks, 0 mismatches**, plus 1 962 differences that are NaN payloads only, accepted as in U1.
  - The SSE expressions were lifted from the listing with the new `tools/sse_lift.py`. It turns a block of SSE
    scalar code into SSA, or into nested expressions with the original association.
  - Matrix3 members are thiscall, Matrix4 members `__cdecl` with `this` on the stack (QAA), Quaternion thiscall.
    Matrices are row-major. Rotate(x,y,z) rewrites rows 0-2 (all 4 columns for Matrix4). It skips `fsincos` for
    an angle that is exactly 0, so -0 gives sin +0. RotateX/Y/Z and SetRotate* have no such check.
  - The Matrix4 RotateX/Y/Z and SetRotAxisAngle functions touch only the upper 3x3. SetRotate*(a, keepRest)
    set column 3 and row 3 to the identity when keepRest is false. Negations are `-0.0f - x` (subss), not
    sign flips.
  - Invert computes the adjugate in place and multiplies it by 1/det. There is no singularity check.
    GetDeterminant uses the same SSE cofactors but expands them in x87 double: ((m12 C3 + m8 C2) + m4 C1) + m0 C0.
    It returns the value unrounded, through an alias to a `__cdecl double (const Matrix4*)` worker.
  - Quaternion::Set(x,y,z) builds three identity Matrix3s, applies Rotate(x,0,0), Rotate(0,y,0) and
    Rotate(0,0,z), then calls Set(Rx * (Ry * Rz)).
  - Set(Matrix3) and Set(Matrix4) take the square roots in x87 double, then normalise. The length is
    computed in x87 and the scaling in SSE.
    - Matrix3 branch tests: trace >= 0.
    - Matrix4 branch tests: trace > 1e-6f, and |r| > 1e-6f before dividing.
    - When |r| <= 1e-6f, the scale factor is r itself (a bug, kept).
  - Slerp copies `a` when a == b, using x87 copies for y, z and w. It negates b when the dot product is
    negative. It uses the linear weights when 1 - dot <= 0.001f. Otherwise it runs `_CIacos` and three `fsin`
    in extended precision.
  - GetEulerAngles takes the gimbal branch when |sp| > 0.99999f. That branch gives (0, pi/2, acos(clamped)).
    The normal branch uses `_CIasin` and two `fpatan`.
  - Set*Axis / SetViewVector:
    - Normalise the axis (InvSqrt from U1).
    - Cross it with up. Below |c| <= 0.0001 the default basis rows are used.
    - Feed the Matrix3 to Set(Matrix3).
    - Row orders: X (n, c, t), Y (c, n, t), Z (t, c, n), View (c, t, -n).
    - SetXAxis uses c = up x n. The others use c = n x up with t in the opposite order.
  - x87 transcendentals (`fsincos`, `fsin`, `fpatan`, msvcr80 `_CIacos`/`_CIasin`) are inline asm on Win32.
    The CRT intrinsics are looked up in the process's msvcr80.dll, the one the original calls. On x64 they fall
    back to <cmath>. Math2.cpp is also compiled into the x64 stand-in.
- Checks after U2: diff_ks_math/small/sync/rand unchanged. Kernel probes (task 92, EditItem 307, plugin 2 732
  lines) match the previous run, and the original's known plugin-probe difference is unchanged. The x64 harness
  shows 0 differences. In game, with all of U1-U5 installed: menu reached, clean exit, JetLog identical to
  the baseline.

### Step 5, small Jet modules (2026-10-19, in progress)
- Sizes of code in the five modules (Ghidra function sizes): KernelResources ~10 KB, InputWin32 ~13 KB,
  Render2DCore ~17 KB, KernelResource ~31 KB, InputSystem ~73 KB.
- Dependencies:
  - KernelResource is the base. KernelResources, Render2DCore and InputSystem import it.
  - Render2DCore also imports RenderSystem.
  - InputWin32 is a plugin built on InputSystem.
  - All five also import Kernel_7c and KernelStandard_7c.
- **U0 proxies for all five** (`jet_module` + empty `implemented.txt`): all exports forward to `<module>_orig.dll`.
  In game: menu, clean exit, JetLog identical.
- **KernelResources_7c: 51/51 exports ours** (`src/jet/core/KernelResources_7c/BitmapResources.cpp`, headers
  `jet/{Resource,KernelResources,Bitmap,Config,Result}.h`). It holds:
  - BitmapBaseResource, BitmapFrameBaseResource and BitmapResource, plus their instances.
  - Two internal resources: BitmapFrameResource ("bmframe.txt": `Frame { Image, Area }`, an area of another
    bitmap resource) and ColourResource ("colour.txt"/"color.txt": `Color { Red Green Blue Alpha Blend }`,
    rendered as a box, mode 5 when blended, else 1).
  - The plugin table: ResourceType<T> factories for tga (UId ...81), bmp/jpg/pcx (all ...82), bmframe.txt (...83),
    colour.txt (...84) and color.txt (...85), in the group 4C414E80-BBDA-11d4-9AD7-0001025364B8.
- Things from the asm:
  - Resource has 18 virtuals, all `__cdecl` except the destructor. The order is Lock, Unlock, LoadAll,
    IsAllLoaded, SetPriority, Add/RemoveObserver, GetMustResetWhenFinished, ~Resource, CreateInstance, Load,
    LoadTxt, LoadComplete, OnLoadNotify, Unload, CalculateSize, CalculateDataSize, GetRedirection.
  - The bitmap classes add Render x3 (slots 18-20, overloads in reverse declaration order), GetBitmap, GetArea,
    GetOffset, and SetBitmap for BitmapResource.
  - Result codes are `ResultClass<&X::results, X::Results>` (one int, returned in memory). `jet/Result.h`
    reproduces the mangling, so exports such as `BitmapResource::Load` link under the original names.
  - The rectangle intersection (0x10001840) clears the wrong fields when the result is empty: x for an empty
    height and y for an empty width. Kept.
  - The tiled Render clips each tile to the area's right/bottom edge (not the left/top) before the clip rect.
  - BitmapResource::Load halves both dimensions until w*h <= MAX_STREAMING_TEXTURE_SIZE^2 (default 2048) and fails
    if either reaches 0. It scales with Bitmap::Scale.
  - BitmapBaseResource::GetBitmap throws `Jet::Exception` ("GetBitmap() not implimented", line 0x118).
- Checks:
  - `tools/compare_vtables.py` shows the three exported vftables match slot for slot (same import in every
    imported slot).
  - `tests/diff_krs.cpp` drives both modules in one process with a stand-in resource whose vtable logs calls, and
    with real KernelStandard bitmaps:
    - the three Render variants (random areas, clips, justifications, repeat limits) are compared on destination
      pixels and call logs;
    - all BitmapBaseInstance members;
    - BitmapResource is built through its constructor and loads generated TGA files through a stand-in database,
      with small streaming limits.
  - Result: **1 096 760 checks, 0 mismatches**.
  - Limit: in this harness Bitmap::Scale returns null for the 32-bit format, so the scaled loads end with no
    bitmap in both modules.
  - The factories and the two internal resources are only covered in game: menu, clean exit, JetLog identical.
- **Render2DCore_7c: 85/85 exports ours** (`src/jet/graphics/Render2DCore_7c/{Canvas,Sprites}.cpp`, headers
  `jet/{RenderSystem,Render2DCore,VCall}.h`):
  - Canvas: an off-screen surface drawn as up to 7 x 5 textures, holding at most 16 dirty rectangles per tile;
    more than 16 turn into the whole tile.
  - SelectionBox: a translucent quad with a dark and a light outline, at a 0.375 pixel offset.
  - SpriteInstance and SpriteManager: priority-sorted sprites, drawn higher priority first via qsort.
  - SpriteResource ("sprite.txt": `Sprite { Texture UV UVI Color Size Position Rotation Scale AlphaBlendMode
    AlphaTestMode AlphaTestFunction AlphaRef SrcFactor DstFactor }`; UId ...8A).
- RenderSystem is only partly declared. Layer and Overlay are declared with their TS12 layout (0x20, 0x3c) and
  11-slot vtable: dtor, RenderLayer, UpdateLayer, Collides x2, Reset, Render, Update, Begin, End, CanRender.
  Display, Texture and the kernel are opaque. They are called through `VCall` wrappers by vtable offset:
  - Display: +0x80 texel offset (a float in st(0)), +0x98 create texture, +0xa0 set texture, +0xe0/+0xe4 draw,
    +0x10c vertex declaration.
  - Texture: +0x04 create, +0x0c upload, +0x10 update a rectangle, +0x20/+0x44 bind, +0x38 size, +0x3c touch.
  - Kernel: +0x7c Display, +0x94 texture manager.
  - Display +0x54 is the state block that Display's non-virtual setters cache.
- Things from the asm:
  - SpriteInstance's 0x50-byte attribute block is a copy of the resource's: position (z is the priority), size,
    uv, colour, rotation, scale, alpha flags and factors.
  - The vertices are 4 x {x, y, z, colour, u, v}.
  - EnableFixedPixelSize divides both sizes by the screen *height* (kept).
  - Canvas::Render shifts the texture coordinates by Display's texel offset over the tile size.
  - The rotation's cos/sin are separate x87 fcos/fsin rounded to float.
  - The same rectangle intersection as KernelResources, with the same wrong-field clearing, is kept.
- Bug found by the test: SpriteInstance and the layers need `JET_HEAP_OBJECT`. Their deleting destructors free
  with `Alloc::Delete`, and SpriteManager::Purge deletes sprites that callers allocated in Jet's heap.
- `tests/diff_r2d.cpp` drives both modules from identical memory against a stand-in renderer. The Display's
  virtual slots log every call, draws included with all vertex data. Textures, the texture manager and the kernel
  log their calls too. RenderSystem's real non-virtual Display setters run on the stand-in's state block.
  - Covered: SelectionBox render; the whole Canvas cycle (Create / setters / Update with random dirty lists /
    Render with texel offsets and wire frame / Destroy); sprites (Open with UVI handling, all setters, the
    attribute update with rotation and aspect correction, Render with the alpha save/restore, SpriteManager
    Add / Render / Update / Remove / Purge).
  - Compared: the call logs, the Display state and the objects' memory.
  - Result: **249 020 checks, 0 mismatches.**
  - SpriteResource is not exported. The test finds its vtable through RTTI.
  - Limit: its loader cannot be compared outside the game. ConfigParser opens the file, then MemoryFile::Open
    fails (result 3), even for the game's own config files. Only the failure path is compared.
- In game: menu, clean exit, JetLog identical. The menu does not seem to draw sprites.
- Harness notes:
  - Resource's constructor reads `Resource::resourceSystem` (+0xdc). The test points it at a zeroed buffer.
  - KernelStandard's 32-bit bitmap format is enum value 2.
- New public tools:
  - `tools/annotate_asm.py`: labels a dumpbin listing with import names (demangled), string literals and export
    names.
  - `tools/compare_vtables.py`.
- Modules in the game process at the menu (32-bit PowerShell, `probe/modules_run.ps1`):
  - Kernel_7c, KernelResources_7c and Render2DCore_7c run without their `_orig`.
  - Both input drivers are loaded: InputWin32 and InputDirectX, plus DINPUT8. **The game takes keyboard and
    mouse from InputWin32**: it calls Init and creates exactly the keyboard and the mouse (in-game trace below).
- **InputWin32_7c: all ours** (`src/jet/input/InputWin32_7c/InputWin32.cpp`; its only export is
  `JetGetPluginsInputWin32`). Header `jet/InputSystem.h` holds InputDevice, InputDriver, InputDeviceCaps,
  InputData, DeviceProperty, ObserverSubject and AutoReference. Header `jet/Window.h` holds the Window members the
  driver calls. Factory "Win32" 445ED0AC-..., group F3117B18-...
  - Keyboard: a window hook turns WM_KEYDOWN/UP, WM_SYSKEY* and WM_CHAR/WM_IME_CHAR into Key/Char events.
    Lock-key modifiers sit in bits 20..27, and Print Screen only sends a release.
  - Keyboard Update polls GetKeyboardState through a VK → Jet key table (the original builds it at load time).
  - Keyboard properties: the Caps/Num/Scroll lock state. Setting one toggles it with SendInput.
  - Mouse: hook-driven moves, buttons and wheel, including the old MSWHEEL_ROLLMSG message.
  - Mouse properties: range, threshold, position and exclusive mode (a clipped, hidden cursor kept at the window
    centre).
  - Joystick: WinMM joyGetPosEx. 8 axes are scaled into ±32767. 32 buttons, with optional auto-repeat. One POV.
  - All 11 vftables match slot for slot (`compare_vtables.py --rtti`: the classes are found by RTTI).
  - Quirks kept:
    - The original's `_ftol2` takes the SSE2 path (cvttsd2si), so out-of-range axes become 0x80000000.
    - WM_XBUTTONDOWN reports a release.
    - The joystick's auto-repeat flag stores the raw byte it is given.
    - The axis ranges sit at a 12-byte stride with overlapping tails.
  - Bugs found by the test (both fixed):
    - joyGetPosEx flags must be 0x2bf (all axes, buttons, POVCTS, no JOY_RETURNPOV).
    - The auto-repeat byte must be kept as given, not normalised to a bool.
- `tests/diff_iw32.cpp`: nothing is exported, so each module's **import table is patched** instead.
  - Every Win32 call (user32, winmm, GetVersionExA), Window / OS / Time / Alloc, the InputDevice constructor,
    destructor and events, and the plugin type base go to logging stubs. The stubs answer from a seeded generator.
  - Both modules run the same scenarios:
    - the factory from JetGetPlugins creates the driver;
    - Init, then Enumerate for each device type;
    - CreateDevice for keyboard, mouse, both joysticks and an unknown id;
    - random properties, updates, critical shutdown / startup;
    - window messages through the registered hooks;
    - release.
  - Compared: the call logs and the objects' memory, with pointers named by allocation number and RTTI / vbtable.
  - Result: **1 462 488 checks, 0 mismatches** over 10 000 scenarios, stable across runs.
- In game (`probe/input_run.ps1` + `tools/input_check.ps1`, real SendInput clicks and keys), originals vs ours:
  - Steps: hover; click SELECT ROUTE; click the search box and type (the Russian layout gives "ьщо": WM_CHAR);
    3 × Backspace; open the Main menu dropdown; Esc; Back.
  - The same screens and the same text. The only differences are time-driven: the route preview slideshow, the
    caret blink, and a 7-pixel strip at the bottom edge.
  - The optional trace (`-DIW32_TRACE`, off by default) logged Init, the keyboard and mouse creation, the button,
    key and char events with the expected codes (cp1251 0xfc 0xf9 0xee, Backspace, Esc), and the mouse moves.
  - Menu check with the final build (trace off): clean exit, JetLog identical to the baseline.
- Screenshots of the game:
  - GDI copies (BitBlt, PrintWindow) of the GL window come out black.
  - `tools/dupshot.cpp` (DXGI desktop duplication) sees it, together with whatever window is on top, so the
    check makes the game window topmost.
  - Print Screen is no use on Windows 11: it opens the Snipping Tool overlay, which takes the focus.
- **InputSystem_7c** (327 exports, 147 KB) is replaced unit by unit; the rest forwards to `_orig`.
  - Planned units:
    - U1 ObserverSubject + observer bases (done);
    - U2 InputDevice / Keyboard / Mouse / Joystick / InputDriver + device ids (done);
    - U3 ASCIICode / InputStrings;
    - U4 EmuDevice;
    - U5 Input;
    - U6 InputExtension / InputUIExtension;
    - U7 InputAction / InputActionMapper;
    - U8 ForceEffect / ForceResource + the plugin table.
  - Jet SDK Beta2/RC1 `ObserverSubject.h` is the template's source. Its input headers describe an older design and
    do not apply.
- **U1, ObserverSubject<T>** (`src/jet/input/InputSystem_7c/Observers.cpp`, explicit instantiations for InputObserver,
  InputActionObserver and InputUIExtensionObserver):
  - Layout: `{vptr, FastDynamicArray<UncertainObserver, 4> observers, int observerLock}`. An entry is
    `{bool isValid, T* observer, int priority}`, and a chunk is 0x3c bytes.
  - TS12 adds a priority: the list is kept sorted highest first (Less = `a.priority > b.priority`). Re-adding an
    observer with another priority moves it.
  - While locked, removals only clear the flag; the last unlock drops the cleared entries.
  - The copy keeps the chunks as they are (memcpy, new `FastDynamicArray::AppendChunks`). operator= has no
    self-check, so a self-assignment empties the subject.
  - The slots of a fresh chunk are default-constructed (flag and priority 0). `NewChunk` now does that; it is a
    no-op for Kernel's pointer arrays, and diff_fda is still 0 mismatches.
  - The observer bases' virtuals do nothing (folded bodies).
  - InputUIExtensionObserver's slots:
    - 0 OnInputGetEvent and 1 OnInputLostEvent (from InputUIExtension::Activate);
    - 4 OnMouseEvent;
    - 2 and 3 (Keyboard / Char) are not yet checked.
  - `tests/diff_is_obs.cpp`: all three instantiations with random add / remove / lock / unlock / iterators /
    copy / assignment. Compared after each step: the return values and the whole chunk ring.
    **1 753 750 checks, 0 mismatches.**
- **U2, devices** (`src/jet/input/InputSystem_7c/{Devices,Ids}.cpp`):
  - InputEvent is 0x14 bytes `{type, code/down | ch | id, value, delta, time}`.
    - The kind sits in bits 4..7: 0x10 lost, 0x20 get, 0x30 key/button, 0x40 char, 0x50 axis, 0x60 pov,
      0x70 property.
    - NotifyObservers ORs the device type into bits 0..3 and drops events from untyped devices.
    - A STOP from an observer ends the round, except for lost / get / property events.
  - The InputDevice constructor zeroes both states, sets valid and takes a driver reference. The instance id is
    left as is.
  - Joystick state: buttons[32], pressure[32] (GetButtonPressure), delta[8] and position[8] (x y z rx ry rz u v),
    pov[4].
  - MouseDevice::SetPosition / SetBoundings call SetProperty twice on one local. SetPosition never sets `b`.
  - The base virtuals (GetCaps, Update, InputDriver's Init / Enumerate / CreateDevice) have bodies, because
    NullInputDevice keeps them. Our base CreateDevice writes a null result, while the original only clears eax;
    nothing calls it.
  - All 13 exported vftables of U1+U2 match. The InputDriver vbtable `{0, 12}` matches.
  - `tests/diff_is_dev.cpp`: devices of all four kinds around a stand-in driver.
    - Stand-in observers log each event and, during the notification, return STOP, remove themselves or add
      others.
    - Covered: every event function, the accessors, Done / Critical*, the base virtuals, and SetPosition /
      SetBoundings (caught through a patched vtable slot).
    - **583 202 checks, 0 mismatches, 181 159 observer calls.**
  - In game with U1+U2 (plus our InputWin32): the same input scenario gives the same screens; menu check clean,
    JetLog identical to the baseline.
- **U3, ASCIICode / InputStrings** (`src/jet/input/InputSystem_7c/Strings.cpp`):
  - Both are singletons built on first use.
  - ASCIICode is 256 entries of `{normal, shifted, kind}`. The kinds are control, letter (shift xor caps),
    symbol (shift), keypad operator, and keypad digit (NumLock only).
  - InputStrings holds 314 PStrings: 256 keys, 37 buttons (32 joystick + 5 mouse), 8 axes, 4 povs and 9 pov
    directions ("KC_Esc", "BC_Left", "AC_X", "PC_1", "NE").
  - Both tables are taken store by store from the original constructors by a small mov emulator. The 166 name
    assignments are in the original order; BC_32 is assigned twice.
  - The reverse lookups compare interned nodes. A name not found gives 0 for keys and pov directions and -1 for
    the others.
  - `tests/diff_is_str.cpp`: whole tables, every lookup over the code range, copy / assign / destroy.
    **2 711 checks, 0 mismatches.**
- **U4, EmuDevice** (`src/jet/input/InputSystem_7c/Emu.cpp`, 0x360 bytes): a mouse or joystick played from the keyboard.
  - Keyboard events go into a 256-event ring and are played in Update:
    - bindings `{key, target, shift/ctrl/alt, kind, direction}` are kept sorted by key;
    - buttons press and release;
    - axes accelerate while their key is held (`start ± speed·t²/2`), and joystick axes may auto-centre;
    - povs take hundredths of a degree.
  - Original bugs kept:
    - Bind packs the low word of the key's argument slot (the target store is dead code), so the joystick
      defaults land on button / axis 0.
    - GetCaps' bit count uses `(x & m) > s` for `>> s`.
    - The joystick's AxisEvent reports the mouse fields.
    - A lost event is never delivered: the state, type included, is zeroed first.
  - Load reads a config through a flex scanner (yyFlexLexer) built into the module. It stays the original's for
    now: our code calls `InputSystem_7c_orig.dll`'s Load on our object (new `implib/InputSystem_7c_orig.lib`).
    The game uses it only with the `InputEmulation` option.
  - `tests/diff_is_emu.cpp`: Time::Ms is patched in both import tables. Covered: random bindings and defaults,
    key floods past the queue, updates, properties, Done.
    **329 376 checks, 0 mismatches** (axis / button / pov / property / get events all seen). Init needs a real
    Input (U5).
  - In game EmuDevice is still built by the original Input until U5 is ours.
  - Menu check with U1-U4: clean, JetLog identical.
- **U5, Input** (`src/jet/input/InputSystem_7c/Input.cpp`, 0x44 bytes) and the **InputExtension** base (0x10):
  - Init loads one driver per type from the "InputKeyboard" / "InputMouse" / "InputJoystick" options ("Default" is
    Win32, Win32, DirectX; factories looked up in group F3117B18-...). A name with no factory ends that type's search;
    a factory already loaded for another type is shared; the first failing type stops the loading, Init still
    succeeds.
  - GetDevice builds the emulated mouse / joystick itself; other ids are looked up among the devices handed out,
    else created by the driver of the type the drivers reported (pushed at the front of the list, updated once).
  - Original quirks kept: EnumerateAll appends to the caps list without clearing it; the copy and the assignment
    copy the driver and extension pointers without references (the assignment has no self check); a type reported
    with no driver is called through null.
  - The device list removes and inserts by moving AutoReferences one by one (0x10007eb0 / 0x10007f90), kept.
  - `tests/diff_is_input.cpp`: both modules' Inputs against stand-in factories, drivers, devices and extensions,
    with Options, the OS, the window, the crash-handler registry, LookupPluginType and Profile patched in both
    import tables. **305 301 checks, 0 mismatches** (3122 devices, 511 emulated devices, 1520 extension Done calls);
    a deliberate change in Input.cpp gives 621 mismatches.
  - In game: menu check clean (JetLog identical); the input scenario gives the same screens.
- **U6, InputUIExtension** (`src/jet/input/InputSystem_7c/UIExtension.cpp`, 0x148 bytes), used by trainz.exe and
  InterfaceSystem:
  - Focus byte +0x108: 0x01 keyboard, 0x80 mouse, 0x02 deactivated; observers hear lost / got only when both
    devices have the focus (0x81).
  - Keys held as a 256-bit set (+0x10c); repeats of a key not down and releases of a key not down are dropped.
  - Mouse events {header, buttons, x, y, dx, dy, z, dz}: header bits 0..5 modifiers, 7 down, 16..23 kind (1 down,
    2 up, 3 click, 4 double click, 5 move, 6 drag start, 7 drag end, 8 position), 24..28 button. Moves are summed
    and sent once per Update.
  - Double click: a second release within doubleClickTime (250 ms) and < 8 px of the first click. Drag: a held
    button moved > 50 px, or > 2 px after 200 ms.
  - Original quirks kept: the button index is not range-checked; the constructor leaves byte +0x109 (attached) as
    is; UpdateMouseButton calls Update(0) first through the vtable.
  - `tests/diff_is_uiext.cpp`: both modules' extensions on their own Input, stand-in keyboard / mouse / observers
    (which stop rounds and remove themselves), random event streams plus click bursts. **1 019 467 checks,
    0 mismatches**, every mouse event kind seen (2150 double clicks, 530 drag ends); a deliberate change gives 36.
  - In game: the input scenario gives the same screens (only the route preview slideshow, the caret and a few
    pixels of the bottom strip differ, as between two runs of the original); menu check clean.
- Who uses the rest (imports of the game's modules): InputAction / InputActionMapper (U7) by nobody; ForceEffect
  (U8) by InputDirectX only.
- **U8, force feedback and the null driver** (`src/jet/input/InputSystem_7c/Force.cpp`):
  - ForceEffect (0x88) passes every call to its attached child effects; Create(name, db, flags) builds one effect
    per entry of a ForceResource.
  - ForceResource (0x68) loads / saves DirectInput .ffe files: RIFF "FORC", the file GUID, one LIST per effect with
    "data" blocks, 11 effect type GUIDs (13541C20..2A except custom).
  - Original quirks kept: an unknown effect is skipped by seeking to start + size, 8 bytes short. SaveEffect writes
    the name from the start of a template buffer ("efctHere goes effect #%d name ...", 0x44 bytes, NUL at 0x43).
  - The null driver and device ("Null", "System stub", type 8). `JetGetPluginsInputSystem` registers the "ffe"
    resource type and the "None" input driver factory.
  - `tests/diff_is_force.cpp`: an in-memory File with a read budget (the original's LIST loop can spin), plugin
    factories through a patched PluginType constructor. **41 067 checks, 0 mismatches.** Menu check clean.
- **U4b, the binding scanner and EmuDevice::Load** (`src/jet/input/InputSystem_7c/Lexer.cpp`, `Emu.cpp`):
  - The module's flex 2.5 C++ scanner (36 rules, REJECT-style state buffer, yylineno, 7-bit with bytes > 0x7b
    clamped to class 1) is written out as the flex skeleton. The DFA tables (acclist, accept, ec, meta, base, def,
    nxt, chk) are read from the installed original at build time (`extract_data.py`, never committed).
  - Tokens: numbers (hex, int, float), true / false, inc / dec, pressed / released / changed, positive / negative,
    the pov directions, +shift / +ctrl / +alt, and KC_ / BC_ / AC_ / PC_ names.
  - JoinValues (0x10002550) joins an entry's values with spaces (count from the list data, or the first value).
  - Load reads the "MouseEmulator" / "JoystickEmulator" section. Each entry's name is lexed for a button, axis or
    pov, and its values for the key, modifiers, speed, direction and pov direction. The load is complete when button
    0 and both directions of X and Y are bound.
  - Original quirk kept: an unknown axis given a speed writes at 0x260 + 0x14 * 0xff, past the object.
  - `tests/diff_is_lex.cpp`: our scanner against the original's, called inside the module by address. Inputs are
    random texts made of the scanner's words, the names, numbers, punctuation and random bytes (cut words, random
    tails), through both constructors. **1 959 637 checks, 0 mismatches**; mutations of a rule or of the clamp are
    caught.
  - `tests/diff_is_emuload.cpp`: KernelStandard's ConfigParser gives "Syntax error." on any non-empty text outside
    the game, even a real config.txt, from a file or from memory. So the ConfigParser / ConfigData imports of both
    modules are patched to stand-ins serving random trees, including parse failures, no root, empty lists and lists
    with no array. **5 000 trees, 0 mismatches** (1080 complete loads). Three mutations are caught; swapping the up
    and down bits of `bound` is equivalent (all four bits are required).
- **U7, InputAction / InputActionMapper** (`src/jet/input/InputSystem_7c/ActionMapper.cpp`); nobody in the game imports
  them:
  - An action (0x20 bytes) is a name, a flush flag and an `Array<StrokeDef, 2>` of strokes `{code, ms}`. The code
    holds the kind in bits 24..31 (key, button, axis, pov) and the code in bits 16..23.
    - Key: bit 7 pressed, bits 2 / 1 / 0 shift / ctrl / alt.
    - Axis: modification (changed / inc / dec) in bits 8..9, direction (any / positive / negative) in bits 10..11.
    - Pov: direction in bits 12..15 (8 = centred).
  - The mapper (0x34 bytes) keeps action lists by name in a 97-bucket table. The hash is `h = h*8 ^ c` over the
    last 16 characters, so only the last 11 count. Buckets are FastDynamicArray<ActionList*, 4>.
  - Each added device (0x58 bytes) gets copies of the actions it can issue, as `InputActionMapper::DeviceAction`
    (RTTI name) with a progress count and the time of the last stroke. It also gets per-axis dead zones taken from
    properties 3 and 4.
  - OnInputEvent advances the copies:
    - a timeout is checked against the next stroke's time;
    - lost / get events reset the progress;
    - a property 3 / 4 event re-reads the zones.
    - A completed action is reported through `InputActionObserver::OnInputAction(device, name, event)`. STOP stops
      the event only if the mapper is exclusive. A flush resets the device's other actions and ends that device's
      turn.
  - Load / LoadActionMap read "actionmap" sections with the module's scanner. Save writes the same format through
    File::WriteLine. Results: 2 invalid, 3 partial, 4 not supported, 5 file error.
  - Original quirks kept:
    - InputAction's operator= appends strokes; operator== compares only the left side's strokes.
    - EndAdd takes the count off the capacity (an empty action with room gets count = capacity).
    - The zones are read through the event's code, past the eight for other codes.
    - Inc wants a falling value on the negative side, dec a rising one.
    - Save's `%0d.%0d` writes 5 ms as "0.5".
  - `tests/diff_is_mapper.cpp`, both mappers driven with the same random steps:
    - actions built through each module's own InputAction exports;
    - stand-in devices, logging observers that sometimes stop;
    - action maps served through patched ConfigParser / ConfigData imports;
    - Save captured through patched File::Open / WriteLine / Close and UId::SetRnd;
    - events mostly aimed at the next stroke of a bound action, some exactly at its deadline.
    - The whole mapper state (buckets in order, lists, devices, zones, bound actions, references) is compared
      after every step.
    - **1 111 475 checks, 0 mismatches.** Every result code was seen, with 3 160 notifications, STOPs and flushes.
    - Mutations caught: the timeout boundary, axis inc flags, Save's modifier order, a hash over 8 characters, and
      STOP when not exclusive. A hash over 15 characters is equivalent.
  - vftables: every exported one matches, plus DeviceAction by RTTI. Only the scanner's FlexLexer classes differ:
    ours has no virtuals, by design.
- **InputSystem_7c complete (2026-10-21): 327/327 exports ours.**
  - The module no longer links `InputSystem_7c_orig.lib`.
  - In game with InputSystem_7c_orig.dll removed: menu check clean, JetLog identical.
  - Input scenario: the same screens as the previous run. Only the caret and the network-driven "Content updates"
    counter differ.
  - All InputSystem tests (obs, dev, str, emu, input, uiext, force, lex, emuload, mapper): 0 mismatches.
- **KernelResource_7c complete (2026-10-21): 187/187 exports ours, nothing forwarded.**
  - Sources: `Resource.cpp` (Resource, RedirResource, ResourceVariant, ResourceInstance, the three bins),
    `Database.cpp` (ResourceDatabase and its 401-bucket table, the nameless database), `System.cpp`
    (ResourceSystem, its Task, the loader thread and its heap), `FileExists.cpp` (File::Exists as the original
    imports it). Analysis: `notes/KernelResource.md`.
  - Original quirks kept:
    - Remove(const Resource*) finds by name, and two resources of one name can share a table. Deleting the second
      one takes the first one's slot, and the first one's own Remove then finds the second by the name left in its
      freed memory. So `~PString` no longer zeroes the node, as in the original (PString.h, all modules).
    - The cache bin's priority is NextFloat + oldest + time, summed in double and rounded once. NextFloat is a
      double function in KernelStandard and is called as one.
    - The next size check is a static, so a new ResourceSystem skips CheckSize until its clock passes the old one.
  - `tests/diff_kr_system.cpp`, both modules with their own ResourceSystem in one process:
    - Patched imports: File::Exists / Subst, Time, Options, Task, LCRand (the bin's seed; the real one reads the
      clock), plugin type lookup.
    - Stand-in resource types; resources whose vftable logs loads, sizes and priorities.
    - Steps: databases, parents, variants, Get / Exists / Remove, held references, instances, Cache, bursts
      (the LRU pass), Update, DumpUnused, databases deleted.
    - Every table (bucket order, every resource), the system and the bin are compared after each step.
    - One scenario in eight runs with the loader thread made but paused, so its queue is driven deterministically.
      Queued resources are claimed before Remove and before a database goes: the original crashes otherwise too
      (details in the notes).
    - **3000 scenarios: 813 619 checks, 0 mismatches.** Statuses seen: 0 redir, 1, 2, 6, 7, 8.
    - Mutations caught (1000 scenarios each): the bin's eviction slack, the LRU pass's limit of five, the size
      check interval, Inactive after a load, Unqueue's reference, and ~PString zeroing the name (crash). Not
      caught: Unbin in DumpUnused, the refs check after eviction and the claim's Locking status (all need the
      loader thread or another thread), the LRU early stop across databases, DumpUnused setting Ready (the
      resource goes anyway), and the priority's rounding order (rare).
    - Not reachable here: the loader thread's own loads (Loaded / Loading, ProcessLoaded). Those run only in game.
  - Bugs fixed on the way: Nameless constructor argument order, the invalid resource's names, the twin-name
    Remove above, and the bin priority's rounding.
  - vftables: all match; Thread's is a module-local copy.
  - In game: menu check clean, also with KernelResource_7c_orig.dll removed. Input scenario: the same screens as
    the previous run.
  - Other tests rerun after the header changes: KernelResources, Render2DCore, every InputSystem test, InputWin32,
    the KernelStandard tests, FDA, LZSS, Blowfish, UId/Version: 0 mismatches. The exception is LCRand's known
    NaN-only difference. diff_r2d now takes Resource from KernelResource_7c.dll, since ours no longer loads the
    original.
- Offline file reading:
  - Outside the game, Jet's MemoryFile cannot read a file opened through File. The read goes through
    IOSlaveResource / IOSlaveFile, and IOSlaveFile reports a size of 0.
  - So ConfigParser-based loaders (sprite.txt, bmframe.txt, colour.txt, and KernelResource itself) cannot be
    diff-tested in-process yet. Fixing this means finding what Jet initialises for File I/O.

### Third-party deps and their x64 availability
| 32-bit dep | x64 replacement |
|---|---|
| msvcr80/msvcp80 | modern MSVC CRT (rebuild) |
| cg.dll / cgGL.dll 2.2 | Cg 3.1 x64 (decided 2026-10-21; GLSL later) |
| mozilla\ (XULRunner 1.9pre) | WebView2 behind an IWebBrowser adapter (decided 2026-10-21) |
| plugins\TNISpeedTree.dll (SpeedTree 5) | stays 32-bit in a helper process behind a TNI bridge (decided 2026-10-21) |
| d3dx9_42.dll | d3dx9_43 x64 (June 2010 redist), or remove d3dx |
| OpenAL32.dll / wrap_oal.dll | OpenAL Soft x64 |
| nvtt.dll 2.0.6 | nvtt is open source, build x64 |
| cudart.dll | only for nvtt GPU path, optional |
| libeay32/ssleay32 0.9.8e | OpenSSL x64 (API migration needed) |
| ijl10.dll (Intel JPEG) | libjpeg-turbo (who imports it: TODO) |

Tools that are separate processes and can stay 32-bit: Launcher, ContentManager (CMP), CCP, TADDaemon,
TrainzUtil, TrainzDiag, JAConverter, XCrashReport.

### KernelStandard_7c U6, small platform (2026-10-21): 308/1762 exports ours
- `Platform.cpp`, 66 exports: DynamicLibrary, Events / Events::Event, SharedMemory, SharedScratchPad, Job, Jobs.
  Behaviour notes are in `notes\KernelStandard.md` ("U6 done").
  - Events is a timed queue with three lists (removed / queue / requeue). Remove(id) defers the delete to the
    next Update.
  - Jobs is a worker thread. A job whose children are not dispatched yet waits until the next job has been
    processed.
- Fixed on the way: the header's DynamicLibrary slot order. The original's is dtor, Open, **Close, Import**.
- `tests/diff_ks_platform.cpp` runs one script per scenario on both modules:
  - Events with our Event vftable. Its Update decides from a hash of tag and count, the same on both sides,
    and removes itself or others or adds events from inside the update. Every list is compared after each
    step, links included.
  - DynamicLibrary on real and bogus libraries and procs.
  - SharedMemory mappings.
  - SharedScratchPad from two threads, with growth at the exact capacity.
  - Jobs batches, including parent/child waiting and a kick job.
  - Job's own members and its default constructor closure.
  - **3000 scenarios: 700 360 checks, 0 mismatches.** All 9 mutations caught: the sort tie, the due boundary,
    Remove(id) ownership, id generation, the pending rescan, parent unlocking, scratch growth, Import's result,
    and Event's copy links.
- In game: menu clean. In the input scenario the screens match. The only differences are network-driven: the
  content update counter (Offline / 2490) and the random route preview.
- The other modules' tests rerun: 0 mismatches, except LCRand's known NaN-only one.
- A note for test runs: diff tests that load the game's KernelStandard keep `bin\JetLog.txt` open. A game
  started during them exits at once.

### KernelStandard_7c U7a, Options (2026-10-21): 349/1762 exports ours
- `Options.cpp`, 41 exports: the parser for the command line, files and the environment. It covers every Get /
  GetNext overload, separators, string arrays and ranges. Details are in `notes\KernelStandard.md` ("U7a done").
- The game's msvcr80 is shared through `Crt80.h`:
  - atof rounding, ctype and case tables, wcstombs and stdout all come from it;
  - it is found by enumerating loaded modules, because it is loaded side-by-side and `GetModuleHandle`
    misses it;
  - Math2's `_CIacos` lookup had the same bug and was silently falling back to `std::acos`. It is fixed now.
- Two latent bugs fixed on the way:
  - **`File` is 0x13c bytes**, not 0x3c: the path buffer sits at +0x38. File locals were overrun by the
    original's Open and Close; our InputSystem's force-feedback load and save were affected too. The size is
    now checked by `JET_LAYOUT32`.
  - **Silently re-exported thunks**: a declaration that mangles differently from the original resolves through
    the original's import library, and the module re-exports the original function. `tools/check_thunks.py`
    finds these. It found two (ProcessCmdLine and Init with `char* argv[]`); every other module is clean.
- `tests/diff_ks_options.cpp`:
  - option lines from a generator of the syntax (switches, quotes either way, multi-word and empty values,
    key- / key+, stray signs, tabs and newlines, high bytes);
  - command lines, ANSI and UTF-16 files, and the environment through msvcr80's `_putenv`;
  - then every Get and GetNext, with the entry list compared after each step.
  - **5000 scenarios: 233 527 checks, 0 mismatches.** All 10 mutations caught.
  - Two behaviours of the original stay outside the comparison, because they read freed or uninitialised
    memory in both modules:
    - a cursor left from a replaced value;
    - UTF-16 files without a trailing wide NUL.
- In game: menu clean except two network-dependent Planet Auran lines (absent with the previous KernelStandard
  too). The input scenario runs through. The other modules' tests and the x64 harness are clean.

### KernelStandard_7c U7b1, ConfigData data model (2026-10-21): 488/1762 exports ours
- `ConfigData.cpp`, 139 exports: the tree that every config.txt, .kin and tag file becomes. Covered:
  - constructors, copies and the destructor;
  - New, operator(), Find x8, Count, Replace, Remove x3, RemoveAll, Extract and AssignNodes;
  - every typed setter and getter, Add*Array, SetAnsi / SetUnicode / GetAnsi / GetUnicode;
  - the 14 conversions, 22 operator<< and 23 operator>>, CompareData, CopyData and Crc.
  - Writers and the binary format are U7b2.
  - Details are in `notes\KernelStandard.md` ("U7b1 done").
- The nodes still come from the original's pool: its operator new and delete, and the static `unknownItem`,
  stay forwarded.
- Layout: {PString name, type, data, FastDynamicArray<ConfigData*,13> children}, 0x10 bytes.
  - Types are 0 none, 1 int, 2 float, 3 PString, 4 bytes, 5 ints, 6 floats, 7 PStrings.
  - The arrays are one Alloc block {count, items}.
- Quirks kept:
  - AddPStringArray leaks one reference per old string.
  - The vector, Color and Quaternion conversions read the block without checking it, so a node with no array
    gives garbage in the original.
  - `operator Quaternion` normalises with the sum of squares and fsqrt on the x87, then divss / mulss on SSE.
    This is matched NaN for NaN with inline asm and intrinsics.
- Header bugs found on the way:
  - `AnsiString()` was an inline null pointer. The original points it at a shared empty buffer with a refcount,
    and its operator= crashed on ours. It is now the original's export.
  - `UnicodeString` is now declared.
  - The original's wide-string exports mangle native `wchar_t` (`_W`), while the tree builds with
    `/Zc:wchar_t-`, so those declarations take `__wchar_t`.
- `tests/diff_ks_configdata.cpp`:
  - each side builds a tree through raw exports and runs random ops: add, set any value, remove, extract and
    move, copy over, swap children;
  - every op is followed by a dump of the whole tree (names, types, value bits, chunk counts) and its Crc;
  - read ops call every Find, getter, conversion and operator>>, including on `unknownItem`;
  - reference counts of the test's strings are checked at the end.
  - **3000 scenarios: 152 094 checks, 0 mismatches** (and 0 with the sides swapped). All 14 mutations caught.
  - Three-item Quaternions go through `Quaternion::Set` (U1), whose NaN payloads differ as already accepted in
    `diff_ks_math2`. The test treats those NaNs as equal.
  - The first runs found a real bug: the copy constructor dropped int and float values. My reading of the
    disassembly had been wrong.
- In game:
  - The menu is clean (only the StringData memory line differs, as with U7a).
  - The input scenario matches the U7a screenshots.
- The other modules' tests are clean, except LCRand's known NaN-only difference. The x64 harness is clean.

### KernelStandard_7c U7b2, ConfigData text and binary (2026-10-21): 510/1762 exports ours
- `ConfigWrite.cpp`, 22 exports: Write x3, WriteBin x5, ReadBin x3, and the internal writers (WriteChildList,
  WriteBlock, WriteDataItem, WriteDataBlock, WriteWrap, WriteTabs, WriteChars, WritePString, FormatDataString,
  WriteBinFile, ReadBinFile). With U7b1, all of ConfigData is ours except its pool's operator new / delete and
  `unknownItem`. Details are in `notes\KernelStandard.md` ("U7b2 done").
- `ConfigData::Writer` is an abstract sink {vftable: deleting destructor, Write, Encoding}. The module has a file
  writer and a string writer.
  - The string writer appends each piece with `PStringCache::Ins`, which copies up to a NUL whatever the size.
    So text written to a PString always gets 16-space indents, and a binary written to a PString reads past each
    field into the stack (kept: it is the original's behaviour).
- Text format: one line per item, "name = value;". Arrays are "a,b,c;", a single item is "(x);", and from ten
  items on ";//count" follows. Bytes are hex, 32 a line. Blocks are "name(value)" plus braces.
  - Lines wrap past column 100.
  - Strings are escaped and quoted as the parser expects.
  - In UTF-16 mode the text goes through msvcr80 `mbstowcs`.
- Binary: "CD", version 0x101, then depth first: name, a type byte, the value, the child count, the children.
- `tests/diff_ks_configdata.cpp` now also covers:
  - text to a PString, a File and a test Writer (whose vftable logs every call and can fail at any call);
  - the binary to a Writer, a File, a path and a subst drive;
  - each binary read back, into a new node or an existing one, and files cut at every field boundary;
  - corrupted magic or version words;
  - FormatDataString on random texts, and direct calls of the internal writers at random depths.
  - **3000 scenarios: 157 428 checks, 0 mismatches** (both side orders).
  - Of the 16 U7b2 mutations, 15 are caught. The 16th (Read(p, 1, 4) against Read(p, 4, 1) for the child
    count) is equivalent.
- Cases the test avoids, because the original itself fails there (in File/PString or the CRT, not ConfigData):
  - a binary cut inside a field: File::Read takes a partial field as read, so heap or stack bytes stay in the
    value, and a string cut at its text overruns the heap;
  - a PString array with a null item written as UTF-16 text: `mbstowcs(NULL, ..., -1)` ends the process
    through the CRT's invalid-parameter handler.
- Header fixes: AnsiString(const char*) is declared (null gives the empty buffer, Size() 0), and the code builds
  AnsiStrings where the original does. File gains Read(PString&), Read(long&) and Open(base, path). The base is
  a subst drive name.
- In game:
  - Only Render3DCore / Render3DExtension import ConfigData::Write (for a debug spline.txt), so this unit is
    hardly exercised in normal play.
  - The menu shows 0 differences. The input scenario is unchanged. The settings files the game rewrites at exit
    are byte-identical.
  - No "faulty built-in assets" box appears from the game or from a fresh Launcher. The one reported came from a
    Launcher and TADDaemon left running since a failed morning run, with that run's DLLs.
- `run_diffs.sh` puts the bin on PATH for diff_is_lex only. With it on PATH under Git bash,
  diff_kernel_uid_version's UId::Write check fails in every build (an environment effect, not a module one).

### KernelStandard_7c U7c1, ConfigParser (2026-10-21): 533/1762 exports ours
- `ConfigParser.cpp` and `ConfigLexer.cpp`, 23 exports: constructors, copy, assignment, destructor, the five
  Parse overloads, Save x2, SaveString, WriteData, and the getters and setters. Details are in
  `notes\KernelStandard.md` ("U7c1 done").
- **The "Syntax error outside the game" mystery is solved.** KernelStandard's ConfigParser does not read Trainz
  config.txt at all. It reads its own Save format: a `version x.xx` line, then ConfigData::Write text.
  - The scanner's first state accepts only comments and the `ver` keyword; anything else makes yylex return -2,
    and Parse returns 3 silently. Every earlier test fed it config.txt.
  - The game's own config loaders use other parsers. This one serves the Jet modules' own files (Interface*,
    Render*, InputSystem, CCP, ContentManager).
- Inside: a flex 2.5.4 C++ scanner (81 rules, 6 start conditions, REJECT-style state buffer) and an old Lemon
  parser (2000-era hash-table states, 34 rules, `error` recovery).
  - The DFA and Lemon tables are read from the installed original at build time, like InputSystem's scanner
    tables. The Lemon action links are converted from addresses to indices.
  - The skeletons and all 81 scanner actions and 34 grammar actions are hand-ported from the disassembly and
    Ghidra's decompilation.
  - Error messages went to a vsprintf buffer that nothing reads (the logging was compiled out), so they are left
    out.
- Original behaviour kept: a backslash outside quotes in the body hangs both modules forever.
  - Rule 29 puts the backslash back and enters the bare-string state. Rule 43 there puts it back again and
    returns an empty STRING, which the grammar keeps joining.
  - Found by minimising a hanging test input down to 11 bytes: "version.", a line feed, a backtick and a
    backslash.
- `tests/diff_ks_configparser.cpp`, comparing per side the result, version, tree, file name, SaveString and both
  Save encodings, then a re-parse through every overload. Inputs:
  - texts written by the original's ConfigData::Write from random trees, behind a version line;
  - damaged copies of those texts;
  - token soup (raw strings, special floats, octal ints, hex bytes, comments).
  - **3000 scenarios: 30 000 checks, 0 mismatches** (and 1500 with the sides swapped).
  - Of 14 mutations, 12 are caught. The other two are equivalent: `atof` of a special-float token is never 0,
    and the error counter does not matter after the driver stops at the first error.
  - Texts with a bare backslash are not drawn, and neither is UTF-16 SaveString content of non-ASCII text (the
    string writer reads past it into the stack). `KSP_WATCH=<s>` reports where a scenario hangs.
- **A U7b2 bug was found and fixed**: ConfigData::Write wrapped string-array items one column short (the item
  wraps to the column, only the comma before it to one less). The U7b2 test had never produced a wrapping string
  array.
- In game:
  - The menu matches, except the network line that varies.
  - The input scenario is unchanged.
  - No "faulty" box from the game or from a fresh Launcher.
- The other modules' tests and the x64 harness are clean. diff_iw32 had one mismatch once, run from a
  PowerShell-to-bash chain; it was clean when rerun.

### KernelStandard_7c U7c2, StringTables (2026-10-21): 568/1762 exports ours
- `StringTables.cpp`, 35 exports: StringTablesBase and StringTablesDefaultTraits, the reader and writer of string
  (localisation) tables as config text or binary files. Details are in `notes\KernelStandard.md` ("U7c2 done").
  Nothing in this install imports them, so the game check is a regression check only.
- The base class is abstract. Six of its 20 vftable slots are pure: the table class supplies them. From how the
  base calls them, they are SaveConfigData(ConfigData&), SaveBin(DynamicBuffer&), and AddTable and AddEntry, one
  each for the config form and one each for the binary form.
  - Config form: a node whose value is `Table` and that has a non-zero `Id` child is a table. Its `Entry` nodes
    with an `Id` are its entries. Valueless nodes are groups, so names are dotted paths.
  - Binary form: "ST", version 0x101, a names flag and the key character size, then for each table "TABL", an
    id, an optional name and the entries.
- The vftable matches the original slot for slot (overloads in MSVC's reverse declaration order). The file and
  close errors are thrown and caught as `FileEx` by reference, as in the original. The data exports (tokens,
  "TABL", the result table) stay the original's.
- `tests/diff_ks_stringtables.cpp`. Each side's object gets a copy of that side's vftable with the pure slots
  replaced by logging test functions. Inputs:
  - random tables in a config tree with noise;
  - its text, written by the original's ConfigParser::Save (sometimes cut);
  - a binary image (cut, a byte flipped, a word overwritten).
  Each side reads them through every overload, saves through every overload and reads its own output back. The
  test also runs the copy, assignment and deleting destructor, CreateNode, the Scan functions and the traits
  directly.
  - **3000 scenarios: 288 756 checks, 0 mismatches.** 20 000 scenarios: 1 896 249 checks, 0 mismatches. 1500
    with the sides swapped: 0 mismatches.
  - Of 16 mutations, all 16 are caught.
- Original-only garbage, not compared: `DynamicBuffer::Read(long&)` succeeds when only 1-3 bytes are left and
  leaves the rest of the value as it was on the stack.
  - This happens in a binary table cut mid-field (a count, an entry id or a name length).
  - The test emulates the reader to find such a read and compares everything up to it: about 1.5% of the
    scenarios.
- In game:
  - The menu matches.
  - The input scenario is unchanged.
  - No "faulty" box from the game or from a fresh Launcher.
- The other modules' tests and the x64 harness are clean, except one old difference in `diff_ks_rand`, which the
  U7c1 build gives too.
  - It is in LCRand (seed 1129, step 1412): with two NaN arguments we return the second one's NaN, the original
    (x87) the first one's.
  - It is not caused by U7c2; it is left for the Rand unit.
- Also found: test runs killed by a timeout had left
  2000+ temporary files of the original's `File` (`%08X.T%08X`) in the build and source folders. They are removed,
  and the new test runs in the temp folder.

### KernelStandard_7c U9, streams and JIRF (2026-10-22): 763/1762 exports ours
- The first half of the bigger U8 + U9 step: 195 exports in four files. Details are in `notes\KernelStandard.md`
  ("U9 done").
  - `Streams.cpp`, DynamicBuffer (51 exports). The strings' binary Read / Write go through a reader / writer object
    the strings call back.
  - `IOSlave.cpp`, the byte-stream family: memory, growing memory (an MSVC8 std::vector kept by hand), file, read
    cursor, write cursor with LZ history, and the scrambled resource stream (header, per-position byte scrambling,
    4 KB padding).
  - `BlockReader.cpp`, the "name { ... }" text reader, and BPE (Philip Gage's byte pair encoding, as compiled:
    codes from 254 down).
  - `JIRF.cpp`, JIRF chunk files over the resource stream, and the size writers.
- Three new diff tests: `diff_ks_dynbuf`, `diff_ks_ioslave`, `diff_ks_jirf`.
  - 649 420 + 392 314 + 141 544 checks, 0 mismatches; 0 with the sides swapped too.
  - Mutations: 29 of 33 caught, the other 4 equivalent.
- Original-only garbage, not compared:
  - bytes of a buffer that no write has set;
  - BlockReader's unset line buffer (File::ReadLine puts no NUL after a line; the test zeroes the buffers);
  - reads past a short JIRF stream (they leave stack values);
  - the bits and bytes MSVC8 does not copy in the File copy.
- The time-based parts of the resource stream (its key word, its padding) are set or skipped by the tests.
- Kernel_7c is rebuilt against the new IOSlave header: the same imports.
- In game (KernelStandard_7c and Kernel_7c deployed):
  - The menu matches, except the network line.
  - The input scenario is unchanged.
  - No "faulty" box from the game or from the Launcher.
- The other modules' tests and the x64 harness are clean, except the old LCRand NaN case.
- Next: U8 Bitmap. Its blitters are hand-written assembly: 12 000 lines of disassembly for the class.

### KernelStandard_7c U8, Bitmap (2026-10-22): 861/1762 exports ours
- 98 exports in two files. Details are in `notes\KernelStandard.md` ("U8 done").
  - `Bitmap.cpp`: construction, the per-format pixel primitives, drawing (pixels, lines, boxes, circles,
    triangles, textured polygons), blits and tiling, scaling, crop / flip / rotate, the colour operations.
  - `BitmapIO.cpp`: TGA, BMP, PCX and JPEG info and loading, the alpha files, saving TGA / BMP / JPEG.
- The primitives were hand-written assembly. They are rewritten register by register where the arithmetic
  depends on it: the 24-bit blend adds all three channels in one 32-bit sum, so carries cross channels.
- The copies are element-by-element forward copies (rep movs): the tiled Image repeats a pattern through an
  overlapping copy.
- JPEG decoding still goes through ijl10.dll. Encoding uses libjpeg-turbo 3.2.0 (vcpkg x86-windows-static) in
  place of the original's static IJG encoder: the same bytes (quality 100, every quantiser 1).
- Bugs kept (they are listed in the notes): BlendPixel does not blend; drawing trusts the clip to lie inside the
  bitmap; Triangle's alloca'd scan table overflows in two cases; MapPolygon and the 24-bit ScaleArbitrary write
  past their rows; PCX colour runs are the whole byte long.
- Two new diff tests: `diff_ks_bitmap_draw` and `diff_ks_bitmap_io`.
  - Drawing: 3000 scenarios, 665 324 checks, 0 mismatches; 0 with the sides swapped and another seed.
  - Files: 1500 synthetic scenarios, 0 mismatches. JPEG saves are byte-equal.
  - All 28 904 images of the install (TGA, BMP, JPG): 0 mismatches. Images over 2048 x 4096 are compared by
    GetInfo only: their 64 MB block fails now and then on either side in the 32-bit test process.
  - Mutations: 43 of 43 caught.
- Test traps, all original behaviour: a constructed bitmap's pixels and bytes per pixel are not set; GetInfo
  and loaders read short headers into uninitialised buffers; Crop and PCX indices are packed rows.
- The other modules' tests are clean, except the old LCRand NaN case. `Bitmap.cpp` also builds in the x64 stand-in.
- In game (KernelStandard_7c deployed, U9 build kept as `KernelStandard_7c.dll.u9`):
  - The menu log matches the baseline (0 lines differ).
  - The route list shows its thumbnails.
  - No "faulty" box from the Launcher.
- Next: U10, the File family.

### KernelStandard_7c U10a, files and paths (2026-10-25): 1048/1762 exports ours
- U10 is split in two. U10a, 187 exports in three files (details in `notes\KernelStandard.md`, "U10a done"):
  - `FilePath.cpp`: File::Path (50 of 52), File::Time, File::Entry.
  - `File.cpp`: File (buffered I/O, the BPE-compressed "CFIL" format, ReadLine / WriteLine in ANSI and UTF-16,
    blocks, stamps) and FileEx.
  - `MemoryFile.cpp`: MemoryFile.
- U10b keeps the substitute system for later: Subst*, the path statics that consult it, Path::GetSubst, Find,
  FileSub* (the .ja archive reader and writer), File::Sep and the progress callback. Our File::Open looks a
  "name:path" up in the original's list and lets the original's FileSub objects open the file into our object.
  Its helpers are __stdcall (ret 14h).
- The .ja files are "ARCH" containers ("INFO", "FIL " records with a File::Entry and the data, "IDX "). The
  largest is 1.68 GB, so every offset fits in 32 bits (notes, "U10b preparation").
- File descriptors are msvcr80's (`Crt80.h`), because the original's FileSubs fill our objects.
- A header bug was fixed: Exception is 0x1110 bytes, not 0x110c, so StringTables' `throw FileEx` wrote 4 bytes
  past the object.
- Bugs kept: SafeRead / SafeWrite compare the items moved with the item size; the typed reads count a short read
  as failed; MemoryFile never converts UTF-16 memory (it clears the first character before wcstombs); failed
  opens 5 / 6 leave a closed descriptor in the object.
- The original crashes on some inputs: msvcr80's invalid-parameter exit on fd -1 or a closed fd, and
  MakeRelativeTo / Normalize("") reading address 0. The test avoids these inputs.
- New diff test `diff_ks_file`:
  - Paths: the whole 0x104-byte buffer is compared after each call.
  - File scripts over every open mode, compressed files written and sought, and MemoryFile.
  - A directory and a new .ja archive written and read back through the original's substitutes.
  - 2000 scenarios: 734 279 checks, 1 mismatch. The original failed to open a file that had just been renamed
    (code 2), and it did not happen again on a rerun, so it looks like an outside file lock. With the sides
    swapped: 500 scenarios, 0 mismatches.
- The U7-U9 tests and the Platform test are clean.
- In game (the U8 build kept as `KernelStandard_7c_u8.dll`):
  - The menu log matches the baseline (0 lines differ).
  - The Launcher shows no "faulty" box.
- Next: U10b, the substitutes and the .ja archive.

### KernelStandard_7c U10b, substitutes and .ja archives (2026-10-25): 1096/1762 exports ours
- 48 exports in four files (details in `notes\KernelStandard.md`, "U10b part 1 done" and "U10b part 2 done"):
  - `FileSub.cpp`: the substitute list ("data:path" names standing for directories, archives and links) and every
    static that consults it (Subst, SubstLink, Exists, IsDirectory, Unlink, Rename, Md, Prune, GetStamp, SetReadonly,
    CheckWriteMode, TrueName, Path::GetSubst, GetSubstBuildVersion / ProductCode); FileSubPath and FileSubLink.
  - `FileFind.cpp`: File::Find over plain directories and a substitute's directories and archives.
  - `FileSubArchive.cpp`: the archive reader and writer (records in file order, a name hash, a directory set, read
    cursors, the "IDX " directory rewritten at exit), SetDefaultBuildVersion / ProductCode, the temp-name counter.
  - `File.cpp`: File::Sep, File::callback and SetCallBack.
- Nothing in KernelStandard calls the original by address any more; what is left of the original is reached only
  through its exports (the foundations: Alloc, PString, Log...).
- Archive facts: by name, a lookup normalizes the key ('\' to '/') but not the stored name, so a file stored with
  '\' is found only by a scan or a search, never by name, in an archive with a directory (the game's files use '/').
  Records do not keep times: GetStamp of an indexed file is 0. A written file is appended as a "FIL " record (an old
  one of that name becomes "FIL!"); the directory is rewritten in place at exit and the file is never truncated.
- Bugs kept: FileSubPath packs a compressed write under a temp name that overflows into its read buffer, so the file
  is lost and a hidden temp file stays; the original writes stack and heap leftovers after the names in "FIL " and
  "IDX " entries (ours: zeros).
- The original crashes on some inputs, and so do we: writing to a writable archive whose file was turned away (no
  INFO, a foreign magic or version: the writes go to fd -1); reading back a compressed file written to a version
  0x101 archive (12-byte "CFIL" header written, 4-byte one read); Unlink through a record with a garbage offset
  writes "FIL!" hundreds of MB out (scanning that file then takes hours). The test avoids the first two and ends a
  scenario at the third.
- Unlinking a path with '\' drops the record but leaves its name in the hash (the erased key is not normalized); a
  later lookup reads the freed record, so the original's answer depends on its heap (once in 1000 scenarios it
  differed from ours). The test writes and unlinks with '/' only, as the game does.
- Diff test `diff_ks_filesub` (each side its own substitutes, temp files and current directory):
  - Real archives: Base.ja and Data.ja listed by Find and every file read on both sides (2964 files, 107 MB).
  - Read, write and archive scenarios: directories, links, an archive written by the original, and each side's own
    .ja (new or built by the test: versions, magics, right / missing / wrong directories, deleted and foreign chunks)
    written plain and compressed, read, searched, unlinked and remounted; the archive file compared chunk by chunk.
  - 1000 scenarios: 347 983 checks, 0 mismatches; with the real archives first, 300 scenarios. With the sides
    swapped: 150 scenarios, 0 mismatches. `diff_ks_file` is
    clean (110 087 checks).
- In game (the U10b part 1 build kept as `KernelStandard_7c_u10b1.dll`): the menu log matches the baseline (0 lines
  differ) and the Launcher shows no "faulty" box.
- Next: U11 (PORT_PLAN 7b).

### KernelStandard_7c U11, OS / Window / WindowPlacement (2026-10-25): 1167/1762 exports ours
- 71 exports in `Window.cpp` (details in `notes\KernelStandard.md`, "U11 done"):
  - OS: the platform object (instance, window, command line, options, parent, application name); Init processes
    the default options and the command line under ExceptionHandler::Main.
  - Window: the "JET" window class, the four create calls (windowed, 2D, default, full screen), host-state
    switching, the message pump (HandleMessages, dispatchers), WindowProc (hooks, activation bits, system commands,
    input forwarded to a parent), the FPS title.
  - WindowPlacement: save and restore every visible top-level window.
- The critical-shutdown entry goes through the exported ExceptionHandler::Add / RemoveCriticalShutdown (the
  original builds the same node inline on the private list).
- `OS::defaultOptions` is `char const * const`: VS2005 mangled it `0PBDB`, VS2026 mangles `0QBDB`, so it is exported
  through an alias in implemented.txt.
- `Window::Message` is both a method and (in our header) a hook-message struct name: the struct is now
  `Window::HookMessage` (InputWin32 updated).
- Diff test `diff_ks_window` on a private desktop (nothing shows on screen; WindowPlacement sees only the test's
  windows), MessageBoxA patched in both modules' import tables: 300 scenarios, 77 173 checks, 0 mismatches; swapped
  100 scenarios, 0 mismatches. Only messages the test sends are logged with parameters: activation, paint, input and
  WM_END_OF_MESSAGES traffic depends on the desktop's history; after SetHostState the test forces a frame recompute.
- Found while re-running the other KernelStandard tests: `diff_ks_configdata` and `diff_ks_configparser` had crashed
  since U10b part 2 (temp-name collisions between the two modules' lost FileSubPath temp files). Fixed in the tests
  (each side its own current directory); every KernelStandard diff test passes again.
- In game: the menu log matches the baseline (0 lines differ); real clicks and keys work (SELECT ROUTE, Esc back,
  the Main menu dropdown); the Launcher shows no "faulty" box. The U10b build is kept as
  `KernelStandard_7c_u10b2.dll`.
- Next: U13 -> U12 -> U14 (PORT_PLAN 7b).

### KernelStandard_7c U13, Thread / ThreadLocalStorage / Time (2026-10-26): 1216/1762 exports ours
- 49 exports in `Thread.cpp` and `Time.cpp`. All U13 data stays the original's and is imported by our code
  (`JET_KS_FORWARDED`): `Time::freq` (filled by the original's CRT initialiser), the frame-lock state,
  `Thread::mainThreadId`, `ThreadLocalStorage::singleton` and its four index statics. The original's
  KernelStandardInit (still the original's, U12) creates the singleton through the original's PStringCache, its
  Profile reads the indices and its Log reads the clock, so one copy of the state is kept until the U14 cut-over.
- Thread is 0x20 bytes: `{vptr, thread handle, WEvent done @8, alive @0x10, fn @0x14, arg @0x18, id @0x1c}`.
  The destructor and Terminate set `done`, resume the thread and wait for it. The destructor sets the thread handle
  to null without closing it (the original leaks it). `IsDone` consumes the auto-reset event. `Start` runs the body
  under the exported `ExceptionHandler::Main` and clears `alive` afterwards.
- ThreadLocalStorage is `{Mutex*, count, capacity, {PStringCache::Node*, TlsAlloc index}*}`. The array grows in
  blocks of 128 entries. Copies share the mutex. PString copies and releases are the original's inlined forms (a
  release never destroys the node).
- `FrameLockMs` truncates `frameLockTime * 0.001` through a 64-bit `fistp` (RC = chop) and returns the low half.
  `IncFrame` stores the time of the count before the increment. The x87 code runs at the CRT's 53-bit precision,
  so SSE doubles give the same bits. A game that switches the FPU to 24 bits (D3D without FPU_PRESERVE) could
  differ there, but such a mode has not been seen.
- Diff test `diff_ks_thread`: frame-lock arithmetic over 12 deltas x 6 counts (wrap at 2^32, NaN, overflowing
  products), the live clocks as relations, Str/HHMM, Thread (suspended start, every state word, priorities,
  done event, Terminate, destructor of a running and of a finished thread, GlobalInit from another thread, process
  helpers with a child process), TLS (the original's names, 140 new names past the 128 growth step on a fresh
  instance, repeats, null name, per-thread data on 4 threads, copy constructor / operator=, a private instance and
  its destructor, CreateTLS/DestroyTLS). 2731 results per side, 0 mismatches over 4 runs. 12 of 12 mutations were
  caught.
- `tools/check_thunks.py` now skips one-instruction wrappers that jump into a system DLL's import slot
  (GetCurrentThreadId, timeGetTime). Only slots of the original's modules count as thunks.
- The x64 harness had not built since U10b (the stand-in still used File::Find's old opaque buffer and the bool
  `File::Exists`). It is fixed in `tests/x64/KernelStandardStub.cpp`. `probe/run_x64.ps1`: task, EditItem and golden
  all 0 differences.
- Earlier KernelStandard tests still pass, except the known NaN-only LCRand difference (U5).
- In game: the menu log matches the baseline (0 lines differ), and the Launcher shows no "faulty" box. The U13 build
  is kept as `KernelStandard_7c_u13.dll`.
- Next: U12 (diagnostics), then U14 (PORT_PLAN 7b).

### KernelStandard_7c U12a, Exception / Debugger / Log / LogClient (2026-10-26): 1257/1762 exports ours
- U12 is split like U10: U12a (Exception, Debugger, Log, LogClient), U12b (Profile, QuickTimer, Console), U12c
  (ExceptionHandler, HardwareException, the x86 crash code).
- 41 exports in `Exception.cpp` and `Log.cpp`.
- Exception is 0x1110 bytes: `{vptr, description[0x1000] @4, reason[0x100] @0x1004, hardware @0x1104,
  line @0x1108, file @0x110c}`. The earlier header comment had the two texts swapped. Description holds the call
  stack text ("No call stack available\r\n" from the file/line constructors). Reason holds the message.
  - The texts are formatted with msvcr80's vsprintf, as in the original.
  - AddReasonf appends "  -  " and then the text.
  - Copies take every byte of both buffers.
- Log state stays the original's. The exported statics are imported. The one private flag, `LogFlush`
  (0x1007287b), is read and written in the original module by address. Its Alloc and strings (U14) still log
  through the same clients and file.
- LogClient is 0x24 bytes: `{next, prev, AnsiString name @8, AnsiString @0xc (unused), level @0x10 (None 0, Err 1,
  Warn 2, Diag 3, Prog 4), kind @0x14 (an index into " !?+-", set by the callers; the constructor leaves it
  unset), busy @0x18, outputs @0x1c (1 file, 2 debugger, 8 window), format @0x20 (1 CRLF, 2 time prefix)}`.
  - The constructor keeps the heap garbage in the other bits of outputs and format.
  - The time prefix is `"%c %3.0f:%04.1f : "`. Minutes come from a fistp of `elapsed/60 - 0.5` (float math);
    seconds are `elapsed - minutes*60`.
  - The stream copy uses msvcr80's vfprintf.
- Log::Init reads LogFlush, LogDisplayModuleName and LogAnsi (the File::Open encoding argument). It reads LogFile
  (default "JetLog.txt", mode 0x101). It writes a 0xFEFF BOM when the encoding is 2, and "Begin:\r\n" with a
  commit. With NoLog it writes "Log Disabled.\r\n" and closes. An empty name, a failed Open or NoLog drops the
  file. A failed Open or NoLog also sets LogClient::disabled. LogStdOut and LogStdErr pick msvcr80's stdout
  (`__iob_func()+0x20`) or stderr (+0x40).
  - It then resets the "Log" option entry's separators to "," and its cursor, and parses every
    "client:setting:..." run (Process: Err/Warn/Diag/Prog/None; mono, window, newlines, displaymodulename, file
    with an optional "=on"/"=off").
  - It walks the client ring without a null check, as the original does.
- Diff test `diff_ks_log`: Exception constructors, copies and every formatter, with the whole 0x110c bytes
  hashed. Log runs 7 option sets through Init, two clients per case (5 kinds, time prefix and line-break
  combinations, a busy client), 12 option lines through Process, and Shutdown/Done. The log files' bytes are
  compared. 1245 results per side, 0 mismatches. 14 of 14 mutations were caught. The other KernelStandard tests
  (file, filesub, config, ioslave, jirf...) still pass.
- In game: the menu log matches the baseline (0 lines differ; JetLog.txt is now written by our Log), and the
  Launcher shows no "faulty" box. The build is kept as `KernelStandard_7c_u12a.dll`.
- Next: U12b, then U12c, then U14.

### KernelStandard_7c U12b, Profile / QuickTimer / Console (2026-10-26): 1312/1762 exports ours
- 55 exports in `Profile.cpp` and `Console.cpp`. The statics stay the original's, imported. The three private
  accumulators of `Profile::Display(float, ...)` (0x100ae210/214/218) are used in the original module by address.
  Sorting uses msvcr80's qsort, so equal keys keep the original's order.
- `Profile::GetDisplayFrequency` returns `fld1 / fdiv` unrounded in st(0). It is exported as a worker that
  returns a double, like Rand's float members.
- Profile::Display(console, max) works on the first functions with at least 64 ticks (`time & ~63`). It prints
  calls, total ms and average ms (32-bit arithmetic, ms = ticks*1000/(freq_low/1000)) and the peak average. It
  draws a bar of `time*36/sum >> 6` half-steps when that is 1..35, and then clears the counters. QuickTimer keeps
  {count, min, max, total} per frame in a ring of 4.
  - The QuickTimer destructor never updates min once a call is not shorter than it ("min" was the "last" slot).
  - `Update` empties the slot two frames ahead. Its report every `logEvery` frames formats into the empty logging
    function, so only the `numFrames % logEvery` (a trap when logEvery is 0) is kept.
- Console (0x1c): `{width, height, buffer, top, spans, cursor, lineEnd}`.
  - Print takes `<color:RRGGBB>` (the last 8 characters of the tag; the hex value with red and blue swapped) and
    `</color>` tags out into a ring of 13-span chunks (0xa8 bytes). Any other tag is printed as text: the '<' is
    printed, read again and skipped.
  - NewLine scrolls `top` when the cursor enters it.
  - `Print(int, width)` writes right-aligned digits after the cursor without moving it, and prints no digit for 0.
  - Copies share the character buffer.
- Diff test `diff_ks_profile`: Console runs 20 random scripts over 6 sizes, with the buffer, cursor, top and every
  span compared, plus both blits and StringToHex. Profile and QuickTimer use fixed clock values: scopes, sorting
  by all options, the table, Display(float), trace points and Update. 127 728 results per side, 0 mismatches.
  20 of 20 mutations were caught.
  - Bar with a width below its length makes a negative count, a 4-billion-step loop in the original too. Goto
    wraps only once. The test stays inside both limits.
- **Lesson (mutation runs):** the scripts restored each source from a `.bak` copy that was older than the
  mutated .obj, so MSBuild kept the mutated objects. The "restored" DLL contained the last mutation of each file.
  - The U13 and U12a builds deployed to the game were made right after such runs. Their committed sources are
    right, and later header changes recompiled everything.
  - All sources were touched, rebuilt and every KernelStandard diff test rerun (15 tests, 0 mismatches). The
    suspect game backups `_u13`/`_u12a` were deleted.
  - The scripts now touch the restored file.
- In game (clean build): the menu log matches the baseline (0 lines differ) and the Launcher is clean. The x64
  harness shows 0 differences. The build is kept as `KernelStandard_7c_u12b.dll`.
- Next: U12c (ExceptionHandler, HardwareException), then U14.

### KernelStandard_7c U12c, ExceptionHandler / HardwareException (2026-10-26): 1337/1762 exports ours
- 25 exports in `ExceptionHandler.cpp`:
  - Main, MainTryCatch, Process / DoFilter / FilterException;
  - Assert, AssertInit, SetAssertions;
  - the critical shutdown and startup lists, Init, InitExceptionDialog, LogLine;
  - HardwareException's constructors, assignment and class name;
  - an `AssertEx` class (global namespace, class name "Assertion"), which catchers in other modules match by
    name.
- Still the original's, called by address: the crash report writers (the hardware-fault filter of Main and the
  assertion report) and `HardwareException::GetCallStack` (through its export). They are x86-specific and move
  with the x64 rewrite. The private statics are used in the original by address: the report lock, Main's nesting
  count, the re-entry flags of the two lists and the "Exception" log channel. `file` and `reason` are
  `char const * const`, which VS2026 mangles differently, so they are looked up by name at run time.
- The original's behaviour is kept:
  - Main returns 0 without running the function when `ignore` is set.
  - With `ignore` set, FilterException and ProcessException return 1 while still holding the report lock.
  - InitExceptionDialog releases the list box's DC through the dialog's window.
  - Init copies an empty CorePrefix from a null pointer.
- Our module is built with /EHsc, so the `catch (...)` around the critical-list callbacks does not catch hardware
  faults. The original may have been built with /EHa.
- Diff test `diff_ks_eh` covers the ordinary paths only. Prompting is off, so no report is written or shown.
  - Main with returns, a Jet::Exception and an AssertEx (stand-in classes with the same names).
  - The lists: order, removal, re-entry, a throwing callback.
  - Assert, Init, HardwareException copies, and the dialog's list box on a hidden window (texts and horizontal
    extent).
  - 131 results per side, 0 mismatches. 11 of 13 mutations were caught: one is equivalent, and one is in the
    unreachable branch for a failing log write.
- All KernelStandard diff tests pass (16), and the x64 harness shows 0 differences. In game: the menu log matches
  the baseline and the Launcher is clean. The build is kept as `KernelStandard_7c_u12c.dll`.
- Next: U14 (Alloc, PackedHeap, PString/PStringCache, the strings, KernelStandardInit) and the cut-over.

### KernelStandard_7c U14a, UnicodeString / MultiByteString / AnsiString / String (2026-10-26): 1606/1762 exports ours
- U14 is split. The decompile shows that the string classes reach the foundation only through Alloc's exports,
  the string-heap helper (0x10039780) and the shared empty block. So U14a makes all string code ours and leaves
  that state with the original, used by address, as U12/U13 did with their statics. U14b moves the state:
  Alloc, PackedHeap(MP), PoolBlockAlloc, ReferenceCount, PString/PStringCache (its lock, hash and bucket helpers),
  KernelStandardInit and the 68 data exports still forwarded.
- 269 exports in `UnicodeString.cpp` (83), `MultiByteString.cpp` (78) and `AnsiString.cpp` (AnsiString 92,
  String 16). The CRT calls are msvcr80's (`Crt80.h`): case mapping, MBCS and wide conversion and number
  parsing follow its locale. The float getters return atof's / wcstod's double unrounded in st(0) (aliases of
  double workers).
- UnicodeString is `{text, capacity in bytes, length}`. The capacity is `(2n + 0x12) & ~0xf`, and buffers come
  from Alloc. SetLength only grows the buffer and never the length. `Ins(ch, pos, count)` memsets `count` bytes
  with the low byte of ch, on a wide buffer. Read takes a 4-byte header: bit 30 set means a UTF-16 byte count,
  otherwise an ANSI character count, which is converted.
- MultiByteString is `{data, capacity, bytes, characters}`, with capacity `(n + 0x11) & ~0xf`. Copies, Set,
  Setn, SetLength, Ins and Del use the character count as a byte count. A double-byte fill or a UTF-16 Read
  header leaves bytes != characters, and the original's next Ins, Del or Read then writes past the buffer.
  A failed SetUnicode clears the buffer but keeps the character count, so the next Del reads address 0.
- AnsiString points at `{capacity, length, chars}` with the reference count at -4.
  - Blocks of `capacity + 0xc` bytes, rounded to 8, come from the "#StringData" PackedHeapMP up to 0xa0 bytes
    and from Alloc above that. The heap is made on first use.
  - A capacity of 0 reads as "null": empty strings share the original's static block (0x100ae254, guard
    0x100ae264, refs 1, never counted).
  - MakeBufferUnique reallocates when the block is shared, too small or has capacity 0. With `keep`, the size is
    at least the old capacity. A null `data` with size 0 gets a 0x10-byte block that claims 4 characters.
  - Compare and CompareIgnoreCase return 1 / 0 / 2 for the null cases (the CompareWith values) and -1 / 0 / 1
    otherwise.
  - `x += x` copies from the block it has just released when the buffer grows.
- Diff test `diff_ks_string`: one random 20 000-step script per class, made before either side runs. After
  every step all fields and the text of each touched object are compared, with every return value.
  - AnsiStrings are made and freed through the other module every fifth step, since both share the heap and
    the empty block.
  - Conversions from and to PString (nodes of the live cache), UnicodeString and AnsiString are covered, and
    so are Read/Write round trips with short writes and crafted headers.
  - 462 738 results, 0 mismatches. 10 of 16 mutations were caught; the other 6 are equivalent (boundary `<`/`<=`
    with equal results, a clamp that never applies, the stack-or-heap choice of a temporary buffer).
  - Inputs that crash or corrupt the original are left out: null text into _mbslen/mbstowcs/wcstombs, the two
    MultiByteString count mismatches above, and `x += x`.
- The other KernelStandard tests still pass, except the known NaN-only LCRand difference (U5).
  `diff_ks_thread` failed once in 3 runs on a Terminate timing race; Thread is unchanged. The x64 harness shows
  0 differences. In game: the menu log matches the baseline (0 lines differ), and the Launcher shows no
  "faulty" box. The build is kept as `KernelStandard_7c_u14a.dll`.
- Next: U14b (the foundation cut-over), done below.

### KernelStandard_7c U14b, the foundation cut-over (2026-10-26): 1761/1762 exports ours
- New: `Memory.cpp` (Alloc, PackedHeap / PackedHeapMP, PoolBlockAlloc, ReferenceCount, ConfigData's pool operators,
  KernelStandardInit and the shutdown table, g_jetSTLAllocHeap) and `PString.cpp` (PStringCache, PString). Every
  data export is now defined in the unit that uses it (Log, Profile / QuickTimer, Time, Thread / TLS,
  ExceptionHandler / HardwareException, Console's font, StringTables, ConfigData::unknownItem, the result tables,
  ConfigParser's vftable). `JET_KS_FORWARDED` is empty in the KernelStandard build.
- Still the original's: HardwareException::GetCallStack (the one forwarded export) and the two crash report writers
  called by address. They keep their own state (report buffer, ERRORLOG.TXT), so the original's other statics
  are no longer used by anyone. The original module stays loaded for them and for msvcr80.
- Our statics come first: Memory.cpp and Time.cpp are `#pragma init_seg(lib)`, in the original's order (Alloc,
  free lists, the shutdown table runner, KernelStandardInit, the counter frequency, ConfigData's pool, the
  "PackedHeap" log client, g_jetSTLAllocHeap "#jetstd::"). The tokens and File::Sep stay ordinary statics after them.
- Found on the way (the decompile hid them):
  - PackedHeap::Alloc bumps in the blocks from the highest address down (a reverse iterator of the set). Blocks come
    top-down from VirtualAlloc, so the oldest block is tried first.
  - PackedHeap's copy constructor and operator= copy both trees: the copy shares the blocks and its size chains
    point into the source's map. Destroying two copies frees the blocks twice in the original too.
  - PStringCache::CreateUnicode interns the bytes wcstombs wrote, not the character count.
  - PString::Read frees a node cut short with Alloc::Delete even when it came from the string heap (length below
    0x87): that corrupts the msvcr80 heap in the original. Tests stay away from it.
- Tests: new `diff_ks_alloc` (Alloc reuse, free-list capacity and Dump counters, PackedHeap / MP layout and block
  headers, copies, four PoolBlockAlloc shapes, ConfigData's pool, ReferenceCount slots, a 6-thread stress per side;
  about 1.6 million results, 0 mismatches) and `diff_ks_pstring` (every PStringCache and PString export over a
  corpus; 1.5 million results, 0 mismatches). 24 of 27 mutations caught; the other 3 are equivalent (the hash over
  15 bytes instead of 16: with h * 8 the older bytes leave the 32 bits anyway; Del's `<` / `<=`; a Free bound
  no pointer can reach).
- The older diff tests assumed one shared heap, cache and set of statics. They now make every input through the
  side under test (a UseHelpers(side) per test). Our private LogFlush byte and Display accumulators are found
  in `KernelStandard_7c.map` copied next to the DLL (`tests/ks_private.h`). `diff_ks_filesub` interns its substitute
  names on both sides in one address order, because that list is sorted by node address.
- Also fixed: LCRand::NextDouble(lo, hi) runs on the x87 stack, like the original. With two NaN arguments the x87
  keeps the larger significand and SSE keeps the first operand, so this was the U5 NaN-only difference.
- PackedHeap::DebugPrintAll lists heaps by address. Ours now has the original's two static heaps, "FileSubArchive"
  (the archive records) and "#jetstd::", in a section of their own in the original's order. The game's exit summary
  then matches the baseline line for line, except #StringData's fill (486k used against 507k): since U10b the
  archive directories and file searches keep their strings in std containers.
- Checks: all 25 diff_ks_* tests pass. diff_ks_file showed one mismatch once while the game ran at the same time;
  it did not come back in 2 reruns. The x64 harness shows 0 differences.
- In game:
  - 3 menu checks, JetLog identical to the baseline.
  - An input run (hover, SELECT ROUTE with Cyrillic route names, Esc, the Main menu dropdown) shows the same
    screens as the U14a build.
  - The Launcher shows no "faulty" box.
  - The build is kept as `KernelStandard_7c_u14b.dll`.
- KernelStandard_7c is now ours except HardwareException::GetCallStack and the crash report writers, which move
  with the x64 rewrite.

### Step 8a, RenderSystem_7c inventory and U0 proxy (2026-10-26)
- Plan in `notes/RenderSystem.md`: 2687 exports (76 vftables, 9 vbtables, ~240 static data), 3628 functions,
  351 KB; units U1-U9, order U1, U4, U2, U3, U5-U9 (Display and RenderKernel last).
- RenderSystem imports only KernelStandard (168), Kernel (24), KernelResource (28), KernelResources (1): all ours.
- 855 exports are imported by anyone: trainz.exe 457, Render3DCore 320, TrainzUtil 222, ContentManager 198,
  Render3DFoundation 196, RenderOpenGL 178, CCP 177, WaterSystem 149, RenderDirectX9 143, Render3DExtension 111,
  Launcher 80, TADDaemon 51, DebugSupport 32, Render2DCore 28, TrainzDiag 24, InterfaceSystem 24.
- Render module dependencies (Jet imports): Render3DCore → RenderSystem 320, DebugInterface 3;
  Render3DFoundation → RenderSystem 196, Render3DCore 108; Render3DExtension → Render3DCore 134, RenderSystem 111;
  RenderOpenGL → RenderSystem 178 only; WaterSystem → RenderSystem 149, Render3DCore 80; InterfaceSystem →
  InterfaceResources 32, RenderSystem 24, Render2DCore 15, InputSystem 16. trainz.exe → RenderSystem 457,
  Render3DCore 431, Render3DFoundation 88, InterfaceSystem 81, WaterSystem 36.
- Most module state is exported static data (registries, managers, texture defaults, vertex offset tables), so
  units can use the original's statics through the `_orig` import library until U9.
- U0: `src/jet/RenderSystem_7c` with an empty `implemented.txt` (all 2687 exports forwarded to
  `RenderSystem_7c_orig.dll`). In game: menu reached, JetLog identical to the baseline (diff_lines=0).

### RenderSystem_7c U1, value types and vertex records (2026-10-26): 939/2687 exports ours
- `include/jet/VertexFormats.h` + `RenderSystem_7c/VertexFormats.cpp` (generated from the TS12 assembly and data):
  118 vertex records (PackedVertex*, GeoLOD*Vertex*, TerrainBlend*, StreamedVertex*), 652 members, 178 data exports.
  - Each record: type id, size, accessors returning pointers into the record, `operator=` (Vector) setters, copies.
  - Item offset tables have **39 entries** (VertexItemType count; the 40th dword of the int tables is padding):
    `int` (GeoLOD/Terrain), `OffsetLookupCheck {offset, type}` (PackedVertex), `CheckVertexItemTypeInt
    {stream, offset, type}` (Streamed). The `type` column is not always the index.
  - TerrainBlend `readDataOffset/Size` live in .bss and nothing writes them: always 0.
  - StreamedVertexPCTBase/PNTBase are empty records (statics only).
- `include/jet/RenderTypes.h` + `RenderSystem_7c/RenderTypes.cpp`, by hand:
  - DisplayFormat {id, bits, shift/size per channel, stencil, depth} (0xc): `Set` is a 28-case table (id kept as a
    byte); `operator==` compares ids unless one is 0 or 20 (custom), then layouts; `Classify` probes ids 1..28.
    `GetFormatIDString` reads a 29-entry name table the original fills at start-up.
  - DisplayLight (0x50, default: type 3, constant attenuation 1, spot cutoff 180), DisplayMaterial (0x44, colours
    with alpha 1), RenderTextureConstruct (shared context is a ReferenceCount; `operator=` is a plain copy).
  - TextureArgs (0x44) and its text decoder (`COLOR MODULATE TEX COLOR PREV COLOR`, 19 tokens, strtok/_strupr/
    sscanf). Short lines make the original strcmp a null token (crash): kept.
  - TextureStage (0x94: two TextureArgs, stage, display; Build calls Display slot 0x11c).
  - DisplayDescriptor (0x1c, four PStrings), Element (0xc; `Element::currentVisited` stays the original's, imported
    from `RenderSystem_7c_orig.lib`, new in `implib/`), Projector (`projectorCache` stays the original's).
  - DisplayLight slots 2-5 and Projector slot 1 have no exported name; the bases answer 0.
- **VertexType moved to U5**: it owns a global registry (hash tables, `nextFreeId`, `display`), so it goes with the
  geometry unit.
- Test `tests/diff_rs_u1.cpp` (557 051 checks at 400 rounds, 0 mismatches, ~1 s): vertex records found in the export
  table and classified by their undecorated signature; the other classes driven through their exports.
  - Kept difference: the original copies floats with x87 `fld/fstp`, which quiets signalling NaNs; ours copies
    bits. The test generates no SNaNs (vertex data never holds them).
  - Mutations caught: a format table entry, the MINUS flag in Decode, a GetMorphedTex stride.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U4, modifiers (2026-10-26): 993/2687 exports ours
- `include/jet/Modifier.h` + `graphics/RenderSystem_7c/Modifiers.cpp`: Modifier, ModifierHost, ModifierList,
  ModifierUpdateTask (54 exports). The four vftables match the original slot by slot (`tools/compare_vtables.py`).
  - Modifier (0x18): +4 host, +8 name, +0xc priority (0.5 by default), +0x10 bit 0 complete, +0x14 the frame stamp
    of its last update (no constructor sets it). Slot 8 is the per-frame `Update(dt)`: not exported, the base is the
    shared `xor eax,eax; ret` stub (it writes no result); 2 detaches the modifier, 3 detaches and deletes it.
  - ModifierHost is abstract: the inserts answer 2, the other slots are that stub. Overloads sit in reverse in the
    vtable (InsertBefore(m, other) 3, InsertBefore(m) 4, ...; Remove(name) 7, Remove(m) 8).
  - ModifierList (0x18): +4 a VC8 `std::list<Modifier*>` laid out by hand ({empty allocator, head, size}, nodes
    {next, prev, value} of 0xc from `Jet::Alloc`), +0x10 set while a modifier updates, +0x14 the frame counter.
    `QuickRemove` takes and returns the checked `_Iterator<1>` {container, node}: our `ModifierIterator` has the
    same layout and a constructor (so it is returned in memory) and is exported under the original name by an
    `implemented.txt` alias.
  - InsertBefore(m) goes before the first modifier of equal or higher priority, InsertAfter(m) after the last of
    equal priority. The two-argument forms answer 4 unless both priorities are equal, then insert next to `other`,
    or, when `other` is not in the list, where the one-argument form would (InsertAfter: after the last modifier of
    equal or higher priority).
  - Nothing removes a modifier from its old host when it gets a new one (SetHost, the host constructor). Remove(m)
    answers 5 for a modifier without a host and does not compare the host. QuickRemove clears the host, then calls
    the list's virtual Remove(m) (5 in the base: a derived list sees the modifier go).
  - SetHost(RenderKernel*) uses the kernel's scene graph (+0x80) and its modifier task (+8); 6 when there is none.
  - UpdateModifiers holds `modifierListLock` (still the original's static) and a Profile scope
    ("ModifierList::UpdateModifiers"; our own Info), bumps the frame counter and updates each modifier whose stamp
    differs; 3 when any modifier was taken out, else 0. ModifierUpdateTask::Update calls it through the vtable.
  - `results` of Modifier and ModifierHost point at the text "RES_UNKNOWN" (0x10070fac): ours.
- Test `tests/diff_rs_u4.cpp` (2000 scenarios of up to 60 calls, 0 mismatches, ~1.5 s): two lists and six
  modifiers per side, built with that side's exports; Update, the deleting destructor and the lists'
  Remove(const Modifier*) wrapped to log; snapshot of every list and modifier after each call. Update never
  deletes a modifier that is still in another node (the original would leave it dangling as well).
  - Mutations caught: InsertAfter `<` as `<=`, InsertBefore(m, other) position, the virtual Remove in QuickRemove,
    the UpdateModifiers result, the result 2 detach, the frame-stamp test.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U2a, shader-language data (2026-10-26): 1092/2687 exports ours
- `include/jet/GfxLanguage.h` + `graphics/RenderSystem_7c/GfxSymbols.cpp` (99 exports):
  - SymbolEntry (0x48): +0 alias, +4 type, +8..+0x14 four ints (-1 when new), +0x18 hint, +0x1c 5, +0x20 byte,
    +0x21 flags (bit 0 anonymous, cleared by SetName; bit 1 compared by LikeType; bit 2 owns the alias), +0x24..+0x30
    left by the constructors, +0x34, +0x38 name, +0x3c reference count, +0x40 next, +0x44 API data. A symbol deletes
    its `next` chain, its API data (virtual deleting destructor) and, with bit 2, its alias. AddRef/DecRef walk the
    alias chain. The exported default constructor closure is SymbolEntry(0, false) (alias in implemented.txt).
  - Vertex/TextureSymbolEntry add the class at +0x48 (shared bodies). VertexSymbolTable is an owning
    Array<VertexSymbolEntry*> growing in steps of 128; its copy and assignment copy the pointers only.
  - RegisterItem (0x18): symbol, OffsetAddress* {index register, offset}, swizzle text, flag, two modifiers. The
    copy from a pointer gives the swizzle a 6-byte block, shares the offset's register and drops the modifiers.
    RegisterModifier's assignment copies nothing.
  - Instruction {vtable, operands}: the destructor deletes the operands and their array, reading the array
    without a null test. Vertex/TextureInstruction add the opcode (+8).
  - PreProcessorDefine {name, value}; PreProcessorArray = Array<PreProcessorDefine, 3>; ConcatenateStrings gives
    ":name:value" for each define. `Jet::Array<T, SIZ>` (DynamicArray.h) now has the full member set of the
    exported instance, iterators {array, index} (null at the end). Shrink on an empty array with room left destroys
    index 0xffffffff in the original too.
- Test `tests/diff_rs_u2.cpp` (1254 scenarios, 0 mismatches): both modules' imports of Alloc::New/Delete are hooked,
  so every free is logged with the allocation's number and ownership is compared; symbols with chains and aliases,
  the table past 256 entries, registers and instructions, every Array member.
  - Mutations caught (8 + 3): LikeType and FindLike fields, the vertex class test, RegisterItem's dropped modifiers,
    alias ownership, the ":" separator, the Remove iterator, the 128-entry step, the operand array free, SetName's
    anonymous bit.
- In game: menu reached, JetLog identical to the baseline.
- U2b (ExtendedGfxLanguage and the four flex scanners with their Lemon parsers) is next.

### RenderSystem_7c U2b1, ExtendedGfxLanguage and the combiner records (2026-10-26): 1136/2687 exports ours
- U2b is split: U2b1 the class and records (here), U2b2 the four scanners/parsers (internal, reached only from
  VertexProgram::Load, TextureCombiner::Load and VertexProgramManager::RegisterBuiltInPrograms).
- `graphics/RenderSystem_7c/ExtendedGfxLanguage.cpp` (44 exports):
  - ExtendedGfxLanguage : virtual ReferenceCount, 0x3c: +0 vftable, +4 vbtable, +8 Display*, +0xc source
    (AnsiString), +0x10 filename, +0x14 programName, +0x18 registeredName, +0x1c generation (1.0), +0x20
    Array<SymbolName, 3> (SymbolName = {PString, GfxSymbolId {bool local, int number, int extra}}), +0x2c user data,
    +0x30 fallback program (referenced; SetFallbackProgram/SetUserData are const), +0x34 the ReferenceCount.
  - vftable slots: 0 SupportedByPlatform(float) false, 1 LoadProgram, 2 LoadFromFile, 3 LoadNativeFormat true,
    4 CompileProgram true, 5 Bind false, 6 End false, 7 (unnamed) false, 8 Load(MemoryFile&, name, file) false,
    the one the derived classes fill in.
  - LoadProgram runs the defines over the text and hands it to Load as a memory file named "built-in";
    LoadFromFile opens `db->GetFileName(file)` (mode 0x10) and passes (name, file) on.
  - ProcessDefines writes each value straight into the text's block (shared or not), padding with spaces or
    deleting first, and resumes the search one value length past the end of the replacement: near the end of a
    text that start lies past the terminator and strstr reads beyond it (tests keep a tail on every text).
  - SetFilename cuts a trailing ".txt". InitInstance copies everything into the other object without releasing
    its old fallback. GetLocalConstantNumber returns the number of the first matching symbol that is not local.
  - TextureCombiner::Instruction {vft, opcode, ParseList<RegisterItem>*, ParseList<ModifierItem>*, address type,
    bool, four ints +0x18..+0x24, float +0x28}; RegisterItem {type, index, two ParseList<ModifierItem>*, next}.
    The copies from a pointer are deep and leave +0x18..+0x24 alone; the destructors free both lists.
- Test `tests/diff_rs_u2b.cpp` (602 scenarios, 0 mismatches): heap hooked as in diff_rs_u2; language objects get
  a vftable copy whose slot 8 logs what Load is handed, LoadFromFile reads %TEMP% files through a stand-in
  ResourceDatabase, objects are deleted through their reference count with fallbacks still set.
  - Mutations caught (14): the search restart, the missing old-fallback release in InitInstance, symbol field
    order, the local test, the "built-in" name, the Load argument order, the define text length, generation 1.0,
    the destructor's fallback release, CompileProgram's result, the deep copies' second list and field copies.
    The rest were equivalent (Close before the destructor, list tail handling, the pad character that is
    overwritten anyway).
- In game: menu reached, JetLog identical to the baseline. diff_rs_u1/u2/u4 still 0 mismatches.

### WaterSystem_7c U1, noise and the water grids (2026-10-26): 40/490 exports ours
Started ahead of the Render stack (the plan had WaterSystem at 8e; the inventory was ready). Sources:
`src/jet/graphics/WaterSystem_7c/{FrameAnimation,PNoise,WaterGrid}.cpp`, headers `src/jet/include/jet/…`,
unit order in `notes/WaterSystem.md`.
- **FrameAnimation** (0x18, POD): frame clock `{numFrames, currentFrame, time, sampleRate, period, deltaFrame}`.
  Update: time += dt, currentFrame = (int)(sampleRate*time) % numFrames, deltaFrame = frac via double floor
  (the product is x87 53-bit, so it is computed in double like KernelStandard's Math). Dtor shares the no-op
  body 0x10091D0 with ~PNoise and a dozen other empty functions (identical-code folding); GetSampleRate shares
  0x1008750 with WaterGrid::GetData.
- **PNoise** (0x383C, POD): classic Perlin 1/2/3 with the 1985 smoothstep, fBm turbulence, tileable and looping
  tiled variants. Layout byte-exact (permutation + 4 gradient tables + their shifted wrap copies). Quirks kept:
  the permutation is 1-based so perm[0] reads numOctaves (and the last cell's second hash index is byte-
  truncated, reading it again), Turbulence3 doubles x and y but not z, the ctor's Perm(0) read happens before
  numOctaves is set. All float math follows the module-wide x87 rule: double intermediates, float rounding
  where the assembly stores to a float local (D-suffixed workers behind the public float exports).
- **WaterGrid<float>/<Vector2>** (0x10): `{vftable, width, height, data}`, one virtual (Initialize), operator=
  returns void, the MSVC 2005 array-new overflow guard passed to Alloc::New.
- Build: WaterSystem_7c joined the drop-in module list (`JET_WS_BUILD`), 40 exports ours, the rest forwards to
  `WaterSystem_7c_orig.dll`; `tests/diff_ws_u1.cpp` covers the three classes against the original (PNoise tables
  compare byte for byte: both CRTs start a fresh process at the default seed; plus a cross-assign pass so the
  noise math is checked independently of the tables). Diff run and in-game check pending the next Windows session.

### WaterSystem_7c U2 + U3 (2026-10-27): 82/490 exports ours
- **U3, DisplacementHull** (DisplacementHull.cpp): a height field loaded from a bitmap, derives from
  WaterGrid<float>. Initialize resolves the file through the database exactly like the original: a raw
  __cdecl call of db's vtable slot 3 with (db, &out node, name) on the stack, both outputs used, the out node
  released (shared helper CallDbSlot3 in WaterSystemInternal.h; ResourceDatabase's virtuals are __cdecl with
  `this` pushed and slot 3 is GetFileName, whose by-value PString return slot the caller reuses — the call
  shape matches the assembly either way).
- **U2, the config model** (WaterManagerConfig.cpp): AnimProp (0x60, WANI binary load/save, .txt config
  fallback with the "DetailAnim" key), MaterialProp (0x50), TileProp (0x24), PhysicsProp (0xc) and
  WaterManagerConfig (0x160, two virtual Loads, the six cube textures). All 46 config keys are interned static
  PStrings, as the original's init function (0x100198e0..0x10019cc0) creates them. The operator>> overload per
  field (PString/int/float/Size/Vector2/Color/Vector3) was mapped from the delay/import table, the field types
  cross-checked against the ctors' constants (450.0 world size, {-15,5} FFT wind, 0.7071 light dir...).
  AnimProp::LoadFile(File&) returns false for non-"WANI" magic so the .txt fallback runs; the config ctor's
  detail-anim resolution (relative name under the config's path through the db, then the raw name) follows the
  assembly including the path/colon/slash checks.
- WaterGrid's NewBytes guard moved to WaterSystemInternal.h (shared with DisplacementHull). Everything is
  syntax-checked; bit-exactness waits for the Windows build (diff_ws_u1 first, then diff tests for U2/U3 to be
  written, then the menu check).

### WaterSystem_7c U4 (2026-10-27): 106/490 exports ours
- WaterAnimation : PluginInstance (+ protected PluginBase for the factory lookup) and WaterAnimationPN, the
  noise-driven frame animation (WaterAnimation.cpp). Layouts: WA 0x24, PN 0x3870 (Size + animMaxHeight + the
  frame bookkeeping + a PNoise member + the virtual ReferenceCount at the end); the vtables/vbtables/vtordisps
  and adjustor thunks come from the compiler.
- Quirks kept: GetXYFrame is a null stub (folded with the other "return 0" bodies at 0x10015db0), Init is a
  "return true" stub (0x10013f10), CreateInstance calls the factory's slot-3 CreateInstance raw through VCall
  and releases the type, PN multiplies by the float 1/25 constant 0x3d23d70a instead of dividing, and the
  original's caller-side 53-bit use of the noise exports is kept via PNoise's D-workers (now public).
- groupUid 2D54F63B-FF0A-4877-9561-0BBC3E2AC6DB, ANIM_NAME "WaterAnimationPN", factory id
  E43947F9-6280-4141-B8BC-8847196B62AF (RegisterWaterAnimationFactory, called by U10's JetGetPluginsWaterSystem).
  WaterGrid gained JET_HEAP_OBJECT (the original allocates the grids through Jet::Alloc).

### WaterSystem_7c U6 (2026-10-27): 119/490 exports ours
- WaterTextureAnimation (WaterTextureAnimation.cpp): the per-frame bump textures, both Initialize overloads and
  CreateAnimationTexture. RenderSystem is not reconstructed yet, so the module carries a local view of the
  classes it needs (RenderSystemShell.h, everything imported by name): the Texture ctor and vftable (the
  original's explicit vftable write reproduced via a linker /alternatename onto ??_7Texture@Jet@@6B@), and the
  RenderKernel texture manager + DisplayTexture make/fill calls raw through VCall (kernel slot 38, display
  slots 1 and 3). The bump map keeps the original's quirks: wrapping gradients, the lagging dx and the
  127.5-scaled channels with the double 1/maxHeight division. Verification stays deferred to the Windows build,
  per the user's call to pick up the pace.

### WaterSystem_7c U5 (2026-10-27): 152/490 exports ours
- The geometry unit (WaterGeometry.cpp): WaterGeometry (the chunk holder; CreateGeometryChunk through the
  material's raw slot 46, the chunk's item arrays through slots 49/47/4/31, GetIndexArray for the indices),
  PhysicsWaterGeometry (the three-buffer wave simulation, both flavours, the hull displacement overlay),
  WaterGeometryAnimation (the frame morphing) and WaterGeometryVS (the vertex-program attributes). All
  RenderSystem types stay local shells / raw VCall (RenderSystemShell.h gained PackedVertexPCT, GeometryChunk,
  StandardChunkHint); the original's quirks are kept (the last-row overread, the wrapped right neighbour, the
  unused PNoise constructions, the mixed old/new buffer reads in the attrib gradients). Verification deferred
  to the Windows build, per the user's call.
### RenderSystem_7c U2b2a, the quick parser and the pre-parser (2026-10-27): internal code, no new exports
- The four shader-language front ends are internal (no exports); only VertexProgram::Load, TextureCombiner::Load and
  VertexProgramManager::RegisterBuiltInPrograms (all U3) run them. U2b2a does two of them; the texture and vertex
  program parsers (tp, vp) are U2b2b. Our code is reached from the test only until U3 calls it.
- `GfxParse.h/.cpp`: the shared machinery.
  - FlexScanner: the flex 2.5.4 C++ skeleton with REJECT (the same as KernelStandard's ConfigLexer: state buffer of
    0x10008 bytes from Jet::Alloc, 16 KB buffers read 8 KB at a time from the MemoryFile, yy_get_previous_state reads
    chars >= 0x7f as ec[0x7e]). Scan() hands back the next rule; each language's Lex() holds the actions.
  - LemonParser<Traits>: an old Lemon (before the yy_action[] arrays): per state {hash block, mask, default}, actions
    {lookahead, action, next}; stack {index, error countdown, top, 100 x {state, major, minor}} (0x4bc,
    Jet::Alloc); overflow drops everything; minors of terminals above 0x100 are freed by the destructor. The
    driver's error recovery is the classic one. The tables hold absolute pointers: `tools/extract_lemon.py` turns
    them into indices at build time (like extract_data.py for the flex tables).
  - GfxNameList / GfxIntStack: circular lists of 0x40-byte chunks {next, prev, count, 13 items}; append at the end,
    clear from the second chunk to the head, pop frees an emptied last chunk.
  - GfxLexStatics: the scanners' rule state is module statics (0x100b7dc8..0x100b8077: start-condition stacks,
    #if level flags, block depths), shared by every instance and kept between runs. Same layout in ours.
  - Error messages are vsprintf'd into a local buffer and dropped: only the error flag is observable.
- `GfxQuickParser.cpp`: GfxLanguageQuickParser (gfxqk, 26 rules, Lemon 32 states / 19 rules): collects the program
  names of a built-in program text ("program <name> (...)").
- `GfxPreParser.cpp`: GfxLanguagePreParser (gfxpp, 92 rules, 12 start conditions, Lemon 95 states / 54 rules).
  Syntax (from the DFA): `version x`, `vertexProgram(Name) { program n (FMT) = flag, ...; generation = x; }`,
  `API(a|b) { ... }`, `API_NATIVE(a) : { binding } { native }`, `%ifdef/%ifndef/%elseif/%else/%endif` with
  `( | & ! )`, program flags, `API(name)` (ExtendedGfxLanguage vftable slot 7, now `Defines(const PString&)`) and
  `extension(D_...)` (Display vftable slot 7 with the index into 33 capability names at 0x100b6c9c). It keeps the
  program asked for and writes the kept text with `\n#VERTEX fmt`, `#CODE`/`#ENDCODE`, `#BINDING`, `#NATIVE`/
  `#ENDNATIVE` and `#LINE n` markers; the language gets the program's generation when SupportedByPlatform takes it.
  Versions below 2.0 are refused. A duplicate program block leaks its flag list (as in the original).
- Test `tests/diff_rs_u2b.cpp` parts quick/qkengine/pre/ppengine (1406 scenarios in all, 0 mismatches). The
  original's internal functions are called by address through asm thunks (register arguments); ours are
  `RsTest*` functions found through RenderSystem_7c.map (the map must sit next to the deployed DLL). Inputs: the 17
  program texts embedded in the render modules and trainz.exe, damaged copies, token soups, long texts with bytes
  above 0x7f, made-up pre-parser files using every construct; the drivers alone get random and grammar-guided
  token streams (each next token one our driver shifts). The statics are zeroed on both sides in most scenarios;
  runs that fault (the original reads address 0 on some broken texts, e.g. an #if stack popped empty) are caught
  and must fault on both sides.
  - Mutations caught: 42 of 58. Not reached by any input: the 99-entry stack overflow, error-symbol recovery
    (neither grammar has error rules, so those branches are dead), the brace counter reset at `vertexProgram`
    (only differs after an unbalanced earlier run), `= {` directly opening a skipped block, one rule's free order.
- In game: menu reached, JetLog identical to the baseline (our parsers are not called there yet).

### RenderSystem_7c U2b2b1, the texture program parser (2026-10-27): internal code, no new exports
- `TexProgramParser.cpp`: TextureProgramParser (scanner tp 0x10044d30, 90 rules, 8 start conditions, no REJECT;
  Lemon 119 states / 69 rules, driver 0x10044830, reduce 0x10043760; run 0x100462d0, called by
  TextureCombiner::Load with the parser at +0, the file name +0x7c, the line +0x80 and the combiner's vftable slot
  17 value (the API this build runs: 0, 1, 2) at +0x84). Same flex skeleton and Lemon engine as U2b2a.
  - Syntax (from the DFA and the corpus): `version x`, `header(name) { reg n (name) - x, y, z, w; ... }`,
    `API(common|OpenGL|DirectX) { ps.x.y instructions }` and `API_NATIVE(OpenGL|DirectX) { native text }`. An API
    block of another API is skipped as a brace-counted block; a native block's text is collected whole.
  - Statics: the start-condition stack at 0x100b7fa0 (so the quick parser's is 20 ints, not 60), 0x100b8067 (the
    block is native), 0x100b8090..0x100b809f (depth, condition, brace depths) in GfxLexStatics, now 0x2d8 bytes;
    the native text is a module AnsiString at 0x100b8800 (gfxTpCode), kept between runs. One rule pops through the
    entry below the stack top, so an empty stack reads the quick parser's last entry.
  - The reduce actions only build records: TexHeaderItem, TexRegisterDefinition {index, name, 0x18 constant
    {0, 4 floats, count}}, TexApiSection {lines, text}, TexApiCodeLine (a ParseItem with a TextureCombiner::
    Instruction at +0xc), TexApiDataItem; lists of them, operand lists (RegisterItem) and modifier lists. Lookup
    tables: 35 instructions (lower case, unknown = add), 5 address types, register files c/r/v/t (unknown = t), 41
    modifiers ("_x2" is 1 on an instruction, 13 on a register; one rule takes only "_x2" and sets the error flag
    for anything else).
    Rules without a value leave the goto minor as it is (stack garbage in the original); nothing reads those.
  - The run then hands the records over: TrackConstant (slot 13) per constant, then per API section either
    LoadNativeFormat (slot 3, skipped when empty) or every instruction (slot 11) and CompileProgram (slot 4),
    stopping at the first refusal. SetProgramName gets the header's name. The final parse is not checked for errors.
  - The original faults in the reduce when an operand is appended to an empty operand list (the empty-list rule
    gives null and the append reads address 4); ours does the same.
- Test `tests/diff_rs_u2b.cpp` parts tp and tpengine (1908 scenarios in all parts, 0 mismatches): the 58 texture
  programs embedded in the render modules and trainz.exe, damaged copies and token soups, run through each side's
  parser against a stand-in combiner that logs every call (and refuses some); the scanner statics and the native
  text logged after each run. The driver alone gets random and grammar-guided token streams; a step that faults
  is caught on both sides (new in the engine harness). RS_U2B_ONLY=tp,tpengine runs just these parts.
  - Mutations caught: 46 of 58. Not reached by any input: the "c<n> (x, y, z, w)" constant line (its register
    number and values), the API_NATIVE-in-header branch, the rule popping below the stack top, a line-counting
    pop, case-insensitive instruction names and constant lookup (the corpus is lower case), the register "_x2"
    value, empty native sections and a refused compile (one section per program).
- In game: menu reached, JetLog identical to the baseline (the parser is not called there until U3).

### RenderSystem_7c U2b2b2, the vertex program parser (2026-10-27): internal code, no new exports
- `VertexProgramParser.cpp`: VertexProgramParser (scanner vp 0x10051bd0, 125 rules, 5 start conditions; Lemon
  480 states / 268 rules, driver 0x100518c0, reduce 0x1004e710 (11 KB); run 0x10054f00, called by
  VertexProgram::Load with the program name, the file name and the program). Its input is the pre-parser's output:
  `#VERTEX fmt`, `#CODE n` / `#ENDCODE n` around the ARB-style program text, `#BINDING n` / `#ENDBINDING` and
  `#NATIVE n` / `#ENDNATIVE n` around a native one, `#LINE n` markers.
- Its Lemon is a later generation than the other three: int actions (shift below 0x1e0, reduce below 0x2ec, error
  0x2ec), actions {byte lookahead, int action, next} (12 bytes; `tools/extract_lemon.py ... wide`), no-code 0xf1,
  error symbol 0x76, 100 terminals, and 16-byte values: the stack (0x96c) holds 100 {state, major, value[4]}, and a
  token's text is freed whenever set (no "above 0x100" test). GfxParse.h's LemonParser now takes the value type
  from the traits (LemonMinor16); the older three are unchanged (their tests still 0 mismatches).
- The reduce's value is a stack slot nothing initialises. Rules without a value, or setting one byte of it, leave
  what the previous reduction of the same driver call wrote there; between tokens the scanner's frame overwrites it.
  Ours carries the value within one driver call and starts each call from zero, which matches every real text
  tried. Where the original then stores non-reproducible garbage (a program parameter's value when the text gives
  none, words 1-3 of it; a byte read after a syntax error message was formatted), the test does not compare.
- Statics: the condition stack at 0x100b7ff0 (so the texture scanner's is 20 ints), the block flag 0x100b80a3 and
  0x100b8114..0x100b8127 (depth, condition, marker count, the code block's buffer of the file's size and its write
  pointer); GfxLexStatics is now 0x360 bytes. One rule pops through the entry below the top, as in tp.
- The actions build the program: VertexSymbolEntry symbols of classes 0-10 (parameters, attributes, outputs,
  temporaries, constants with a Vec4, aliases, address registers) added to a VertexSymbolTable (FindLike merges a
  symbol with an equal one), RegisterItem operands with swizzles ("x", "xyzw", "aa,bb,cc,dd" for SWZ) and
  OffsetAddress indices, VertexInstruction (ARL, 7 unary, 6 scalar, POW, 18 binary, LRP/MAD, SWZ; names looked up
  ignoring case in five static PString tables built at module start, 0x100b8820..0x100b8950).
- The run then hands them over: SetProgramName, slot 12 the program names, slot 13 the symbol table (and whether
  the program is native), slot 29 "has a vertex type" (if not: VertexType::GetVertexType(name), a copy of it to
  slot 28), then either LoadNativeFormat (slot 3) or slot 14 with the instruction list and CompileProgram (slot 4).
  VertexType is still the original's (unit U5): called through the import library.
- Test `tests/diff_rs_u2b.cpp` parts vp and vpengine (0 mismatches; 2410 scenarios over all parts): the 155
  program texts the corpus gives after our pre-parser (equal to the original's), the native programs it writes for
  400 made-up pre-parser files, damaged copies and token soups, against a stand-in VertexProgram that dumps every
  symbol, operand and instruction it is handed; the driver alone on random and grammar-guided streams (the stack
  below the driver is zeroed before each call, as the scanner leaves it in a real run). VertexType::GetVertexType
  knows no types in the test, so that path faults on both sides.
  - Mutations caught: 51 of 58. Not reached by any input: ABS (no program uses it), one popped condition's token,
    a declared symbol aliasing a type-12 parameter, the copy of a constant's first word, a negative ARL offset, the
    texture coordinate "s", and the alias DecRef.
- In game: menu reached, JetLog identical to the baseline (the parsers are not called there until U3).

### RenderSystem_7c U3a1, VertexProgram and TextureCombiner (2026-10-27): 1186/2687 exports ours
- U3 (215 exports, ~40 KB) is split: U3a1 the two program base classes (50 exports, this step), U3a2 their
  managers, resources and the Cg/GLSL helpers, U3b VertexProgramBasic (the text program generator, 14 KB).
- `VertexProgram.cpp`: VertexProgram (0x50 bytes: ExtendedGfxLanguage, then vertex type +0x34 (referenced),
  influence base/max registers +0x38/+0x3c (-1), a byte +0x40, the manager +0x44; ReferenceCount virtual base
  +0x48). Its ExtendedGfxLanguage vftable has 35 slots; slots 9, 12-15, 20-27, 30-34 answer 0 in the base and are
  the API module's: 13 symbol table, 14 instructions, 17 SetProgramState, 19 LoadConstantRegisters (both empty),
  23/24/27 SetLocalConstant / SetLocalConstantDirect / SetEnvConstant (int, Vector4), 26 several constants,
  30-32 the register limits, 33 the API id handed to the pre-parser, 34 the native program text. The Vector3 and
  Color forms are exported non-virtual helpers (w = 1).
- Load (0x10049540) runs the pre-parser with the API id, then the vertex program parser over the pre-parser's
  output (both ours since U2b2; entry points `GfxPreParse` / `VertexProgramParse` in GfxParse.h).
- LoadCommonStates (0x100499b0, 2.6 KB) fills the render-state constants (ConstantData: register +0, kind +4
  (4 = state), state id +8 (0x12..0x27), light index +0x10 among lights of type +0x1c (5 or more: all lights)):
  material colours and power, light colours / attenuation, light position or direction and the spot direction in
  view space (w = cos(cut-off) through x87 fcos), the half vector (normalised with StdMath's table 1/sqrt), ambient
  times material plus emissive, light times material colour, the display's ambient (state block +0x7c) and fog
  (+0x8c..+0x94, w = 1 / (end - start)). The lights are sorted into five FastDynamicArrays by type on first use.
  Ours keeps the original's order of float operations (the test compares bits).
- `TextureCombiner.cpp`: TextureCombiner (0x6c bytes: tracked constants +0x34 (Array of {index, Vector4}), a
  16-slot table of them by register +0x40, bump scale/bias per stage +0x44, bump matrices +0x50, a changed flag
  +0x5c, the manager +0x60 (left uninitialised by the constructor, as in the original)). 18-slot vftable: 11 the
  program parser's instructions and 17 the API id are the API module's. Load runs the texture program parser.
- Both classes' support flags (`supported`/`supportChecked`, VertexProgram's by generation 1..2) and the register
  limits stay the original's statics (imported), since the original's Display and materials ask them too.
- Test `tests/diff_rs_u3.cpp` (200 scenarios, 0 mismatches): programs and combiners built with each side's
  constructor and a logging copy of its vftable, a stand-in Display (capabilities, a program for the register
  limits, a random render-state block), stand-in lights; random constants, influences (transposed matrices),
  LoadCommonStates over random states/lights/matrices, vertex types, InitInstance, and real program texts from the
  render modules through LoadProgram (910 vertex program and 1226 texture program loads succeed). The original's
  scanner statics (0x100b7dc8..0x100b8127) hold these statics too, so the test keeps them across its resets.
  - Mutations caught: 15 of 16 (float regrouping in the transform, swapped operands, the light-type limit, the
    fog reciprocal, influence stride, support range, bump defaults, the symbol's local flag...). Survivor: the CRT's
    cos instead of x87 fcos gives the same bits for the cut-off angles tried.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U3a2, the program managers and resources (2026-10-27): 1221/2687 exports ours
- `ProgramManagers.cpp`: VertexProgramManager and TextureCombinerManager (0x18 bytes each: ReferenceCount, the
  Display +8, a HashTable of 97 FastDynamicArray buckets +0xc keyed by the PString node address modulo 97). The
  vertex manager's vftable holds its three Creates in reverse order (slot 2 the protected one, 3 by type, 4 by id).
  - VertexProgramManager::Create registers a program as "file:program" + the defines' text. A program already
    made is shared through slot 9 CreateShared (null in the base class) or referenced; without a database it
    comes from the built-in lists (result 2 becomes 4); else from the `.vtx` resource (the file, or
    "<program>.vtx"): 2 no resource or no instance, 6 the program failed to load, 8 the resource did not lock.
    A vertex type asked by id comes from Display slot 0x110 (referenced, none from id 99 on) and is released.
  - RegisterBuiltInPrograms parses a program list once (the quick parser of U2b2a) into {list, program, text}
    items; RestorePrograms asks every program even after one fails (5).
  - TextureCombinerManager: the latest manager made takes the texts registered so far; Create strips a trailing
    ".pxl", looks the name up, else loads "<name>.pxl" (4 without a database). When that resource does not lock,
    `out` is read as the caller left it, as in the original.
- `ProgramResources.cpp`: VertexProgramResource (0x88 bytes on KernelResource's Resource 0x5c: instances
  {program, name, defines text}, the last program, the file name, kernel, vertex type, manager, program name, a
  copy of the defines, error type) and TextureCombinerResource (0x70). CreateInstance reuses the last program or
  an instance of the same name and defines, else reloads the file with the new defines (the previous copy of the
  defines is leaked, as in the original).
- The managers' statics (theManager, builtInProgramList, registeredProgramList) stay the original's until U9, since
  only the original's Display::InitializeState makes the managers. RenderSystem now links KernelResource_7c.lib.
- CGShaderLoader/Technique/Constant and GLSLShaderBase moved to U3b: CGShaderLoader holds two MSVC8 std::map trees
  whose Jet-heap nodes other modules walk inline, so it needs a layout-exact tree.
- Test `tests/diff_rs_u3.cpp` part "managers" (200 scenarios, 0 mismatches; the whole test 600): both sides' real
  managers and resources over a stand-in Display/Kernel/database, a stand-in Lock that calls LoadTxt, Resource
  set up with a zeroed fake ResourceSystem, real program texts from the render modules written as
  `%TEMP%\rs_u3_*` files. The dump shows every hashed program with its reference count and the vertex types'
  counts; a success is sometimes asked again with `shared` flipped, and a few programs refuse RestoreProgram.
  - Program names per run come mostly from one list and the file is often the program's own list, so one
    resource hands out several programs (335 vertex program creates succeed over 200 runs); loaded resources are
    also asked for instances directly (slot 18).
  - Mutations caught: 22 of 22 (the shared/referenced branch, result codes 2/4/6/8, the ".vtx"/".pxl" names, the
    key separator, missing AddReference/ReleaseReference on programs, combiners and vertex types, Register's
    duplicate code, RestorePrograms' failure, the instance match without the defines, the copy/assign of the error
    type, the TC copy's combiner, SetParameters' program name, LoadTxt's last program).
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U3b1, VertexProgramBasic (2026-10-27): 1275/2687 exports ours
- U3b is split: U3b1 VertexProgramBasic (this step, 54 exports with ApiDataBase), U3b2 CGShaderLoader/Technique/
  Constant and GLSLShaderBase.
- `VertexProgramBasic.cpp`: the generator under the API modules' vertex programs. It writes DirectX vertex
  shader assembly ("def c%d, ..." declarations, "dp3"/"mad" macros). Layout 0xb4 bytes: its own part (vftable,
  vbptr, the declarations text +8, three ConstantData lists +0xc/+0x18/+0x24 for parameters, render states and
  matrix states, local and env register maps +0x30/+0x3c with their ranges starting at 999999, the API's
  constant register for the macros +0x48, an address offset +0x4c, temporaries +0x50/+0x54, the used-register
  table +0x58, an output flag +0x5c), then the virtual ReferenceCount +0x60, a vtordisp +0x68 and the virtual
  VertexProgram +0x6c. Today's compiler gives the same layout, vbtables and vtordisp thunks.
  - Own vftable: 1 AddSingleInstruction, 2 LoadProgramMatrices, 3 ProcessLocalConstSymbol, 4 ProcessVertexSymbol;
    0, 5 and 6 (the program header, the macros' constant register, a last touch on each macro line) answer 0
    and belong to the API module. VertexProgram's slots 13/14 are LoadSymbolTable / LoadInstructions.
  - LoadSymbolTable sorts the parser's symbols by class (program parameters, constants, states, address,
    vertex inputs, outputs, aliases, temporaries, Jet symbols) and gives each a VPSymbol {indexed, name,
    number} as its API data. LoadInstructions writes each instruction by the API's opcode name, or as a macro
    for ABS, DPH, FLR, LRP, NRM, POW, SGN, SUB, SWZ, XPD and M3x2..M4x4, then puts the API's header and the
    declarations in front of the text.
  - The statics (limits, symbol names, opcode/vertex/result maps, envParameters) are written by the API module,
    so they stay the original's. VS2005 mangled their `T const * const` type as P..B, today's compiler as Q..B:
    the imports are mapped with /alternatename.
  - Kept as in the original: FLR and XPD destroy a field-by-field copy of an operand, which frees that
    operand's swizzle, offset and modifiers; a failed load leaves wider register ranges over the older map;
    an unhandled matrix state leaves the stack's matrix.
- Test `tests/diff_rs_u3.cpp` part "basic" (200 scenarios, 0 mismatches; the whole test 800). Each side's
  constructor, the shared API statics set by the test, stand-in slots 0/5/6 and display matrix slots. Real
  programs go through LoadProgram (the parser calls slots 13/14). The macros also run directly on made-up
  operands (each both succeeds and fails), along with registers, temporaries, InitInstance, CompileProgram,
  LoadProgramMatrices and SetProgramState. Local parameter values are masked: they come from the parser's
  uninitialised stack in the original (U2b2b2).
  - Mutations caught: 32 of 36 (every macro's text and signs, matrix rows and offsets, the relative-address
    forms, free-register search and alignment, the program text order, the matrix product's order of
    operations, transposes, InitInstance's copies, the env parameters' release). Survivors are inputs the
    corpus never produces: the parser's hint bit 0 on local parameters, 3-row aligned matrix states, env
    parameters in a copied program, an alias with an empty name.
- In game: menu reached, JetLog identical to the baseline.

### Shared CRT for all modules (2026-10-27)
- Every module builds with /MD (`CMAKE_MSVC_RUNTIME_LIBRARY` MultiThreadedDLL unless a build passes its own):
  one CRT heap and one set of CRT statics for all of our modules instead of one per DLL. All modules now import
  VCRUNTIME140.dll, Kernel_7c and KernelStandard_7c also MSVCP140.dll; ucrtbase comes from Windows. The game bin
  gets the x86 `vcruntime140.dll` and `msvcp140.dll` from the VS redist (14.51). InputWin32_7c.dll shrank to
  29 KB, the size of the original. libjpeg-turbo (Bitmap::SaveJPG) is the vcpkg x86-windows-static-md build of
  the same version (3.2.0).
- All 48 diff tests rerun against the deployed /MD modules: 47 give 0 mismatches. diff_ks_window gives 2 per
  full run, only the Window active bit 1 (WM_ACTIVATE) set on the original's side: the same scenarios pass when
  replayed alone (`KSW_ONLY=<n>`) on the /MD and on the old static build, so it is an activation race with
  whatever window holds the foreground, not a regression.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U3b2, the Cg loader and GLSLShaderBase (2026-10-27): 1323/2687 exports ours
- `ShaderLoaders.cpp`, 48 exports. U3 is complete.
- CGShaderLoader (0x20 bytes: vftable, the last error String +4, two MSVC8 `std::map<const char*, T*>` +8 / +0x14
  for techniques and constants). The maps are written out with their VC8 layout ({allocator byte, head,
  size}; nodes {left, parent, right, key, value, colour, isnil}, 0x18 bytes on the Jet heap, keys compared
  with strcmp) and the Dinkumware algorithms the loader instantiates: insert with a hint (operator[]),
  insert, the red-black fix-up and rotations, copy, clear and lower_bound/find.
  - GetShader / GetShaderConstant find by name, else ask the API module's slot 4 / 5 (thiscall), keep the
    object under a Jet-heap copy of the name, or set the error text.
  - Release deletes every name and object in key order, then the nodes. The copy constructor and operator=
    copy the trees and share the names and objects.
  - The base slots return without popping their thiscall argument. Called through the original's
    GetShader, they make it restore the wrong registers, so only loaders with real factories can use it.
- CGShaderTechnique / CGShaderConstant: empty bases (every slot 0 / false; their deleting destructors are
  slots 5 / 13).
- GLSLShaderBase (0x2c bytes on ReferenceCount: name +8, the uniforms +0xc, the vertex attributes +0x18,
  an unset int +0x24, several light modes +0x28). The constructor binds position, texture0uv, color0 and
  normal.
  - SetParameter / SetParameterArray find or make a record per name (sized by value type). On first use
    they resolve it through the API's slot 8 for each light mode, then store a value only when it changed:
    NaN always counts as changed, and arrays are stored as their pointer. Each store marks all light modes
    dirty.
  - The original copies some floats with x87 loads, which would quiet a signalling NaN; ours copies them as
    they are (no SNaN reaches these).
- Test `tests/diff_rs_u3.cpp` part "shaders" (200 scenarios, 0 mismatches; the whole test 1000). Stand-in
  factories and uniform locations; both trees dumped node by node (links, colours, keys, values) after each
  request, copy, assignment and Release. GLSL uniforms of every type with repeated and NaN values, attribute
  rebinding, IsTextureUsed, IndexForLightMode.
  - Mutations caught: 24 of 26 (the key order, rotations, leftmost/rightmost upkeep, root colour, hinted
    insert positions, copy order and size, clear order, Release order, find, light-mode resolution, record
    growth, dirty flags, change tests, transpose flags, attribute rebinding and defaults, light-mode index,
    record sizes, the error copy). Both survivors give the same result as the original: an unhinted insert
    lands where the hint puts it, and the attribute array's growth formula differs only for counts it
    never reaches.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U5a, vertex types, rendering caches, buffer manager base, chunk hints (2026-10-28): 1420/2687 exports ours
- `VertexType.cpp`, `RenderCaches.cpp`, `include/jet/Geometry.h`: 97 exports. U5 was split into U5a (this step),
  U5b (GeometryChunk) and U5c (the stream chunks and GeometryChunkTemplate).
- VertexType (0x24 bytes on ReferenceCount): the id, the streams (stride, item capacity, item count, items) and
  the 0x20-byte items (type, name, offset, element count, data type, semantic, shared-with type, flag), the API
  data with the display's add/remove slots (0x3c / 0x40), API flags, user data, the registered name.
  - AddItem takes the item type's defaults for a count of -1, data type 0, semantic 1 and an empty name; the flag's
    default (-1) comes from the data type. ConfigureStream places the items once the stream is full: offsets
    one after another (a given offset stays), shared items on the item they share, whose type it then lowers
    by 15; the stride is the furthest end.
  - CompareVertex compares every item slot up to the capacity, filled or not, and leaves names out; a shared
    item's flag is never set, so it compares whatever the heap held.
  - The item-type defaults (39 entries made at module start) and the element sizes (9 data types) are our own
    copies. The statics `display`, `nextFreeId` and the registry (a 97-bucket HashTable of {type, name} keyed by
    the name's node) stay the original's until U9, since the original's Display builds the types.
  - The temporary VertexType declaration in VertexProgramParser.cpp (which imported GetVertexType and the copy
    constructor from the original) is gone; the parser uses ours.
- RenderingCache (0x28: vfptr, list node +4, vbptr +0xc, name, priority, host, two flags, virtual ReferenceCount
  +0x20; slots SetHost, Activate, Reset, Begin, End, Flush = End then Begin, Enter, Leave) and
  RenderingCacheManager (a ring through the caches' nodes with the manager as its head, by rising priority; a
  cache goes before the first one not below it). Begin keeps a geometry-only flag that End, Enter and Leave also
  honour; End activates every cache before ending any. Today's compiler gives the same layout and vbtable
  without a vtordisp.
- VertexBufferManager (0x78, 23 slots: 14 answer 0, 6 do nothing), the dynamic buffer sizes from the options
  "DynamicVertexBufferSize" / "DynamicIndexBufferSize", GetBufferStats copied under its critical section.
  StaticBuffer (8 bytes on ReferenceCount). GeometryChunkCreateHint and its Standard/Database/TerrainMorph
  variants (system bits 1/3/5; a 4-bit field at +0x10 is cleared, the rest of its byte kept). The internal
  NullVertexBufferManager stays with the Display (U9).
- Test `tests/diff_rs_u5.cpp` (parts types, caches, misc; 900 scenarios, 0 mismatches). Heap hooked on both
  sides; types built from random item lists with defaults, explicit offsets, shared items and partly filled
  streams; copies, copies with one field changed, comparisons, queries, registration and lookups, Shutdown,
  API data through a stand-in display, and the name reference counts at the end. Caches with logging slots in
  two managers: adds, removes, priority changes, Begin/End/Enter/Leave, assignment, destruction through the
  manager, the vbase destructor and the last reference. Buffer manager slots and stats, StaticBuffer, every
  hint constructor, assignment and Supports test.
  - Mutations caught: 39 of 40 (defaults tables, block sizes, item destruction, copies, comparisons, API data
    calls, AddItem defaults, sharing, placement and stride, queries, registry results and references, cache
    flags, Flush order, priority order, removal count, reset on destruction, geometry filter, buffer manager
    fields and slots, hint bits). The survivor changes a branch that cannot be reached (the shared item is
    always found in the stream GetItemStream picked).
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U5b1, GeometryChunk without the interpolation (2026-10-28): 1502/2687 exports ours
- `GeometryChunk.cpp`, 82 exports. U5b was split: U5b1 (this step) and U5b2 (InterpolateChunkData,
  InterpolateAllChunkData, GetAllChunkWeights, CopyChunkData: ~14 KB of float code). Until U5b2, slots 61..70 of
  our vftable call the original's exports through GetProcAddress.
- GeometryChunk (0x4c on ReferenceCount) is the base of the API modules' vertex and index buffers: primitive type
  and state flags, counts, three shared reference-counted arrays (indices, face neighbours, face normals), named
  float properties, bone matrices, opacity, surface area, user data. Layout and the 73 vftable slots in
  notes/RenderSystem.md.
- Every vftable slot of TS12 that "answers 0" is a pure virtual. The module's `_purecall` is `xor eax, eax; ret`,
  so a call into a pure slot returns 0 instead of aborting. Our module defines the same `_purecall`. This is also
  why base slot 4 answers 0 while the exported `GeometryChunk::InitializeBuffers` answers true (a pure virtual
  with a body).
- TS12 behaviour kept:
  - Without an index array, the index lookups (GetVertexIndex, GetTriangleIndexes) read address 0x10: the code
    tests `block + 0x10 != 0`.
  - RenderOutlines advances two vertices per triangle while writing six, so only the last triangle keeps all its
    edges, and the draw call reads unwritten stack after it.
  - A fan triangle n is (0, n, n+1).
  - Several floats go through the x87 stack (InitInstance, outline positions, GetChunkProperty, property removal),
    which quiets a signalling NaN. Ours does the same explicitly.
  - With two NaN operands, SSE `addss`/`mulss` return the first one, so operand order matters. Today's compiler
    commutes even `_mm_add_ss`, so the vertex/triangle normal lines and ResetToAmbient use inline assembly.
  - ReleaseRegistrationData frees the registry entries but leaves null slots in the buckets (any later lookup
    faults in both modules). The registry stays the original's static.
- Test `tests/diff_rs_u5.cpp` part "chunks" (300 scenarios per run, 1500 checked once, 0 mismatches; the whole
  test 1200). Chunks are built with each side's constructors, then given a test vftable whose pure slots provide
  vertex arrays, a vertex type built with that side's VertexType, static buffers and logging. Covered: index
  lookups for every primitive type, properties, bones, the array set/duplicate/resize calls, InitInstance,
  AppendChunk, the load format choice and loading, opacity, ambient and lighting, the debug drawing (lines compared
  as far as written), the registry. A stand-in display has debug flags, state block and line drawing; there is
  also a stand-in light and material.
  - Mutations caught: 61 of 61. The test was strengthened along the way (the name reference counts, properties over several chunks, signalling NaNs passed as bits: a float returned by value goes through the x87 stack and comes back quiet, inexact sums for the addition order); one mutant writing past a block end shows as a heap fault.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U5b2, GeometryChunk interpolation and copy (2026-10-28): 1514/2687 exports ours
- `GeometryChunk.cpp`, 12 exports: InterpolateChunkData (the virtual one between two vertices at a position, and
  the non-virtual one in a triangle), the eight InterpolateAllChunkData, GetAllChunkWeights, CopyChunkData. The
  GetProcAddress forwards of vftable slots 61..70 are gone. Slot meanings in notes/RenderSystem.md.
- Each one walks the list of item types and handles an item only when both chunks have it (slot 44 not -1) with
  the same data type and element count: floats of 1..4 elements, udwords of 1. Then it calls slot 31 (1, 0, -2).
- MSVC lays out overloaded virtual functions in reverse order of declaration. The first build had slots 62..69
  in the wrong order (the test crashed on the first call); the header now declares those overloads backwards.
- Float details kept from the TS12 assembly:
  - The compiler unrolled the loops by four and chose a different operand order for some unrolled steps and for
    the tail (for instance the two-vertex float case adds `b*wy + a*wx` on the second step of each four, `a*wx +
    b*wy` elsewhere). With two NaN operands SSE returns the first one, so the order is kept per step.
  - The three-vertex and two-vertex position cases read the source vertices once before the loop, so a
    destination that overlaps the source sees the old values.
  - Lengths are KernelStandard's table square root, inlined on the x87 stack and left unrounded for the divide.
    The udword blends are x87 sums of `u * w` (u read as unsigned), truncated through `fistp qword`. Both run
    as inline assembly, so they follow the caller's precision control as the original does.
  - CopyChunkData and GetAllChunkWeights copy floats through the x87 stack, which quiets a signalling NaN.
- Test `tests/diff_rs_u5.cpp` part "interp" (2000 scenarios, 0 mismatches; the whole test 1500 at 300 rounds per
  part, 0 mismatches). Chunks get a test vftable: a made-up vertex type, one array per item type behind slots 46..55,
  logged slots 41, 44 and 31. The real slots 61..70 call each other through it. Item layouts differ a little
  between the chunks; some type lists span two FastDynamicArray chunks; the source and the destination are
  sometimes the same chunk; positions sometimes equal the endpoints (the 0 / 1 shortcuts) or make a degenerate
  triangle. Weights and data include NaNs with their own payloads, signalling NaNs and inexact values.
  - Mutations caught: 45 of 47. The two survivors change the operand order of a product inside the triangle
    normal. Only the normal's NaN payload depends on it, and GetBaryCentricWeights does not pass that payload
    on.
- In game: menu reached, JetLog identical to the baseline.

### RenderSystem_7c U5c, the stream chunks and GenericReferenceArray (2026-10-28): 1658/2687 exports ours
- `GeometryStreams.cpp`, 144 exports: GeometryChunkSingleStream, GeometryChunkMultiStream,
  GenericGeometryChunkSingleStream, GenericGeometryChunkMultiStream (122) and GenericReferenceArray (22, moved
  here from U8: the Generic chunks keep their vertices in it, and its code sits next to theirs). U5 is complete.
- GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> has no exports. Only
  NullMaterial::CreateGeometryChunk makes it, so it moves to U7 with NullMaterial.
- The base GeometryChunk now declares its pure slots by name (CreateInstance, LoadBuffers, the render and pass
  calls, the vertex state bits, static buffers, vertex type, item views, vertex program). Overload pairs are
  declared backwards (MSVC reverses them); our five vftables match TS12 slot by slot (scratchpad vtcmp.py
  compares the export names behind every slot).
- Layouts: SingleStream 0x5c (+0x4c static vertex buffer, +0x50 static index buffer, +0x54 vertex type, +0x58
  vertex program; the vertex state lives in the chunk flags: 0x10000 dynamic, 0x20000 dirty, 0x40000 unshared).
  MultiStream 0x60 (+0x4c an array of {static buffer, state bits 1 dynamic / 2 dirty / 4 unshared} per stream,
  +0x5c stream count). The Generic chunks add the vertex memory (+0x5c one GenericReferenceArray, or +0x60 a
  shared block of one per stream) and a shared ItemOffsetTable (+0x60 / +0x64: the stride, or the strides and
  total size, and the offset, or stream and offset, of each of the 39 item types).
- The draw calls go to the Display: slot 0xf0 (dynamic vertices and indices), 0xec (static buffers), 0xf4 (multi
  stream: an on-stack array of {data, buffer, dirty} per stream), 0xf8 (one pass), 0xfc (set up the passes),
  0x100 (clean up), 0x104 / 0x108 (make a vertex / index buffer); uploads go through the display's vertex buffer
  manager (+0x20c, slots 0x20 and 0x2c). Before a multi stream draw the manager gets slot 0x40 unless the display's
  byte +0x213 is set. Without a vertex program the "program" handed to the display is the vertex type.
- TS12 behaviour kept:
  - GenericGeometryChunkMultiStream's constructor passes its flags argument as the stream count of the base and 4
    as the flags. So CreateInstance makes a stream record array as long as the chunk flags, and every Generic
    multi stream chunk has flags 4.
  - Its CreateInstance(vertices, ...) builds the copy with the vertex type (arrays and an offset table), then
    replaces the table and builds the arrays again; the first ones are leaked.
  - The multi stream RenderPrimitives draws the chunk's own primitive type and count, not the ones it is given.
  - GeometryChunkMultiStream::InitInstance copies the bones a second time after GeometryChunk::InitInstance.
  - SetVertexDataDynamic(false) sets the dynamic bit as well as the unshared bits.
  - SetVertexType lets the old type go before taking the new one.
  - The "dirty" bools handed to the display are `flags >> n & 0xffffff01`: the bit with everything above it.
  - Freeing a multi stream array block that is still shared leaves the pointer in place (its callers overwrite it).
- Test `tests/diff_rs_u5.cpp` part "streams" (1000 scenarios, 0 mismatches). The four classes are made with each
  side's constructors and driven through their own vftables and the non-virtual exports (InitInstance,
  CreateVertexArray, SetupMultipassRenderInternal, the stream flags), with real VertexTypes (1..3 streams) and
  StaticBuffers of the side. A stand-in display logs the draw calls and makes buffers, a stand-in buffer manager
  logs the uploads, a stand-in vertex program has its ReferenceCount behind a vbtable as VertexProgram does; their
  answers come from a script RNG reset per side. GenericReferenceArray gets its own random sequences. Inputs that
  would make both modules read or write past a block are avoided (a resize that copies from a short array, a
  multi stream chunk with fewer stream records than streams); item views are logged as offsets into their stream's
  block, because past a short block they point at whatever the heap has there.
  - Mutations caught: 50 of 51. The survivor hands the vertex type of stream 0 instead of the given stream when
    there is no program; the vertex type getter of these classes ignores the stream, so it is equivalent. The test
    was strengthened along the way: the index/vertex state bits are flipped and the vertex count lowered before
    draws (the static/dynamic branches and the max/num vertex choice), bones are added before InitInstance.
  - The first mutation run gave false survivors: the incremental build sometimes kept the previous DLL after the
    source was rewritten. The harness now moves the source's mtime into the future and checks the DLL changed.
- The other RenderSystem diff tests (u1, u2, u2b, u3, u4) still give 0 mismatches.
- In game: menu reached, JetLog identical to the baseline.

### Prior art: open source around Trainz and Jet (searched 2026-10-21)

- No open engine, decompilation or leaked source of Auran Jet or trainz.exe was found. The only reference is
  the Jet SDK 1.0 headers we already have. (Dark Reign 2 went open source in 2012, but it is Pandemic's
  engine, not Jet.)
- Usable as format references, all on GitHub:
  - `rileyzzz/ChumpLib` (MIT): .chump / .tzarc loading and saving, plus an early `.ja` test app.
  - `pgroenbaek/cdp-unpack` (MIT): CDP / CHUMP extraction (Python).
  - `rileyzzz/BlenderIMExporter` (MIT): writes .im and .kin.
  - `naseno15/mstn-trainz-studio` (C++/Qt, no licence stated): reads .im / .kin.
  - `rileyzzz/TrainzTexture` (GPL-3.0, keep as a reference only): .texture.
  - `rileyzzz/TrainzMapFiles` (no licence): .gnd, Trainz 1.1-2019.
  - `rileyzzz/TwinklesEditor` (no licence): PFX particles.
  - `rileyzzz/TANESubstance` (MIT): legacy material shaders.
  - `rileyzzz/TNICPPHelpers` (MIT): TNI type helpers. The TNI SDK itself is licensed by N3V on request.
  - `rileyzzz/TrainzScript-antlr4` (MIT): a GS grammar.
  - `Cethric/trainz-language-server` (Apache-2.0, Rust): GS and Soup parser.
- SpeedTree: Panda3D (BSD) has an SRT loader, but it calls the SpeedTree SDK. There is no open SRT parser.
- Mozilla and Cg need nothing found here: WebView2 and the Cg 3.1 x64 binaries cover them.

### Step 6/7 decisions: Mozilla, Cg, SpeedTree (2026-10-21)
- **Two separate HTML stacks.**
  - The game's own HTML renderer covers TrainzScript's Browser / HTMLBuffer natives and the in-game info
    windows:
    - RTTI: `IBrowser`, `HTMLWindow`, `HTMLWindowBase`, `HTMLWindowPhone`, `HTMLBuffer`, `HTMLToken*`,
      `IHTMLBox/Button/Clock/Video`;
    - GS natives such as `Browser::LoadHTMLString`.
    - It contains no Mozilla, so the add-on HTML contract does not depend on XULRunner.
  - Mozilla (`bin\mozilla`, XULRunner 1.9pre) is used only by `IWebBrowser` / `DlgWebBrowser` /
    `lyrWebBrowser`:
    - trainz.exe loads `mozilla\xpcom.dll` and `mozilla\xul.dll` at run time (no static imports anywhere);
    - it runs the browser on its own thread (`MozillaThread`) and marshals `Mozilla*Event`s (create, destroy,
      resize, mouse, focus, navigate, clipboard);
    - XPCOM contracts it uses: `nsWebBrowser`, window-watcher, prefs, cookies, observer, directory service.
    - What it shows: online pages only, under `game.ts2009.com` (home, news bar, help wiki), simulatorcentral
      DLC pages and the auran asset search.
  - **Decision: WebView2** behind a small `IWebBrowser` adapter (navigate, resize, focus, close, events).
    - It is a native child window and Windows-only like the game; the runtime ships with Windows 11.
    - CEF is not needed: nothing renders off-screen into the GL scene that we know of. Still to confirm in
      Ghidra: how `lyrWebBrowser` presents. trainz.exe imports `CreateDIBSection` / `GetBitmapBits`.
    - A first x64 fallback can simply open the URL in the system browser.
- **Cg.**
  - Only RenderOpenGL_7c imports Cg: cg.dll (13 calls) and cgGL.dll (15 calls). The calls are the CgFX effect
    API (`cgCreateEffectFromFile`, techniques and passes, `cgGLRegisterStates`, `cgGLGetLatestProfile`) plus
    parameter setters.
  - Profiles named: vp40 / fp40 / gp4vp / gp4fp. Compile defines name SpeedTree profiles
    (`-DSPEEDTREE_PIXEL_PROFILE=`).
  - The user's content (UserData, Builtin) holds no loose .fx/.cg/.cgfx: the shaders are built in.
  - **Decision:** for x64, the Cg Toolkit 3.1 x64 runtime (same API, redistributable). Translating the
    effects to GLSL is a later renderer task, not a blocker.
- **SpeedTree.**
  - Not in trainz.exe. It is the TNI plugin `bin\plugins\TNISpeedTree.dll` (196 KB, Jan 2014; plus a `_d`
    debug build), with SpeedTree 5.x statically linked (RTTI `SpeedTree::CCore`, `CForest`, `CTreeCell`,
    `CInstance`).
  - Shaders: `bin\plugins\SpeedTreeShaders\*.fx`, compiled by RenderOpenGL through Cg.
  - Exports: `TNIEntryPoint` only. Imports: KERNEL32, the VC80 CRT and 37 functions of
    trainznativeinterface.dll:
    - streams, strings, arrays, objects;
    - `TNIGraphicsBegin/EndScene`;
    - `TNIGraphicsGetStreamForCommand`;
    - view, light, fog and wind queries;
    - game time;
    - `TNICheckUserHasProductRight`.
    It draws by writing command streams, never by touching the renderer.
  - Content: **524 `.srt` trees** in the user's UserData (for example kuid2:211025:20261 and other community
    trees), so this matters for add-on compatibility.
  - **Decision:** a 32-bit helper process.
    - It loads the original TNISpeedTree.dll with a stand-in trainznativeinterface.dll.
    - The stand-in forwards the 37 calls over shared memory to the x64 game. The traffic is value data and
      streams, plus two calls per frame.
    - The same bridge serves any other 32-bit TNI plugin.
    - Rewriting the SRT loader and renderer is possible later, but the format is proprietary.

### TrainzScript native API (the add-on compatibility contract)
- `scripts\*.gs` (104 headers) declare **859 `native` methods in 60 classes** (Vehicle 82, Train 76, World 72,
  Interface 40, Browser 38, MapObject 31, Asset 29...). Inventory: `gs_native_api.tsv` (`tools/gs_natives.py`).
- trainz.exe holds 1009 `Class::Method` strings in 140 classes (`qualified_names_trainz.txt`) with the same
  per-class counts: almost certainly the native-binding registration table (name string ↔ C++ implementation).
  Following xrefs from these strings maps every GS native to its implementation. TODO after Ghidra analysis.
- **Binding format, confirmed**: each native is a signature string `$<ret>@<Class>::<Method>(<params>)`,
  e.g. `$string@Asset::GetCreatorName()` at 0x00ae53e0. A per-class registrar (Asset: FUN_009cfd80) interns
  it with `Jet::PStringCache::Create`, passes the handler address (PUSH imm in Asset, MOV EBP,imm in Vehicle)
  and calls a register function (Asset: FUN_00611230(node,&sig,&cls,handler)); nodes go into a global list
  at FUN_007b2f80()+0x4c.
- `ghidra_scripts/MapGsNatives.java` (signature-driven forward scan) → `gs_native_handlers.tsv`:
  936 signature strings, **850 natives with handler, 113 engine→script callback refs (83 distinct)**.
  Handlers renamed `gs_<Class>_<Method>` in the project. `tools/verify_gs_map.py` checks: 782 header natives
  matched; 0 handlers shared between methods; 0 wrong names. Remaining diffs are a header-parser bug
  (`OnlineAccess` read as `<global>`) plus a few natives not in the headers (e.g. Interface::EnableMessageBar).
- trainz.exe after mapping: 46 420 functions, 51.0% named.
- Only 82 strings use the `Class::Method>` log form (`log_names_trainz.txt`); JetLog builds most prefixes at runtime.

### Config
- `Trainz.cfg` (88 bytes) is binary, not the options file. Real options: TODO (look for `trainzoptions.txt`).

## Open questions
- Who loads `ijl10.dll`, `cudart.dll`, `dx7vb.dll` at runtime?
- Does `trainznativeinterface.dll` + OpenSSL do licence checks that a rebuilt exe must keep?
- Exact GS (TrainzScript) bytecode format and whether it stores 32-bit pointers in the VM.
