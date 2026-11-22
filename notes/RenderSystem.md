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
GetAllChunkWeights, CopyChunkData (0x10026e70..0x1002a720, ~14 KB of float code); until then our vftable slots
61..70 call the original's exports through GetProcAddress. GeometryChunk 0x4c on ReferenceCount: +8 properties
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
CleanupMultipassRender, 23/24 stream / buffer counts (AppendChunk loops vertex types x streams), 25..32 vertex data
dynamic/shared/dirty, 35..38 static vertex / index buffers, 39/40 AssignVertexType / SetVertexType, 41
GetVertexType(i), 42 GetVertexTypeID, 43 GetVertexSize, 44 GetItemOffset, 45 GetItemStream, 46..53 GetFloatArray,
54/55 GetUDwordArray, 59/60 Set/GetVertexProgram. TS12 quirks kept: without an index array the index lookups
read address 0x10 (the test avoids it); RenderOutlines advances 2 vertices per triangle while writing 6 (only the
last triangle is whole, the draw reads unwritten stack after it); a fan triangle n is (0, n, n+1); x87 copies quiet
signalling NaNs (InitInstance, outlines, GetChunkProperty, property removal); SSE operand order decides which NaN
comes out (kept with inline asm, the compiler commutes even intrinsics). The registry (HashTable of {name,
creator}) stays the original's; ReleaseRegistrationData frees the entries but leaves null slots in the buckets (a
later lookup faults in both).
U5c done 2026-10-28 (GeometryStreams.cpp; 144 exports, 1658/2687; diff_rs_u5 part "streams"): U5 complete.
GeometryChunkSingleStream 0x5c (+0x4c vertex StaticBuffer, +0x50 index StaticBuffer, +0x54 VertexType, +0x58
VertexProgram behind its vbtable; vertex state in the chunk flags 0x10000 dynamic / 0x20000 dirty / 0x40000
unshared), GeometryChunkMultiStream 0x60 (+0x4c {buffer, bits 1 dynamic 2 dirty 4 unshared} per stream, +0x5c
stream count), GenericGeometryChunkSingleStream 0x64 (+0x5c GenericReferenceArray of vertices, +0x60
ItemOffsetTable 0xa8 {RC, stride, 39 offsets}), GenericGeometryChunkMultiStream 0x68 (+0x60 block {refs, count, 2
unused, GenericReferenceArray per stream}, +0x64 ItemOffsetTable 0x148 {RC, strides block, total size, 39 {stream,
offset}}). GenericReferenceArray (moved here from U8) is one pointer to {refs, count, item size, capacity, items}.
Base slot names now in include/jet/Geometry.h (2/3 CreateInstance, 5 LoadBuffers, 7 DuplicateVertexArray, 12/13
GetVertexArray, 14 RenderGeometry, 15 RenderPrimitives, 16/17 SetupMultipassRender, 18/19 BindVertexProgram,
20/21 RenderGeometryPass, 22 CleanupMultipassRender, 23 GetNumPasses, 24 GetNumStreams, 25..28 and 31/32 the
vertex state, 35..38 static buffers, 39 AssignVertexType, 40/41 Set/GetVertexType, 42 GetVertexTypeID, 43
GetVertexSize, 44 GetItemOffset, 45 GetItemStream, 46..55 Vector2/3/4/float/udword views (const first), 59/60
Set/GetVertexProgram). Display slots used: 0xec static draw, 0xf0 dynamic draw, 0xf4 multi stream draw, 0xf8
pass, 0xfc pass setup, 0x100 pass cleanup, 0x104/0x108 make vertex/index buffer; +0x20c the vertex buffer manager
(0x20 upload vertices, 0x2c upload indices, 0x40 catch up unless the byte +0x213 is set).
GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> (0x10037bb0..0x10038340, no exports) is made only
by NullMaterial::CreateGeometryChunk (0x10037a30): it goes to U7.
U6 inventory (2026-10-28): 382 exports, ~50 KB of named code in 11 classes, none ours yet. By class (exports /
functions / bytes): TextureResource 89/84/8655 (0x10001030.., 0x10046ff0..0x10048ee8; on KernelResource's
Resource; the binary .texture reader, 0x100470d0.. holds four loaders of 2.9 KB), BitmapTextureResource 0/20/1761
and BitmapTextureResourceType 0/2/673 (internal: a texture resource from a bitmap, 0x10008360..0x100090a8,
0x1003b7c0), BinaryTextureResourceExport 42/35/1532 (the .texture writer, 0x10007cc0..0x10008360),
AsciiTextureResource 53/50/12839 (.texture.txt; TextureResource + ResourceObserver; 0x10001610.., 0x10004ed0..
0x10007b58, the parser is 7.6 KB in 20 functions), Texture 37/27/2077 (0x10004aa0.., 0x1003f4f0..), TextureRes
34/31/2964 (Texture + ResourceObserver: a texture made from a TextureResource; 0x1003fd50.., 0x10046640..),
DisplayTexture 34/20/1385 and DisplayTextureNull 32/20/3631 (the API texture base and the null one, 0x10004730..,
0x1001dfd0.., 0x1001f420..0x10020418), TextureManager 45/37/6194 (streaming, 0x100413c0..0x10042cf6; made by
RenderKernel::Init), SharedTextureCollection 16/13/8596 (the generated built-in textures, 0x1003bec0..0x1003e05b;
GenerateNormalCubeMapTexture alone is 3.4 KB of float code). Unnamed helpers sit between the runs (0x10007a40..,
0x10008950.., 0x1003e060.., 0x1003ffe0.., 0x10042d30.., 0x100462d0..): attribute them per step.
Other modules import 100 of these names (trainz.exe 82, TrainzUtil 44, Render3DFoundation 38, ContentManager 24,
WaterSystem 21, the two API modules 8 / 7): Texture's Activate/Bind/..., AsciiTextureResource's size calls, the
TextureResource defaults MAG/MIN/MIP_FILTER_DEFAULT, WRAP_S/T_DEFAULT, ANISOTROPY_DEFAULT(_TOKEN_VALUE) (data
imports), DisplayTexture::CheckInitialized and the stats.
Statics: the TextureResource defaults are written by their static setters and by TextureManager::Init (from the
options), AsciiTextureResource::noPath by RenderKernel::Init (U9), DisplayTextureNull::bound / numObjects by
DisplayTextureNull's constructor and destructor, Texture::uniqueTexId by Texture's constructors,
SharedTextureCollection::textureArray / textureNameArray by a static initializer (0x100617f0). As in U3/U5 they
all stay the original's (dllimport) until U9.
Real inputs: .texture.txt in UserData/editing (add-ons being edited), binary .texture in UserData/local/hash-*, and
Data.ja / Base.ja.
U6 split, in this order (resource readers first: they are the add-on contract and test offline on real files):
- U6a: TextureResource (+TextureInfo, the default setters), BitmapTextureResource(+Type),
  BinaryTextureResourceExport: ~13.5 KB, 131 exports. Test: real .texture files read on both sides, fields and
  pixel data compared; the export written by both and compared byte for byte.
- U6b: AsciiTextureResource: ~12.8 KB, 53 exports. Test: every .texture.txt of the install (and mutated copies)
  parsed on both sides, the resulting TextureResource compared.
- U6c: Texture, TextureRes, DisplayTexture, DisplayTextureNull: ~11 KB, 137 exports. Test: stand-in Display
  logging the API calls, TextureRes made from the U6a/U6b resources.
- U6d: TextureManager, SharedTextureCollection: ~15 KB, 61 exports. Test: streaming decisions against stand-in
  textures and memory figures; the generated textures compared bit for bit (inline asm where SSE operand order
  decides NaNs, as in U5b2).
