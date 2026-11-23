"""Port progress: how much of the game's native code is ours, weighted by code size.

Code size and function counts come from ghidra_maps/*/[<group>/]<module>.functions.tsv (non-thunk functions),
exports we own from src/jet/<group>/<module>/implemented.txt, export totals from tools/progress_exports.tsv.
A finished module counts whole; a partial one counts the code of the functions behind its exports that are ours
(cached in tools/progress_exports.tsv, recomputed with --exports). Modules with no src/ directory count as 0.

usage: python -I tools/progress.py                 print the README block
       python -I tools/progress.py --write         put it between the progress markers in README.md
       python -I tools/progress.py --exports DIR   refresh tools/progress_exports.tsv from exports/*.tsv (run it
                                                   with --write after each step)
"""
import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOTALS = ROOT / "tools" / "progress_exports.tsv"
README = ROOT / "README.md"
START, END = "<!-- progress:start -->", "<!-- progress:end -->"

# (label, what it is, map dir, modules); the last group was not seen loaded (MODLOG "What actually loads").
# Jet modules sit in a folder per group under src/jet/ and ghidra_maps/jet/ (FOLDERS); EMOJI marks the README block and tables
# (one emoji per line of the block, so the bars stay aligned; no variation-selector emoji like ⚙️, their width varies).
GROUPS = [
    ("Engine core", "Jet: memory, strings, files, .ja archives, config, threads, logs, resources", "jet",
     ["Kernel_7c", "KernelStandard_7c", "KernelResources_7c", "KernelResource_7c"]),
    ("Input", "Jet: keyboard, mouse, joysticks, key bindings", "jet",
     ["InputWin32_7c", "InputSystem_7c", "InputDirectX_7c"]),
    ("Graphics", "Jet: 2D/3D scene, meshes, materials, water; OpenGL and DirectX 9 renderers (picked in the launcher)",
     "jet",
     ["Render2DCore_7c", "RenderSystem_7c", "Render3DCore_7c", "Render3DFoundation_7c", "Render3DExtension_7c",
      "RenderOpenGL_7c", "RenderDirectX9_7c", "WaterSystem_7c"]),
    ("User interface", "Jet: windows, buttons, menus, fonts", "jet",
     ["InterfaceSystem_7c", "InterfaceResources_7c"]),
    ("Debug", "Jet: debug console and overlays", "jet",
     ["DebugSupport_7c", "DebugInterface_7c"]),
    ("Game", "trainz.exe: simulation, trains, routes, Surveyor, TrainzScript, assets; TNI plugin API", "trainz",
     ["trainz.exe", "trainznativeinterface.dll"]),
    ("Not seen loaded", "Jet: Lexer (nothing references it), movie player (not loaded at the menu; re-check in a session)",
     "jet", ["Lexer_7c", "DSMoviePlayer_7c"]),
]
JET = [n for _, _, g, names in GROUPS if g == "jet" for n in names]
FOLDERS = {"Engine core": "core", "Input": "input", "Graphics": "graphics", "User interface": "ui",
           "Debug": "debug", "Not seen loaded": "not_loaded"}
EMOJI = {"Engine core": "🔧", "Input": "🎮", "Graphics": "🎨", "User interface": "🪟", "Debug": "🐞",
         "Game": "🚂", "Not seen loaded": "💤"}
STATUS_ICON = {"done": "✅", "done*": "✅", "partial": "🟡", "not started": "⬜"}
FOLDER = {n: FOLDERS[label] for label, _, g, names in GROUPS if g == "jet" for n in names}
ORIGINAL_REQUIRED = {"RenderOpenGL_7c"}


def map_path(group, fname):
    """ghidra_maps/jet/<group folder>/<file> for Jet modules, ghidra_maps/trainz/<file> for the game."""
    name = fname.split(".dll", 1)[0]
    return ROOT / "ghidra_maps" / group / FOLDER.get(name, "") / f"{fname}.functions.tsv"


def code_size(group, name):
    """(bytes, functions) of non-thunk functions in a Ghidra map."""
    fname = name if name.endswith((".exe", ".dll")) else name + ".dll"
    size = count = 0
    with open(map_path(group, fname), encoding="utf-8") as f:
        next(f)
        for line in f:
            col = line.rstrip("\n").split("\t")
            if col[5] != "true":
                size += int(col[1])
                count += 1
    return size, count


def owned_names(name):
    if name not in FOLDER:  # the game (src/trainz/) is not started
        return []
    folder = ROOT / "src" / "jet" / FOLDER[name] / name
    paths = [folder / "implemented.txt"]
    paths += sorted((folder / "implemented.d").glob("*.txt")) if (folder / "implemented.d").is_dir() else []
    names = []
    for path in paths:
        if not path.exists():
            continue
        names += [line.split("=", 1)[0].strip() for line in path.read_text(encoding="utf-8").splitlines()
                  if line.strip() and not line.startswith("#")]
    return names


def owned(name):
    return len(owned_names(name))


def refresh_totals(exports_dir):
    rows = []
    for name in JET:
        tsv = Path(exports_dir) / f"{name}.tsv"
        lines = [l.split("\t") for l in tsv.read_text(encoding="utf-8").splitlines()[1:] if l.strip()]
        # the functions behind our exports (folded bodies counted once), by the Ghidra map's sizes
        sizes = {}
        with open(map_path("jet", f"{name}.dll"), encoding="utf-8") as f:
            next(f)
            for line in f:
                col = line.split("\t")
                sizes[int(col[0], 16)] = int(col[1])
        rva = {l[2]: 0x10000000 + int(l[1], 16) for l in lines}
        mine = {rva[n] for n in owned_names(name) if n in rva}
        rows.append(f"{name}\t{len(lines)}\t{sum(sizes.get(a, 0) for a in mine)}")
    TOTALS.write_text("module\texports\towned_bytes\n" + "\n".join(rows) + "\n", encoding="utf-8")


