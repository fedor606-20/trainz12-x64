# 🚂 trainz12-x64

⚠️ THIS IS NOT FINAL WORK, I POSTED THAT PEOPLE COULD HELP MAKE IT FASTER!

🎯 A 64-bit port of Trainz Simulator 2012 that keeps add-ons working. The approach: the game's Jet engine modules
are rewritten one by one as drop-in 32-bit DLLs that run inside the original game. Each one is checked against
the original in the same process (differential tests) and in game. Once nothing original is left, the code is
built for x64.

🔒 This repository holds **only our own work**. It contains no game files, no original DLLs, no decompiled code and
no Jet SDK files. The modules in `src/jet/` are a clean C++ rewrite made by studying the game's binaries. You need
your own installed copy of the game to build against it, test it or run it.

## ✍️ From author
I am not programmer, but i wanted project like that long time and i guess many people do. So, seeing ai boom
in decompiling and rewriting games i decided try do same thing and its already did lot of work.
I published there stuff that people can work with to help speed up process and make it faster.

Currently its in start, and its 10% of reversed engine stuff.
It's planned to reverse engine and game, after that start to rewrite and optimize the game. 
Main goal also is save compatibility with addons

💬 Here is a discord server, so you can join and talk here!
https://discord.gg/DmqMJGtCZ

## 📊 Progress

<!-- progress:start -->
Measured by code size: a finished module counts whole, a partial one by the code behind the exports that are already ours (`tools/progress.py`, 2026-10-28).

```
🌍 Whole game         ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░   7.9 %   7,112 KB left of 7,719

🧩 Jet engine         ████████░░░░░░░░░░░░░░░░░░░░░░  27.5 %   1,601 KB left of 2,208  7/19 modules
  🔧 Engine core      ██████████████████████████████ 100.0 %       0 KB left of   378  4/4 modules
  🎮 Input            ████████████████████████░░░░░░  81.2 %      19 KB left of   103  2/3 modules
  🎨 Graphics         ███░░░░░░░░░░░░░░░░░░░░░░░░░░░  10.7 %   1,210 KB left of 1,355  1/8 modules
  🪟 User interface   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   0.0 %     354 KB left of   354  0/2 modules
  🐞 Debug            ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   0.0 %      18 KB left of    18  0/2 modules

🚂 Game               ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   0.0 %   5,434 KB left of 5,434  0/2 modules
💤 Not seen loaded    ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   0.0 %      76 KB left of    76  0/2 modules
```

Jet is Auran's general engine, a set of DLLs: core services, input, graphics and UI. The train simulation itself (physics, routes, sessions, Surveyor, TrainzScript, the asset system) is the game, `trainz.exe`, the biggest part, not started yet.

| Group | What it is |
|---|---|
| 🔧 Engine core | Jet: memory, strings, files, .ja archives, config, threads, logs, resources |
| 🎮 Input | Jet: keyboard, mouse, joysticks, key bindings |
| 🎨 Graphics | Jet: 2D/3D scene, meshes, materials, water; OpenGL and DirectX 9 renderers (picked in the launcher) |
| 🪟 User interface | Jet: windows, buttons, menus, fonts |
| 🐞 Debug | Jet: debug console and overlays |
| 🚂 Game | trainz.exe: simulation, trains, routes, Surveyor, TrainzScript, assets; TNI plugin API |
| 💤 Not seen loaded | Jet: Lexer (nothing references it), movie player (not loaded at the menu; re-check in a session) |

Plan steps finished: 9 of 11 (`PORT_PLAN.md`). After the 32-bit replacement come the x64 build and the add-on compatibility tests.

<details><summary>Per module</summary>

| Module | Code | Functions | Exports ours | Status |
|---|---:|---:|---:|---|
| 🔧 **Engine core** | | | | |
| Kernel_7c | 52 KB | 534 | 256/256 | ✅ done |
| KernelStandard_7c | 286 KB | 2,250 | 1761/1762 | ✅ done* |
| KernelResources_7c | 10 KB | 118 | 51/51 | ✅ done |
| KernelResource_7c | 31 KB | 396 | 187/187 | ✅ done |
| 🎮 **Input** | | | | |
| InputWin32_7c | 12 KB | 100 | 1/1 | ✅ done |
| InputSystem_7c | 71 KB | 560 | 327/327 | ✅ done |
| InputDirectX_7c | 19 KB | 122 | 0/1 | ⬜ not started |
| 🎨 **Graphics** | | | | |
| Render2DCore_7c | 17 KB | 167 | 85/85 | ✅ done |
| RenderSystem_7c | 351 KB | 3,628 | 1776/2687 | 🟡 partial |
| Render3DCore_7c | 458 KB | 3,168 | 0/2500 | ⬜ not started |
| Render3DFoundation_7c | 194 KB | 1,453 | 0/1084 | ⬜ not started |
| Render3DExtension_7c | 38 KB | 326 | 0/186 | ⬜ not started |
| RenderOpenGL_7c | 135 KB | 944 | 0/51 | ⬜ not started |
| RenderDirectX9_7c | 70 KB | 404 | 0/41 | ⬜ not started |
| WaterSystem_7c | 92 KB | 714 | 152/490 | 🟡 partial |
| 🪟 **User interface** | | | | |
| InterfaceSystem_7c | 319 KB | 3,358 | 0/2022 | ⬜ not started |
| InterfaceResources_7c | 34 KB | 345 | 0/169 | ⬜ not started |
| 🐞 **Debug** | | | | |
| DebugSupport_7c | 16 KB | 201 | 0/119 | ⬜ not started |
| DebugInterface_7c | 3 KB | 50 | 0/38 | ⬜ not started |
| 🚂 **Game** | | | | |
| trainz.exe | 5,411 KB | 45,438 | - | ⬜ not started |
| trainznativeinterface.dll | 23 KB | 369 | - | ⬜ not started |
| 💤 **Not seen loaded** | | | | |
| Lexer_7c | 18 KB | 233 | 0/185 | ⬜ not started |
| DSMoviePlayer_7c | 58 KB | 728 | 0/1 | ⬜ not started |

