// Unit U6b of RenderSystem_7c: AsciiTextureResource (0x10001610..0x10001e36, 0x10004ed0..0x10007b58, folded
// getters at 0x10008030 / 0x10009500 / 0x1001dec0, the key PStrings 0x100b8134..0x100b8170 of the initializer
// 0x1005dbd0). A texture made of bitmap resources named by a .texture.txt. Behaviour follows the TS12 assembly,
// quirks included (noted per function): Save's roll-back unlocks by the wrong index, IsAllLoaded reads the primary
// bitmap's flag for the alpha one, GetImage reads one past the alpha list, the first row of GetImage's merge loops
// runs once even for a zero width. noPath stays the original's static (RenderKernel, U9).
#include <jet/Config.h>
#include <jet/KernelResources.h>
#include <jet/Texture.h>

#include <cstdint>
#include <cstring>
#include <new>

struct EnumCheck {
    const char* name;
    int value;
};

namespace Jet {

namespace {

// the Bitmap fields the original reads inline
struct BitmapBits {
    int width, height;
    void* palette;
    unsigned char* data;
    int pitch;
    int bytesPerPixel;
    int format;
};
const BitmapBits& Bits(const Bitmap* b) { return *reinterpret_cast<const BitmapBits*>(b); }
const Size& SizeOf(const Bitmap* b) { return *reinterpret_cast<const Size*>(b); }
std::uint32_t Px(const unsigned char* p) { return *reinterpret_cast<const std::uint32_t*>(p); }

bool Ok(const Resource::Result& r) { return r.code == 0; }
const Bitmap* BitmapOf(const BitmapResource* r) { return static_cast<const BitmapBaseResource*>(r)->GetBitmap(); }

void DeleteBitmap(Bitmap* b)
{
    b->~Bitmap();
    Alloc::Delete(b);
}

// a view of `src`'s pixels (Init with the source's data, not owned)
Bitmap* View(const Bitmap* src, int format)
{
    void* const mem = Alloc::New(0x44);
    Bitmap* const b = mem ? ::new (mem) Bitmap() : nullptr;
    b->Init(SizeOf(src), 0, static_cast<Bitmap::Format>(format), Bits(src).data, 0);
    return b;
}

Bitmap* NewBitmap(const Size& size, int format)
{
    void* const mem = Alloc::New(0x44);
    return mem ? ::new (mem) Bitmap(size, static_cast<Bitmap::Format>(format), nullptr) : nullptr;
}

// the key names (0x100b8134..0x100b8170)
const PString kTexture("Texture");
const PString kPrimary("Primary");
const PString kAlpha("Alpha");
const PString kDimension("Dimension");
const PString kDepth("Depth");
const PString kCompression("Compression");
const PString kHint("Hint");
const PString kNormalMapHint("NormalMapHint");
const PString kModifyMap("ModifyMap");
const PString kAlphaHint("AlphaHint");
const PString kAnisotropy("Anisotropy");
const PString kAutoMip("AutoMip");
const PString kMagFilter("MagFilter");
const PString kMinFilter("MinFilter");
const PString kMipFilter("MipFilter");
const PString kTile("Tile");

// 0x1006eab0..0x1006eb98
const EnumCheck kDimensions[4] = {{"two", 0}, {"cube", 1}, {"volume", 2}, {"one", 3}};
const EnumCheck kCompressions[8] = {{"none", 0}, {"dxt1", 1}, {"dxt3", 2}, {"dxt5", 3},
                                    {"atc", 4},  {"atce", 5}, {"atci", 6}, {"etc1", 9}};
const EnumCheck kFilters[3] = {{"nearest", 0}, {"linear", 1}, {"default", 2}};
const EnumCheck kNormalMaps[2] = {{"none", 0}, {"normalmap", 1}};
const EnumCheck kModifyMaps[2] = {{"none", 0}, {"flipgreen", 1}};
const EnumCheck kAlphas[4] = {{"opaque", 0}, {"semitransparent", 1}, {"masked", 2}, {"default", 3}};
const EnumCheck kHints[2] = {{"static", 0}, {"dynamic", 1}};
const EnumCheck kAutoMips[4] = {{"none", 0}, {"default", 1}, {"fastest", 2}, {"nicest", 3}};

// 0x10007b60: a PString against a name, ignoring case (a null PString equals only a null name)
bool Is(const PString& s, const char* name)
{
    if (!s.node) return name == nullptr;
    if (!name) return false;
    return _stricmp(s.Text(), name) == 0;
}

// the first character of a String's own block (the original reads it raw, also when the string is empty)
char FirstChar(const String& s) { return (*reinterpret_cast<const char* const*>(&s))[8]; }

// DisplayTexture::GetNumMipLevels(Size) (0x10004950, U6c): the levels down to 1x1 of the larger side
int NumMipLevels(const Size& s)
{
    int m = s.h;
    if (s.w >= m) m = s.w;
    if (m <= 1) return 1;
    int n = 0;
    for (int v = m / 2; v; v /= 2) ++n;
    return n + 1;
}

// IsAllLoaded's write-only counter (0x100b8c74)
unsigned int pendingLoads;

// UpdateAverageColor's x87 sums (exact whatever the FPU precision, like the original)
#if defined(_M_IX86)
const double k255 = 255.0;
void AddRow(double* acc, int v)
{
    __asm {
        fild v
        mov eax, acc
        fadd qword ptr [eax]
        fstp qword ptr [eax]
    }
}
void Average(int n, const double* sums, float* dst)
{
    __asm {
        mov ecx, sums
        mov edx, dst
        fild n
        fmul qword ptr k255
        fld1
        fdivrp st(1), st
        fld qword ptr [ecx]
        fmul st, st(1)
        fld st(1)
        fmul qword ptr [ecx + 8]
        fxch st(2)
        fmul qword ptr [ecx + 16]
        fxch st(1)
        fstp dword ptr [edx]
        fxch st(1)
        fstp dword ptr [edx + 4]
        fstp dword ptr [edx + 8]
    }
}
#else
void AddRow(double* acc, int v) { *acc = static_cast<double>(v) + *acc; }
void Average(int n, const double* sums, float* dst)
{
    const double r = 1.0 / (static_cast<double>(n) * 255.0);
    dst[0] = static_cast<float>(sums[0] * r);
    dst[1] = static_cast<float>(r * sums[1]);
    dst[2] = static_cast<float>(r * sums[2]);
}
#endif

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// construction

// 0x10001a60
AsciiTextureResource::AsciiTextureResource(const PString& name, const ResourceDatabase* owner)
    : TextureResource(name, owner)
{
    wrapS = static_cast<char>(GetDefaultWrapS());
    wrapT = static_cast<char>(GetDefaultWrapT());
    mipFilter = 2;
    alphaHint = -1;
    minFilter = 2;
    magFilter = 2;
    hint = static_cast<char>(GetDefaultTextureHint());
    compression = 7;
    autoMip = static_cast<char>(GetDefaultAutoMip());
    dimension = 0;
    useMipMaps = true;
    depth = 1;
    anisotropy = ANISOTROPY_DEFAULT_TOKEN_VALUE;
    normalMapHint = static_cast<NormalMapHint>(0);
    modifyMap = static_cast<ModifyMap>(0);
    averageColor.r = averageColor.g = averageColor.b = averageColor.a = 0.0f;
    locked = false;
    lockedSize.w = lockedSize.h = 0;
    lockedMips = 0;
    GetDefaultFileCompression(fileCompression);
}

// the fields both copies take (not the padding)
void AsciiTextureResource::CopyFields(const AsciiTextureResource& s)
{
    std::memcpy(&wrapS, &s.wrapS, 9);  // +0x8c..+0x94
    useMipMaps = s.useMipMaps;
    dimension = s.dimension;
    std::memcpy(&depth, &s.depth, 0xc8 - 0xa0);  // depth .. averageColor
    locked = s.locked;
    lockedSize = s.lockedSize;
    lockedMips = s.lockedMips;
}

// 0x10001610 (the info pointer and the bitmap lists are shared/copied as TS12 does: no references taken)
AsciiTextureResource::AsciiTextureResource(const AsciiTextureResource& o)
    : TextureResource(o), ResourceObserver(), primary(o.primary), alpha(o.alpha), images(o.images)
{
    CopyFields(o);
}

// 0x10001820
AsciiTextureResource& AsciiTextureResource::operator=(const AsciiTextureResource& o)
{
    TextureResource::operator=(o);
    primary = o.primary;
    alpha = o.alpha;
    images = o.images;
    CopyFields(o);
    return *this;
}

// 0x10001cc0
AsciiTextureResource::~AsciiTextureResource()
{
    for (unsigned int i = 0; i < primary.Count(); ++i) {
        if (primary[i]) {
            primary[i]->RemoveObserver(this);
            primary[i]->ReleaseReference();
        }
    }
    for (unsigned int i = 0; i < alpha.Count(); ++i) {
        if (alpha[i]) {
            alpha[i]->RemoveObserver(this);
            alpha[i]->ReleaseReference();
        }
    }
    if (images.Count()) AsciiTextureResource::GetMipUnlock();
}

// ---------------------------------------------------------------------------------------------------------------
// the fields

TextureHint __cdecl AsciiTextureResource::GetHint() const { return static_cast<TextureHint>(hint); }               // 0x10001ba0
TextureWrap __cdecl AsciiTextureResource::GetWrapS() const { return static_cast<TextureWrap>(wrapS); }             // 0x10001bb0
TextureWrap __cdecl AsciiTextureResource::GetWrapT() const { return static_cast<TextureWrap>(wrapT); }             // 0x10001bc0
TextureFilter __cdecl AsciiTextureResource::GetMinFilter() const { return static_cast<TextureFilter>(minFilter); }  // 0x10001bd0
TextureFilter __cdecl AsciiTextureResource::GetMagFilter() const { return static_cast<TextureFilter>(magFilter); }  // 0x10001be0
TextureFilter __cdecl AsciiTextureResource::GetMipFilter() const { return static_cast<TextureFilter>(mipFilter); }  // 0x10001bf0
TextureDimension __cdecl AsciiTextureResource::GetDimension() const                                                 // 0x10001c00
{
    return static_cast<TextureDimension>(dimension);
}
TextureAutoMip __cdecl AsciiTextureResource::GetAutoMipHint() const { return static_cast<TextureAutoMip>(autoMip); }  // 0x10001c10
bool __cdecl AsciiTextureResource::UseMipMaps() const { return useMipMaps; }                                         // 0x10001c20
NormalMapHint __cdecl AsciiTextureResource::GetNormalMapHint() const { return normalMapHint; }                       // 0x10001c30
ModifyMap __cdecl AsciiTextureResource::GetModifyMap() const { return modifyMap; }                                   // 0x10001c40
TextureAlpha __cdecl AsciiTextureResource::GetAlphaHint() const { return static_cast<TextureAlpha>(alphaHint); }     // 0x10001c50
Color __cdecl AsciiTextureResource::GetAverageColor() const { return averageColor; }                                 // 0x10001c60
float __cdecl AsciiTextureResource::GetAnisotropyFilterLevel() const { return anisotropy; }                          // 0x10001c90
void __cdecl AsciiTextureResource::GetFileCompression(char* const out) const                                        // 0x10001ca0
{
    std::strncpy(out, fileCompression, 8);
}
unsigned int __cdecl AsciiTextureResource::GetDepth() const { return depth; }               // 0x10008030 (folded)
bool __cdecl AsciiTextureResource::GetMustResetWhenFinished() const { return false; }       // 0x10009500 (folded)
unsigned int __cdecl AsciiTextureResource::CalculateSize() const { return 0xd8; }          // 0x10001e30
unsigned int __cdecl AsciiTextureResource::CalculateDataSize() const { return 0; }         // 0x1001dec0 (folded)
void __cdecl AsciiTextureResource::SetUsePath(bool on) { noPath = on; }                     // 0x10004ed0

// ---------------------------------------------------------------------------------------------------------------
// the bitmap resources

// 0x100051b0: every bitmap the texture uses is available (cube and volume textures: all faces, else the first)
bool __cdecl AsciiTextureResource::AllValidBitmapsAvailable() const
{
    const unsigned int na = alpha.Count();
    if (na && na != primary.Count()) return false;
    if (dimension == 1 || dimension == 2) {
        for (unsigned int i = 0; i < primary.Count(); ++i) {
            if (primary[i] && primary[i]->status != STATUS_AVAILABLE) return false;
            if (na && alpha[i] && alpha[i]->status != STATUS_AVAILABLE) return false;
        }
    } else {
        if (primary.Count() && primary[0] && primary[0]->status != STATUS_AVAILABLE) return false;
        if (na && alpha[0] && alpha[0]->status != STATUS_AVAILABLE) return false;
    }
    return true;
}

// 0x10004ee0: the resource's own lock, then the bitmaps loaded through a lock and unlock each
Resource::Result __cdecl AsciiTextureResource::Lock() const
{
    Result r = Resource::Lock();
    if (r.code) return r;
    if (dataUnloaded) {
        r = const_cast<AsciiTextureResource*>(this)->Load(name.Text());
        if (r.code) return r;
    }
    if (!AllValidBitmapsAvailable()) {
        if (alpha.Count() && alpha.Count() != primary.Count()) {
            Resource::Unlock();
            return Result(1);
        }
        for (unsigned int i = 0; i < primary.Count(); ++i) {
            const BitmapResource* const p = primary[i];
            if (p && p->status != STATUS_AVAILABLE && Ok(p->Lock())) primary[i]->Unlock();
            if (alpha.Count()) {
                const BitmapResource* const a = alpha[i];
                if (a && a->status != STATUS_AVAILABLE && Ok(a->Lock())) alpha[i]->Unlock();
            }
        }
    }
    if (AllValidBitmapsAvailable()) return Result(0);
    Resource::Unlock();
    return Result(1);
}

bool __cdecl AsciiTextureResource::LoadAll() const { return Resource::LoadAll(); }  // 0x10005030

// 0x10005050 (the alpha bitmap's turn reads the primary's notified flag, as in TS12)
Resource::Result __cdecl AsciiTextureResource::IsAllLoaded() const
{
    Result r = Resource::IsAllLoaded();
    if (r.code) return r;
    if (alpha.Count() && alpha.Count() != primary.Count()) return Result(1);
    unsigned int n = 1;
    if (dimension == 1 || dimension == 2) n = primary.Count();
    if (n) {
        bool all = true;
        for (unsigned int i = 0; i < n; ++i) {
            if (primary[i]) {
                r = primary[i]->IsAllLoaded();
                if (r.code) return r;
                all = all && primary[i]->observersNotified;
            }
            if (alpha.Count() && alpha[i]) {
                r = alpha[i]->IsAllLoaded();
                if (r.code) return r;
                all = all && primary[i]->observersNotified;
            }
        }
        if (!all) return Result(0);
    }
    if (!observersNotified) {
        ++pendingLoads;
        return Result(1);
    }
    return Result(0);
}

// 0x10005190
bool __cdecl AsciiTextureResource::IsAvailable() const
{
    return AllValidBitmapsAvailable() && status == STATUS_AVAILABLE;
}

// 0x10006650: watch the bitmaps that are not available yet
Resource::Result __cdecl AsciiTextureResource::LoadComplete()
{
    if (alpha.Count() && alpha.Count() != primary.Count()) return Result(1);
    if (dimension == 1 || dimension == 2) {
        for (unsigned int i = 0; i < primary.Count(); ++i) {
            if (primary[i] && primary[i]->status != STATUS_AVAILABLE) primary[i]->AddObserver(this);
            if (alpha.Count() && alpha[i] && alpha[i]->status != STATUS_AVAILABLE) alpha[i]->AddObserver(this);
        }
    } else {
        if (primary.Count() && primary[0] && primary[0]->status != STATUS_AVAILABLE) primary[0]->AddObserver(this);
        if (alpha.Count() && alpha[0] && alpha[0]->status != STATUS_AVAILABLE) alpha[0]->AddObserver(this);
    }
    return Result(0);
}

// 0x10006750 (the observer: a bitmap has loaded)
void __cdecl AsciiTextureResource::OnLoad(const Resource*) const
{
    if (!AllValidBitmapsAvailable()) return;
    if (alphaHint == -1) UpdateAlphaHint();
    UpdateAverageColor();
    if (status == STATUS_AVAILABLE) OnLoadNotify();
}

// 0x100067a0
void __cdecl AsciiTextureResource::OnLoadNotify() const
{
    if (!AllValidBitmapsAvailable()) return;
    if (alphaHint == -1) UpdateAlphaHint();
    UpdateAverageColor();
    Resource::OnLoadNotify();
}

// ---------------------------------------------------------------------------------------------------------------
// the .texture.txt

// 0x10005260: the old "key=value" lines into config("Texture"); false when the file starts a "version" line (the
// ConfigData format) or names no primary/alpha bitmap
bool __cdecl AsciiTextureResource::ParseObsoleteTxtFormat(const char* name, ConfigData& config)
{
    File file;
    bool failed;
    {
        const PString key(name);
        const PString path = owner->GetFileName(key);
        failed = file.Open(path.Text(), 0x10, 0).code != 0;
    }
    if (failed) return false;
    ConfigData& texture = config(kTexture);
    bool found = false;
    char line[0x800] = {};
    while (!file.Eof()) {
        PString value;
        PString key;
        file.ReadLine(line, 0x800, 0);
        char* p = line;
        while (*p == '\t' || *p == ' ') ++p;
        char* q = p;
        while (*q && *q != '=') ++q;
        {
            const String s(p, static_cast<unsigned int>(q - p));
            key = PString(static_cast<const char*>(s));
        }
        if (key.node && AnsiString::FindIgnoreCase(key.Text(), key.Length(), "version", 0) == 0) {
            file.Close();
            return false;
        }
        if (*q != '=') continue;
        const char* const v = q + 1;
        const char* e = v;
        while (*e && *e != '\r' && *e != '\n') ++e;
        {
            const String s(v, static_cast<unsigned int>(e - v));
            value = PString(static_cast<const char*>(s));
        }
        const PString* k = &key;
        if (key.node && _stricmp(key.Text(), "primary") == 0) {
            k = &kPrimary;
            found = true;
        } else if (key.node && _stricmp(key.Text(), "alpha") == 0) {
            k = &kAlpha;
            found = true;
        } else if (key.node && _stricmp(key.Text(), "hint") == 0) {
            k = &kHint;
        } else if (key.node && _stricmp(key.Text(), "tile") == 0) {
            k = &kTile;
        } else if (key.node && _stricmp(key.Text(), "Compression") == 0) {
            k = &kCompression;
        } else if (Is(key, "NormalMapHint")) {
            k = &kNormalMapHint;
        } else if (Is(key, "ModifyMap")) {
            k = &kModifyMap;
        } else if (Is(key, "AlphaHint")) {
            k = &kAlphaHint;
        } else if (Is(key, "Anisotropy")) {
            k = &kAnisotropy;
        } else if (Is(key, "AutoMip")) {
            k = &kAutoMip;
        } else if (Is(key, "MagFilter")) {
            k = &kMagFilter;
        } else if (Is(key, "MinFilter")) {
            k = &kMinFilter;
        } else if (Is(key, "MipFilter")) {
            k = &kMipFilter;
        }
        texture(*k) << value;
    }
    file.Close();
    return found;
}

// 0x100058d0
Resource::Result __cdecl AsciiTextureResource::LoadTxt(const char* name)
{
    ConfigParser parser;
    ConfigData data;
    const ConfigData* config = &data;
    if (!ParseObsoleteTxtFormat(name, data)) {
        bool parsed;
        {
            const PString key(name);
            const PString path = owner->GetFileName(key);
            parsed = parser.Parse(path.Text()).code == 0;
        }
        if (!parsed) return Result(1);
        config = parser.RootPtr();
        if (&config->Find(kTexture) == &ConfigData::unknownItem || !config) return Result(1);
    }
    return LoadConfigData(*config) ? Result(0) : Result(1);
}

// 0x100064a0
bool __cdecl AsciiTextureResource::LookUpEnum(String& text, const EnumCheck* table, unsigned int count, int& value)
{
    text.ToLower();
    for (unsigned int i = 0; i < count; ++i) {
        if (text == table[i].name) {
            value = table[i].value;
            return true;
        }
    }
    return false;
}

// 0x10005fb0
bool __cdecl AsciiTextureResource::LoadTextureOptions(const ConfigData& config)
{
    String s;
    int v;
    const ConfigData* c = &config.Find(kCompression);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        s.ToLower();
        int i = 0;
        while (!(s == kCompressions[i].name))
            if (++i >= 8) return false;
        compression = static_cast<char>(kCompressions[i].value);
    }
    c = &config.Find(kHint);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        s.ToLower();
        int i = 0;
        while (!(s == kHints[i].name))
            if (++i >= 2) return false;
        hint = static_cast<char>(kHints[i].value);
    }
    c = &config.Find(kNormalMapHint);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        s.ToLower();
        int i = 0;
        while (!(s == kNormalMaps[i].name))
            if (++i >= 2) return false;
        normalMapHint = static_cast<NormalMapHint>(kNormalMaps[i].value);
    }
    c = &config.Find(kModifyMap);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        if (!LookUpEnum(s, kModifyMaps, 2, v)) return false;
        modifyMap = static_cast<ModifyMap>(v);
    }
    c = &config.Find(kAlphaHint);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        if (!LookUpEnum(s, kAlphas, 4, v)) return false;
        alphaHint = static_cast<char>(v);
    }
    config.Find(kAnisotropy) >> anisotropy;
    if (&config.Find(kAnisotropy) == &ConfigData::unknownItem) {
        anisotropy = ANISOTROPY_DEFAULT_TOKEN_VALUE;
    } else {
        const float a = anisotropy;
        if (!(a == 1.0f || a == 2.0f || a == 4.0f || a == 8.0f || a == 16.0f)) return false;
    }
    c = &config.Find(kAutoMip);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        if (!LookUpEnum(s, kAutoMips, 4, v)) return false;
        autoMip = static_cast<char>(v);
    }
    c = &config.Find(kMagFilter);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        if (!LookUpEnum(s, kFilters, 3, v)) return false;
        magFilter = static_cast<char>(v);
    }
    c = &config.Find(kMinFilter);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        if (!LookUpEnum(s, kFilters, 3, v)) return false;
        minFilter = static_cast<char>(v);
    }
    c = &config.Find(kMipFilter);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        bool none;
        {
            const String n("none");
            none = s == n;
        }
        if (none) {
            useMipMaps = false;
        } else {
            if (!LookUpEnum(s, kFilters, 3, v)) return false;
            mipFilter = static_cast<char>(v);
        }
    }
    c = &config.Find(kTile);
    if (c != &ConfigData::unknownItem) {
        *c >> s;
        wrapS = 1;
        wrapT = 1;
        s.ToLower();
        if (s.Find('s', 0) != -1) wrapS = 2;
        if (s.Find('t', 0) != -1) wrapT = 2;
    }
    if (!UseMipMaps()) autoMip = 0;
    return true;
}