def plan_steps():
    text = (ROOT / "PORT_PLAN.md").read_text(encoding="utf-8")
    steps = re.findall(r"^(\d+b?)\. (.*)$", text.split("## Next steps", 1)[1], re.M)
    return sum(s.startswith("~~") for _, s in steps), len(steps)


def bar(frac, width=30):
    full = round(frac * width)
    return "█" * full + "░" * (width - full)


def block():
    totals, owned_bytes = {}, {}
    for l in TOTALS.read_text(encoding="utf-8").splitlines()[1:]:
        col = l.split("\t")
        totals[col[0]] = col[1]
        owned_bytes[col[0]] = int(col[2]) if len(col) > 2 else 0
    rows, stats = [], []  # stats: (label, what, map dir, done bytes, bytes, modules done, modules)
    for label, what, group, names in GROUPS:
        g = [label, what, group, 0, 0, 0, len(names)]
        rows.append(f"| {EMOJI[label]} **{label}** | | | | |")
        for name in names:
            size, funcs = code_size(group, name)
            total = int(totals.get(name, 0))
            mine = owned(name)
            # a module with one or no export (a plugin entry only) is ours when its entry point is
            share = (mine / total if total > 1 else float(mine > 0)) if group == "jet" else 0.0
            # partial modules: the code behind our exports, not their count (many exports are tiny accessors)
            finished = share >= 0.999 and name not in ORIGINAL_REQUIRED
            code_share = share if finished or share == 0 else min(owned_bytes.get(name, 0) / size, share)
            g[3] += size * code_share
            g[4] += size
            g[5] += finished
            status = ("done" if finished and share >= 1 else "done*" if finished else
                      "partial" if share > 0 else "not started")
            exp = f"{mine}/{total}" if group == "jet" else "-"
            rows.append(f"| {name} | {size / 1024:,.0f} KB | {funcs:,} | {exp} | {STATUS_ICON[status]} {status} |")
        stats.append(g)

    def line(label, done, size, mods_done=None, mods=None):
        mods_txt = f"  {mods_done}/{mods} modules" if mods else ""
        return (f"{label:<21}{bar(done / size)} {100 * done / size:5.1f} %  "
                f"{(size - done) / 1024:>6,.0f} KB left of {size / 1024:>5,.0f}{mods_txt}")

    def total(sel):
        return [sum(g[i] for g in sel) for i in (3, 4, 5, 6)]

    jet = [g for g in stats if g[2] == "jet" and g[0] != "Not seen loaded"]
    steps_done, steps = plan_steps()
    today = datetime.date.today().isoformat()
    out = [
        START,
        f"Measured by code size: a finished module counts whole, a partial one by the code behind the exports "
        f"that are already ours (`tools/progress.py`, {today}).",
        "",
        "```",
        line("🌍 Whole game", *total(stats)[:2]),
        "",
        line("🧩 Jet engine", *total(jet)),
        *(line(f"  {EMOJI[g[0]]} {g[0]}", g[3], g[4], g[5], g[6]) for g in jet),
        "",
        *(line(f"{EMOJI[g[0]]} {g[0]}", g[3], g[4], g[5], g[6]) for g in stats if g not in jet),
        "```",
        "",
        "Jet is Auran's general engine, a set of DLLs: core services, input, graphics and UI. The train simulation "
        "itself (physics, routes, sessions, Surveyor, TrainzScript, the asset system) is the game, `trainz.exe`, "
        "the biggest part, not started yet.",
        "",
        "| Group | What it is |",
        "|---|---|",
        *(f"| {EMOJI[g[0]]} {g[0]} | {g[1]} |" for g in stats),
        "",
        f"Plan steps finished: {steps_done} of {steps} (`PORT_PLAN.md`). After the 32-bit replacement come the "
        f"x64 build and the add-on compatibility tests.",
        "",
        "<details><summary>Per module</summary>",
        "",
        "| Module | Code | Functions | Exports ours | Status |",
        "|---|---:|---:|---:|---|",
        *rows,
        "",
        "`done*`: KernelStandard_7c keeps one export, `HardwareException::GetCallStack` (an x86 stack walk), "
        "forwarded to the original until the x64 rewrite.",
        "RenderSystem_7c has no export forwarders and no private-code dependency on the retail DLL; "
        "only optional data/table mirrors remain. RenderOpenGL_7c exports are local, but the internal "
        "display/vtable, vertex-buffer and shader backend is still being ported.",
        "",
        "</details>",
        END,
    ]
    return "\n".join(out)


def main(argv):
    if "--exports" in argv:
        refresh_totals(argv[argv.index("--exports") + 1])
    text = block()
    if "--write" in argv:
        readme = README.read_text(encoding="utf-8")
        if START not in readme or END not in readme:
            sys.exit(f"README.md has no {START} ... {END} markers")
        head, rest = readme.split(START, 1)
        README.write_text(head + text + rest.split(END, 1)[1], encoding="utf-8", newline="\n")
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        print(text)


if __name__ == "__main__":
    main(sys.argv[1:])