`done*`: KernelStandard_7c keeps one export, `HardwareException::GetCallStack` (an x86 stack walk), forwarded to the original until the x64 rewrite.

</details>
<!-- progress:end -->

## 🗂️ What is here

- 🗺️ `PORT_PLAN.md`: the approach and the numbered queue of next steps.
- 📒 `MODLOG.md`: confirmed facts about the binaries (with addresses) and what each step did and measured.
- 📝 `notes/`: per-module analysis (`KernelStandard.md` is the main one: units, layouts, kept bugs, test notes).
  `notes/data/*.py` and `notes/ghidra/*.java` are the scripts behind it (their data dumps are not included).
- 💻 `src/`: our code, split like the game: `src/jet/` for the Jet engine modules, `src/trainz/` (later) for
  trainz.exe and trainznativeinterface.dll.
- 🧩 `src/jet/`: the rewritten Jet modules and their tests (CMake), one folder per group from the table above:
  - ⚙️ `core/`: `Kernel_7c` (all exports ours), `KernelStandard_7c` (1761 of 1762 exports ours;
    `HardwareException::GetCallStack` still forwards to the renamed original), `KernelResource_7c`,
    `KernelResources_7c`.
  - 🎮 `input/`: `InputSystem_7c`, `InputWin32_7c`.
  - 🎨 `graphics/`: `Render2DCore_7c`, `RenderSystem_7c` (in progress).
  - `ui/`, `debug/` and `not_loaded/` appear when their first module is started.
  - 📐 `include/jet/`: our declarations of the Jet classes with their binary layouts.
  - 🧪 `tests/diff_*.cpp`: differential tests. Each loads our module and the original in one process and runs the
    same random scenarios on both.
- 🔍 `probe/`: PowerShell scripts that start the game and check the menu, real input and the Launcher.
- 🛠️ `tools/`: export / import dumps, `.def` and import-library generation, thunk checks, the SSE lifter,
  screenshots through desktop duplication, census.
- 🐉 `ghidra_scripts/`: headless Ghidra scripts (function stats, decompile, vtables, TrainzScript native mapping).
- 🧭 `ghidra_maps/`: function and vtable tables (addresses, sizes, names) from Ghidra. `ghidra_maps/jet/` has the
  same group folders (`core/`, `input/`, `graphics/`, `ui/`, `debug/`, `not_loaded/`); `ghidra_maps/trainz/` is
  the game.
- 📜 `gs_native_api.tsv`, `gs_native_handlers.tsv`: the TrainzScript native API and the trainz.exe address of each.

## 🔨 Building (32-bit drop-in modules)

Requirements: Visual Studio 2022 or later (x86 tools), CMake, Python 3, and an installed Trainz 2012 (build
61388). For `KernelStandard_7c`, libjpeg-turbo from vcpkg (`vcpkg install libjpeg-turbo:x86-windows-static`).

The CMake files still use the paths of the original setup (`D:/trainz12-decomp/...`, `D:/games/...`). Change
`IMPLIB`, `EXPORTS`, `ORIG_KS` / `ORIG_IS` and `VCPKG_X86` in `src/jet/CMakeLists.txt` to your paths. The export lists
(`exports/*.tsv`) and the import libraries of the original modules (`implib/`) are generated from your own game
files with `tools/dump_exports.py` and `tools/gen_implib.py`. A few data tables are also read from your installed
game at build time and never committed.

```
cmake -S src/jet -B build32 -A Win32
cmake --build build32 --config Release
```

To try a module in game, rename the original (for example `KernelStandard_7c.dll` to `KernelStandard_7c_orig.dll`)
and put ours next to it. To run a test, pass it that directory: `diff_ks_bitmap_draw.exe "<game>\bin"`.
Keep a backup of the original files.

## 🤝 How to help

Pick the next step in `PORT_PLAN.md`, read the notes for its module, and follow the existing pattern: rewrite a
unit, add its names to `implemented.txt`, write or extend a differential test until it shows 0 mismatches, then
check in game. `MODLOG.md` records what has been measured. Then refresh the progress block above with
`python -I tools/progress.py --write`.


