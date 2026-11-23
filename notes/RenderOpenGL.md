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

The source folder started as a **proxy**. Unit 1 is now local: the
`ContextDescriptorOpenGL(WGL)` and `DisplayDescriptorOpenGL` classes (16
exports, `RenderOpenGLTypes.*`). `Initialize` reads the GL strings from
`opengl32!glGetString` and takes the context from interface slot 0x58. The
remaining exports still forward to `RenderOpenGL_7c_orig.dll`.
`build-trainz32\game-smoke` ran with the proxy plus the renamed original and
stayed in the main menu, so the build/link/forwarding path is proven.

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
