// RenderOpenGL_7c: the OpenGL display texture (unit 3 of the port).
#include "DisplayTextureOpenGL.h"

#include "OpenGLExtensions.h"

#include <jet/RenderSystem.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

// GL 1.2+ / vendor tokens that the Windows 1.1 header does not carry
namespace {
constexpr GLenum kTextureCubeMap = 0x8513;
constexpr GLenum kTextureCubeMapPositiveX = 0x8515;
constexpr GLenum kTextureMaxLevel = 0x813D;
constexpr GLenum kGenerateMipmap = 0x8191;
constexpr GLenum kTextureMaxAnisotropy = 0x84FE;
constexpr GLenum kCompressedRgbDxt1 = 0x83F0;
constexpr GLenum kCompressedRgbaDxt1 = 0x83F1;
constexpr GLenum kCompressedRgbaDxt3 = 0x83F2;
constexpr GLenum kCompressedRgbaDxt5 = 0x83F3;
constexpr GLenum kBgra = 0x80E1;
constexpr GLenum kBgr = 0x80E0;
constexpr GLenum kClampToEdge = 0x812F;
constexpr GLenum kUnsignedShort4444Rev = 0x8365;
constexpr GLenum kUnsignedShort1555Rev = 0x8366;
constexpr GLenum kLuminance8 = 0x8040;
constexpr GLenum kLuminance16 = 0x8042;
constexpr GLenum kLuminance8Alpha8 = 0x8045;
constexpr GLenum kAlpha8 = 0x803C;
constexpr GLenum kDepthComponent16 = 0x81A5;
constexpr GLenum kDepthComponent24 = 0x81A6;
constexpr GLenum kDepthComponent32 = 0x81A7;
constexpr GLenum kRgba8 = 0x8058;
constexpr GLenum kRgb8 = 0x8051;
constexpr GLenum kRgba4 = 0x8056;
constexpr GLenum kTexture3D = 0x806F;
constexpr GLenum kMaxTextureUnits = 0x84E2;
constexpr GLenum kUnsignedShort565 = 0x8363;
constexpr GLenum kRgb5A1 = 0x8057;
constexpr GLenum kSgisGenerateMipmap = 0x8191;
}  // namespace

namespace {

// Temporary: dump the first UI atlas (GL texture 5) once, so its gradient/alpha can be checked outside the game.
void DumpUiAtlas(unsigned int id)
{
    static bool dumped = false;
    if (dumped) return;
    GLint width = 0;
    GLint height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) return;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    HANDLE file = CreateFileA("D:\\universal modder\\build-trainz32\\game-rogl\\tex5.raw", GENERIC_WRITE,
                              FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr);
        CloseHandle(file);
    }
    char info[32];
    const int length = wsprintfA(info, "%dx%d\r\n%u\r\n", width, height, id);
    file = CreateFileA("D:\\universal modder\\build-trainz32\\game-rogl\\tex5.txt", GENERIC_WRITE, FILE_SHARE_READ,
                       nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, info, static_cast<DWORD>(length), &written, nullptr);
        CloseHandle(file);
    }
    dumped = true;
}

}  // namespace

