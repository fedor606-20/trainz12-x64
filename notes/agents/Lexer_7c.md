# Lexer_7c.dll — analysis/planning report (read-only)

## Overview / role

`Lexer_7c.dll` is a small Jet engine module (census: 21,684 bytes `.text`/code, 185 exports, 185 C++ exports, 8 RTTI entries; `census.json:430-441`). README/PORT_PLAN place it in **`jet/not_loaded` / "Not seen loaded"** and describe it as "Lexer (nothing references it)" / "not loaded at the menu; re-check in a session" context (`README.md:42,55,90-92`, `PORT_PLAN.md:31`, `MODLOG.md:92`).

**Evidenced API role (from exports, not runtime observation):** a compiler-front-end utility library exposing source lexing/tokenizing and compiler diagnostic collection/formatting in namespace `Jet`:
- `Jet::Lexer` — core lexer over input/string with cursor/marker state, line tracking, character classification and skip helpers (static and instance), number/string/file-name/line-directive lexing, token extraction/case conversion, save/restore state (virtual), and marker management (`exports/Lexer_7c.tsv` ordinals 9-11,19,25,34,43-44,64-85,87,89,93-95,99,107,109-116,118-140,145-156,158-159,164-179,182-184 etc.).
- `Jet::Tokenizer` — token-stream wrapper over lexer/string with current/next token control, match/skip operations, next-token function hook, save/restore state (virtual) (`ordinals 12-15,20,26,35,46,86,88,117,121,141-144,156,108,114,122`).
- `Jet::CompilerError` / `Jet::FatalCompilerError` — error objects with source file/line/type/message formatting and vftables (`ordinals 3-4,8-9,16,18,22,24,30-31,49-51,55,161-162,164,174`; `vtnames_Lexer_7c.dll.tsv` shows `CompilerError`/`FatalCompilerError`/`Lexer::Error` vfunc slots).
- `Jet::ErrorList` — error/warning/fatal collection and printing, printf/varargs formatting, add/merge operations (`ordinals 5-7,17,23,37-38,48,89-92,96,179-180,184-185`).
- `Jet::FastDynamicArray<Jet::CompilerError*,13, Jet::Less<...>>` — specialized container used by `ErrorList` (many exported ctors/dtor/operators/mutators/iterators/search/sort; `ordinals 1-2,16,21,27-29,36,40-42,44-45,47,52-54,56-64,66,68,75-77,95,97-106,160,163`).
- 4 exported vftable symbols: `CompilerError`, `FatalCompilerError`, `Lexer`, `Tokenizer` (`ordinals 30-33`).

All 185 exports in `D:\trainz12-decomp\exports\Lexer_7c.tsv` demangle to `Jet::...` symbols (plus vftables); no non-`Jet` exported API was observed.

## Inventory

### Source/evidence artifacts present
- `ghidra_maps/jet/not_loaded/Lexer_7c.dll.functions.tsv` — 248 function entries (header+248 rows).
- `ghidra_maps/jet/not_loaded/vtnames_Lexer_7c.dll.tsv` — 8 named vtable-slot entries (CompilerError/FatalCompilerError/Lexer::Error/Tokenizer slots).
- `D:\trainz12-decomp\exports\Lexer_7c.tsv` — 185 exports (ordinal/rva/mangled/demangled).
- `tools/progress_exports.tsv:21` — `Lexer_7c	185	0`.
- `census.json:430-441` — module metadata/imports.

No implementation sources, `.def`, import-lib references, or CMake integration for `Lexer_7c` were found in this repo (`src/`, `exports/` in-repo, `tools/` references limited to progress/census/docs).

