// Unit U6a of RenderSystem_7c: TextureResource (0x10001030..0x100015d0, 0x10008de0, 0x10046ff0..0x10048ee8) and
// BinaryTextureResourceExport (0x10007cc0..0x10008360, its getters folded elsewhere). Behaviour follows the TS12
// assembly (addresses per function), quirks included: the INFO block of version 0x103 comes out as version 0x105,
// a failed write in SaveMipMap leaves its compressor referenced, the export's GetMip ignores the level and face.
// The defaults and the TextureManager limits stay the original's statics.
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

constexpr unsigned long kMMTF = 0x46544d4d;  // 'MMTF'
constexpr unsigned long kINFO = 0x4f464e49;  // 'INFO'
constexpr unsigned long kMMAP = 0x50414d4d;  // 'MMAP'

// the compressed display formats (DXT and friends)
bool Compressed(int f) { return f == 10 || f == 11 || f == 12 || f == 0x19 || f == 0x1a || f == 0x1b || f == 0x1c; }

// a multiplication with the original's overflow check (all bits set on overflow)
unsigned int Times(unsigned int n, unsigned int size)
{
    const unsigned long long b = static_cast<unsigned long long>(n) * size;
    return b >> 32 ? 0xffffffffu : static_cast<unsigned int>(b);
}

// a PString node made from 8 (or more: the text need not end within them) bytes
PStringCache::Node* AnsiNode(const char* text)
{
    return PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1);
}
void DropNode(PStringCache::Node* n)
{
    if (n && PString::Cache()) PString::Cache()->Destroy(n);
}

// the bitmap format of an image format id
Bitmap::Format BitmapFormat(int f)
{
    if (f == 1) return Bitmap::ARGB;
    if (f == 3) return Bitmap::XRGB;
    if (f == 5) return Bitmap::RGB;
    return Bitmap::UNSPECIFIED;
}

const ReferenceCount* Ref(const Compress* c) { return c; }

void DeleteBitmap(const Bitmap* b)
{
    const_cast<Bitmap*>(b)->~Bitmap();
    Alloc::Delete(const_cast<Bitmap*>(b));
}