namespace {
// one bitmap name of the .texture.txt as a resource of the texture's database (0x10005bc3..)
const BitmapResource* FindBitmap(const AsciiTextureResource* self, const ResourceDatabase* db, const ConfigData& texture,
                                 const PString& key, int n, bool noPath)
{
    String text;
    texture(key, n) >> text;
    File::Path path(static_cast<const char*>(text));
    if (noPath && (FirstChar(text) == '\\' || FirstChar(text) == '/')) path.DelPath();
    String name(static_cast<const char*>(path));
    self->ConvertResourceName(name);
    const PString pname(static_cast<const char*>(name));
    return dynamic_cast<const BitmapResource*>(db->Get(pname, 0));
}
}  // namespace

// 0x10005a70
bool __cdecl AsciiTextureResource::LoadConfigData(const ConfigData& config)
{
    String s;
    const ConfigData& texture = config.Find(kTexture);
    if (&texture == &ConfigData::unknownItem) return false;
    unsigned int n = static_cast<unsigned int>(texture.Count(kPrimary));
    if (!n) return false;
    unsigned int na = static_cast<unsigned int>(texture.Count(kAlpha));
    if (na && na != n) return false;
    const ConfigData& dim = texture.Find(kDimension);
    if (&dim != &ConfigData::unknownItem) {
        dim >> s;
        int v;
        if (!LookUpEnum(s, kDimensions, 4, v)) return false;
        dimension = static_cast<char>(v);
    }
    switch (static_cast<unsigned int>(static_cast<int>(dimension))) {
    case 0:
    case 3:
        depth = 1;
        if (n > 1) {
            n = 1;
            if (na) na = 1;
        }
        break;
    case 1:
    case 2:
        if (dimension == 1) depth = 6;
        if (n < depth) return false;
        n = depth;
        if (na) na = depth;
        break;
    default:
        return false;
    }
    if (!LoadTextureOptions(texture)) return false;
    for (unsigned int i = 0; i < n; ++i) {
        const BitmapResource* const b = FindBitmap(this, owner, texture, kPrimary, static_cast<int>(i), noPath);
        if (!b) return false;
        b->SetPriority(priority);
        primary.Add(b);
    }
    for (unsigned int i = 0; i < na; ++i) {
        const BitmapResource* const b = FindBitmap(this, owner, texture, kAlpha, static_cast<int>(i), noPath);
        if (!b) return false;
        b->SetPriority(priority);
        alpha.Add(b);
    }
    return true;
}