### Function map breakdown (`ghidra_maps/jet/not_loaded/Lexer_7c.dll.functions.tsv`)
- Total rows: **248**.
- Namespaced/recognized API-ish entries include `Jet::Lexer` (many), `Jet::Tokenizer`, `Jet::CompilerError`, `Jet::FatalCompilerError`, `Jet::ErrorList`, `Jet::FastDynamicArray<Jet::CompilerError*,13,...>`, and `Jet::Lexer::Error` vfunc slots.
- **16 unnamed/global helper functions** remain (`FUN_100019b0`, `FUN_10001a10`, `FUN_10002880`, `FUN_100028e0`, `FUN_10002960` [813 bytes, largest unnamed], `FUN_10002ca0`, `FUN_10002d80`, `FUN_10002da0`, `FUN_10002dc0`, `FUN_10003510`, `FUN_10003a70`, `FUN_100056d0`, `FUN_10005750`, `FUN_10005800`, `FUN_100058de`, `FUN_10005c3d`) — purpose not analyzed here.
- **Compiler/runtime/thunk/unwind/import plumbing present** (~52 entries by heuristic): MSVCR80 imports/thunks (`terminate`, `initterm`, `initterm_e`, `_amsg_exit`, `__CppXcptFilter`, `_crt_debugger_hook`, `except_handler4_common`, `type_info` dtor, `memcpy`, `_CxxThrowException`), CRT/security/EH helpers (`__security_check_cookie`, `__ArrayUnwind`, `` `eh_vector_destructor_iterator' ``, `__CRT_INIT@12`, `___DllMainCRTStartup`, `__SEH_prolog4/epilog4`, `__ValidateImageBase`, `__FindPESection`, `__IsNonwritableInCurrentImage`, `__alloca_probe`, `___security_init_cookie`, `___report_gsfailure`, `entry`), FID conflict helper, and many `Unwind@...` entries.
- Exported symbols vs function-map entries: **185 exports** vs **248 analyzed functions**; delta consistent with internal helpers + CRT/unwind/thunks + vtable-slot/helper entries not all exported.

### Class/API surface summary (from demangled exports)
- **CompilerError (base):** construct/copy/assign/dtor, `SourceFileName`, `SourceLineNo`, `Type`, `ErrorTypeString` (instance/static), `FormatOutput`, vftable.
- **FatalCompilerError:** construct/copy/assign/dtor, vftable (specialized fatal type).
- **ErrorList:** construct/copy/assign/dtor, counts, `Add`, `AddErrorList`, `ErrorPrintf`/`vErrorPrintf`, `WarningPrintf`/`vWarningPrintf`, `PrintErrors`.
- **Lexer:** rich char/token parsing and source-position/marker API, several static parse/convert helpers (`Is*`, `Skip*` static variants, `ToLower/Upper` static, `antoin/atoin/atofn/hexatoin`, `IsHexNumber`), virtual state save/restore, vftable.
- **Tokenizer:** token iteration/control, `Match`, `SkipTo` overloads, `SkipTokens`, virtual state save/restore, vftable.
- **FastDynamicArray<CompilerError*> specialization:** full value/container API (add/insert/remove/pop/push/sort/binary-search/index/swap/reset etc.), indicating diagnostics storage implementation detail exposed in ABI.

## Evidence of usage / references (or lack thereof)

**Project-documented status (strong, but documentary):**
- `README.md:55,91` lists Lexer under "Not seen loaded": "Lexer (nothing references it)" and module table `Lexer_7c | 18 KB | 233 | 0/185 | ⬜ not started`.
- `MODLOG.md:92` states: "**Not loaded at the menu**: Lexer_7c (nothing references it; not in plugins.ini, so probably dead), ..." with advice to re-check inside a driving session.
- `MODLOG.md:25,35` mention Lexer in binary/analysis context ("Lexer 22 KB..." in an early inventory note; "Best: Lexer 86%..." as naming/analysis quality, not implementation).
- `PORT_PLAN.md:23,31` places "Kernel, Lexer, KernelResources..." in early small-module discussion but also states "Modules not seen loaded (Lexer, movie players) come last or not at all."

**Repo-local reference scan (this workspace, 2026-11-19):** `Lexer_7c` string occurrences found only in:
- `README.md`, `PORT_PLAN.md`, `MODLOG.md` (status/planning/history text)
- `census.json` (module entry)
- `tools/progress.py` (module listed in `jet/not_loaded` group) and `tools/progress_exports.tsv`
- `notes/KernelStandard.md` (import-count/context line: `Lexer_7c.dll | 30` — appears as a data point in KernelStandard notes; not an implementation dependency in this repo)
- `ghidra_maps/jet/not_loaded/Lexer_7c.dll.functions.tsv`, `ghidra_maps/jet/not_loaded/vtnames_Lexer_7c.dll.tsv`

No `src/**` implementation, no in-repo `exports/Lexer_7c*` artifacts, no CMake/build wiring, no loader/patch/test references to `Lexer_7c` were found.

**Important disambiguation:** MODLOG also references other lexer/scanner code unrelated to this DLL (e.g. KernelStandard `ConfigLexer` flex scanner, InputSystem binding scanner `Lexer.cpp`; `MODLOG.md:737,792,850,1058,1745`). These are different implementations/contexts and are **not evidence** that `Lexer_7c.dll` is loaded or required.

**Conclusion:** Available evidence supports treating `Lexer_7c` as *currently unused/unloaded in observed/menu context and unreferenced in this port repo*, but absence-of-load was only explicitly stated for menu/plugins.ini in existing notes. A definitive runtime-negative would require a targeted session/path check (e.g. driving session/tools/scripts) against original game behavior — outside this read-only analysis scope.

## Dependencies

From `census.json` imports:
- `KernelStandard_7c.dll` (primary Jet dependency)
- `MSVCR80.dll` (VC8 CRT/C++ runtime: exceptions, memcpy, init/terminate, EH helpers implied by function map)
- `KERNEL32.dll`

From API signatures (`exports/Lexer_7c.tsv`): heavy use of KernelStandard types in public ABI — e.g. `Jet::String`, `Jet::PString` parameters/returns (`TokenString`, `UserMarkerString`, `ParsedFileNameString`, `ErrorPrintf`/`WarningPrintf`/`v*Printf`, constructors taking `Jet::String`). Therefore a faithful reimplementation would depend on **KernelStandard_7c ABI/layout/behavior** (U14 string classes/foundation are relevant) and matching VC8-era calling conventions/object layout if used as drop-in.

Reverse dependencies (who imports/loads Lexer_7c): **not evidenced** in available repo/census data; project notes claim "nothing references it".

## Proposed units/order if ever needed

**Recommendation:** do **not** schedule implementation now. If future evidence shows it is required, treat as a small, mostly self-contained leaf-ish Jet module after KernelStandard is stable.

If ever needed (hypothetical, to minimize integration risk):
1. **U0 Proxy/exports scaffolding** — export table/vftables/data layout parity, forwarding or stub harness only if a load/use case is identified.
2. **U1 Error types** — `CompilerError`, `FatalCompilerError`, `ErrorList` formatting/ownership semantics; likely coupled to `Jet::String` formatting behavior.
3. **U2 FastDynamicArray specialization** — `FastDynamicArray<CompilerError*,13,...>` behavior must match `ErrorList` expectations (ABI-visible template instantiation).
4. **U3 Lexer core/state** — buffer/cursor/line/marker state, save/restore, primitive reads/skips/classifiers and static helpers (many are straightforward/pure-like candidates but must be verified against original).
5. **U4 Lexer parsing routines** — `LexStringLiteral`, `LexFileName`, `LexFractional`, `LexLineDirective`, number parsing, comment skipping (`SkipCComment/CPPComment`) — higher bug-compatibility risk.
6. **U5 Tokenizer** — token dispatch, match/skip APIs, state save/restore, next-token callback integration.
7. **U6 Unknown internals** — resolve/verify 16 `FUN_*` helpers (notably 813-byte `FUN_10002960`) and any non-exported behavior relied upon internally.

Verification strategy if activated: differential tests against original `Lexer_7c.dll` for exported methods/vftables/state transitions and string/error formatting; only meaningful if a real load/call site is discovered (menu path appears negative per notes).

## Priority / deferral recommendation

**Priority: very low. Recommended: defer (last-or-never), unless new evidence appears.**

Rationale (evidenced):
- Explicitly categorized not-loaded/unreferenced in project docs (`README.md`, `MODLOG.md`, `PORT_PLAN.md`).
- 0/185 exports implemented, no in-progress code, no build integration in repo.
- Small size (18-22 KB class range in docs/census) and isolated API suggest limited project leverage compared to active render/UI/game work.
- Even if implemented, value is uncertain without a proven consumer/load path.

**Trigger to revisit:** any observed runtime load/import of `Lexer_7c.dll` outside menu (driving session, tool, editor/compiler/script path, plugin), or discovery of another module/export path requiring it.

## Cross-agent notes

- **Read-only constraint respected:** no source/build/doc/progress files modified; report written only to `notes/agents/Lexer_7c.md` (and STATUS append intended separately as requested by workflow).
- **Do not confuse with other lexers:** KernelStandard/InputSystem contain separate lexer/scanner code (including flex-derived scanners) mentioned in `MODLOG.md`; they are unrelated to `Lexer_7c.dll` unless independently proven.
- **ABI caution:** public signatures expose `Jet::String`/`Jet::PString` and exported template instantiations; any future drop-in work must align with KernelStandard types/layouts and VC8/MSVCR80 semantics.
- **Evidence gaps:** importer/load-site analysis not performed beyond repo/doc/census scan; runtime behavior not probed. Claims of "dead" remain project-documented rather than exhaustively proven here.
- **Shared-file caution:** `README.md`, `PORT_PLAN.md`, `MODLOG.md`, `tools/progress*` already mention Lexer; this analysis did not edit them. Coordinate before any status-table/doc changes.
- If another agent needs this module, first capture concrete load/use evidence (process/module load trace or import reference) before investing in reimplementation.