U6a analysis (2026-10-28). BitmapTextureResource(+Type) moves to U9: no exports, made only through the resource
types registered by FUN_1003ad10/1003afe0/1003b0d0 (RenderKernel side). U6a = TextureResource (+TextureInfo) and
BinaryTextureResourceExport (131 exports; the 13 TextureResource default statics stay the original's).
TextureResource 0x64 on Resource 0x5c: +0x5c TextureInfo* (null until loaded), +0x60 bool "data unloaded" (set by
UnloadDataOnly; Lock reloads through slot 10 when set). Vftable 45 slots: 0..17 Resource's (0 Lock is
TextureResource's: Resource::Lock then reload; 7 GetMustResetWhenFinished = 0x100015d0; 8 dtor; 10 Load(char*);
14 Unload; 15 CalculateSize = 100; 16 CalculateDataSize = sum of 4 + block sizes), 18 IsAvailable (status == 6),
19 GetNormalMapHint, 20 GetAlphaHint, 21 GetAverageColor, 22 GetDimension, 23 GetDepth, 24 GetSize, 25
GetAnisotropyFilterLevel, 26 GetFileCompression(char[8]), 27 GetFileCompressionRatio (5), 28 Load(DisplayTexture*),
29 CreateInstance(PString, TextureManager*) (a 0x60 TextureRes), 30 Save(char*, Display*), 31 GetFormatId (slot
32), 32 GetImageFormatId (+0x10, else DISPLAYFORMATID_DEFAULT), 33 GetDisplayFormatId (picks a compressed default
10/11/12/0x19..0x1c by the alpha hint), 34 GetHint, 35/36 WrapS/T, 37..39 Min/Mag/MipFilter, 40 GetNumMipLevels,
41 GetAutoMipHint, 42 GetMipLock (0x100015d0), 43 GetMip(mip, face, Size) (a Bitmap over the stored block), 44
GetMipUnlock (0x100015d0). BinaryTextureResourceExport overrides 8, 19..27, 32..41, 43.
TextureInfo 0x68: +0 version (0x108), +4 dimension, +8 mip levels, +0xc depth (faces), +0x10 image format id,
+0x14/+0x18 size, +0x1c hint, +0x20/+0x24 wrap S/T, +0x28 unused, +0x2c/+0x30/+0x34 min/mag/mip filter, +0x38
alpha hint, +0x3c anisotropy, +0x40 auto mip, +0x44 char[8] file compression, +0x4c normal map hint, +0x50 average
Color, +0x60 mip levels on disk, +0x64 the blocks (levels x depth pointers to {size, bytes}). 0x10047010 frees the
blocks, 0x10047070 finds the first and last stored level (from the smallest).
The .texture file is JIRF: header type 'MMTF' (0x46544d4d), an 'INFO' block (0x4f464e49, 0x60 bytes, versions
0x102..0x108 upgraded to 0x108 on read: older ones lack anisotropy / auto mip / compression / normal hint / colour)
and one 'MMAP' block (0x50414d4d) per level from the smallest, each the faces' pixels (or, with a compression name,
{uncompressed size, compressed bytes} through Compress::CreateInstance and SharedScratchPad). The static Load
(0x10047380) reads it, skipping the levels outside [first, last] requested (TextureManager streaming); Load(char*)
reorders the levels largest first. Save writes INFO then the levels smallest first (SaveMipMap recurses); a
compressed display format (10/11/12/0x19..0x1c) different from the image format goes through Display slot 0x148.
U6b done (2026-11-08): AsciiTextureResource (AsciiTextureResource.cpp, 52 exports; noPath stays the original's,
written by SetUsePath on both sides). 0xd8 = TextureResource + ResourceObserver (+0x64; TS12's observer has a
virtual destructor at slot 1, ours now too, and its slot 0 is `OnLoad`) + three flat {count, capacity, items}
arrays growing by 6 (Jet::Array<T*,6>): +0x68 primary BitmapResources (one per face), +0x74 alpha ones (none or
one per face), +0x80 the merged images while GetMipLock holds them; bytes +0x8c wrap S/T, +0x8e min/mag/mip filter,
+0x91 alpha hint (-1 until worked out), +0x92 hint, +0x93 compression (none dxt1 dxt3 dxt5 atc atce atci, 7 unset,
etc1 9), +0x94 auto mip, +0x98 use mips, +0x9c dimension (two cube volume one), +0xa0 depth, +0xa4 anisotropy,
+0xa8 char[8] file compression, +0xb0 normal map hint, +0xb4 modify map, +0xb8 average Color, +0xc8 "locked" with
+0xcc size and +0xd4 mips cached under GetMipLock. New slots 45 GetModifyMap, 46 UseMipMaps, 47 LoadConfigData,
48 GetImage. The key PStrings are statics 0x100b8134..0x100b8170 (initializer 0x1005dbd0); the enum tables
0x1006eab0..0x1006eb98. LoadTxt reads the obsolete "key=value" text first (ParseObsoleteTxtFormat: lines into
config("Texture"); a line starting with "version" means the ConfigData format, parsed by ConfigParser, items end
with ';'). Every .texture.txt of the install (19 091, UserData/local and original) is the obsolete kind. The bitmaps
are `dynamic_cast<BitmapResource*>(db->Get(ConvertResourceName(name), 0))` (relative to the load name; noPath
drops a leading slash first). Quirks kept: IsAllLoaded reads the primary's notified flag for the alpha one and
dereferences primary[0] of an empty list (both modules fault: tests avoid it); Save's failed lock rolls back by
the wrong index and goes on; GetImage reads the alpha list at `face` while face <= its count and its merge loops
test the format for the first row; GetMip ignores the level; UpdateAverageColor sums in x87 doubles (ours in
inline x87). IsAllLoaded's pending counter 0x100b8c74 is write-only (ours is private). DisplayTexture's static
GetNumMipLevels(Size) (U6c) is reproduced locally. Test: tests/diff_rs_u6b.cpp (402 real files + 160 generated of
both formats, stand-in BitmapResources with a copy of KernelResources' vftable for the RTTI; the pixels of a new
bitmap are compared only where written: KernelStandard allocates them uninitialised).
U6c split (2026-11-08): U6c1 DisplayTexture + DisplayTextureNull (the API texture layer, 60 exports with the
ctors and vftables), U6c2 Texture + TextureRes (63; they lean on TextureManager, U6d).
U6c1 done (2026-11-08, DisplayTexture.cpp, 1888/2687). DisplayTexture 0x38: +4 state (CheckInitialized: == 1), +8
memory size, +0xc Display*, +0x10 Size, +0x18 dimension, +0x1c depth levels, +0x20 mip levels, +0x24 requested
and +0x28 effective anisotropy, +0x2c bits (0 compressed, 1 can mip, 2 data lost; the other bits are never
written, copies keep the destination's), +0x30 auto mip hint (char), +0x34 RenderTextureConstruct* of a render
target (copies share it, InitInstance copies it). Vftable 17: 0 dtor, 1 Create, 2 Destroy, 3 Load, 4 SubLoad,
5 SubloadFromFrame (true), 6 CreateRenderContext (true), 7 Bind(unit, keep, aniso, wraps, filters), 8 Bind(unit),
9 CopyMipToMip, 10 CreateInstance, 11 GetBitmap (__thiscall), 12 GetDataLost, 13 GetJetFormat, 14
CopySamplerAttributesFrom, 15 RefreshFromChangedSettings (nothing), 16 GetAPITexture (null); the rest pure
(answering 0). DisplayTextureNull 0x70: +0x38 -1, +0x3c render context, +0x40 the smallest level loaded (-1 none),
+0x44/+0x48 min/mag filter, +0x4c/+0x50 wrap S/T, +0x54 never written, +0x58, +0x5c format id, +0x60 dirty, +0x61,
+0x64 units holding it, +0x68 cleared for a depth format, +0x6c the level-0 bitmap kept by Load. The first one asks
Display slot 0x1c (5) for automatic mips and makes the 1000 texture units `bound`; Display slots 0x28 (maximum
anisotropy), 0x98 (CreateTexture) and 0xa0 (SetTexture). The null display's format table at 0x100b8228 (29
entries x 36: flags bit 0 supported, bit 1 compressed, +0x10 bits per 4x4 block, +0x14 the format it converts to,
+0x18 that DisplayFormat) is filled by DisplayNull (U9): read by address. CheckFormat converts through the
original's Jet::Convert, whose scratch buffer is static. TS12 faults kept: Create follows an unsupported format's
link without a null check; CreateInstance loads the kept bitmap with a DisplayFormat whose id and bits are 0 and
the null Load divides the pitch by bits / 8 (it also frees the bitmap per level and face and leaves the pointer);
SubLoad(bitmap, area, ...) walks the levels through Bitmap::Scale, which answers null for 24/16-bit or non-power-of-
two sizes and the null SubLoad then reads it. Load hands CheckFormat its level twice as an output and carries on
with what Convert wrote there. Test: tests/diff_rs_u6c.cpp (stand-in Display, the table filled per scenario).
U6c2 done (2026-11-09, TextureObject.cpp, 36 exports, 1924/2687): Texture only. TextureRes moves to U6d: its
ForceTextureLoad / ChangeMipCount / Test* / async loading work inside the TextureManager's lists (a 0x1c block at
+0x5c with list code 0x1000ddd0 / 0x10042d30, the manager's display at +0x14 and its slot 0x9c), so it is tested
with a real manager. Texture 0x34 on ReferenceCount (vptr, refs): +8 0, +0xc bit 0 "requires an update", +0x10
PString name, +0x14 active frame, +0x18 owned DisplayTexture*, +0x1c TextureAlpha (-1), +0x20 average Color, +0x30
TextureManager* (referenced; slots 4 add, 6 remove, 9 activate; +0x28 its current frame). Vftable 23: 0
ReferenceCount::DeleteThis, 1 dtor, 2 CreateInstance, 3 IsAvailable, 4 IsLoadFailure (false), 5 SetName, 6
GetResourceName, 7 GetResource (null), 8 GetMemorySize, 9 GetAlphaHint, 10 GetAverageColor, 11 GetDimension (3
without an API texture), 12 GetDepth, 13 GetSize, 14 GetAbsoluteSize (slot 13), 15 Activate, 16 Update, 17 Bind, 18
SetResourcePriority, 19 DynamicSubload (DisplayTexture::SubLoad), 20 SetDisplayTexture (frees the old one, also
when it is the new one), 21 GetDisplayTexture, 22 GetTextureManagerInfo (null). Instances are named
"%08x%%name" from uniqueTexId (an instance's own 8-digit prefix replaced). The update flag goes through
TextureManager::SetTextureUpdate (0x10041a50, U6d: an std::set of textures at manager+0x18). Test:
tests/diff_rs_u6c2.cpp (stand-in manager; SetTextureUpdate patched with a jump to a logger; null API textures).
U6d analysis (2026-11-09). Split: U6d1 TextureManager + TextureRes (the streaming core, ~9 KB), U6d2
SharedTextureCollection (the generated built-in textures, 8.6 KB of float code). U6d1 may split again: U6d1a the
manager's containers and lifetime (ctor/dtor, Add, RemoveTexture, Find, Begin, Activate, End, SetTextureUpdate,
RegisterStreamableTexture, the static setters, memory getters, Create, GetSubstituteTexture, Init), U6d1b the
streaming (AddAsyncTextureLoad, RemovedAsyncTextureLoad, Update, AppendMips, CleanupAsyncTextureLoad, Up/DownSize,
the Test* and Check* functions) with TextureRes.
TextureManager 0xd8 on ReferenceCount (vftable 0x10072338, 11 slots: 0 DeleteThis, 1 dtor, 2 Init(RenderKernel*),
3 Create(db, name, key), 4 Add, 5 RegisterStreamableTexture, 6 RemoveTexture, 7 Find(PString), 8 Begin, 9
Activate(texture, mips), 10 End(float)). +8 HashTable<PString, Texture*> (the U3 template: 97 FDA buckets,
0x10042d80 init, 0x10042ed0 find, 0x10042e60 insert, 0x10035cf0 erase, 0x10035db0 destroy) keyed by the texture's
name node; +0x18 FastDynamicArray<Texture*> of the textures needing an update (SetTextureUpdate: find 0x10042ff0,
remove 0x1000d5b0, add 0x1000d530; End calls slot 0x40 Update(float) of each); +0x1c Array<Texture*> of the frame's
active textures {count +0x1c, capacity +0x20, data +0x24} (Begin: count 0, active memory 0, ++frame, Update();
Activate: appended once per frame, memory += GetMemorySize >> 10, then with streaming on the TextureRes's wanted
mip count clamped to MIN/MAX_STREAMING_TEXTURE_MIP_COUNT and UpSizeTexture(1 << (mips - 1))); +0x28 the frame,
+0x2c the active memory (KB), +0x30 byte, +0x34 an FDA, +0x38, +0x3c an embedded TextureInfo (0x68), +0xa0..+0xa8,
+0xac an FDA, +0xb0 two 0x10-byte list heads (vector ctor 0x10042d00 / dtor 0x10042d20, chosen by +0xd0; a
TextureRes's async-info node is linked into list [+0xd0] by RegisterStreamableTexture), +0xd0 index, +0xd4 1.
Create: names without a ".texture"/".movie" extension get "-tex" appended (0x100411a0), the database name is
prefixed with ':' (PStringCache::Ins), the hash table is asked first (a found texture gets a reference); else
db->Get(name) (slot 1), SetScrambleKey(key), resource slot 0x74 CreateInstance(name, manager) and, failing that or
without a resource, GetSubstituteTexture (a Texture over SharedTextureCollection::GenerateSubstituteLoadTexture,
named by the static PString 0x100b8734). Init reads the RenderKernel's OS options GlobalTextureResolution
(globalSkipLevels), TextureResThreshold (globalSkipThreshold), DefaultTextureFormat, DefaultAnisotropy,
DefaultTextureFilter ("none"/"trilinear"/...), DefaultAutoMip, DefaultMin/Mip/MagFilter, UseSubstituteTexture.
Init details (0x10041db0): manager+0x14 = the RenderKernel (its OS at +0x8c, Display at +0x7c); the two skip
options are unsigned Gets defaulting to the statics; DefaultTextureFormat is a PString Get defaulting to the
PString at (0x100351d0())+8, matched case-insensitively against a 16-entry {PString, DFormatId} table at
0x100b8780 (sets DISPLAYFORMATID_DEFAULT); DefaultAnisotropy (default ANISOTROPY_DEFAULT) is clamped to Display
slot 0x28 and kept if >= 1; DefaultTextureFilter: "none" -> min/mip/mag 0, "trilinear" -> 1/1/1, else
(bilinear) 1/0/1 (mip 0), then, if Display slot 0x1c(5) answers, DefaultAutoMip from {name, value} x4 at
0x1006b110 (else AUTO_MIP_DEFAULT 0); without DefaultTextureFilter, DefaultMin/Mip/MagFilter each from the 2 names
at 0x100b1bec (the Mag match jumps into the "none" block's MAG store, then on to the auto-mip part);
UseSubstituteTexture into +0x30 (bool). Always true once the OS has options.
GetSubstituteTexture (0x100421a0): only with +0x30; the cached texture at 0x100b8708 (=
SharedTextureCollection::textureArray[2], an AutoReference) with a reference, else Find (slot 7) of the name
PString 0x100b8734 (= textureNameArray[2]), else a Texture over GenerateSubstituteLoadTexture(display, 0, 0)
cached there. So the substitute path needs SharedTextureCollection (U6d2) or its statics by address.
GetTotalTextureMemory: the hash table walked (0x100430d0 first, 0x10054880 next), slot 8 GetMemorySize >> 10
summed.
U6d1a done (2026-11-09, TextureManager.cpp, 21 exports with the ctor/dtor/vftable, 1945/2687): the containers and
lifetime. The class moved into include/jet/Texture.h (0xd8, members at their TS12 offsets, so the implicit member
destructors run in the original's order: lists, +0xac, the TextureInfo, +0x34, the active array, the update list,
the table's buckets). TextureRes is declared as Texture + ResourceObserver (still the original's code, dllimport
ctor) so that RemoveTexture's dynamic_cast works by RTTI name on the original's objects. Still the original's,
called through its exports (U6d1b): Update (Begin's tail), UpSizeTexture (Activate), CleanupAsyncTextureLoad (dtor,
__thiscall), RemovedAsyncTextureLoad / RemovedTextureDownSize (RemoveTexture by the request's state +0x14: 2 / 1).
Read by address: the 16 format names (0x100b8780, {PString*, DFormatId}), the default format's name
(0x100351d0()+8), the substitute cache and name (SharedTextureCollection textureArray[2] / textureNameArray[2] =
"AuranJetTexture:SubstituteLoadTexture"); GenerateSubstituteLoadTexture by export. Quirks kept: SetTextureUpdate(on)
appends again when already listed; Create keys the table by "dbname:name" (Ins ':' then the database's name at 0)
but asks the database for the bare "-tex" name; GetSubstituteTexture finding nothing on the first Find but something
on the second answers null; a texture Find finds is cached and gets two references; RemoveTexture unlinks a
TextureRes's request node without a null check on GetTextureManagerInfo. RenderSystem now links KernelResources_7c
(BitmapResource::SetMaxSizeTextureStreaming, its one import). Test: tests/diff_rs_u6d.cpp (stand-in textures over
copies of the original's Texture / TextureRes vftables for the RTTI, kernel + OS + options from a random command
line, database and resource; the original's streaming functions and the generator jump to loggers; Resource
SetScrambleKey / ReleaseReference hooked in both import tables). diff_rs_u6c2 now jumps both modules'
SetTextureUpdate to its logger. 53/55 mutants (scratchpad mut_u6d1a.py; 2 equivalent).

U6d1b, downsize queue (2026-11-11): the request shared with the original loader is 0x18 bytes: texture at 0,
database at +4, cancellation flag at +8, reverse mip level at +0xc, first mip at +0x10 and request kind at
+0x14 (1 downsize, 2 async load). The original Update still pops these requests from the +0xac array.
DownSizeTexture (0x100427a0) clamps the requested dimension to 128 before deriving the mip count. It queues
only if the texture has more than one mip on disk, a display texture with more mips than the target, and no
existing request. CancelResizeTexture (0x10042950) dispatches by request kind. RemoveTexture uses the same
dispatch. RemovedTextureDownSize (0x10042980) erases the queue entry if present, clears the texture's request
pointer and frees the request. AddTextureDownSize (0x100429f0) and IsTextureBeingResized (0x10042940) complete
the five exports.

Next async slice: +0x34 holds queued loads and +0x38 the in-flight request. The worker started by 0x100400a0
fills the embedded TextureInfo at +0x3c and publishes its result at +0xa4 (1 running, 2 ready, 3 failure,
4 cancelled). Cancellation 0x10040070 sets the request's flag and waits for the running state to end before
0x10040190 destroys the worker and frees the request. Keep that ordering when replacing Update and cleanup.

Async enqueue (2026-11-11): AddAsyncTextureLoad at 0x10042370 is now ours. Its two integer arguments are
first mip, then reverse level. It preserves an existing resize request, including a downsize. The queue at
+0x34 uses the same 13-pointer FDA chunks as the original. diff_rs_u6d compares 31 requests across three
chunks, repeated requests, null/non-null databases and signed mip values, then lets the original cancellation
code remove every request from both implementations.

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
| U8 layers and scene | Layer, LayerGroup, LayerManager, ClearLayer, Overlay, SceneGraph, SystemResource* (GenericReferenceArray went to U5c) | 144 | 35 | 9 KB |
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

U6d1b async cancellation: RemovedAsyncTextureLoad removes queued loads using the same
queue and allocator as enqueue. Active cancellation now waits for the worker to publish completion, destroys the worker, clears the request and frees the loaded blocks. diff_rs_u6d now compares real queue cancellation
on both sides, including its 31-request fixture. Next: active cancellation and cleanup,
then Update and TextureRes.

U6d1b cleanup and active cancellation: CleanupAsyncTextureLoad and the active branch of
RemovedAsyncTextureLoad now follow the original worker cancellation and release sequence.
diff_rs_u6d compares both populated queues and active states 1 through 4, including a
worker that observes cancellation. Update and worker startup still use the original DLL.

U6d1b upsize: UpSizeTexture now selects the target mip count and queues the load,
using the resource owner and display mip count. diff_rs_u6d compares request values
and missing owner/display cases with the original. The following work supersedes the counts above.

## 2026-11-15: U6d and initial U7

The TextureManager worker, Update, streaming and TextureRes function exports are reconstructed. Shared texture
generators include D2 attenuation, substitute, default/bump, power/specular power, spot cube and normal cube.
`diff_rs_u6d` compares generator uploads, flags, worker queues, texture behaviour and multiple cube sizes against
the original. SharedTextureCollection and streaming data exports still use the original module's shared state;
power generators use its VC80 pow import to preserve x87 results. Both dependencies must be removed for x64.

U7 begins with MaterialFileSupport name conversions (including the original DFunction reverse-lookup quirk),
MaterialSpec constructors, destruction and chunk-preserving copy, and Material state and manager name/flag
accessors. Differential tests include two-chunk lists, string ownership, copying, flags and blend modes.
2078 of 2687 exports are implemented; `check_thunks.py` reports zero implemented exports accidentally forwarded.
A separate laboratory install with the 2078-export build reached the main menu outside the restricted sandbox;
the sandbox's TADDaemon launch failed to relocate a system DLL. This was a test-environment problem, not proof
that every user's database error has the same cause. A driving session remains unverified. U6 regression tests
passed 30 scenarios each: 55 real binary textures, 402 real plus 240 generated texture descriptions, DisplayTexture,
and Texture. The real content was read from the original installation; outputs went to the temporary directory.

## 2026-11-18: U7 instances and state (MaterialInstance.cpp)

The material state the manager and the config extractors build on is ours: Equals (Material and NullMaterial),
InitInstance (both classes share one body at 0x100331b0 / 0x10037a20), Rename, SetTextureDefinition and the
NullMaterial SetTexture pair, VerifyDecoratedName and six leaf accessors (Get/SetShininess at 0x10004330 /
0x10004340, GetNumPasses, IsLoaded, GetRenderKernel, GetTextureManager). 15 more exports; RenderSystem is now
2127 of 2687 ours, 560 forwarded (`check_thunks.py`: 2127 checked, 0 not ours). The manager's table trio
(Add / Remove / Find) followed in the same session: the table at +0x14 is {buckets, count, size} over 97
FastDynamicArray buckets of Material*, keyed by the decorated name node (+0x24); 2131 of 2687 ours.

Layouts and behaviour kept from the assembly:

* Material's lists are ring chunks of seven items: properties {PString key, value} at +0x88 (chunks 0x44) and
  texture definitions {int type, PString name, float weight} at +0x90 (chunks 0x60). `SetTextureDefinition`
  replaces the first definition with the same type (the old name node goes first) or appends; only the append
  bumps the count at +0x8c.
* `InitInstance(Material&) const` reads the prototype (`this`) and writes the instance (the reference): a fresh
  name from `uniqueMatId` ("%08x%%%%%s", or the "xx..%%" prefix replaced when the name already carries one:
  `Find(node, "%%", 8) == 8`, Del 8 characters, Ins the new id), then the names, DisplayMaterial copy,
  blend/opacity/priority/flag bits 0 and 1, both lists appended item by item, and finally the prototype's
  manager Add of the instance (slot 4). A name without a node goes to Setf as a null `%s`, like the original.
* `Rename(decorated, plain)`: no manager -> false; Find of the new decorated name (slot 6) that answers non-null
  -> false; else Remove (5), SetName, Add (4), true.
* `VerifyDecoratedName(String&, unsigned, DynamicArray<TextureDef,7> const&)`: ToLower, and when the name has no
  "m." token (the original's DecoratedTypeToken) the tag is appended at the end (Ins at Length()): "m.notex" /
  "m.onetex" from the hint alone, else by the texture list (reflect -> "m.gloss" with opacity else "m.reflect";
  a diffuse map whose name contains "sb_" -> "m.billboard", "tb_" -> "m.tribillboard"; else the diffuse count).
* Equals compares the DisplayMaterial block through the U1 `operator!=`, the scalars (blend pair, opacity, flag
  bit 0, priority, the count at +0x8c), then the property walk while both sides run and the texture walk until
  one side ends (a length difference in the properties alone still answers true: TS12 quirk, kept).

`diff_rs_u6d` gained "op material leaf / equals / texture definitions / null material set texture / init
instance / rename / verify name": 500 rounds, 0 mismatches against the original, including the two Equals
quirks, the same-name texture definition replace and the three InitInstance naming paths. The shared statics
(uniqueMatId; DecoratedTypeToken is only read) start alike on both sides. Remaining U7: MaterialManager
Create/Add/Remove/Find (0x10035670..0x10035e20), the MaterialConfig extractors (0x10033d40..0x10034b40),
MaterialResource::CreateInstance and LoadTxt (0x10036060 / 0x10036080), NullMaterial::CreateGeometryChunk
(0x10037a30, with GeometryChunkTemplate 0x10037af0), SetLightingState (0x10032960), the PluginFactoryBase
families and the seven shared data statics (they stay the original's until U9, like the other module state).

## 2026-11-18: U7 MaterialManager lifetime and vftable (MaterialManager.cpp)

The manager is a real class now (`jet/Material.h`), so the module exports its own `??_7MaterialManager@Jet@@6B@`
(0x100714fc, 7 slots: DeleteThis, the deleting dtor, Init, Create, Add, Remove, Find) and virtual dispatch
through a manager reaches our code. Layout: ReferenceCount base, kernel +0x08, texture manager +0x0c
(referenced), the default type PString +0x10 and the material table +0x14 ({buckets, count, size}, the same
HashTable as the TextureManager's, 97 buckets of 13-item rings).

Kept from the assembly: the ctor (0x10035510) seeds the default type with the node "notex" (0x100714f0),
inits the table (0x10042d80), then references the texture manager (no null test, as the original) and zeroes
the kernel; the dtor (0x100355b0) empties the table (0x10035db0), releases the texture manager (null-tested),
and lets the members go in reverse order (the buckets last, 0x10042e30) before the reference count base.
Init (0x1000c920) and the table trio moved from their free-function aliases (MaterialFields.cpp,
MaterialInstance.cpp) into members, so the exports and the vftable slots now name the same code; the table
behaviour (Add as it comes, Remove the first of the name with the chunk compaction, Find by node) is unchanged
and still comes from the U7 ring helpers. Create (0x10035670) remains the original's: its plugin-type walk
is not reconstructed, and both the forwarded export and the vftable slot reach the original.

`check_thunks.py`: 2134 checked, 0 not ours. `compare_vtables.py`: `??_7MaterialManager@Jet@@6B@` 7 slots,
same. `diff_rs_u6d` gained "op material manager lifetime" (the ctor's state and vftable identity, the empty
97 buckets, Init through the export and through slot 2, the default type round trip, three materials driven
through the constructed manager with Find through slot 6 and Remove by export, and the dtor's cleanup read
back through the alloc log and the cache's usedNodes): 500 rounds, 0 mismatches. The U6 regression probes are
green (diff_rs_u6c2 500, diff_rs_u6c 500, diff_rs_u6b 300, diff_rs_u6 300 with 400 real .texture files).
Remaining U7: MaterialManager::Create (0x10035670) is still the original's, the MaterialConfig extractors (0x10033d40..0x10034b40),
MaterialResource::CreateInstance and LoadTxt (0x10036060 / 0x10036080), NullMaterial::CreateGeometryChunk
(0x10037a30, with GeometryChunkTemplate 0x10037af0), SetLightingState (0x10032960), the PluginFactoryBase
families, the Material / NullMaterial / MaterialResource ctors and dtors, and the seven shared data statics.

## 2026-11-19: U7 MaterialManager::Create and the MaterialConfig extractors (MaterialManager.cpp, MaterialConfigExtract.cpp, MaterialSpec.cpp)

MaterialSpec is now the real layout in `jet/Material.h` (name +0x00, rename +0x04, DisplayMaterial +0x08,
values[3] +0x4c, flag +0x58, the properties ring +0x5c, the textures ring +0x60, flags[8] +0x64; sizeof 0x6c).
`MaterialEntries<T>` (the seven-item ring chunks, {next, prev, count, 7 items}) moved to the header with
Append/Count/At; MaterialSpec.cpp keeps only the lifetime and the chunk-preserving copy.

MaterialManager::Create (0x10035670) and SplitDecoratedMaterialName (0x10035b80) are ours. The split (the
static block of the original's first call included) makes the type from the text after the last dot and the
name from the lowercased prefix minus the "m" of the "m." tag; Create looks the decorated name up in the table
(an existing material gets AddReference), else walks the material plugin types of the split type (IsSupported
0x10 per display, CreateInstance 0x14, the fallback name retried, the same name or none stopping the walk),
sets the skipped flag +0x84 bit 3, the creation type and the names. The SplitBlock (18 display format names) is
a namespace-scope static: the original builds it from its module CRT initialisers (off FUN_100351d0's guard,
already tripped at startup), so constructing ours at DLL load keeps the 18 cache nodes out of the log (the
imports are patched after load).

The MaterialConfig extractors: ExtractMaterialSpec (0x100343a0), ExtractMaterialSpecs (0x10034a30),
ExtractMaterial (0x10034350), ExtractMaterials (0x10034960), GetMaterial (0x10033d40) and CreateNewMaterial
(0x10034120). GetMaterial makes the material through the manager (a non-empty "Rename" different from the
material's name makes a second one and releases the first), then writes the spec field by field and replays
the properties (a matching key replaces its value, anything else goes through AddProperty) and the textures
(always through the name form of SetTexture). CreateNewMaterial clones: the blend/opacity/two-sided state, the
DisplayMaterial by copy, the properties through AddProperty and the textures through GetTexture / AddReference
/ SetTexture / ReleaseReference.

Two node-traffic details the test pinned: the original passes module-scope static PStrings as its literal keys
(built at load, so no Create/Destroy per lookup) — replicated with the `keyMaterial`/`keyRename`/`keySrcBlend`/
`keyDstBlend`/`keyProperties`/`keyTexture`/`keyBitmap`/`keyName` module statics; and the temporary node handed
to the manager is a raw node, so ours nulls the PString after Destroy rather than letting ~PString double-destroy.

`diff_rs_u7` (new, `tests/diff_rs_u7.cpp`): both modules run the same RNG script over stand-in plugin types,
materials, textures, a stand-in manager and real ConfigData trees (from KernelStandard), comparing the full log
(allocs, PString node traffic, vtable calls, material bytes) after every scenario: 8 scenarios a round, 3000
rounds, 0 mismatches (24000 checks); 200 rounds re-run after the debug prints came out, 0 mismatches.
`check_thunks.py`: 2141 checked, 0 not ours. `compare_vtables.py`: the exported vftables are unchanged (the
not-yet-implemented classes still differ, as before). U6 regressions green: diff_rs_u6d 200, diff_rs_u6c2 200,
diff_rs_u6c 200, diff_rs_u6b 200, diff_rs_u6 300 (400 real .texture files).

Remaining U7: MaterialResource::CreateInstance and LoadTxt (0x10036060 / 0x10036080),
NullMaterial::CreateGeometryChunk (0x10037a30, with GeometryChunkTemplate 0x10037af0), SetLightingState
(0x10032960), the PluginFactoryBase families, the Material / NullMaterial / MaterialResource ctors and dtors,
and the seven shared data statics.


## 2026-11-19: U7 MaterialResource (MaterialResource.cpp)

The material text resource is ours: the ctor from a name and owner (0x10035fb0), the copy ctor (0x10035f00),
operator= (0x10035f30), the dtor (0x10035fe0), the vftable (0x10071554) and with them LoadTxt (0x10036080)
and CreateInstance (0x10036060). 7 more exports; RenderSystem is 2148 of 2687 ours (539 forwarded),
`check_thunks.py`: 2148 checked, 0 not ours. `compare_vtables.py`: `??_7MaterialResource@Jet@@6B@` 19 slots,
same (the 18 Resource slots, half of them folded thunks into KernelResource_7c, plus CreateInstance at 18).

Layout kept from the assembly: `Resource` at +0x00, the extracted `MaterialSpec*` at +0x5c and a
`MaterialConfig` subobject at +0x60. The subobject is a member (not a base: the original exports no secondary
vftable for it) whose vptr the ctors and the dtor write by hand to the shared `MaterialConfig` vtable -
ours does the same through `RsMaterialConfigSetVtable` (MaterialConfigExtract.cpp), so the vtable data export
stays the original's until U9. The copy ctor copies the raw spec pointer with no reference (kept).

LoadTxt (0x10036080, __cdecl: `this`, hidden result, name): a ConfigParser reads the text form through the
owner's `GetFileName` (slot 3) when there is an owner, else the name is the path; the parse result is 0 on
success. Then `root != null && version == 2.0f` exactly (the literal at 0x10062b9c) or the result is 1; the
old spec (if any) goes, the "Material" item (the original's static PString at 0x100b86ec, interned at load)
is extracted into a fresh spec, and every texture definition's file name is converted through
`ConvertResourceName` and re-interned (the old node destroyed first, the new one raw, no cache-null test -
the walk is FUN_1000d390's state machine over the seven-item ring chunks and stops at the head). No spec or
an extract failure answers 1. CreateInstance (0x10036060) is `config.GetMaterial(manager, db, *spec)`.

`diff_rs_u7` gained two scenarios: "material resource" (the ctor's vtable identity on each side, the shared
MaterialConfig vptr, LoadTxt over files written by ConfigParser::Save from the same script tree - version 2.0
and 1.0, with and without a "Material" item, 0..11 texture definitions crossing the ring chunks, names with
paths and empty names, the owner's GetFileName answering the name or an uppercase alias, reloads replacing
the spec - then CreateInstance over the stand-in manager and the dtor) and "resource copy" (the copy ctor and
operator= sharing the spec pointer, one owner frees it). 3000 rounds x 8 scenarios: 24000 checks, 0
mismatches. Regressions green: diff_rs_u6d 200, diff_rs_u6c2 200, diff_rs_u6c 200, diff_rs_u6b 300 (402
real + 240 generated .texture.txt), diff_rs_u6 300 (400 real .texture files). The menu check passed (the
game reached the menu and closed cleanly, JetLog identical to the baseline: diff_lines=0). New test hook:
`RS_U7_ONLY=<scenario>` runs just one scenario kind.

Remaining U7: NullMaterial::CreateGeometryChunk (0x10037a30, with GeometryChunkTemplate 0x10037af0),
SetLightingState (0x10032960), the PluginFactoryBase families, the Material / NullMaterial ctors and dtors,
and the seven shared data statics (they stay the original's until U9, like the other module state).


## 2026-11-19: U7 NullMaterial::CreateGeometryChunk and Material::SetLightingState (GeometryChunkTemplate.cpp, MaterialFields.cpp)

The null material's chunk and the lighting state are ours. `GeometryChunkTemplate.cpp` holds
GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> (the vtable at 0x10071834, the functions
0x10037bb0..0x100383a3) and CreateGeometryChunk (0x10037a30); `MaterialFields.cpp` adds SetLightingState
(0x10032960). Two more exports: RenderSystem is 2150 of 2687 ours (537 forwarded), check_thunks.py 2150
checked / 0 not ours.

Layout and vtable: the class is 0x60 bytes = GeometryChunkSingleStream (0x5c) + the vertex block pointer at
+0x5c (a GenericReferenceArray-shaped block). Its vtable differs from the exported
GeometryChunkSingleStream's exactly in the slots 1-5, 7-9, 12/13, 39, 42-44 and 46..55 (dumped from the
original's two vftables and compared), so the class derives from ours and overrides those. The views return
through the hidden result pointer a member function gets for an 8-byte class return; a free function would
return it in EDX:EAX (compile-tested), so they must stay members.

Kept from the assembly: the block writes only its reference count and count (the item size and the capacity
keep the heap garbage; nothing reads them); DuplicateVertexArray copies maxVertices items whatever the old
block holds; ResizeVertexArray copies min(old max, n) then writes both counts; GetVertexArray is
block + 0x10 without a null test (0x10 without a block); the no-upload dirty call passes (on, i) without the
stream argument (the implementations ignore it); the view table is PackedVertexPNT::vertexItemOffset (the
same 0x100664b0 table, ours since U1). CreateGeometryChunk: the flags are RsMatGeometryFlags (the
0x1006cd84 table and the hint bits), and a hint of version 1 makes it SetVertexDataDynamic(0, -2, true).
Deviation: on a failed 0x60 allocation with a version-1 hint the original calls through null; ours answers
null.

SetLightingState (the second Matrix4 argument is never read; kept): with a program its slot 17 gets (lights,
the display state block's colour at +0xa4 copied through the x87 stack - a signalling NaN comes out quiet,
the material block, the transform or a local identity matrix); without one the display's light slots fill up
to its light count at +0x50 (0x22c per light, 0x224 enabled, the state's lightSet[i] = 1), then 0x94, then
0xcc with the material block, and the tail (0x1f0 with the state bit +0xa1 bit 2) only when the bit is
clear. Both flagged paths share one synthetic white material (ambient 1111, diffuse (1,1,1,opacity),
specular and emissive 0001, power 0; the test corrected a first misread of the no-program stores); the
unflagged paths write opacity into the material's diffuse alpha at +0x50 through the const this. With no
program and no lights array both modules read address 0 (the test avoids it).

diff_rs_u7 gained "lighting" and "chunk": the state colour with NaNs, light rings (up to two chunks, a
count-0 one included), the mode flag, both material blocks and SNaN opacity on one side; on the other
CreateGeometryChunk over every GeometryType and hint shape (null, base, Standard with the bits byte,
Database; version 1 and 2), then the 39 item offsets, all ten views, the buffer cycle with scripted
no-buffer and failed uploads, DuplicateVertexArray, ResizeVertexArray, DuplicateIndexArray, both
CreateInstance overloads and the deleting destructor (the block refs and the vertex bytes compared through
the allocation names). 3000 rounds x 8 scenarios: 24000 checks, 0 mismatches; each kind also 3000 rounds
alone. New hook RS_U7_TRACE. The mixed run caught a real bug: the template's default constructor left the
block pointer uninitialised (the original writes it at the end of 0x10038340) - visible only on recycled
heap memory, the kind-alone runs had only seen fresh pages.

Regressions green (diff_rs_u6d 200, u6c2 200, u6c 200, u6b 300, u6 300, all 0 mismatches); the exported
vftables are unchanged (the not-yet-implemented classes differ as before). Menu check: alive at the menu,
clean exit, JetLog identical (diff_lines=0). Since then the PluginFactoryBase / MaterialPlugin families and
the Material / NullMaterial lifetime, instance creation, comparison and data mirrors have been ported
(2185/2687 exports, 502 forwarded). Remaining U7: the five U9/foundation members (the three Display and two
RenderKernel material-mode/manager accessors) and the seven shared data statics, which stay original until
U9.

### U8: layer state (2026-11-22)

The existing `Layer`/`Overlay` declarations were sufficient for the first seven leaf exports.
The saved RenderSystem Ghidra export shows `Layer::IsVisible` (0x1000ae40) and `SetVisible`
(0x1000ae50) use bit 0 at `+0x14`, `IsGroup` (0x1000aef0) uses bit 1, and `GetPriority`
(0x100106d0) reads the signed word at `+0x10`. `Overlay::SetViewPortMode` (0x10038570) updates
bit 1 at `+0x20`; `SetClipping`/`GetClipping` (0x10038c00/0x10038c20) copy four ints at `+0x24`.
`diff_rs_u8` checks all initial flag-byte values and both setter states, compares surrounding bytes,
and exercises the original cdecl member ABI for the rectangle returned by value.

Do not group `Layer::GetLayerManager` with the local reads: it uses the original shared manager.

### U8: LayerGroup and root-group operations (2026-11-22)

- `Layer +0x04` is an embedded `{next, prev}` link. `LayerGroup` has its sentinel at `+0x20`, count
  at `+0x28` and view rect at `+0x2c`; `LayerManager` embeds the root group at `+0x04` and stores its
  viewer array immediately after it at `+0x40`.
- `Add` walks forward in increasing priority order, inserting after ties; `BringToBack` reinserts before
  ties. `Remove` searches for membership, unlinks and clears both links but leaves `Layer::group` alone.
  `SetParent` removes, assigns, then adds even if the new group is the same. `SetPriority` writes the priority
  before removing and adding.
- Find and hit tests visit the list in forward order, recursing into group children. Group render checks the
  group's visibility; group update/reset do not. Base render calls `CanRender` then `Begin`, `Render`, `End`
  in that order when visible; base update calls `Update` only when visible.
- View calculations preserve the original fallback: if no explicit positive size, read the active display
  target (the render target if set, otherwise the window). The kernel cache uses the original's shared
  `Layer::layerManager` when not already set. Keep its pointer storage original until U9.
- `diff_rs_u8` compares 8632 group trace entries as well as the 512 state cases.

### U8: layer lifetime and rendering (2026-11-22)

- `LayerLifetime.cpp`: default and named Layer constructors use the shared manager root; the grouped
  overload accepts a null manager without inserting. Copies and assignment are shallow for links and
  groups, retain the interned string and preserve undefined flag bits/padding. The group owns its
  children: destruction unlinks each child before calling its deleting destructor, so the child does not
  remove itself twice. Default constructor names are `Not initialised` and `ClearLayer`.
- ClearLayer renders with the clipped group viewport, converts Y using the current target height,
  sets the color and clears with mask 7. Overlay sets viewport/projection and translated scissor,
  configures blending, and saves the mode, wireframe, depth and culling bits. End restores those saved
  states in the retail order; it does not restore every setting changed by Begin.
- `diff_rs_u8` gained 4744 lifetime and rendering trace entries: raw initialization patterns,
  group membership, balanced name references, deletion order, clear and overlay backend calls,
  reversed culling, and both projections. The constructors still use retail virtual tables.

### U8: manager, scene graph and virtual table cutover (2026-11-22)

- `LayerManager` 0x10031df0/0x10031ec0 (lifetime) and 0x10031f80 (find): it publishes the manager after
  constructing the root group and unpublishes it only if the pointer still refers to this instance.
  Its owning viewer list is a ring of 13-pointer chunks. Find compares the interned name at viewer `+0x08`.
  Copy/assignment copy chunk fill and viewer pointers; assignment frees old chunks without deleting viewers.
- SceneGraph holds active scenes and a delayed-delete queue in two such rings and owns the modifier task.
  Shutdown purges delayed deletes first, then destroys active scenes, then destroys the modifier task.
  Copy and assignment are shallow for scene and task pointers, preserving the original behavior.
- The seven U8 virtual tables now dispatch non-abstract entries to the port. The locator at `vptr[-1]`
  and the pure-call entries remain original. Slot zero implements scalar and vector deleting destruction,
  including the original array header and Jet allocator; the other slots use the x86 stack-`this` ABI.
- U9 must migrate the `Layer::layerManager` pointer and the two LayerManager profiling records together
  with their original callers. The Win32 U8 code is ported, not a standalone x64 module.

### U9: Display viewport and clear/cull state (2026-11-22)

- `DisplayState.cpp` owns the viewport/scissor cache, pixel and normalized 2D projections, window and
  render-target sizes, clear color/depth/stencil values, and cull/reflection settings (20 exports).
  Changing the viewport always resets the scissor, even when the two rectangles already match.
  Reflection changes swap the backend cull modes 1 and 2 without changing the cached logical mode.
- Overlay and ClearLayer now call these implementations for their Display state; target-size and
  normalized-projection logic is no longer duplicated in the U8 code.
- `diff_rs_u9` compares 12216 entries across 64 rounds, including the backend call order, both target
  sizes, redundant-state suppression, NaNs and signed zero. `diff_rs_u8` passes on the same build.
- Material state ported: `Display::SetMaterialProperties` (0x10003360) compares the cached
  `DisplayMaterial` at state `+0x114` and calls backend slot `+0x21c` before updating it; color material
  mode is at state `+0x98`. `RenderKernel::SetMaterialManager` (0x10039650) releases the old pointer
  at `+0x90`, stores/retains the new one and calls its initialization slot `+0x08`. Setting the same
  pointer still releases, retains and initializes it. Differential checks cover that case, replacing
  the last reference to the old manager, and setting the pointer to null.

### U9: Display write masks, depth and alpha state (2026-11-22)

- 21 exports now come from `DisplayState.cpp`: color write mask, depth write/test/function/clamp,
  alpha test mode/function/reference, and alpha blend mode/equation/factors. Lighting, fog and
  ambient remain forwarded.
- Backend slot `0x1ac` takes `(target, uint32_t mask)`: target 1 is color, target 2 is depth.
  Widen the depth boolean before the call so the upper 24 bits are zero on x86. Cached setters call
  the backend before writing state, and the original reads the state pointer again after the callback.
- Alpha-test reference clamps to `[0, 1]`, with NaN and negative zero mapped to positive zero
  for a changed state. An equal cached pair skips the write, including for opposite signed zeros.
- `diff_rs_u9` compares the backend argument trace, duplicate-call suppression, shared bits and
  cache bytes; it also switches the state pointer during the write-mask callback.
- For the next lighting batch, the retail light loops at `0x10003cf0` and `0x10003d50`
  reload both the flag array after a backend callback and the hardware-light count for the
  loop bound. Check that behavior before activating the draft lighting methods.

### U9: Display lighting, fog, ambient and scalar colors (2026-11-22)

- 24 lighting/fog/ambient exports and three scalar color exports now come from
  `DisplayState.cpp`. Light setters and loops reread the active flag array after backend
  callbacks, and loops reread the light count. The original base `EndLightingUpdate`
  is an empty method; derived virtual overrides are unchanged.
- Packed formats 1 and 2 are ARGB and ABGR. Unsupported encode returns `0xffffffff`;
  unsupported decode leaves the caller's output buffer untouched. Scalar routines read
  the shared `Display::internalColorFormat` data from the original until its writers move.
- `diff_rs_u9` exercises backend ordering, callback-driven replacement of the flag
  array, count changes, light bounds, NaNs, both packed formats and all 256 byte values.
- Next array batch: `0x1001c0c0` and `0x1001c110` swap red and blue in place only for
  internal format 1. The latter takes a byte stride followed by a packed data pointer by value.
  Compare arbitrary strides, untouched padding, nonpositive counts and other format IDs.
- Remaining export inventory: 148 `Display`, 23 `RenderKernel`, 8 `DisplaySelector`,
  6 `NonRenderContext`, 4 `DisplaySaveBuffer`, 3 `SharedContext`, and 116 other symbols.
  The latter group includes streamed vertex and element lifetimes, shared data and
  virtual tables. Move these with their users rather than counting forwarding as a port.

### U9: color arrays and geometry display modes (2026-11-22)

- All four packed-color array overloads are local. The two typed conversions use the
  same channel transforms as the scalar versions and leave buffers untouched for
  unsupported formats. The in-place routines swap red and blue only in format 1.
- The Win32 `VertexItemPtr` adapter takes its two words in order: signed byte stride,
  then pointer. Differential tests compare negative, zero, positive and unaligned
  strides, padding and nonpositive counts.
- Twenty geometry display exports are local. Wireframe uses cached state byte `0xa0`,
  bit zero, and calls slot `0x18c` only on change. Other view modes update the Display
  object itself, not its state block. Tests compare raw bytes and backend calls.
- Next polygon-offset probe: `0x10004480` caches modes 0–2 separately, ignores 3 and
  4, and always calls slot `0x238` for mode 5 before setting all three cached flags.

### U9: stencil, polygon offset and texture capabilities (2026-11-22)

- 14 exports are local. Stencil mode caches enable and two-sided flags. If backend
  slot `0x1bc` rejects an update, only the enable flag is cleared.
- The per-face stencil record is 28 bytes. Its setters keep the record selected
  before the callback even if the callback changes the active Display state pointer.
  Combined setup changes the function and operations without changing the write mask.
- Polygon offset uses cached float pairs and mode bytes. Mode 5 always calls the
  backend and updates all three mode bytes. The probe compares mode-cache bytes,
  redundant updates, NaNs, signed zero, stencil rejection and old-record ownership.
- Next getter batch: at `0x100041f0`–`0x10004240`, the retail capability flags
  and three manager pointers are object fields, not cached backend calls.
  `GetAllVideoModes` returns the embedded array address even if its count is zero,
  while current and default video mode copy 16 bytes and always return true.

### U9: Display capabilities, video, managers and frame information (2026-11-22)

- 24 exports are local. The manager and frame-info getters return existing pointers;
  video-mode and current-format getters copy 16 and 32 bytes, respectively.
  The video lists always return the embedded pointer, with a true result for at
  least one mode or at least two formats.
- `SetAntiAliasing` updates its cached bit before calling the backend and skips
  the call when unsupported or already equal. `GetAntiAliasModes` clears the
  output count, deduplicates sample counts in input order, and uses Jet's allocator
  to grow capacity in groups of ten.
- Differential tests cover pointer identity, copy footprints, video-list count
  thresholds, first-occurrence order, capacity reuse and growth, and cache-before-call ordering.
- Next projection probe: `IsNormalized2DViewport` at `0x10004580` requires a
  cached mode of 0 and an absolute horizontal span strictly below `1.01f`.
  Compare values at the threshold, reversed bounds and unordered inputs.

### U9: Display projection, texture state and render targets (2026-11-22)

- 15 exports are now local. Projection setters cache raw inputs before dispatch.
  The transitional setter reorders the cache relative to the backend arguments;
  the offset setter passes `near + offset` to the backend but caches both separately.
  `SetViewModeParams` stores all eight floats even when a mode has no backend action.
- Texture generation caches mode and coordinate count, not the stage. Global
  texture toggles use `global && local` and preserve the local flag while disabled.
  Render-target assignment always calls the backend after storing the target.
  Projected-texture updates call before caching the enabled flag. The retail base
  active-stage selector is empty; renderer overrides are unchanged.
- The oracle covers exact callbacks for view modes 0–5, out-of-range modes, near-plane
  offsets, repeated updates, NaNs, reversed spans, null targets and both texture flags.
- Next RenderKernel ownership probe: `DestroySceneGraph` and `SetSceneGraph`
  destroy and free the prior pointer at `+0x80`; neither checks for equal pointers.
  `GetRenderKernel` at `0x1000d300` is a secondary-base adjustment (`this - 0x28`).

### U9: RenderKernel access and resource ownership (2026-11-22)

- Ten exports are local. RenderKernel owns the SceneGraph at `+0x80` and frees its previous
  allocation after destruction, even if a destructor callback changes the field.
  The SceneGraph destructor still owns pending and live scenes and its modifier task.
- TextureManager uses the same release, store, retain and initialize order as MaterialManager.
  System resources use a 13-pointer chunk ring at `+0x88`: append preserves duplicates and
  removal acts on the first equal pointer without releasing the pointed-to resource.
- Differential checks cover the secondary-base adjustment, replacement/destruction order,
  callback mutation, manager reference counts and a three-chunk resource ring.
- Next context batch: `SharedContext`, `NonRenderContext` and `DisplaySaveBuffer` all
  construct `ReferenceCount` with count 1. Their base destructors restore their own
  vtable before tail-calling `ReferenceCount::~ReferenceCount`. The save buffer also
  stores its type at `+0x08`. Verify RTTI and deleting-destructor slots before making
  their exported vtables local.

### U9: remaining export groups at 2580/2687 (2026-11-22)

- 21 functions still forward, including Display lifecycle, vertex-type construction,
  pixel conversion and scratch storage, RenderKernel lifetime/frame control,
  screenshot saving, and `JetGetPluginsRenderSystem`. DisplaySelector video and
  format selection and `Display::SetupVideoFormat` are now local.
- 11 virtual/base tables still forward. Context, selector, and element-loader
  constructors still install original tables; direct export tests do not establish
  that original vtable dispatch uses the replacements.
- 75 other data exports still forward. Migrate each state owner with every remaining
  reader and writer. In particular, do not create a second mutable registry or
  interned-key table while the original still initializes or updates the first one.
- `RenderKernel::Init` tests cover absence of a renderer and the `NullRender` backend
  with existing and newly allocated managers. Windows gamma calls are intercepted in
  the test process; the test compares exact ramp values without touching the desktop.

### U10: pixel conversions at 2585/2687 (2026-11-22)

- The three `Convert` overloads, `Repitch` and `FlushScratchBuffer` are local.
  The row helpers keep their original strides: 4/4 for 32-bit RGB swaps, 4/3 and
  3/4 for channel expansion, 3/3 for 24-bit swaps, and 3/2 for 24-bit RGB565.
- 32-bit RGB-to-565 loads four bytes even for 24-bit sources; the upper byte is
  masked away but must be readable. L16 scaling uses `fistp` with nearest-even
  rounding, not a truncating integer conversion.
- The scratch buffer reallocates only when `need != size && size <= need`.
  `Repitch` returns `scratch + need` in the data reference and clears `a` and `b`.
- `diff_rs_u10` compares 1969 trace entries, including guard bytes and all
  supported format pairs. Game check `game-20261122-233441` exited with code `0`
  and matched the baseline JetLog line for line.

### U11: data mirrors at 2671/2687 (2026-11-23)

- All 86 remaining data exports are local byte mirrors: 75 plain statics, 10
  vftables/vbtables plus the `?__info@` records, and one Display vbtable. The
  original module still owns the objects. `DataMirrorList.h` is the single list
  used by both `DataStatics.cpp` and `diff_rs_u11.cpp`.
- Every vftable mirror owns its RTTI locator at `table[-1]`. A dedicated
  section keeps that word adjacent to the exported array. A layout
  `static_assert` and a load-time address check enforce that adjacency. Plain
  mirrors use `alignas(4)` with their exact byte sizes
  (1, 4, 8, 12, 16, 24, 32 or 320).
- A data export is a snapshot. `RsMirrorSync(name)` recopies one mirror from
  the original after a ported writer changes the live static. Every local
  writer of a mutable mirror calls it: texture defaults, file compression,
  streaming limits and enable flag, `TextureManager::Init` options,
  `Texture::uniqueTexId`, both manager pointers, `Element::currentVisited`,
  `AsciiTextureResource::noPath`, `TextureRes::delayedLoading`, texture support
  flags, program lists, registration tables, `VertexType::display`,
  `DisplayTextureNull` counters and LayerManager profile counters. Do not alias
  an export to our IAT slot.
- `diff_rs_u11` checks all 86 mirrors plus 33 writer checks (119 total).
- `gen_def.py` classifies `?__info@` and static data inside a quoted function
  scope as data; forwarders now carry `DATA` too.
  `diff_rs_u9` reads `internalColorFormat` from the original module because the
  U11 export is a mirror and the code under test still uses the owner's value.
- Sixteen functions still forward: eight Display lifecycle (including the
  deferred 38 KB `CreateVertexTypeObject`), seven RenderKernel lifetime/frame
  exports and `JetGetPluginsRenderSystem`. RenderKernel ownership and Display
  layout notes from the U9 analysis remain the starting point for U12.
- Game checks `game-20261123-001309` and, after review fixes,
  `game-20261123-003656` exited with code `0` and matched the baseline JetLog
  line for line.