namespace Jet {
namespace {

struct BmpDesc {
    GLenum format;
    GLenum type;
    int bytes;
};

BmpDesc Describe(const Bitmap& bitmap)
{
    switch (bitmap.GetFormat()) {
    case Bitmap::RGB: return {kBgr, GL_UNSIGNED_BYTE, 3};
    case Bitmap::ARGB: return {kBgra, GL_UNSIGNED_BYTE, 4};
    case Bitmap::XRGB: return {kBgra, GL_UNSIGNED_BYTE, 4};
    case Bitmap::RGB565: return {GL_RGB, kUnsignedShort565, 2};
    case Bitmap::INDEX: return {GL_LUMINANCE, GL_UNSIGNED_BYTE, 1};
    default: return {kBgra, GL_UNSIGNED_BYTE, 4};
    }
}

struct GlFormat {
    GLint internalFormat;
    GLenum format;
    GLenum type;
    bool compressed;
};

GlFormat MapFormat(int id)
{
    switch (id) {
    case 1: return {kRgba8, kBgra, GL_UNSIGNED_BYTE, false};
    case 2: return {kRgba8, GL_RGBA, GL_UNSIGNED_BYTE, false};
    case 3: return {kRgb8, kBgra, GL_UNSIGNED_BYTE, false};
    case 4: return {kRgb8, GL_RGBA, GL_UNSIGNED_BYTE, false};
    case 5: return {GL_RGB8, kBgr, GL_UNSIGNED_BYTE, false};
    case 6: return {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, false};
    case 7: return {kRgba4, kBgra, kUnsignedShort4444Rev, false};
    case 8: return {GL_RGB, GL_RGB, kUnsignedShort565, false};
    case 9: return {kRgb5A1, kBgra, kUnsignedShort1555Rev, false};
    case 10: return {kCompressedRgbDxt1, 0, 0, true};
    case 11: return {kCompressedRgbaDxt3, 0, 0, true};
    case 12: return {kCompressedRgbaDxt5, 0, 0, true};
    case 13: return {kLuminance8, GL_LUMINANCE, GL_UNSIGNED_BYTE, false};
    case 14: return {kLuminance16, GL_LUMINANCE, GL_UNSIGNED_SHORT, false};
    case 15: return {kLuminance8Alpha8, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, false};
    case 16: return {kAlpha8, GL_ALPHA, GL_UNSIGNED_BYTE, false};
    case 17: return {kDepthComponent16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false};
    case 18: return {kDepthComponent24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false};
    case 19: return {kDepthComponent32, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false};
    case 21: return {kRgba8, GL_RGBA, GL_UNSIGNED_BYTE, false};
    case 22: return {kRgba4, GL_RGBA, kUnsignedShort4444Rev, false};
    default: return {kRgba8, kBgra, GL_UNSIGNED_BYTE, false};
    }
}

Size MipSize(Size size, unsigned level)
{
    int w = size.w;
    int h = size.h;
    for (unsigned i = 0; i < level; ++i) {
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
    }
    return Size{w > 0 ? w : 1, h > 0 ? h : 1};
}

unsigned Logo2(int value)
{
    unsigned p = 0;
    while (p < 31 && (1u << p) < static_cast<unsigned>(value)) ++p;
    return p;
}

GLenum FaceTarget(unsigned face)
{
    return face < 6 ? static_cast<GLenum>(kTextureCubeMapPositiveX + face) : GL_TEXTURE_2D;
}

}  // namespace

bool DisplayTextureOpenGL::anisotropySupport = false;
bool DisplayTextureOpenGL::canAutoMip = false;
unsigned int DisplayTextureOpenGL::numObjects = 0;
const DisplayTextureOpenGL** DisplayTextureOpenGL::bound = nullptr;
unsigned DisplayTextureOpenGL::maxTextureUnits = 1;

DisplayTextureOpenGL::DisplayTextureOpenGL(Display* display)
    : DisplayTexture(display), apiTexture(0xffffffff), renderContext(0), firstLevel(0xffffffff),
      minFilter(static_cast<TextureFilter>(GL_NEAREST)), magFilter(static_cast<TextureFilter>(GL_NEAREST)),
      wrapS(static_cast<TextureWrap>(GL_REPEAT)), wrapT(static_cast<TextureWrap>(GL_REPEAT)), unused54(0), unused58(0),
      formatId(0), dirty(false), bound61(false), bindCount(0), flag68(true)
{
    if (numObjects++ == 0) {
        GL::LoadExtensions();
        canAutoMip = GL::IsSupported("GL_SGIS_generate_mipmap") || GL::IsSupported("GL_EXT_framebuffer_object");
        GLint units = 1;
        glGetIntegerv(kMaxTextureUnits, &units);
        if (units < 1) units = 1;
        maxTextureUnits = static_cast<unsigned>(units);
        bound = new const DisplayTextureOpenGL*[maxTextureUnits]();
        anisotropySupport = GL::IsSupported("GL_EXT_texture_filter_anisotropic");
    }
}

DisplayTextureOpenGL::DisplayTextureOpenGL(const DisplayTextureOpenGL& o)
    : DisplayTexture(o), apiTexture(o.apiTexture), renderContext(o.renderContext), firstLevel(o.firstLevel),
      minFilter(o.minFilter), magFilter(o.magFilter), wrapS(o.wrapS), wrapT(o.wrapT), unused54(o.unused54),
      unused58(o.unused58), formatId(o.formatId), dirty(o.dirty), bound61(o.bound61), bindCount(o.bindCount),
      flag68(o.flag68)
{
    ++numObjects;
}

DisplayTextureOpenGL::~DisplayTextureOpenGL() { Destroy(); }

DisplayTextureOpenGL& DisplayTextureOpenGL::operator=(const DisplayTextureOpenGL& o)
{
    if (this == &o) return *this;
    DisplayTexture::operator=(o);
    apiTexture = o.apiTexture;
    renderContext = o.renderContext;
    firstLevel = o.firstLevel;
    minFilter = o.minFilter;
    magFilter = o.magFilter;
    wrapS = o.wrapS;
    wrapT = o.wrapT;
    unused54 = o.unused54;
    unused58 = o.unused58;
    formatId = o.formatId;
    dirty = o.dirty;
    bound61 = o.bound61;
    bindCount = o.bindCount;
    flag68 = o.flag68;
    return *this;
}

DisplayOpenGL* __cdecl DisplayTextureOpenGL::GetDisplay() const
{
    return reinterpret_cast<DisplayOpenGL*>(display);
}

DisplayInterfaceWGL* __cdecl DisplayTextureOpenGL::GetWinInterface() const
{
    return *reinterpret_cast<DisplayInterfaceWGL* const*>(reinterpret_cast<const char*>(display) + 0xf00);
}

void __cdecl DisplayTextureOpenGL::InitInstance(DisplayTextureOpenGL&) const {}

void __cdecl DisplayTextureOpenGL::BindTexture(unsigned int unit) const
{
    if (GL::ext.ActiveTexture) GL::ext.ActiveTexture(0x84C0 + unit);
    if (unit >= maxTextureUnits || !bound) {
        glBindTexture(GL_TEXTURE_2D, apiTexture);
        if (apiTexture == 5 && !dirty) DumpUiAtlas(apiTexture);
        return;
    }
    if (bound[unit] != this) {
        if (bound[unit]) bound[unit]->UnbindRenderTexture(unit);
        bound[unit] = this;
    }
    glBindTexture(dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D, apiTexture);
    if (apiTexture == 5 && !dirty) DumpUiAtlas(apiTexture);
}

void __cdecl DisplayTextureOpenGL::UnbindRenderTexture(unsigned int unit) const
{
    (void)unit;
    glBindTexture(dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D, 0);
}

void __cdecl DisplayTextureOpenGL::SetBasicState() const
{
    if (apiTexture == 0xffffffff) return;
    if (GL::ext.ActiveTexture) GL::ext.ActiveTexture(0x84C0);
    if (dimension == static_cast<TextureDimension>(1)) {
        glTexParameteri(kTextureCubeMap, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(kTextureCubeMap, GL_TEXTURE_MAG_FILTER, magFilter);
        glTexParameteri(kTextureCubeMap, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(kTextureCubeMap, GL_TEXTURE_WRAP_T, wrapT);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
        glTexParameteri(GL_TEXTURE_2D, kTextureMaxLevel, static_cast<GLint>(mipLevels ? mipLevels - 1 : 0));
    }
    if (anisotropySupport && requestedAnisotropy > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, kTextureMaxAnisotropy, requestedAnisotropy);
    }
    if (canAutoMip && autoMip) glTexParameteri(GL_TEXTURE_2D, kGenerateMipmap, 1);
    dirty = false;
}

void __cdecl DisplayTextureOpenGL::Destroy()
{
    if (mipLevels != 0 || apiTexture != 0xffffffff) {
        if (apiTexture != 0xffffffff) {
            GLuint id = apiTexture;
            glDeleteTextures(1, &id);
            apiTexture = 0xffffffff;
        }
        mipLevels = 0;
        firstLevel = 0xffffffff;
        renderContext = 0;
    }
}

bool __cdecl DisplayTextureOpenGL::Create(const Size& size, const DisplayFormat& format, TextureDimension dimension_,
                                          unsigned int mips, TextureAutoMip autoMip_, unsigned int depth,
                                          float anisotropy, TextureHint hint, TextureFilter minFilter_,
                                          TextureFilter magFilter_, TextureFilter mipFilter, TextureWrap wrapS_,
                                          TextureWrap wrapT_, const RenderTextureConstruct& construct)
{
    Destroy();
    DisplayFormat local = format;
    local.Classify();
    formatId = local.id;
    this->size = size;
    dimension = dimension_;
    depthLevels = depth;
    mipLevels = mips ? mips : static_cast<unsigned>(DisplayTexture::GetNumMipLevels(size));
    requestedAnisotropy = anisotropy;
    minFilter = minFilter_;
    magFilter = magFilter_;
    wrapS = wrapS_;
    wrapT = wrapT_;
    autoMip = static_cast<char>(autoMip_);
    (void)hint;
    (void)construct;
    (void)mipFilter;
    (void)format;

    GLuint id = 0;
    glGenTextures(1, &id);
    if (!id) return false;
    apiTexture = id;

    GLenum const target = dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D;
    glBindTexture(target, id);
    if (!GL::ext.ActiveTexture) glBindTexture(target, id);

    int const levels = static_cast<int>(mipLevels ? mipLevels : 1);
    GlFormat const mapped = MapFormat(formatId);
    for (int level = 0; level < levels; ++level) {
        Size const levelSize = MipSize(size, level);
        int const width = levelSize.w > 0 ? levelSize.w : 1;
        int const height = levelSize.h > 0 ? levelSize.h : 1;
        if (dimension == static_cast<TextureDimension>(1)) {
            for (unsigned face = 0; face < 6; ++face) {
                glTexImage2D(FaceTarget(face), level, kRgba8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        } else if (mapped.compressed && GL::ext.CompressedTexImage2D) {
            int const blocks = ((width + 3) / 4) * ((height + 3) / 4);
            int const bytes = blocks * (mapped.internalFormat == kCompressedRgbDxt1 ? 8 : 16);
            GL::ext.CompressedTexImage2D(GL_TEXTURE_2D, level, mapped.internalFormat, width, height, 0, bytes,
                                         nullptr);
        } else {
            glTexImage2D(GL_TEXTURE_2D, level, mapped.internalFormat, width, height, 0, mapped.format,
                         mapped.type, nullptr);
        }
    }
    dirty = true;
    SetBasicState();
    return true;
}

bool __cdecl DisplayTextureOpenGL::Load(unsigned int level, DisplayFormat& format, const Bitmap& bitmap,
                                        unsigned int face)
{
    if (apiTexture == 0xffffffff || !bitmap.IsValid()) return false;
    DisplayFormat local = format;
    local.Classify();
    formatId = local.id;
    if (level < firstLevel) firstLevel = level;
    BmpDesc const desc = Describe(bitmap);
    int const pitch = static_cast<int>(bitmap.GetPitch());
    int const bpp = static_cast<int>(bitmap.GetBytesPerPixel());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, bpp ? pitch / bpp : 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLenum const target = dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D;
    glBindTexture(target, apiTexture);
    GLenum const uploadTarget = dimension == static_cast<TextureDimension>(1) ? FaceTarget(face) : GL_TEXTURE_2D;
    Rect const area = static_cast<Rect>(bitmap);
    GlFormat const mapped = MapFormat(formatId);
    if (mapped.compressed && GL::ext.CompressedTexSubImage2D) {
        int const blocks = ((area.w + 3) / 4) * ((area.h + 3) / 4);
        int const bytes = blocks * (mapped.internalFormat == kCompressedRgbDxt1 ? 8 : 16);
        GL::ext.CompressedTexSubImage2D(uploadTarget, static_cast<GLint>(level), 0, 0, area.w, area.h,
                                        mapped.internalFormat, bytes, bitmap.GetPtr());
    } else {
        glTexSubImage2D(uploadTarget, static_cast<GLint>(level), 0, 0, area.w, area.h, desc.format, desc.type,
                        bitmap.GetPtr());
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    dirty = true;
    return true;
}

bool __cdecl DisplayTextureOpenGL::SubLoad(unsigned int level, const Rect& area, DisplayFormat& format,
                                           const Bitmap& bitmap, unsigned int face)
{
    if (apiTexture == 0xffffffff || !bitmap.IsValid()) return false;
    DisplayFormat local = format;
    local.Classify();
    formatId = local.id;
    BmpDesc const desc = Describe(bitmap);
    int const pitch = static_cast<int>(bitmap.GetPitch());
    int const bpp = static_cast<int>(bitmap.GetBytesPerPixel());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, bpp ? pitch / bpp : 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D, apiTexture);
    GLenum const target = dimension == static_cast<TextureDimension>(1) ? FaceTarget(face) : GL_TEXTURE_2D;
    glTexSubImage2D(target, static_cast<GLint>(level), area.x, area.y, area.w, area.h, desc.format, desc.type,
                    bitmap.GetPtr());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    dirty = true;
    return true;
}

bool __cdecl DisplayTextureOpenGL::SubloadFromFrame(unsigned int level, const Point& at, const Rect& area,
                                                    unsigned int face)
{
    if (apiTexture == 0xffffffff) return false;
    glBindTexture(dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D, apiTexture);
    GLenum const target = dimension == static_cast<TextureDimension>(1) ? FaceTarget(face) : GL_TEXTURE_2D;
    glCopyTexSubImage2D(target, static_cast<GLint>(level), at.x, at.y, area.x, area.y, area.w, area.h);
    dirty = true;
    return true;
}

bool __cdecl DisplayTextureOpenGL::CreateRenderContext() { return true; }

bool __cdecl DisplayTextureOpenGL::Bind(unsigned int unit, bool, float anisotropy, TextureWrap wrapS_,
                                        TextureWrap wrapT_, TextureFilter minFilter_, TextureFilter magFilter_,
                                        TextureFilter)
{
    if (apiTexture == 0xffffffff) return false;
    if (anisotropy != requestedAnisotropy || wrapS_ != wrapS || wrapT_ != wrapT || minFilter_ != minFilter ||
        magFilter_ != magFilter) {
        requestedAnisotropy = anisotropy;
        wrapS = wrapS_;
        wrapT = wrapT_;
        minFilter = minFilter_;
        magFilter = magFilter_;
        dirty = true;
    }
    Bind(unit);
    if (dirty) SetBasicState();
    return true;
}

bool __cdecl DisplayTextureOpenGL::Bind(unsigned int unit) const
{
    if (apiTexture == 0xffffffff) return false;
    BindTexture(unit);
    if (dirty) SetBasicState();
    return true;
}

void __cdecl DisplayTextureOpenGL::CopyMipToMip(DisplayTexture* from, int fromLevel, DisplayTexture* to,
                                                int toLevel)
{
    auto* source = static_cast<DisplayTextureOpenGL*>(from);
    auto* dest = static_cast<DisplayTextureOpenGL*>(to);
    if (!source || !dest || source->apiTexture == 0xffffffff || dest->apiTexture == 0xffffffff) return;
    Size const size = MipSize(source->size, static_cast<unsigned>(fromLevel));
    if (size.w <= 0 || size.h <= 0) return;
    GLenum const sourceTarget =
        source->dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D;
    GLenum const destTarget = dest->dimension == static_cast<TextureDimension>(1) ? kTextureCubeMap : GL_TEXTURE_2D;
    glBindTexture(sourceTarget, source->apiTexture);
    GLint compressed = 0;
    glGetTexLevelParameteriv(sourceTarget, fromLevel, 0x86A0, &compressed);
    if (!compressed) {
        void* const data = Alloc::New(static_cast<unsigned>(size.w * size.h * 4));
        if (!data) return;
        glGetTexImage(sourceTarget, fromLevel, kBgra, GL_UNSIGNED_BYTE, data);
        glBindTexture(destTarget, dest->apiTexture);
        glTexSubImage2D(destTarget, toLevel, 0, 0, size.w, size.h, kBgra, GL_UNSIGNED_BYTE, data);
        Alloc::Delete(data);
    } else {
        GLint bytes = 0;
        glGetTexLevelParameteriv(sourceTarget, fromLevel, 0x86A0, &bytes);
        if (bytes <= 0 || !GL::ext.GetCompressedTexImage || !GL::ext.CompressedTexSubImage2D) return;
        void* const data = Alloc::New(static_cast<unsigned>(bytes));
        if (!data) return;
        GL::ext.GetCompressedTexImage(sourceTarget, fromLevel, data);
        glBindTexture(destTarget, dest->apiTexture);
        GL::ext.CompressedTexSubImage2D(destTarget, toLevel, 0, 0, size.w, size.h, source->formatId,
                                        bytes, data);
        Alloc::Delete(data);
    }
}


DisplayTexture* __cdecl DisplayTextureOpenGL::CreateInstance() const
{
    auto* created = static_cast<DisplayTextureOpenGL*>(VCall<DisplayTexture*>(display, 0x98));
    if (!created) return nullptr;
    DisplayFormat format;
    format.Set(static_cast<DFormatId>(formatId));
    RenderTextureConstruct construct;
    if (!created->Create(this->size, format, dimension, mipLevels, static_cast<TextureAutoMip>(autoMip), depthLevels,
                         requestedAnisotropy, static_cast<TextureHint>(0), minFilter, magFilter,
                         static_cast<TextureFilter>(0), wrapS, wrapT, construct)) {
        delete created;
        return nullptr;
    }
    created->dirty = true;
    created->requestedAnisotropy = requestedAnisotropy;
    created->minFilter = minFilter;
    created->magFilter = magFilter;
    created->wrapS = wrapS;
    created->wrapT = wrapT;
    return created;
}

Bitmap* __thiscall DisplayTextureOpenGL::GetBitmap(unsigned int level, unsigned int face, DisplayFormat& format,
                                                DFormatId as) const
{
    return GetTextureBitmap(static_cast<int>(level), face, format, as);
}

Bitmap* __cdecl DisplayTextureOpenGL::GetTextureBitmap(int level, unsigned int face, DisplayFormat& format,
                                                       DFormatId as) const
{
    (void)face;
    if (apiTexture == 0xffffffff) return nullptr;
    Size const size = MipSize(this->size, static_cast<unsigned>(level));
    if (size.w <= 0 || size.h <= 0) return nullptr;
    DisplayFormat local;
    local.Set(as);
    GLenum fmt = kBgra;
    GLenum type = GL_UNSIGNED_BYTE;
    Bitmap::Format bmpFormat = Bitmap::ARGB;
    switch (local.id) {
    case 3:
    case 4:
    case 5:
    case 6: fmt = kBgr; type = GL_UNSIGNED_BYTE; bmpFormat = Bitmap::RGB; break;
    case 8: fmt = GL_RGB; type = kUnsignedShort565; bmpFormat = Bitmap::RGB565; break;
    case 13: fmt = GL_LUMINANCE; type = GL_UNSIGNED_BYTE; bmpFormat = Bitmap::INDEX; break;
    default: break;
    }
    int const bytes = bmpFormat == Bitmap::RGB ? 3 : bmpFormat == Bitmap::RGB565 ? 2 : bmpFormat == Bitmap::INDEX ? 1 : 4;
    auto* data = static_cast<unsigned char*>(::operator new(static_cast<std::size_t>(size.w * size.h * bytes)));
    glBindTexture(GL_TEXTURE_2D, apiTexture);
    glGetTexImage(GL_TEXTURE_2D, level, fmt, type, data);
    format.Set(as);
    Bitmap* bitmap = new Bitmap(size, bmpFormat, nullptr);
    if (bitmap && bitmap->GetPtr()) std::memcpy(bitmap->GetPtr(), data, static_cast<std::size_t>(size.w * size.h * bytes));
    ::operator delete(data);
    return bitmap;
}

bool __cdecl DisplayTextureOpenGL::GetDataLost() const { return false; }

DFormatId __cdecl DisplayTextureOpenGL::GetJetFormat() const { return static_cast<DFormatId>(formatId); }

void __cdecl DisplayTextureOpenGL::CopySamplerAttributesFrom(DisplayTexture* from)
{
    auto* source = dynamic_cast<DisplayTextureOpenGL*>(from);
    if (!source) return;
    requestedAnisotropy = source->requestedAnisotropy;
    minFilter = source->minFilter;
    magFilter = source->magFilter;
    wrapS = source->wrapS;
    wrapT = source->wrapT;
    dirty = true;
}

void __cdecl DisplayTextureOpenGL::RefreshFromChangedSettings() { dirty = true; }

void* __cdecl DisplayTextureOpenGL::GetAPITexture() const
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(apiTexture));
}

PBufferContext* __cdecl DisplayTextureOpenGL::GetRenderContext() const
{
    return reinterpret_cast<PBufferContext*>(static_cast<std::uintptr_t>(renderContext));
}

OpenGLFormat* __cdecl DisplayTextureOpenGL::CheckFormat(DisplayFormat&, void*&, int&, int&, int&, int, int)
{
    static OpenGLFormat fallback{1, 0, kRgba8, kBgra, GL_UNSIGNED_BYTE, DFORMAT_UNKNOWN, 0, 0};
    return &fallback;
}

}  // namespace Jet
