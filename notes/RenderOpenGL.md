# RenderOpenGL_7c

Port plan and milestones: `notes/RenderOpenGL_plan.md`.

OpenGL backend of the Jet renderer. Retail module: 274432 bytes, **51 exports**,
**944 functions / 139 KB of code**, 345 imports. It consumes 178
`RenderSystem_7c` exports, 60 `KernelStandard_7c` and 10 `Kernel_7c`; Cg comes
from `cg.dll` / `cgGL.dll`.

Reference decomp (944/944 functions, 0 failures):
`D:\trainz12-decomp\decomp\RenderOpenGL_7c.dll.c`.
Function/vtable inventory:
`D:\trainz12-decomp\out\RenderOpenGL_7c.dll.functions.tsv`,
`vtnames_RenderOpenGL_7c.dll.tsv`.

## Classes

- `DisplayOpenGL` (13 functions) and `DisplayOpenGLXTerm` (146): the display
  and its windowed backend. `DisplayTextureOpenGL` is 30 functions; the base
  `DisplayTexture` is 0x38 bytes and the OpenGL fields start at +0x38
  (`GLuint` at +0x38, DFormatId at +0x5c, flags at +0x60/+0x61/+0x68).
  `GetDisplay` reads base +0x0c; `GetWinInterface` reads `display + 0xf00`.
- `DisplayInterfaceOpenGL` (4), `DisplayInterfaceWGL` (22),
  `NonRenderContextWGL` (4), `ContextDescriptorOpenGL(WGL)` and
  `DisplayDescriptorOpenGL` (size 0x20, context pointer at +0x1c).
- Vertex buffers: `VertexBufferManagerOpenGLARB/NV/SW/Plugin` (24/34/12/10).
- Shaders/combiner: `VertexProgramOpenGLARB/NV/Plugin` (34/34/7),
  `TextureCombinerOpenGLNV/NVfp/ATI/ARB/Plugin` (21/9/14/8/8),
  `CGShaderLoaderOpenGL`, `CGShaderTechniqueOpenGL`,
  `CGShaderConstantOpenGL`.
- `JetGetPluginsRenderOpenGL` registers the display plugin types.

## Current state

The module still builds as a standalone 51/51 export drop-in and the local display opens in the
game. This pass replaced the hand-built slot table with the module's own 152-slot `Display` table
from `RenderSystem_7c.dll`, then filled the renderer slots by byte offset:

- `0x00` init, `0x04` the local render-kernel's pre-open probe, `0x18` name, `0x1c` extension
  support, `0x28` max anisotropy, `0x30` shutdown, `0x34` open;
- `0x50`/`0x54`/`0x58`/`0x5c`/`0x68`/`0x6c`/`0x70` matrix stack and projection;
- `0x80` texel offset, `0x98` texture factory, `0xa0` active texture unit;
- `0xd0` clear, `0xd4`/`0xd8` colour/depth readback, `0xe4`/`0xec`/`0xf0`/`0xf4` draws;
- `0x104`/`0x108` static vertex/index buffers, `0x150` descriptor;
- `0x180`..`0x240` the whole render-state block: viewport/scissor, clear values and masks,
  depth/stencil/cull/blend/alpha, fog, materials, lights, polygon offset, texture modes and
  texgen, plus `0x244`..`0x25c` pre-open, video-format reset/select and finish;
- `0x260`..`0x280` the XTerm extension slots.

The display object uses the retail layout: PluginInstance at `+0xf98` (base) / `+0xfa0`
(XTerm), factory at `+4`, ReferenceCount at `+0xfa0` / `+0xfa4`, descriptor at `+0xf78`.
Draws use client vertex arrays built from `VertexType` items (position/normal/colour/fog and
`Texture0..7`), with single-stream and multi-stream entry lists; static buffers keep CPU data and
the buffer manager uploads/copies through the RenderSystem geometry slots.

Two RenderSystem cutovers were needed and are in the tree:

- the exported `Display::CreateTexture` now dispatches to vtable slot `0x98` instead of the null
  factory (`RsDisplayCreateTexture` in `DisplayState.cpp`);
- `RenderKernel::LoadRenderSystem` probes vtable slot `0x04`; the display answers true, so the
  kernel proceeds to `Init`/`Open`.

Verification in this pass:

- Win32 Release builds, `51/51` exports, no `_orig` import or forwarder.
- The game reaches the JET window, `Init -> PreOpen -> Finish -> Open`, frames are presented.
- Current local backend: the window stays white. Draws execute (static/dynamic batches with valid
  vertex types, positions, colours and indices), but **no `Display::CreateTexture` call is made**,
  so every UI quad samples an unbound texture and the menu is white. Railyard was not verified
  with this module.
- The WGL context is created in `PreOpen` on the main JET window (after the windowed host it
  landed on a second, invisible JET window); `EndFrame` always swaps and makes the context current.
- `Texture::Bind` now goes through `GetDisplayTexture()`; this is the correct lazy-load path, but
  the UI still does not reach it. A committed run with the retail `RenderOpenGL_7c.dll` next to the
  rebuilt RenderSystem showed the full menu and Railyard 3D scene; those screenshots belong to the
  retail renderer, not to this module.
- Remaining: find which Render2D/TextureManager path bypasses `Display::CreateTexture`, then Cg,
  vertex programs, texture combiners and a route/driving session. The lab install was restored to
  the retail `RenderOpenGL_7c.dll`.


## Port order

1. `DisplayDescriptorOpenGL`, `ContextDescriptorOpenGL(WGL)` — done.
2. `DisplayInterfaceOpenGL/WGL` + `NonRenderContextWGL` (WGL context and GL
   function table). This must come before the texture class: the retail
   `DisplayTextureOpenGL::Create` calls module-level GL pointers
   (`DAT_1003c100` etc.), which `DisplayLibraryOpenGL` fills in. Our copy needs
   its own table.
3. `DisplayTextureOpenGL` (GL texture object, format table, static bound
   list).
4. `DisplayOpenGL` then `DisplayOpenGLXTerm` (the 146-slot windowed backend).
5. Vertex buffers, then vertex programs/combiners/Cg.

The retail `DisplayTextureOpenGL::Create` is 1174 bytes and needs the GL
function table from the interface, so unit 3 cannot be tested in isolation
before unit 2 exists; keep the proxy until then.

## In-game window facts

The live `JET` window is 1029x663 (client 1024x640) at the default launch,
with hidden `NVOpenGLPbuffer` helper windows. `-Width=1280 -Height=800` did
not change the client size. The white menu area is not `ClearLayer` or
`Canvas` (neither is called); `LayerManager::Render` runs with 2-4 layers,
including retail `IView3D`. The next useful trace is the GL call stream from
`IView3D`/`DisplayOpenGLXTerm`.
