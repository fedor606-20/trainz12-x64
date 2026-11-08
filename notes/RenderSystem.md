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
U6d analysis (2026-11-09, in progress). Split: U6d1 TextureManager + TextureRes (the streaming core, ~9 KB), U6d2
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