// 0x10006500 (a failed lock rolls back by the wrong index and the loop goes on, as in TS12)
bool __cdecl AsciiTextureResource::Save(const char* path, Display* display) const
{
    if (alpha.Count() && alpha.Count() != primary.Count()) return false;
    for (unsigned int i = 0; i < primary.Count(); ++i) {
        bool ok = primary[i] && Ok(primary[i]->Lock());
        bool rollBack;
        if (!alpha.Count()) {
            rollBack = !ok;
        } else if (alpha[i] && Ok(alpha[i]->Lock())) {
            rollBack = !ok;
        } else {
            if (primary[i]) primary[i]->Unlock();
            rollBack = true;
        }
        if (!rollBack || !i) continue;
        for (unsigned int k = i; k != 0; --k) {
            if (primary[i]) primary[k]->Unlock();
            if (alpha.Count() && alpha[k]) alpha[k]->Unlock();
        }
    }
    const bool result = TextureResource::Save(path, display);
    for (unsigned int i = 0; i < primary.Count(); ++i) {
        if (primary[i]) primary[i]->Unlock();
        if (alpha.Count() && alpha[i]) alpha[i]->Unlock();
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------
// formats and sizes

// 0x100067e0: 1 (ARGB) when an alpha bitmap marks the texture as not opaque, else 3; the primary decides only
// when its bitmap is not 32-bit
DFormatId __cdecl AsciiTextureResource::GetImageFormatId() const
{
    int f = 3;
    if (primary.Count() && primary[0]) {
        if (Ok(primary[0]->Lock())) {
            const int pf = Bits(BitmapOf(primary[0])).format;
            primary[0]->Unlock();
            if (pf != 2 && pf != 3) return static_cast<DFormatId>(f);
        }
        if (alpha.Count() && alpha[0] && Ok(alpha[0]->Lock())) {
            f = GetAlphaHint() ? 1 : 3;
            alpha[0]->Unlock();
        }
    }
    return static_cast<DFormatId>(f);
}

// 0x10006890: the display format of the compression and the alpha hint
DFormatId __cdecl AsciiTextureResource::GetDisplayFormatId() const
{
    if (GetHint() == 0) {
        const int a = GetAlphaHint();
        const int c = compression;
        switch (c) {
        case 0: return GetImageFormatId();
        case 1: return static_cast<DFormatId>(10 + (a != 0));
        case 2:
        case 3:
            if (a == 1) return static_cast<DFormatId>(0xb + (c != 2));
            return static_cast<DFormatId>(0xa + (a == 2));
        case 4: return static_cast<DFormatId>(0x19 + (a != 0));
        case 5:
        case 6:
            if (a == 1) return static_cast<DFormatId>(0x1a + (c != 5));
            return static_cast<DFormatId>(0x19 + (a == 2));
        case 8: return static_cast<DFormatId>(0x17 + (a != 0));
        case 9:
            if (a == 0) return static_cast<DFormatId>(0x1c);
            return GetImageFormatId();
        default: break;
        }
    }
    return TextureResource::GetDisplayFormatId();
}

// 0x10006990
unsigned int __cdecl AsciiTextureResource::GetNumMipLevels() const
{
    if (locked) return lockedMips;
    if (!UseMipMaps()) return 1;
    const Size s = GetSize();
    if (s.w && s.h) return static_cast<unsigned int>(NumMipLevels(s));
    return 0;
}

// 0x10006a00: the first bitmap's size, halved until it fits the streaming limit
Size __cdecl AsciiTextureResource::GetSize() const
{
    if (locked) return lockedSize;
    Size s = {0, 0};
    if (!primary.Count() || !primary[0]) return s;
    int w = 0, h = 0;
    if (Ok(primary[0]->Lock())) {
        const Bitmap* const b = BitmapOf(primary[0]);
        w = b->width;
        h = b->height;
        primary[0]->Unlock();
    }
    const unsigned int m = static_cast<unsigned int>(TextureManager::MAX_STREAMING_TEXTURE_SIZE) *
                           static_cast<unsigned int>(TextureManager::MAX_STREAMING_TEXTURE_SIZE);
    if (static_cast<int>(static_cast<unsigned int>(h) * static_cast<unsigned int>(w)) > static_cast<int>(m)) {
        do {
            h >>= 1;
            w >>= 1;
        } while (static_cast<int>(static_cast<unsigned int>(h) * static_cast<unsigned int>(w)) > static_cast<int>(m));
    }
    s.w = w;
    s.h = h;
    return s;
}

// ---------------------------------------------------------------------------------------------------------------
// the images

// 0x10006ab0: the primary bitmap (a view of it) or primary and alpha merged into a new ARGB bitmap, scaled down
// to the streaming limit
const Bitmap* __cdecl AsciiTextureResource::GetBitmapImage(const Bitmap* image, const Bitmap* alphaBitmap) const
{
    if (!image) return nullptr;
    const int pf = Bits(image).format;
    if (pf != 2 && pf != 3) return nullptr;
    Bitmap* out;
    if (!alphaBitmap || (alphaBitmap == image && pf == 2)) {
        out = View(image, pf);
    } else {
        const int af = Bits(alphaBitmap).format;
        if (af != 2 && af != 3) return nullptr;
        if (image->width != alphaBitmap->width || image->height != alphaBitmap->height) return nullptr;
        out = NewBitmap(SizeOf(image), 2);
        const BitmapBits& o = Bits(out);
        const BitmapBits& p = Bits(image);
        const BitmapBits& a = Bits(alphaBitmap);
        for (int y = 0; y < o.height; ++y) {
            const unsigned char* ar = a.data + a.pitch * y;
            const unsigned char* pr = p.data + p.pitch * y;
            unsigned char* orow = o.data + o.pitch * y;
            for (int x = 0; x < o.width; ++x) {
                const std::uint32_t pv = Px(pr + 4 * x);
                const std::uint32_t av = Px(ar + 4 * x);
                const std::uint32_t v = af == 2 ? (((pv ^ av) & 0xffffff) ^ av) : ((pv & 0xffffff) | (av << 24));
                *reinterpret_cast<std::uint32_t*>(orow + 4 * x) = v;
            }
        }
    }
    int w = out->width, h = out->height;
    const int m = static_cast<int>(static_cast<unsigned int>(TextureManager::MAX_STREAMING_TEXTURE_SIZE) *
                                   static_cast<unsigned int>(TextureManager::MAX_STREAMING_TEXTURE_SIZE));
    if (static_cast<int>(static_cast<unsigned int>(w) * static_cast<unsigned int>(h)) <= m) return out;
    do {
        h >>= 1;
        w >>= 1;
    } while (static_cast<int>(static_cast<unsigned int>(h) * static_cast<unsigned int>(w)) > m);
    if (w > 0 && h > 0) {
        Bitmap* const scaled = out->Scale(w, h);
        DeleteBitmap(out);
        return scaled;
    }
    DeleteBitmap(out);
    return nullptr;
}

// 0x10006d90: lock every bitmap and build the face images; false if any lock or image fails (all undone)
bool __cdecl AsciiTextureResource::GetMipLock() const
{
    if (images.Count() || !primary.Count()) return false;
    for (unsigned int i = 0; i < primary.Count(); ++i) {
        if (!Ok(primary[i]->Lock())) {
            for (unsigned int k = 0; k < i; ++k) primary[k]->Unlock();
            return false;
        }
    }
    for (unsigned int i = 0; i < alpha.Count(); ++i) {
        if (!Ok(alpha[i]->Lock())) {
            for (unsigned int k = 0; k < i; ++k) alpha[k]->Unlock();
            for (unsigned int k = 0; k < primary.Count(); ++k) primary[k]->Unlock();
            return false;
        }
    }
    for (unsigned int i = 0; i < primary.Count(); ++i) {
        const Bitmap* image;
        if (alpha.Count()) {
            const Bitmap* const a = BitmapOf(alpha[i]);
            image = GetBitmapImage(BitmapOf(primary[i]), a);
        } else {
            image = GetBitmapImage(BitmapOf(primary[i]), nullptr);
        }
        if (!image) {
            for (unsigned int k = 0; k < images.Count(); ++k)
                if (images[k]) DeleteBitmap(images[k]);
            images.RemoveAll();
            for (unsigned int k = 0; k < primary.Count(); ++k) primary[k]->Unlock();
            for (unsigned int k = 0; k < alpha.Count(); ++k) alpha[k]->Unlock();
            return false;
        }
        images.Add(const_cast<Bitmap*>(image));
    }
    lockedSize = GetSize();
    lockedMips = GetNumMipLevels();
    locked = true;
    return true;
}

// 0x10007060
bool __cdecl AsciiTextureResource::GetMipUnlock() const
{
    locked = false;
    lockedMips = 0;
    lockedSize.w = 0;
    lockedSize.h = 0;
    if (!images.Count()) return false;
    for (unsigned int k = 0; k < images.Count(); ++k)
        if (images[k]) DeleteBitmap(images[k]);
    images.RemoveAll();
    for (unsigned int k = 0; k < primary.Count(); ++k) primary[k]->Unlock();
    for (unsigned int k = 0; k < alpha.Count(); ++k) alpha[k]->Unlock();
    return true;
}

// 0x10007120: the locked image of `face` at `size` (a view at its own size, else scaled down; the level is not
// used)
const Bitmap* __cdecl AsciiTextureResource::GetMip(unsigned int, unsigned int face, const Size& size) const
{
    if (face >= images.Count()) return nullptr;
    const Bitmap* const b = images[face];
    if (!b) return nullptr;
    if (size.w == b->width && size.h == b->height) return View(b, Bits(b).format);
    if (size.w > b->width || size.h > b->height) return nullptr;
    return b->Scale(size.w, size.h);
}

// 0x10007220: a new copy of face `face` (merged with its alpha bitmap). The alpha list is read at `face` while
// face <= its count, and the merge loops test the format instead of the width on their first row (TS12)
const Bitmap* __cdecl AsciiTextureResource::GetImage(unsigned int face) const
{
    if (primary.Count() <= face || !primary[face]) return nullptr;
    if (!Ok(primary[face]->Lock())) return nullptr;
    const Bitmap* const pb = BitmapOf(primary[face]);
    const int pf = Bits(pb).format;
    Bitmap* out;
    if (alpha.Count() && alpha.Count() >= face && alpha[face]) {
        out = NewBitmap(SizeOf(pb), 2);
        const BitmapResource* const ar = alpha[face];
        if (Ok(ar->Lock())) {
            const Bitmap* const ab = BitmapOf(alpha[face]);
            const int af = Bits(ab).format;
            if ((af == 2 || af == 3) && (pf == 2 || pf == 3) && ab->width == out->width &&
                ab->height == out->height) {
                const BitmapBits& o = Bits(out);
                const BitmapBits& p = Bits(pb);
                const BitmapBits& a = Bits(ab);
                for (int y = 0; y < o.height; ++y) {
                    const unsigned char* arow = a.data + a.pitch * y;
                    const unsigned char* prow = p.data + p.pitch * y;
                    unsigned char* orow = o.data + o.pitch * y;
                    int x = 0;
                    if (y == 0 || o.width > 0) {
                        do {
                            const std::uint32_t pv = Px(prow + 4 * x);
                            const std::uint32_t av = Px(arow + 4 * x);
                            const std::uint32_t v =
                                af == 2 ? (((pv ^ av) & 0xffffff) ^ av) : ((pv & 0xffffff) | (av << 24));
                            *reinterpret_cast<std::uint32_t*>(orow + 4 * x) = v;
                            ++x;
                        } while (x < o.width);
                    }
                }
            }
            alpha[face]->Unlock();
        }
    } else {
        out = NewBitmap(SizeOf(pb), pf);
        const Point at = {0, 0};
        out->CopyImage(*pb, at, 0);
    }
    primary[face]->Unlock();
    return out;
}

// 0x10007580: the alpha hint from the first alpha bitmap (none: opaque), unless the default is not "default"
void __cdecl AsciiTextureResource::UpdateAlphaHint() const
{
    const int def = GetDefaultAlphaHint();
    if (def != 3) {
        alphaHint = static_cast<char>(def);
        return;
    }
    alphaHint = 0;
    if (!alpha.Count() || !alpha[0] || !Ok(alpha[0]->Lock())) return;
    const Bitmap* const b = BitmapOf(alpha[0]);
    const BitmapBits& a = Bits(b);
    if (a.format == 2 || a.format == 3) {
        alphaHint = 2;
        const int at = a.format == 2 ? 3 : 0;
        for (int y = 0; y < a.height && alphaHint == 2; ++y) {
            const unsigned char* row = a.data + a.pitch * y + at;
            for (int x = 0; x < a.width && alphaHint == 2; ++x)
                if (static_cast<unsigned char>(row[4 * x] - 1) <= 0xfd) alphaHint = 1;
        }
    }
    alpha[0]->Unlock();
}

// 0x100076d0: as UpdateAlphaHint, but an alpha bitmap found fully opaque is dropped from the list
void __cdecl AsciiTextureResource::UpdateAlphaHintStrict() const
{
    if (alphaHint == 0) return;
    const int def = GetDefaultAlphaHint();
    if (def != 3) {
        alphaHint = static_cast<char>(def);
        return;
    }
    alphaHint = 0;
    if (!alpha.Count() || !alpha[0] || !Ok(alpha[0]->Lock())) return;
    const Bitmap* const b = BitmapOf(alpha[0]);
    const BitmapBits& a = Bits(b);
    if (a.format != 2 && a.format != 3) {
        alpha[0]->Unlock();
        return;
    }
    const int at = a.format == 2 ? 3 : 0;
    for (int y = 0; y < a.height && (alphaHint == 2 || alphaHint == 0); ++y) {
        const unsigned char* row = a.data + a.pitch * y + at;
        for (int x = 0; x < a.width; ++x) {
            const unsigned char d = row[4 * x];
            if (d != 0 && d != 0xff) {
                alphaHint = 1;
                break;
            }
            if (d == 0) alphaHint = 2;
        }
    }
    alpha[0]->Unlock();
    if (alphaHint != 0) return;
    alpha[0]->RemoveObserver(this);
    alpha[0]->ReleaseReference();
    const BitmapResource* const last = alpha.Remove();
    if (alpha.Count()) alpha[0] = last;
}

// 0x10007890: the average of the first bitmap's pixels (alpha 1), unless a colour is set already
void __cdecl AsciiTextureResource::UpdateAverageColor() const
{
    if (averageColor.r > 0.0f || averageColor.g > 0.0f || averageColor.b > 0.0f || averageColor.a > 0.0f) return;
    if (!primary.Count() || !primary[0] || !Ok(primary[0]->Lock())) return;
    double sums[3] = {0.0, 0.0, 0.0};
    const Bitmap* const b = BitmapOf(primary[0]);
    for (int y = 0; y < b->height; ++y) {
        int r = 0, g = 0, bl = 0;
        for (int x = 0; x < b->width; ++x) {
            const Point at = {x, y};
            const PackedColor c = b->Pixel(at);
            r += (c.argb >> 16) & 0xff;
            g += (c.argb >> 8) & 0xff;
            bl += c.argb & 0xff;
        }
        AddRow(&sums[0], r);
        AddRow(&sums[1], g);
        AddRow(&sums[2], bl);
    }
    primary[0]->Unlock();
    const int n = static_cast<int>(static_cast<unsigned int>(b->width) * static_cast<unsigned int>(b->height));
    averageColor.a = 1.0f;
    Average(n, sums, &averageColor.r);
}

}  // namespace Jet
