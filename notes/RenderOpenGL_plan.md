# RenderOpenGL_7c port plan

Retail module: 274432 bytes, **51 exports**, **944 functions / 139 KB of code**,
345 imports. It consumes 178 `RenderSystem_7c` exports, 60 `KernelStandard_7c`
and 10 `Kernel_7c`; Cg stays external (`cg.dll`, `cgGL.dll`).

Reference material:

- decomp: `D:\trainz12-decomp\decomp\RenderOpenGL_7c.dll.c` (944/944, 0 failed)
- function inventory: `D:\trainz12-decomp\out\RenderOpenGL_7c.dll.functions.tsv`
- vtables: `D:\trainz12-decomp\out\vtnames_RenderOpenGL_7c.dll.tsv`
- exports: `D:\trainz12-decomp\exports\RenderOpenGL_7c.tsv`

The game must stay launchable after every milestone. Until the final cutover
the module builds as a proxy: local classes are listed in `implemented.txt`,
everything else forwards to `RenderOpenGL_7c_orig.dll`.

## Function budget

```
 537  80302  Global helpers (unnamed, assigned to the unit that calls them)
 146  13612  DisplayOpenGLXTerm
   3  10711  DisplayLibraryOpenGL          (vf4 = 9765-byte GL loader)
  30   7440  DisplayTextureOpenGL
  21   6898  TextureCombinerOpenGLNV
  22   3500  DisplayInterfaceWGL
  34   2509  VertexProgramOpenGLNV
  24   1791  VertexBufferManagerOpenGLARB
  34   1413  VertexProgramOpenGLARB
  13   1285  DisplayOpenGL
   6   1275  CGShaderLoaderOpenGL
   9    888  TextureCombinerOpenGLNVfp
   3    875  DisplayLibraryWGL
   4    769  NonRenderContextWGL
  14    568  CGShaderConstantOpenGL
  12    555  VertexBufferManagerOpenGLSW
   8    512  TextureCombinerOpenGLARB
  14    435  TextureCombinerOpenGLATI
  10    293  VertexBufferManagerOpenGLPlugin
   4    188  DisplayInterfaceOpenGL
   4    179  PluginFactoryOf<DisplayOpenGL, Display>
   5    129  CGShaderTechniqueOpenGL
   7    108  VertexProgramOpenGLPlugin
   8     64  TextureCombinerOpenGLPlugin
```

Do not port the 537 Global helpers as a block. Each one is pulled in with its
first caller; mark it in `implemented.txt` only when that unit compiles.

## Milestones

### M0 — module skeleton (done)

- `src/jet/graphics/RenderOpenGL_7c/`, CMake target, proxy `.def`.
- Full Ghidra decomp generated.
- Proxy + renamed original verified in game.

### M1 — descriptors (done)

- `ContextDescriptorOpenGL`, `ContextDescriptorOpenGLWGL`,
  `DisplayDescriptorOpenGL`: 16 exports in `RenderOpenGLTypes.*`.
- `Initialize` uses `opengl32!glGetString` and interface slot 0x58.
- Verified with proxy + `_orig`; menu stable.

### M2 — GL/WGL platform layer (in progress)

The base `DisplayInterfaceOpenGL` is local: constructor, extension-list test
(`HasExtension`/`IsExtensionSupported`) and the retail stubs. The GL table and
the WGL backend are next.

Classes: `DisplayInterfaceOpenGL` (4), `DisplayInterfaceWGL` (22),
`NonRenderContextWGL` (4), `DisplayLibraryWGL` (3),
`DisplayLibraryOpenGL` (3; `vf4` is the 9765-byte loader).

Work:

1. Define the interface layout and a local **GL function table**
   (`OpenGLFunctions`) resolved from `opengl32.dll` and
   `wglGetProcAddress`. Group it: core 1.1, multitexture, FBO/pbuffer,
   DDS/compressed, anisotropic, shader/ARB paths.
2. Port `DisplayInterfaceOpenGL` base and `DisplayInterfaceWGL` context
   creation/destruction, window/DC/RC handling, extension queries.
3. Port `NonRenderContextWGL`.
4. Port `DisplayLibraryWGL`, then the `DisplayLibraryOpenGL` loader in the same
   groups as the table. Keep the two tables in sync.
5. Test: proxy + `_orig`, create a context, open the menu, read back the GL
   strings, no leaks/crashes. Compare `glGetString`/extension lists with the
   original in one process.

Risk: `vf4` names several hundred pointers; a missing one must fail loudly at
load, not as a null call later.

### M3 — texture backend

`DisplayTextureOpenGL` (30 functions, 7440 bytes) plus its static data
(`anisotropySupport`, `canAutoMip`, `bound`, `numObjects`, format tables).

Work:

1. Extract the format/default tables from the retail image with
   `tools/extract_data.py` (same pattern as the RenderSystem tables).
