# RenderSystem_7c.dll: reconstruction plan

Status: 2026-10-26. U0 (proxy), U1 (value types, vertex records; 939 exports), U4 (modifiers; 54 exports), U2a
(shader-language data; 99 exports) and U2b1 (ExtendedGfxLanguage, TextureCombiner records; 44 exports) are ours and
verified (`tests/diff_rs_u1.cpp`, `diff_rs_u4.cpp`, `diff_rs_u2.cpp`, `diff_rs_u2b.cpp`, MODLOG): 1136/2687. Next:
U2b2 (the scanners/parsers).

U2 is split: U2a the data classes (GfxSymbols.cpp); U2b1 ExtendedGfxLanguage and the TextureCombiner
Instruction/RegisterItem/ModifierItem records (ExtendedGfxLanguage.cpp); U2b2 four flex 2.5 C++ scanners
(gfxpp = GfxLanguagePreParser 0x1002cf30, gfxqk = GfxLanguageQuickParser 0x1002efd0, vp = VertexProgramParser
0x10051bd0, tp = TextureProgramParser 0x10044d30; their `yylex` holds the rule actions) and the Lemon parsers that
drive them (pre-parser driver 0x1002cad0 with reduce 0x1002c340, "Syntax error at %s."; texture 0x10044830; vertex
0x100517d0). Same shape as KernelStandard's ConfigParser: tables read from the installed original at build time.
U2b2a (2026-10-27): quick parser and pre-parser done (GfxParse.*, GfxQuickParser.cpp, GfxPreParser.cpp; MODLOG).
U2b2b: tp = TextureProgramParser (yylex 0x10044d30, driver 0x10044830, reduce 0x10043760, run 0x100462d0 called
by TextureCombiner::Load with ecx = parser object: tpFlexLexer at +0, +0x7c 0, +0x80 line 1, +0x84 the combiner's
vftable slot 17 value; the reduce calls the combiner's virtuals and builds TexApiSection/TexApiCodeLine/
TexRegisterDefinition records), vp = VertexProgramParser (yylex 0x10051bd0, driver 0x100518c0, run 0x10054f00).
Their statics have to be placed against GfxLexStatics (find their addresses first). Syntax of any scanner: walk its
DFA (scratchpad dfa_examples.py did it for gfxpp: shortest text per rule and start condition).
U2b2 findings so far: the parsers are an old Lemon with per-state hash tables in .data (pre-parser: states
{action list VA, mask, default} at 0x100b66c0, entries {byte lookahead, byte action, pad, next VA}; rule
{lhs, rhs count} bytes at 0x100b6c28; 0x5f shifts, then reduces, 0x95 error, 0x96 accept): the tables hold
absolute pointers, so the build-time extractor must turn them into indices. Entry points: pre-parser object ctor
0x1002b9d0 (gfxpp lexer 0x1002e560, output AnsiString +0x88, a #if condition stack 0x1002bee0) and its run
0x1002bbd0 (both called from VertexProgram::Load 0x10049540); quick parser ctor 0x1002f830 + run 0x1002f930
(RegisterBuiltInPrograms); texture parser run 0x100462d0 (TextureCombiner::Load); vertex parser run 0x10054f00
(VertexProgram::Load). Errors go through 0x1002c0c0 ("%s:%s(%u): %s\n").
U3 is split (2026-10-27): U3a1 VertexProgram + TextureCombiner (VertexProgram.cpp, TextureCombiner.cpp; done,
`tests/diff_rs_u3.cpp`), U3a2 the managers (VertexProgramManager 0x10053b70..0x10054880, TextureCombinerManager
0x10040850..0x10041200: a ReferenceCount, the Display +8 and a HashTable of 97 FastDynamicArray buckets +0xc keyed by
the program's PString node, +0x18 registered name / +0x14 program name), the resources (VertexProgramResource
0x88 bytes, TextureCombinerResource 0x70, on KernelResource's Resource 0x5c; ResourceType<> 0x1003b920..), CGShader*
(0x10009180..) and GLSLShaderBase (0x1002faa0..), then U3b VertexProgramBasic. Only the original's
Display::InitializeState constructs the two managers, so their statics (theManager, builtInProgramList,
registeredProgramList) stay the original's until U9; the original's scanner statics range 0x100b7dc8..0x100b8127
also holds VertexProgram/TextureCombiner's support flags and register limits and TextureCombinerManager::theManager.
U3a2 done 2026-10-27: managers and resources (ProgramManagers.cpp, ProgramResources.cpp; 35 exports, 1221/2687),
tests/diff_rs_u3.cpp part "managers" 0 mismatches, 22/22 mutants caught (MODLOG). U3b1 VertexProgramBasic
(0x1004a820..0x1004e5c0, VertexProgramBasic.cpp, 1275/2687, part "basic") done 2026-10-27. U3b2 CGShader*
(0x10009180..0x1000a940 with the VC8 map code) and GLSLShaderBase (0x1002faa0..0x10031350), ShaderLoaders.cpp,
1323/2687, part "shaders", done 2026-10-27: U3 complete. Next U5.
U5 split (2026-10-27, by code size): U5a VertexType (+ registry; 29 fns, 3.3 KB), VertexBufferManager, StaticBuffer,
RenderingCache(+Manager), the chunk hints (~5.5 KB in all); U5b GeometryChunk (73 fns, 23.7 KB, 0x10021a70..
0x1002aa80; may split again); U5c GeometryChunkSingle/MultiStream, Generic*, GeometryChunkTemplate (~11.5 KB).
U5a notes (read 2026-10-27, nothing written yet): VertexType 0x24 bytes on ReferenceCount: +8 VertexTypeID, +0xc
stream count, +0x10 streams (0x10 each: +0 stride, +4 item capacity, +8 item count, +0xc items: an
eh-vector-constructed array of 0x20-byte items {+0 VertexItemType, +4 PString name (copied by FUN_10056ef0),
+8 offset (-1: placed by ConfigureStream), +0xc element count, +0x10 DVertexItemDataType, +0x14 semantic, +0x18
shared-with item type (-1), +0x1c bool}), +0x14 the API data (Display slots 0x3c add / 0x40 remove, through the
static `display`), +0x18/+0x1c, +0x20 registered name (PString). Functions 0x100562b0..0x10056ef0 plus
GetVertexSize 0x100106e0, GetItem / GetNamedItemData / GetVertexItemTypes / GetCompatibleVertexItemTypes
0x10010700..0x10010940, Get/SetApiFlags 0x1000ae00 / 0x10021a60. Tables: per item type 0x14 bytes at
0x100b8968 (default data type, semantic, a PString* name?, the default name PString at +0xc = 0x100b8974),
element sizes 0x10062760 (0xc per data type; +4 a bool), the registry (FUN_100571f0 find by name node,
FUN_10057110 insert, FUN_10056220 entry, FUN_10057180 clear, FUN_10054c10 for-each) in the original's statics.
CompareVertex compares streams by FUN_10056310 (stride, counts, items field by field). The test's
ResetLexStatics keeps 0xb80a0..0xb8113 (VertexProgramBasic's statics, set by the API module).
Inputs: `ghidra_maps/jet/RenderSystem_7c.dll.functions.tsv`, the export list (`tools/dump_exports.py`), the import tables
of every module in `bin\` (pefile), the Jet SDK Beta2 headers (`Display.h`, `Material.h`, `Layer.h`, `Modifier.h`,
`Texture.h`, `RenderKernel.h`...: names and intent only, the TS12 layouts differ) and the private decompile
`decomp\RenderSystem_7c.dll.c`.

## 0. Summary

* RenderSystem is the API-independent half of the renderer: the abstract `Display` (the OpenGL/DX9 modules derive
  from it), vertex formats, geometry chunks, textures and their `.texture`/`.texture.txt` resources, materials,
  vertex/texture programs and their small shader languages, modifiers, layers. **2687 exports** (76 vftables, 9
  vbtables, ~240 static data), 3628 Ghidra functions, 351 KB of code; 1792 functions (96 KB) are unnamed `Global`
  helpers still to attribute.
* It imports only modules that are already ours: KernelStandard 168, Kernel 24, KernelResource 28, KernelResources 1.
* **Every other render module stands on it.** Imports of RenderSystem exports: trainz.exe 457, Render3DCore 320,
  TrainzUtil 222, ContentManager 198, Render3DFoundation 196, RenderOpenGL 178, CCP 177, WaterSystem 149,
  RenderDirectX9 143, Render3DExtension 111, Launcher 80, TADDaemon 51, DebugSupport 32, Render2DCore 28,
  TrainzDiag 24, InterfaceSystem 24. **855 of the 2687 exports are imported by anyone**; the rest is used only inside.
* Unlike KernelStandard, most of the module's state is **exported static data** (registries, managers, defaults,
  per-vertex-format offset tables, `results` tables). Units can keep using the original's statics through the
  import library of `RenderSystem_7c_orig.dll` until the foundation unit moves them, as KS did with its private
  statics by address.
* Dependency order inside the module is the usual Jet one: `Display` and `RenderKernel` are the foundation (every
  class takes a `Display*`), so they move last (top-down, as in KernelStandard).

## 1. Units

Grouping by exported class; code size by Ghidra namespace (the unnamed `Global` helpers are not attributed yet).

| Unit | Classes | exports | imported by anyone | code |
|---|---|---:|---:|---:|
| U0 | forwarding proxy (no code) | 0 | - | - |
| U1 value types and vertex formats (done) | PackedVertex* (60 formats), GeoLOD*/TerrainBlend*/StreamedVertex* tables, Element, TextureArgs, RenderTextureConstruct, DisplayFormat, DisplayDescriptor, DisplayLight, DisplayMaterial, Projector, TextureStage | 980 | 131 | 14 KB |
| U4 modifiers (done; the lock stays the original's) | Modifier, ModifierList (+ global lock), ModifierHost, ModifierUpdateTask | 55 | 33 | 6 KB |
| U2 shader language front-end (U2a data done) | GfxLanguagePreParser/QuickParser, ExtendedGfxLanguage, Vertex/TextureProgramParser, symbol tables, Instruction/RegisterItem, PreProcessor* | 102 | 21 | 6 KB (+ ~10 KB parsers in `Global`) |
| U3 programs and shaders (done) | VertexProgram, VertexProgramBasic (text program generator), VertexProgramManager/Resource, TextureCombiner(+Manager/Resource), CGShader*, GLSLShaderBase | 215 | 78 | 33 KB |
| U5 geometry | VertexType (+ its registry, moved from U1), GeometryChunk, Single/MultiStream, Generic*, GeometryChunkTemplate, RenderingCache(+Manager), VertexBufferManager, StaticBuffer, chunk hints | 268 | 193 | 35 KB |
| U6 textures | Texture, TextureRes, TextureResource, AsciiTextureResource (`.texture.txt`), BitmapTextureResource(+Type), BinaryTextureResourceExport, SharedTextureCollection, TextureManager (streaming), DisplayTexture, DisplayTextureNull | 380 | 113 | 47 KB |
| U7 materials | Material, NullMaterial, MaterialConfig/Spec, MaterialManager, MaterialResource, MaterialFileSupport, MaterialPlugin | 171 | 97 | 16 KB |
| U8 layers and scene | Layer, LayerGroup, LayerManager, ClearLayer, Overlay, SceneGraph, SystemResource*, GenericReferenceArray | 144 | 35 | 9 KB |
| U9 foundation | Display (57 KB), DisplaySelector, DisplaySaveBuffer, NonRenderContext, SharedContext, ApiDataBase, RenderKernel, ResourceType, free functions, `JetGetPluginsRenderSystem` | 257 | 139 | 64 KB |
| (small) | VideoMode/VideoFormat, chunk hints, ModifierItem, Tex2/3Pair... folded into the unit of their owner | 115 | 15 | 1 KB |

Order: U0, U1, U4, U2, U3, U5, U6, U7, U8, U9. Leaves and pure data first; textures and materials (which read
add-on files) in the middle with file-level diff tests; Display last.

## 2. Testing

* Differential tests in one process, as for Render2DCore (`tests/diff_r2d.cpp`): our module and
  `RenderSystem_7c_orig.dll` driven with the same inputs against a stand-in `Display` whose vtable logs every call.
* Parsers (U2, U3, U6 `.texture.txt`, U7 material configs): fed with real files from the user's content and Data.ja,
  output structures compared field by field.
* In game: menu check and Launcher faulty check after each unit; from U5 on also a fixed driving session compared by
  screenshot (`dupshot.exe`).

## 3. Open questions

* Attribute the 96 KB of unnamed `Global` functions (single-caller propagation, as for KS).
* Which statics are touched by address from other modules (data imports): `Layer::layerManager`, `TextureManager`
  streaming limits, `VertexType::display`. They decide what must stay forwarded until U9.
