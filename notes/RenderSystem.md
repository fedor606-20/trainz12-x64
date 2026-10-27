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
U5a done 2026-10-28 (VertexType.cpp, RenderCaches.cpp, include/jet/Geometry.h; 97 exports, 1420/2687; test
tests/diff_rs_u5.cpp). VertexType 0x24 bytes on ReferenceCount: +8 VertexTypeID, +0xc stream count, +0x10 streams
(0x10 each: +0 stride, +4 item capacity, +8 item count, +0xc items: an eh-vector-constructed array of 0x20-byte
items {+0 VertexItemType, +4 PString name, +8 offset (-1: placed by ConfigureStream), +0xc element count, +0x10
DVertexItemDataType, +0x14 semantic, +0x18 shared-with item type (-1; ConfigureStream lowers it by 15 once
placed), +0x1c BoolEnum flag (never set for shared items)}), +0x14 the API data (Display slots 0x3c add / 0x40
remove, through the static `display`), +0x18 API flags, +0x1c user data, +0x20 registered name. Item-type
defaults 0x100b8968 (39 x 0x14: count, data type, semantic, name PString, the type) and element sizes 0x10062760
(9 x 0xc: size, BoolEnum default, the type) are our own copies; the statics `display`, `nextFreeId` and
`registrations` (HashTable<unsigned, RegistrationData*, ...,13>, 97 FDA buckets keyed by the name node;
RegistrationData {type (referenced), name}) stay the original's. CompareVertex compares every item slot up to the
capacity (filled or not), names excluded. AddItem's defaults: count -1, data type 0, semantic 1, empty name.
RenderingCache 0x28 (vfptr, IListNode +4, vbptr +0xc, name +0x10, priority +0x14, host +0x18, rendersGeometry
+0x1c, enabled +0x1d, virtual ReferenceCount +0x20; slots SetHost, Activate, Reset, Begin, End, Flush (End then
Begin), Enter, Leave). RenderingCacheManager 0x10: a ring through the caches' IListNode with the manager as head
(cache = node - 4), sorted by rising priority (inserted before the first not below), count +8, geometry-only flag
+0xc (set by Begin; End runs Activate on all, then End). VertexBufferManager 0x78 (23 slots, 14 answering 0, 6
empty; display +4, dynamic sizes +8/+0xc, BufferStats 0x48 at +0x10, CriticalSection +0x6c). The internal
NullVertexBufferManager (0x1001e5b0..) is made only by the Display and stays with U9. Display::CreateVertexTypeObject
(0x10011f00, 38 KB) and Get/CloneBuiltInVertexType belong to U9.
U5b split 2026-10-28: U5b1 GeometryChunk without the interpolation (GeometryChunk.cpp; 82 exports, 1502/2687; test
diff_rs_u5 part "chunks") done 2026-10-28; U5b2 = InterpolateChunkData (2), InterpolateAllChunkData (8),
GetAllChunkWeights, CopyChunkData (0x10026e70..0x1002a720, ~14 KB of float code; GeometryChunk.cpp, 1514/2687,
diff_rs_u5 part "interp") done 2026-10-28. Slots 61 InterpolateChunkData(src, i0, i1, dst, pos), 62..64 from three
src vertices (Vector3 weights with first/count, all vertices, by the vertex positions' barycentric weights through
the non-virtual 4-index InterpolateChunkData), 65..67 from two (Vector2 weights, all, by the positions along the
segment), 68/69 one vertex scaled by float weights, 70 CopyChunkData(src, from, to, count); each walks the item
type list, needs the item in both chunks (slot 44 not -1) with equal data type and count (floats of 1..4, udwords
of 1), then calls slot 31 (1, 0, -2). MSVC lays out overloaded virtual functions in reverse order of declaration:
the header declares the 69..62 overloads backwards. The compiler unrolled the loops by four and picked a different
operand order per unrolled step (and for the tail); kept per step for the two-NaN case. The three-vertex and
two-vertex position cases read the source vertices once before the loop. Lengths are KernelStandard's LUT square
root inlined on x87 (left unrounded for the divide), udword blends are x87 sums truncated through fistp qword.
GeometryChunk 0x4c on ReferenceCount: +8 properties
(FastDynamicArray<{name node, float}, 3>, chunk 0x24), +0xc Array<VertexProgram::MatrixMap*,128> bones (MatrixMap
{matrix, index} 8 bytes; growth to count+1 rounded up to 128), +0x18 flags (bits 0..4 primitive: 0 points, 1 line
list, 2 line strip, 4 triangle list, 8 strip, 0x10 fan; 0x20 lit; 0x100 index dynamic; 0x200 index dirty; 0x1000
unshared), +0x1c primitives, +0x20 indices, +0x24 vertices (the default constructor leaves it), +0x28 max vertices,
+0x2c/+0x30/+0x34 AutoReferenceArray blocks {refs, count, 2 unused, items} of indices (u16), face neighbours (0xc)
and face normals (Vector3), +0x38 face normal count, +0x3c opacity (1.0), +0x40 surface area, +0x44 user data,
+0x48 referenced user data. 73 slots; every "answers 0" slot of the TS12 vftables is a pure virtual: the module's
_purecall is `xor eax, eax; ret` (ours defines the same), which is why base slot 4 answers 0 while the exported
GeometryChunk::InitializeBuffers answers true. Slot names from the stream chunks' vftables: 2/3 CreateInstance,
4 InitializeBuffers, 5 LoadBuffers, 7 DuplicateVertexArray, 9 ResizeVertexArray, 12/13 GetVertexArray, 14
RenderGeometry, 15 RenderPrimitives, 16/17 SetupMultipassRender, 19 BindVertexProgram, 20/21 RenderGeometryPass, 22
CleanupMultipassRender, 23/24 stream / buffer counts (U5b2 confirmed 41 GetVertexType(stream), 44 GetItemOffset,
46..53 GetFloatArray for 2, 3, 4, 1 elements const/non-const, 31 SetVertexDataDirty) (AppendChunk loops vertex types x streams), 25..32 vertex data
dynamic/shared/dirty, 35..38 static vertex / index buffers, 39/40 AssignVertexType / SetVertexType, 41
GetVertexType(i), 42 GetVertexTypeID, 43 GetVertexSize, 44 GetItemOffset, 45 GetItemStream, 46..53 GetFloatArray,
54/55 GetUDwordArray, 59/60 Set/GetVertexProgram. TS12 quirks kept: without an index array the index lookups
read address 0x10 (the test avoids it); RenderOutlines advances 2 vertices per triangle while writing 6 (only the
last triangle is whole, the draw reads unwritten stack after it); a fan triangle n is (0, n, n+1); x87 copies quiet
signalling NaNs (InitInstance, outlines, GetChunkProperty, property removal); SSE operand order decides which NaN
comes out (kept with inline asm, the compiler commutes even intrinsics). The registry (HashTable of {name,
creator}) stays the original's; ReleaseRegistrationData frees the entries but leaves null slots in the buckets (a
later lookup faults in both).
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