2. Port constructor/copy/destructor, `GetJetFormat`, `GetAPITexture`,
   `GetDisplay`, `GetWinInterface`.
3. Port `Create` (1174 bytes), `Load`, `SubLoad`, `SubloadFromFrame`,
   `GetBitmap`, `CheckFormat`, `Bind`, `BindTexture`, `SetBasicState`,
   `UnbindRenderTexture`, `CopyMipToMip`, `CopySamplerAttributesFrom`,
   `RefreshFromChangedSettings`, `CreateRenderContext`, `CreateInstance`,
   `Destroy`, `GetDataLost`.
4. Verify against the existing texture probes (`u6*`) and in game with the
   proxy: menu, route load, textures visible.

### M4 — display

`DisplayOpenGL` (13 functions) then `DisplayOpenGLXTerm` (146 functions,
13612 bytes).

Work order inside XTerm, by slot role:

1. constructor/destructor/shutdown and plugin-instance glue;
2. mode/format enumeration (`vf2`, `vf3`, `vf8`, `vf9`, `vf14`..);
3. state setters/getters (blend, depth, cull, fog, stencil, viewport,
   scissor, 2D/3D view);
4. `Clear`, render-target/pbuffer, swap, window resize;
5. draw paths (`Draw`, `DrawUser`, declarations, vertex streams);
6. extension/query virtuals.

At the end of M4 the game must render through our display in the proxy run.

### M5 — buffers, programs, combiners

- Vertex buffers: ARB (24), NV (34), SW (12), Plugin (10).
- Vertex programs: ARB (34), NV (34), Plugin (7).
- Texture combiners: NV (21), NVfp (9), ATI (14), ARB (8), Plugin (8).

Each family is a self-contained vtable. Port one family at a time, switch its
whole vtable, test, then move on. These paths are visible in the material and
geometry probes (`u5`, `u7`, `u8`, `u9`).

### M6 — Cg

`CGShaderLoaderOpenGL` (6), `CGShaderTechniqueOpenGL` (5),
`CGShaderConstantOpenGL` (14), plus `CG.DLL`/`CGGL.DLL` imports.

- Generate import libs for `cg.dll`/`cgGL.dll` (or resolve their exports
  dynamically). Cg itself is not reimplemented.
- Port the loader/technique/constant classes; test the shader paths in game.

### M7 — plugin entry and standalone cutover

- `PluginFactoryOf<DisplayOpenGL, Display>` (4), `JetGetPluginsRenderOpenGL`.
- Move every remaining export out of the forward list.
- `python tools/check_exports.py ... --forbid-dll RenderOpenGL_7c_orig.dll`
  must pass; no `GetModuleHandle("RenderOpenGL_7c_orig")`,
  `GetProcAddress` or fixed retail RVAs at runtime.
- Remove `RenderOpenGL_7c_orig.dll` from the lab, launch the game, load a
  route, run TADDaemon, take screenshots.
- Only then delete the `_orig` forwarders from the `.def`.

## Verification per milestone

1. Build all 9+ modules, `check_exports.py` name/ordinal match.
2. `check_thunks.py` on the module: no accidental `_orig` re-exports.
3. `compare_vtables.py` for every switched class vtable.
4. Proxy + `_orig` in `build-trainz32/game-smoke`: menu, route load, TADDaemon,
   clean exit; screenshots compared with the retail control.
5. Texture/geometry/material probes: `u6*`, `u7`, `u8`, `u9` where applicable.
6. At M2/M4 add a temporary GL call trace (`glViewport`, `glClearColor`,
   `glClear`, draw counts) to compare our display with the retail one.

## Known risks

- The GL loader is the largest single function; a missed pointer is a null call
  much later. Resolve all pointers at context creation and log missing ones.
- PBuffer/WGL context paths are Windows-specific; test both windowed and
  fullscreen, and the hidden `NVOpenGLPbuffer` helpers.
- Static format tables must be extracted, not guessed.
- Vtable slots are `__cdecl` in this codebase; do not introduce thiscall
  adapters.
- Keep class vtables atomic: never mix a local vtable with forwarded methods of
  the same class.
- Cg import libs are not in the tree yet; generate them before M6.
- The white menu background is not yet explained. Once M4 runs, trace
  `DisplayOpenGLXTerm` clear/viewport/draw calls; if the white area remains,
  compare against the retail control under the same API trace.

## Next actions

1. Add `OpenGLFunctions` and the interface/base headers.
2. Port `DisplayInterfaceOpenGL`, then `DisplayInterfaceWGL` and
   `NonRenderContextWGL`; keep the proxy build green.
3. Port `DisplayLibraryWGL` and the `DisplayLibraryOpenGL` loader in groups,
   with a missing-pointer log.
4. Move to `DisplayTextureOpenGL` (M3) only after the GL table is complete.