// 0x10047070: the first and the last stored level, counting from the smallest (face 0 only)
void StoredLevels(const TextureResource::TextureInfo* info, int& first, int& last)
{
    first = -1;
    last = -1;
    for (int i = 0; i < static_cast<int>(info->mipLevels); ++i) {
        if (!info->blocks) continue;
        const unsigned int* b = info->blocks[(info->mipLevels - 1 - i) * info->depth];
        if (!b) continue;
        if (first == -1) first = i;
        last = i;
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// TextureInfo

TextureResource::TextureInfo::TextureInfo() : blocks(nullptr) {}  // 0x10046ff0
TextureResource::TextureInfo::~TextureInfo() { FreeBlocks(); }   // 0x10047000

// 0x10047010
void TextureResource::TextureInfo::FreeBlocks()
{
    if (!blocks) return;
    for (unsigned int k = 0; k < mipLevels * depth; ++k) {
        if (blocks[k]) Alloc::Delete(blocks[k]);
        blocks[k] = nullptr;
    }
    if (blocks) Alloc::Delete(blocks);
    blocks = nullptr;
}

namespace {

void DeleteInfo(TextureResource::TextureInfo*& info)
{
    if (info) {
        info->FreeBlocks();
        Alloc::Delete(info);
    }
    info = nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// construction

// 0x100011a0
TextureResource::TextureResource(const PString& name, const ResourceDatabase* owner)
    : Resource(name, owner), info(nullptr), dataUnloaded(false)
{
}

// 0x10001080 (the info pointer is shared, as in TS12)
TextureResource::TextureResource(const TextureResource& o) : Resource(o), info(o.info), dataUnloaded(o.dataUnloaded) {}

// 0x100010b0
TextureResource& TextureResource::operator=(const TextureResource& o)
{
    Resource::operator=(o);
    info = o.info;
    dataUnloaded = o.dataUnloaded;
    return *this;
}

TextureResource::~TextureResource() { DeleteInfo(info); }  // 0x100014d0

// ---------------------------------------------------------------------------------------------------------------
// defaults (the original's statics)

TextureWrap __cdecl TextureResource::GetDefaultWrapS() { return WRAP_S_DEFAULT; }               // 0x100011d0
void __cdecl TextureResource::SetDefaultWrapS(TextureWrap w) { WRAP_S_DEFAULT = w; }            // 0x100011e0
TextureWrap __cdecl TextureResource::GetDefaultWrapT() { return WRAP_T_DEFAULT; }               // 0x100011f0
void __cdecl TextureResource::SetDefaultWrapT(TextureWrap w) { WRAP_T_DEFAULT = w; }            // 0x10001200
TextureFilter __cdecl TextureResource::GetDefaultMinFilter() { return MIN_FILTER_DEFAULT; }     // 0x10001210
void __cdecl TextureResource::SetDefaultMinFilter(TextureFilter f) { MIN_FILTER_DEFAULT = f; }  // 0x10001220
TextureFilter __cdecl TextureResource::GetDefaultMagFilter() { return MAG_FILTER_DEFAULT; }     // 0x10001230
void __cdecl TextureResource::SetDefaultMagFilter(TextureFilter f) { MAG_FILTER_DEFAULT = f; }  // 0x10001240
TextureFilter __cdecl TextureResource::GetDefaultMipFilter() { return MIP_FILTER_DEFAULT; }     // 0x10001250
void __cdecl TextureResource::SetDefaultMipFilter(TextureFilter f) { MIP_FILTER_DEFAULT = f; }  // 0x10001260
TextureAutoMip __cdecl TextureResource::GetDefaultAutoMip() { return AUTO_MIP_DEFAULT; }        // 0x10001270
void __cdecl TextureResource::SetDefaultAutoMip(TextureAutoMip m) { AUTO_MIP_DEFAULT = m; }     // 0x10001280
TextureAlpha __cdecl TextureResource::GetDefaultAlphaHint() { return ALPHA_HINT_DEFAULT; }      // 0x10001290
void __cdecl TextureResource::SetDefaultAlphaHint(TextureAlpha a) { ALPHA_HINT_DEFAULT = a; }   // 0x100012a0
TextureHint __cdecl TextureResource::GetDefaultTextureHint() { return HINT_DEFAULT; }           // 0x10008de0
void __cdecl TextureResource::SetDefaultTextureHint(TextureHint h) { HINT_DEFAULT = h; }        // 0x100012b0
DFormatId __cdecl TextureResource::GetDefaultDisplayFormatId() { return DISPLAYFORMATID_DEFAULT; }   // 0x100012c0
void __cdecl TextureResource::SetDefaultDisplayFormatId(DFormatId f) { DISPLAYFORMATID_DEFAULT = f; }  // 0x100012d0
float __cdecl TextureResource::GetDefaultAnisotropyFilterLevel() { return ANISOTROPY_DEFAULT; }      // 0x100012e0
void __cdecl TextureResource::SetDefaultAnisotropyFilterLevel(float a) { ANISOTROPY_DEFAULT = a; }  // 0x100012f0

// 0x10001300 / 0x10001320: the static's own 8 bytes, as a char[8]
void __cdecl TextureResource::GetDefaultFileCompression(char* const out)
{
    std::strncpy(out, reinterpret_cast<const char*>(&FILE_COMPRESSION_DEFAULT), 8);
}
void __cdecl TextureResource::SetDefaultFileCompression(char* name)
{
    std::strncpy(reinterpret_cast<char*>(&FILE_COMPRESSION_DEFAULT), name, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// the loaded fields

TextureDimension __cdecl TextureResource::GetDimension() const { return info->dimension; }      // 0x10001340
unsigned int __cdecl TextureResource::GetDepth() const { return info->depth; }                  // 0x10001350
float __cdecl TextureResource::GetAnisotropyFilterLevel() const { return info->anisotropy; }    // 0x10001360
void __cdecl TextureResource::GetFileCompression(char* const out) const                        // 0x10001370
{
    std::strncpy(out, info->compression, 8);
}
unsigned int __cdecl TextureResource::GetFileCompressionRatio() const { return 5; }             // 0x10001390
Size __cdecl TextureResource::GetSize() const { return info->size; }                            // 0x100013a0
TextureHint __cdecl TextureResource::GetHint() const { return info->hint; }                     // 0x100013c0
TextureWrap __cdecl TextureResource::GetWrapS() const { return info->wrapS; }                   // 0x100013d0
TextureWrap __cdecl TextureResource::GetWrapT() const { return info->wrapT; }                   // 0x100013e0
TextureFilter __cdecl TextureResource::GetMinFilter() const { return info->minFilter; }         // 0x100013f0
TextureFilter __cdecl TextureResource::GetMagFilter() const { return info->magFilter; }         // 0x10001400
TextureFilter __cdecl TextureResource::GetMipFilter() const { return info->mipFilter; }         // 0x10001410
unsigned int __cdecl TextureResource::GetNumMipLevels() const { return info->mipLevels; }       // 0x10001420
TextureAutoMip __cdecl TextureResource::GetAutoMipHint() const { return info->autoMip; }        // 0x10001430
NormalMapHint __cdecl TextureResource::GetNormalMapHint() const { return info->normalMapHint; }  // 0x10001510
TextureAlpha __cdecl TextureResource::GetAlphaHint() const { return info->alphaHint; }          // 0x10001520
// 0x10001530: through the x87 stack (a signalling NaN comes out quiet)
Color __cdecl TextureResource::GetAverageColor() const
{
    Color c;
    unsigned int u[4];
    std::memcpy(u, &info->averageColor, 16);
    for (unsigned int& v : u)
        if ((v & 0x7f800000) == 0x7f800000 && (v & 0x7fffff) && !(v & 0x400000)) v |= 0x400000;
    std::memcpy(&c, u, 16);
    return c;
}
DFormatId __cdecl TextureResource::GetFormatId() const { return GetImageFormatId(); }           // 0x10001060
bool __cdecl TextureResource::IsAvailable() const { return Status() == STATUS_AVAILABLE; }      // 0x10047150

// 0x10001560
DFormatId __cdecl TextureResource::GetImageFormatId() const
{
    return info ? info->imageFormat : DISPLAYFORMATID_DEFAULT;
}

// 0x10048920: the image format, unless the default display format is compressed and the image's is not: then a
// compressed one by the alpha hint (the default for no alpha, 0x1a/0xb for one bit, 0x19/0xa otherwise)
DFormatId __cdecl TextureResource::GetDisplayFormatId() const
{
    const DFormatId image = GetImageFormatId();
    if (GetHint() != 0 || !Compressed(DISPLAYFORMATID_DEFAULT) || Compressed(image)) return image;
    const bool dxt = DISPLAYFORMATID_DEFAULT == 10 || DISPLAYFORMATID_DEFAULT == 11 || DISPLAYFORMATID_DEFAULT == 12;
    const TextureAlpha alpha = GetAlphaHint();
    if (alpha == 1) return DISPLAYFORMATID_DEFAULT;
    if (alpha == 2) return static_cast<DFormatId>(dxt ? 0xb : 0x1a);
    return static_cast<DFormatId>(dxt ? 0xa : 0x19);
}

bool __cdecl TextureResource::GetMustResetWhenFinished() const { return true; }  // 0x100015d0
bool __cdecl TextureResource::GetMipLock() const { return true; }                // 0x100015d0
bool __cdecl TextureResource::GetMipUnlock() const { return true; }              // 0x100015d0
unsigned int __cdecl TextureResource::CalculateSize() const { return 100; }      // 0x10001580

// 0x10001590
unsigned int __cdecl TextureResource::CalculateDataSize() const
{
    unsigned int total = 0;
    if (!info || !(info->depth * info->mipLevels)) return 0;
    for (unsigned int k = 0; k < info->depth * info->mipLevels; ++k)
        if (info->blocks[k]) total += 4 + *info->blocks[k];
    return total;
}

// 0x100489f0
bool __cdecl TextureResource::Unload()
{
    DeleteInfo(info);
    return true;
}

// 0x10048e80: the data goes, the resource stays (Lock loads it again)
void __cdecl TextureResource::UnloadDataOnly()
{
    Unload();
    dataUnloaded = true;
}

// 0x10048ea0
TextureResource::Result __cdecl TextureResource::Lock() const
{
    Result r = Resource::Lock();
    if (r.code == 0 && dataUnloaded) r = const_cast<TextureResource*>(this)->Load(GetName().Text());
    return r;
}

bool __cdecl TextureResource::CanAsyncLoadMips() const  // 0x10048e40
{
    return info && static_cast<int>(info->mipLevels) < info->levelsOnDisk;
}

int __cdecl TextureResource::GetMipLevelCountOnDisk() const { return info ? info->levelsOnDisk : 0; }  // 0x10048e60

// 0x100470d0
Texture* __cdecl TextureResource::CreateInstance(const PString& name, TextureManager* manager) const
{
    return reinterpret_cast<Texture*>(new TextureRes(name, manager, this));
}

// ---------------------------------------------------------------------------------------------------------------
// mips

// 0x10048c00: the size of level `level` counted from the smallest, the longer side 2^level
Size __cdecl TextureResource::GetMipSizeAtReverseLevel(int level, int width, int height)
{
    if (width <= 0 || height <= 0) return Size{0, 0};
    if (level < 0) level = 0;
    const int side = 1 << (level & 0x1f);
    if (width < height) {
        int w = side * width / height;
        if (w < 1) w = 1;
        return Size{w, side};
    }
    const int h = side * height / width;
    if (h < 1) return Size{side, 1};
    return Size{side, h};
}

Size __cdecl TextureResource::GetMipSizeAtReverseLevel(int level, Size size)  // 0x10001030
{
    return GetMipSizeAtReverseLevel(level, size.w, size.h);
}

// TS12's Size has constructors, so its static functions hand a Size back through a hidden pointer; ours is a
// plain struct (returned in edx:eax). These two are what the static overloads are exported as (implemented.txt).
Size* __cdecl RsMipSizeAtReverseLevel(Size* out, int level, int width, int height)
{
    *out = TextureResource::GetMipSizeAtReverseLevel(level, width, height);
    return out;
}
Size* __cdecl RsMipSizeAtReverseLevelOf(Size* out, int level, Size size)
{
    *out = TextureResource::GetMipSizeAtReverseLevel(level, size.w, size.h);
    return out;
}

// 0x10048c90
Size TextureResource::GetMipSizeAtReverseLevel(int level) const
{
    const Size s = GetSize();
    return GetMipSizeAtReverseLevel(level, s.w, s.h);
}

// 0x10048a20: a Bitmap over the stored block (the block is not copied)
const Bitmap* __cdecl TextureResource::GetMip(unsigned int level, unsigned int face, const Size& size) const
{
    unsigned int* const b = info->blocks[face + GetDepth() * level];
    const unsigned int bytes = *b;
    auto* const bmp = new (Alloc::New(0x44)) Bitmap();
    bmp->Init(size, 0, BitmapFormat(GetImageFormatId()), reinterpret_cast<unsigned char*>(b + 1),
              static_cast<int>(bytes));
    return bmp;
}

// 0x10048b10
const Bitmap* __cdecl TextureResource::GetMip(unsigned int level, unsigned int face, const Size& size,
                                              TextureInfo* info)
{
    if (!info->blocks) return nullptr;
    unsigned int* const b = info->blocks[info->depth * level + face];
    if (!b) return nullptr;
    auto* const bmp = new (Alloc::New(0x44)) Bitmap();
    bmp->Init(size, 0, BitmapFormat(info->imageFormat), reinterpret_cast<unsigned char*>(b + 1),
              static_cast<int>(*b));
    return bmp;
}

// 0x10001440: a copy of the mip that owns its pixels
const Bitmap* __cdecl TextureResource::GetBitmap(unsigned int level, unsigned int face, const Size& size) const
{
    GetMipLock();
    const Bitmap* const b = GetMip(level, face, size);
    b->DetachAlias();
    GetMipUnlock();
    return b;
}

// 0x10001490
unsigned char* __cdecl TextureResource::GetData(unsigned int& size, unsigned int level, unsigned int face) const
{
    unsigned int* const b = info->blocks[GetDepth() * level + face];
    size = *b;
    return reinterpret_cast<unsigned char*>(b + 1);
}

// ---------------------------------------------------------------------------------------------------------------
// loading

// 0x10047380: the INFO block into `info` and the levels first..last (counted from the smallest, clamped to the
// file's) into its blocks; the others are skipped
TextureResource::Result __cdecl TextureResource::Load(const ResourceDatabase* db, const char* name, const String& key,
                                                      int& first, int& last, TextureInfo* info)
{
    Result result(0);
    JIRFFile file;
    bool failed;
    {
        PString base(name);
        PString path = db->GetFileName(base);
        failed = file.OpenJIRF(path.Text(), key, 0x10).code != 0;
    }
    if (failed || file.ReadHeader(kMMTF).code != 0) return Result(1);
    unsigned int blockSize = 0x60;
    unsigned int block[0x18] = {};
    if (file.ReadBlock(kINFO, blockSize, block).code != 0) return Result(1);
    unsigned int* const out = reinterpret_cast<unsigned int*>(info);
    auto copy = [&](int from, int to) {
        for (int k = from; k <= to; ++k) out[k] = block[k];
    };
    auto tail = [&](float anisotropy) {
        std::memcpy(&info->anisotropy, &anisotropy, 4);
        info->autoMip = AUTO_MIP_DEFAULT;
        info->compression[0] = 0;
        info->normalMapHint = NORMAL_MAP_HINT_DEFAULT;
    };
    float blockAnisotropy;
    std::memcpy(&blockAnisotropy, &block[0xf], 4);
    switch (block[0]) {
    case 0x102:
        out[2] = block[2];
        copy(4, 0xe);
        info->version = 0x108;
        info->dimension = static_cast<TextureDimension>(0);
        info->depth = 1;
        tail(ANISOTROPY_DEFAULT_TOKEN_VALUE);
        break;
    case 0x103:
        copy(1, 0xe);
        info->version = 0x105;
        tail(ANISOTROPY_DEFAULT_TOKEN_VALUE);
        break;
    case 0x104:
        copy(1, 0xe);
        info->version = 0x108;
        tail(blockAnisotropy);
        break;
    case 0x105:
        copy(1, 0x10);
        info->version = 0x108;
        info->compression[0] = 0;
        info->normalMapHint = NORMAL_MAP_HINT_DEFAULT;
        break;
    case 0x106:
        copy(1, 0x12);
        info->version = 0x108;
        info->normalMapHint = NORMAL_MAP_HINT_DEFAULT;
        break;
    case 0x107:
        copy(1, 0x12);
        info->version = 0x108;
        info->normalMapHint = NORMAL_MAP_HINT_DEFAULT;
        out[0x14] = out[0x15] = out[0x16] = out[0x17] = 0;
        break;
    case 0x108:
        copy(0, 0x17);
        break;
    default:
        return Result(1);
    }
    const unsigned int levels = info->mipLevels, depth = info->depth;
    if ((info->dimension == 1 && depth < 6) || !levels || !depth) return Result(1);
    info->levelsOnDisk = static_cast<int>(levels);
    info->blocks = static_cast<unsigned int**>(Alloc::New(Times(depth * levels, 4)));
    std::memset(info->blocks, 0, info->depth * info->mipLevels * 4);
    const int top = static_cast<int>(info->mipLevels) - 1;
    if (first < 0)
        first = 0;
    else if (first > top)
        first = top;
    if (last < first)
        last = first;
    else if (last > top)
        last = top;
    const bool single = info->mipLevels == 1;
    Compress* compressor = nullptr;
    SharedScratchPad* pad = nullptr;
    if (info->compression[0]) {
        PStringCache::Node* const node = AnsiNode(info->compression);
        PString compressionName;
        compressionName.node = node;
        compressor = Compress::CreateInstance(compressionName);
        if (!compressor) return Result(1);
        pad = SharedScratchPad::GetAccess();
    }
    for (int i = 0; i <= last;) {
        const bool skip = !single && i < first;
        unsigned long id;
        unsigned int size;
        if (file.ReadBlockHeader(id, size).code != 0) return Result(1);
        if (id != kMMAP) {
            if (!size || file.SeekCur(static_cast<long>(size)) == 0xffffffffu) return Result(1);
            continue;
        }
        if (skip) {
            file.SeekCur(static_cast<long>(size));
            ++i;
            continue;
        }
        unsigned int** const level = info->blocks + (levels - 1 - i) * depth;
        if (!compressor) {
            const unsigned int each = size / depth;
            for (unsigned int f = 0; f < depth; ++f) {
                level[f] = static_cast<unsigned int*>(Alloc::New(each + 4));
                *level[f] = each;
                file.Read(level[f] + 1, each);
            }
        } else {
            unsigned long unpacked;
            file.Read(&unpacked, 4);
            size -= 4;
            unsigned char* const buffer = pad->GetBuffer(unpacked + size);
            unsigned char* const to = buffer + size;
            file.Read(buffer, size);
            const unsigned int each = compressor->DecompressBlock(to, unpacked, buffer, size) / depth;
            const unsigned char* from = to;
            for (unsigned int f = 0; f < depth; ++f) {
                level[f] = static_cast<unsigned int*>(Alloc::New(each + 4));
                *level[f] = each;
                std::memcpy(level[f] + 1, from, each);
                from += each;
            }
        }
        ++i;
    }
    if (compressor) {
        Ref(compressor)->ReleaseReference();
        pad->ReleaseReference();
    }
    return result;
}

// 0x10047160: up to TextureManager::MIN_STREAMING_TEXTURE_MIP_COUNT levels (the smallest), kept largest first
TextureResource::Result __cdecl TextureResource::Load(const char* name)
{
    TextureInfo loaded;
    int first = 0, last = TextureManager::MIN_STREAMING_TEXTURE_MIP_COUNT - 1;
    const Result r = Load(Owner(), name, ScrambleKey(), first, last, &loaded);
    if (r.code != 0) {
        loaded.FreeBlocks();
        return r;
    }
    DeleteInfo(info);
    info = static_cast<TextureInfo*>(Alloc::New(0x68));
    if (info) info->blocks = nullptr;
    std::memcpy(info, &loaded, 0x60);
    info->levelsOnDisk = loaded.levelsOnDisk;
    info->mipLevels = static_cast<unsigned int>(last + 1);
    const unsigned int depth = loaded.depth;
    info->FreeBlocks();
    info->blocks = static_cast<unsigned int**>(Alloc::New(Times(static_cast<unsigned int>(last + 1) * depth, 4)));
    std::memset(info->blocks, 0, static_cast<unsigned int>(last + 1) * depth * 4);
    int to = last;
    for (int i = first; i <= last; ++i, --to) {
        for (unsigned int f = 0; f < depth; ++f) {
            unsigned int** const dst = info->blocks ? info->blocks + info->depth * to + f : nullptr;
            unsigned int** const src = loaded.blocks ? loaded.blocks + (loaded.mipLevels - i - 1) * depth + f : nullptr;
            *dst = *src;
            *src = nullptr;
        }
    }
    dataUnloaded = false;
    loaded.FreeBlocks();
    return Result(0);
}

// 0x10048cd0: the stored levels between first and last (and within the texture's level count) into the API
// texture, smallest first
TextureResource::Result __cdecl TextureResource::LoadMipLevels(TextureInfo* info, DisplayTexture* texture, int first,
                                                               int last)
{
    const DisplayFormat format(info->imageFormat);
    int stored, storedLast;
    StoredLevels(info, stored, storedLast);
    const int levels = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(texture) + 0x20);
    const int top = levels - 1;
    int end = storedLast;
    if (top <= end) end = top;
    int low = top > stored ? stored : top;
    int from = low >= first ? low : first;
    if (from > end) from = end;
    if (low < last) low = last;
    if (low <= end) end = low;
    const int depth = static_cast<int>(info->depth);
    int to = levels - from - 1;
    for (; from <= end; ++from, --to) {
        const Size size = GetMipSizeAtReverseLevel(from, info->size.w, info->size.h);
        const unsigned int source = info->mipLevels - from - 1;
        for (int f = 0; f < depth; ++f) {
            const Bitmap* const b = GetMip(source, f, size, info);
            if (!b) return Result(1);
            const bool ok = VCall<bool>(texture, 0xc, to, &format, b, f);
            DeleteBitmap(b);
            if (!ok) return Result(1);
        }
    }
    return Result(0);
}

// 0x10047c80: the API texture made (slot 1) at a size within TextureManager::MAX_STREAMING_TEXTURE_SIZE, then
// every face of every level not skipped (slot 3); one level only when the texture makes its own mips
TextureResource::Result __cdecl TextureResource::Load(DisplayTexture* texture) const
{
    const DisplayFormat format(GetDisplayFormatId());
    Size size = GetSize();
    const DisplayFormat imageFormat(GetImageFormatId());
    unsigned int levels = GetNumMipLevels();
    unsigned int skip = 0;
    if (GetNumMipLevels() > 1) {
        const int n = static_cast<int>(GetNumMipLevels()) - 1;
        const Size s = GetSize();
        size = GetMipSizeAtReverseLevel(n, s.w, s.h);
    }
    const int limit = TextureManager::MAX_STREAMING_TEXTURE_SIZE * TextureManager::MAX_STREAMING_TEXTURE_SIZE;
    while (size.h * size.w > limit && levels > 1) {
        ++skip;
        size.h >>= 1;
        size.w >>= 1;
        --levels;
    }
    TextureFilter minFilter = GetMinFilter();
    if (minFilter == 2) minFilter = MIN_FILTER_DEFAULT;
    TextureFilter magFilter = GetMagFilter();
    if (magFilter == 2) magFilter = MAG_FILTER_DEFAULT;
    TextureFilter mipFilter = GetMipFilter();
    if (mipFilter == 2) mipFilter = MIP_FILTER_DEFAULT;
    TextureAutoMip autoMip = GetAutoMipHint();
    if (info && info->mipLevels > 1) autoMip = static_cast<TextureAutoMip>(0);
    struct Args {  // what the texture hands back (a referenced object first)
        const ReferenceCount* object;
        int a, b, c, d;
    } args = {nullptr, 0, 0, 0, 0};
    const TextureWrap wrapT = GetWrapT();
    const TextureWrap wrapS = GetWrapS();
    const TextureHint hint = GetHint();
    const float anisotropy = GetAnisotropyFilterLevel();
    const unsigned int depth = GetDepth();
    const TextureDimension dimension = GetDimension();
    const bool made = VCall<bool>(texture, 4, &size, &format, dimension, levels, autoMip, depth, anisotropy, hint,
                                  minFilter, magFilter, mipFilter, wrapS, wrapT, &args);
    if (args.object) args.object->ReleaseReference();
    if (!made) return Result(1);
    const bool ownMips = *(reinterpret_cast<const unsigned char*>(texture) + 0x30) != 0;
    const unsigned int count = GetNumMipLevels();
    const unsigned int faces = GetDepth();
    if (!GetMipLock()) return Result(1);
    int target = -static_cast<int>(skip);
    for (unsigned int level = 0; level < count; ++level, ++target) {
        if (level < skip) continue;
        const Size current = size;
        if (size.w > 1) size.w >>= 1;
        if (size.h > 1) size.h >>= 1;
        for (unsigned int f = 0; f < faces; ++f) {
            const Bitmap* const b = GetMip(level, f, current);
            if (!b) {
                GetMipUnlock();
                return Result(1);
            }
            const bool ok = VCall<bool>(texture, 0xc, target, &imageFormat, b, f);
            DeleteBitmap(b);
            if (!ok) {
                GetMipUnlock();
                return Result(1);
            }
        }
        if (ownMips) break;
    }
    GetMipUnlock();
    return Result(0);
}

// ---------------------------------------------------------------------------------------------------------------
// saving

// 0x10048080: a new JIRF file ('MMTF') with the texture in it
bool __cdecl TextureResource::Save(const char* path, Display* display) const
{
    JIRFFile file;
    const Size size = GetSize();
    if (size.w < 1 || size.h < 1) return false;
    bool failed;
    {
        String key;
        failed = file.OpenJIRF(path, key, 0x101).code != 0;
    }
    if (failed) return false;
    bool ok;
    {
        JIRFWriteHeader header(file, kMMTF);
        ok = Save(file, display);
        if (!ok) return false;
    }
    file.Close();
    return true;
}

// 0x10048200
bool __cdecl TextureResource::Save(JIRFFile& file, Display* display) const
{
    if (!GetMipLock()) return false;
    const bool ok = SaveLockedInternal(file, display);
    GetMipUnlock();
    return ok;
}

// 0x10048250: the INFO block (version 0x108, from the getters), then the levels
bool TextureResource::SaveLockedInternal(JIRFFile& file, Display* display) const
{
    const Size size = GetSize();
    const unsigned int levels = GetNumMipLevels();
    bool ok;
    {
        JIRFWriteBlock block(file, kINFO);
        TextureInfo out;
        std::memset(&out, 0, 0x60);
        out.version = 0x108;
        out.dimension = GetDimension();
        out.imageFormat = GetDisplayFormatId();
        out.mipLevels = levels;
        out.depth = GetDepth();
        out.size = size;
        out.hint = GetHint();
        out.wrapS = GetWrapS();
        out.wrapT = GetWrapT();
        out.minFilter = GetMinFilter();
        out.magFilter = GetMagFilter();
        out.mipFilter = GetMipFilter();
        out.alphaHint = GetAlphaHint();
        out.anisotropy = GetAnisotropyFilterLevel();
        out.autoMip = GetAutoMipHint();
        GetFileCompression(out.compression);
        out.normalMapHint = GetNormalMapHint();
        out.averageColor = GetAverageColor();
        file.Write(&out, 0x60);
        out.blocks = nullptr;
    }
    ok = SaveMipMap(file, 0, static_cast<int>(levels), size, size, display);
    return ok;
}

// 0x100484a0: the smaller levels first (recursing), then this level's faces as one MMAP block: converted by the
// display (slot 0x148) when the display format is a compressed one other than the image's, compressed as a whole
// when a compression is named
bool __cdecl TextureResource::SaveMipMap(JIRFFile& file, int level, int levels, const Size& size, const Size& top,
                                         Display* display) const
{
    if (level < levels - 1) {
        Size half = size;
        if (half.w > 1) half.w >>= 1;
        if (half.h > 1) half.h >>= 1;
        if (!SaveMipMap(file, level + 1, levels, half, top, display)) return false;
    }
    JIRFWriteBlock block(file, kMMAP);
    const DFormatId image = GetImageFormatId();
    const DFormatId target = GetDisplayFormatId();
    char compression[8];
    GetFileCompression(compression);
    const bool convert = image != target && Compressed(target);
    Compress* compressor = nullptr;
    SharedScratchPad* pad = nullptr;
    unsigned char* start = nullptr;
    unsigned char* write = nullptr;
    unsigned char* packed = nullptr;
    unsigned int capacity = 0;
    if (compression[0]) {
        PString compressionName;
        compressionName.node = AnsiNode(compression);
        compressor = Compress::CreateInstance(compressionName);
        if (!compressor) return false;
        pad = SharedScratchPad::GetAccess();
        const DisplayFormat format(target);
        capacity = (format.bits >> 3) * size.h * size.w;
        start = pad->GetBuffer(capacity * 2);
        packed = start + capacity;
        write = start;
    }
    const unsigned int faces = GetDepth();
    for (unsigned int f = 0; f < faces; ++f) {
        const Bitmap* const b = GetMip(static_cast<unsigned int>(level), f, size);
        if (!b) return false;
        const Bitmap* data = b;
        Bitmap* converted = nullptr;
        if (convert) {
            if (!display || !VCall<bool>(display, 0x148, &converted, b, target, image) || !converted) {
                DeleteBitmap(b);
                return false;
            }
            data = converted;
        }
        const unsigned int bytes = data->dataSize;
        if (compressor) {
            std::memcpy(write, data->GetPtr(), bytes);
            write += bytes;
        } else if (file.Write(data->GetPtr(), bytes) != bytes) {
            if (converted) {
                DeleteBitmap(converted);  // (the source bitmap is left, as in TS12)
                return false;
            }
            DeleteBitmap(b);
            return false;
        }
        if (converted) DeleteBitmap(converted);
        DeleteBitmap(b);
    }
    if (compressor) {
        unsigned long length = static_cast<unsigned long>(write - start);
        const unsigned long n = compressor->CompressBlock(packed, capacity, start, length, GetFileCompressionRatio());
        file.Write(&length, 4);
        if (file.Write(packed, n) != n) return false;  // (the compressor stays referenced, as in TS12)
        Ref(compressor)->ReleaseReference();
        pad->ReleaseReference();
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// BinaryTextureResourceExport

// 0x10008170
BinaryTextureResourceExport::BinaryTextureResourceExport(const PString& name)
    : TextureResource(name, nullptr), imageFormat(static_cast<DFormatId>(0)), displayFormat(static_cast<DFormatId>(0)),
      bitmap(nullptr), mipLevels(1), depth(1), ratio(5), anisotropy(ANISOTROPY_DEFAULT_TOKEN_VALUE),
      dimension(static_cast<TextureDimension>(0)), hint(static_cast<TextureHint>(0)), wrapS(static_cast<TextureWrap>(1)),
      wrapT(static_cast<TextureWrap>(1)), minFilter(static_cast<TextureFilter>(1)), magFilter(static_cast<TextureFilter>(1)),
      mipFilter(static_cast<TextureFilter>(1)), alphaHint(static_cast<TextureAlpha>(2)),
      autoMip(static_cast<TextureAutoMip>(3)), compression("")
{
}

// 0x10007d50
BinaryTextureResourceExport::BinaryTextureResourceExport(const BinaryTextureResourceExport& o)
    : TextureResource(o), imageFormat(o.imageFormat), displayFormat(o.displayFormat), bitmap(o.bitmap),
      mipLevels(o.mipLevels), depth(o.depth), ratio(o.ratio), anisotropy(o.anisotropy), dimension(o.dimension),
      hint(o.hint), wrapS(o.wrapS), wrapT(o.wrapT), minFilter(o.minFilter), magFilter(o.magFilter),
      mipFilter(o.mipFilter), alphaHint(o.alphaHint), autoMip(o.autoMip), compression(o.compression)
{
}

// 0x10007e70
BinaryTextureResourceExport& BinaryTextureResourceExport::operator=(const BinaryTextureResourceExport& o)
{
    TextureResource::operator=(o);
    imageFormat = o.imageFormat;
    displayFormat = o.displayFormat;
    bitmap = o.bitmap;
    mipLevels = o.mipLevels;
    depth = o.depth;
    ratio = o.ratio;
    anisotropy = o.anisotropy;
    dimension = o.dimension;
    hint = o.hint;
    wrapS = o.wrapS;
    wrapT = o.wrapT;
    minFilter = o.minFilter;
    magFilter = o.magFilter;
    mipFilter = o.mipFilter;
    alphaHint = o.alphaHint;
    autoMip = o.autoMip;
    compression = o.compression;
    return *this;
}

BinaryTextureResourceExport::~BinaryTextureResourceExport() {}  // 0x10007cc0

bool __cdecl BinaryTextureResourceExport::GetMustResetWhenFinished() const { return false; }         // 0x10009500
NormalMapHint __cdecl BinaryTextureResourceExport::GetNormalMapHint() const { return static_cast<NormalMapHint>(0); }  // 0x1001dec0
TextureAlpha __cdecl BinaryTextureResourceExport::GetAlphaHint() const { return alphaHint; }          // 0x10008010
TextureDimension __cdecl BinaryTextureResourceExport::GetDimension() const { return dimension; }      // 0x1000d230
unsigned int __cdecl BinaryTextureResourceExport::GetDepth() const { return depth; }                  // 0x10007fd0
float __cdecl BinaryTextureResourceExport::GetAnisotropyFilterLevel() const { return anisotropy; }    // 0x10008020
unsigned int __cdecl BinaryTextureResourceExport::GetFileCompressionRatio() const { return ratio; }   // 0x1000c740
DFormatId __cdecl BinaryTextureResourceExport::GetImageFormatId() const { return imageFormat; }       // 0x10007fc0
DFormatId __cdecl BinaryTextureResourceExport::GetDisplayFormatId() const { return displayFormat; }   // 0x10007fb0
TextureHint __cdecl BinaryTextureResourceExport::GetHint() const { return hint; }                     // 0x10007fe0
TextureWrap __cdecl BinaryTextureResourceExport::GetWrapS() const { return wrapS; }                   // 0x10007ff0
TextureWrap __cdecl BinaryTextureResourceExport::GetWrapT() const { return wrapT; }                   // 0x1000c1c0
TextureFilter __cdecl BinaryTextureResourceExport::GetMinFilter() const { return minFilter; }         // 0x1000d2e0
TextureFilter __cdecl BinaryTextureResourceExport::GetMagFilter() const { return magFilter; }         // 0x1000d2f0
TextureFilter __cdecl BinaryTextureResourceExport::GetMipFilter() const { return mipFilter; }         // 0x10008000
unsigned int __cdecl BinaryTextureResourceExport::GetNumMipLevels() const { return mipLevels; }       // 0x10007fa0
TextureAutoMip __cdecl BinaryTextureResourceExport::GetAutoMipHint() const { return autoMip; }        // 0x10008030
Size __cdecl BinaryTextureResourceExport::GetSize() const { return Size{bitmap->width, bitmap->height}; }  // 0x10008270

// 0x10008040
void __cdecl BinaryTextureResourceExport::GetFileCompression(char* const out) const
{
    std::strncpy(out, compression, 8);
}

// 0x10008290: the bitmap at `size` (any level, any face): an unspecified-format bitmap copied, else scaled
const Bitmap* __cdecl BinaryTextureResourceExport::GetMip(unsigned int, unsigned int, const Size& size) const
{
    if (bitmap->GetFormat() != Bitmap::UNSPECIFIED) return bitmap->Scale(size.w, size.h);
    const DisplayFormat format(imageFormat);
    const unsigned int bytes = (format.bits >> 3) * size.h * size.w;
    auto* const b = new (Alloc::New(0x44)) Bitmap();
    b->Init(size, 0, Bitmap::UNSPECIFIED, nullptr, static_cast<int>(bytes));
    std::memcpy(b->GetPtr(), bitmap->GetPtr(), bytes);
    return b;
}

void __cdecl BinaryTextureResourceExport::SetNumMipLevels(unsigned int n) { mipLevels = n; }           // 0x10008070
void __cdecl BinaryTextureResourceExport::SetDimension(TextureDimension d) { dimension = d; }          // 0x10008080
void __cdecl BinaryTextureResourceExport::SetDisplayFormatId(DFormatId f) { displayFormat = f; }       // 0x10008090
void __cdecl BinaryTextureResourceExport::SetImageFormatId(DFormatId f) { imageFormat = f; }           // 0x100080a0
void __cdecl BinaryTextureResourceExport::SetDepth(unsigned int d) { depth = d; }                      // 0x100080b0
void __cdecl BinaryTextureResourceExport::SetHint(TextureHint h) { hint = h; }                         // 0x10055e80
void __cdecl BinaryTextureResourceExport::SetWrapS(TextureWrap w) { wrapS = w; }                       // 0x100080c0
void __cdecl BinaryTextureResourceExport::SetWrapT(TextureWrap w) { wrapT = w; }                       // 0x100080d0
void __cdecl BinaryTextureResourceExport::SetMinFilter(TextureFilter f) { minFilter = f; }             // 0x100080e0
void __cdecl BinaryTextureResourceExport::SetMagFilter(TextureFilter f) { magFilter = f; }             // 0x100080f0
void __cdecl BinaryTextureResourceExport::SetMipFilter(TextureFilter f) { mipFilter = f; }             // 0x10008100
void __cdecl BinaryTextureResourceExport::SetAlphaHint(TextureAlpha a) { alphaHint = a; }              // 0x10008110
void __cdecl BinaryTextureResourceExport::SetAnisotropyFilterLevel(float a) { anisotropy = a; }        // 0x10008120
void __cdecl BinaryTextureResourceExport::SetAutoMipHint(TextureAutoMip m) { autoMip = m; }            // 0x10008130
void __cdecl BinaryTextureResourceExport::SetFileCompression(const AnsiString& name) { compression = name; }  // 0x10008140
void __cdecl BinaryTextureResourceExport::SetFileCompressionRatio(unsigned int r) { ratio = r; }       // 0x10008160

// 0x10008250
void __cdecl BinaryTextureResourceExport::SetData(Bitmap* b, DFormatId format)
{
    bitmap = b;
    imageFormat = format;
    displayFormat = format;
}

}  // namespace Jet
