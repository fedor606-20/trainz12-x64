// U16: BitmapTextureResource and its factory (retail 0x10008360..0x100090a8, 0x1003b7c0, 0x1003b880,
// 0x10008eb0), transcribed from the TS12 assembly. The resource wraps one BitmapResource of the same database:
// Load asks the database for the name with the last four characters ("-tex") removed and keeps a reference;
// GetMipLock presents the bitmap as an aliased Jet::Bitmap. Everything local: no retail data or code is read.
#include "BitmapTextureResource.h"

#include <jet/Bitmap.h>
#include <jet/KernelResources.h>
#include <jet/RenderTypes.h>

#include <new>

namespace Jet {

namespace {

// the four characters a bitmap texture resource name carries past the bitmap resource's name ("-tex")
constexpr unsigned int kSuffixLength = 4;

void DeleteBitmap(Bitmap*& bitmap)
{
    if (!bitmap) return;
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
    bitmap = nullptr;
}

// BitmapResource's own GetBitmap is protected; BitmapBaseResource's (the slot-21 body) is public.
const Bitmap* BitmapOf(const BitmapResource* resource)
{
    return static_cast<const BitmapBaseResource*>(resource)->GetBitmap();
}

// File::Exists as the original imports it (Result by value); KernelStandard.h declares a bool wrapper only for
// Kernel_7c, so bind the exported Result form by its mangled name (0 = there).
extern "C" int* __cdecl RsFileExistsResult(int* result, const char* path);
#pragma comment(linker, "/alternatename:_RsFileExistsResult=?Exists@File@Jet@@SA?AV?$ResultClass@$1?results@File@Jet@@2QBUResultTable@2@BW4Results@File@Jet@@@2@PBD@Z")

bool FileExists(const char* path)
{
    int result = -1;
    RsFileExistsResult(&result, path);
    return result == 0;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// construction

BitmapTextureResource::BitmapTextureResource(const PString& name, const ResourceDatabase* owner)
    : TextureResource(name, owner), bitmapResource(nullptr), alphaHint(-1), bitmap(nullptr), averageColor{}
{
}

// 0x10008cf0: the level-0 bitmap goes (its resource unlocked), the observer leaves the bitmap resource, then the
// TextureResource part frees the texture info.
BitmapTextureResource::~BitmapTextureResource()
{
    if (bitmap) {
        DeleteBitmap(bitmap);
        bitmapResource->Unlock();
    }
    if (bitmapResource) {
        bitmapResource->RemoveObserver(this);
        bitmapResource->ReleaseReference();
    }
}

// 0x100084b0: TextureResource::Lock, then the bitmap resource is locked and unlocked once it is there; a failure
// of the bitmap's lock unlocks this resource and carries the bitmap's result out (the original's quirk).
TextureResource::Result __cdecl BitmapTextureResource::Lock() const
{
    Result r = TextureResource::Lock();
    if (r.code != 0) return r;
    if (bitmapResource) {
        const Result bitmapResult = bitmapResource->Lock();
        if (bitmapResult.code != 0) {
            Unlock();
            return bitmapResult;
        }
        bitmapResource->Unlock();
        return Result(0);
    }
    Unlock();
    return Result(1);
}

// 0x10008580
TextureResource::Result __cdecl BitmapTextureResource::IsAllLoaded() const
{
    const Result r = Resource::IsAllLoaded();
    if (r.code != 0) return r;
    if (bitmapResource) return bitmapResource->IsAllLoaded();
    return Result(0);
}

// 0x10005030 (the original folds this body with AsciiTextureResource::LoadAll)
bool __cdecl BitmapTextureResource::LoadAll() const { return Resource::LoadAll(); }

// 0x10009500 (the module's shared "false" body)
bool __cdecl BitmapTextureResource::GetMustResetWhenFinished() const { return false; }

// 0x10008360: the bitmap resource named without the trailing "-tex"; the old one is released, the new one takes
// this resource's priority.
TextureResource::Result __cdecl BitmapTextureResource::Load(const char* name)
{
    PString resourceName(name);
    const unsigned int length = resourceName.Length();
    if (resourceName.node && length >= kSuffixLength)
        resourceName.node = PString::Cache()->Del(resourceName.node, length - kSuffixLength, kSuffixLength);
    const Resource* const loaded = Owner()->Get(resourceName, 0);
    if (bitmapResource) bitmapResource->ReleaseReference();
    bitmapResource = static_cast<const BitmapResource*>(loaded);
    if (bitmapResource) bitmapResource->SetPriority(GetPriority());
    return Result(bitmapResource ? 0 : 1);
}

// 0x10008480: watch the bitmap resource so its load works out the alpha hint and the average colour.
TextureResource::Result __cdecl BitmapTextureResource::LoadComplete()
{
    if (!bitmapResource) return Result(1);
    bitmapResource->AddObserver(this);
    return Result(0);
}

// 0x10008640
void __cdecl BitmapTextureResource::OnLoadNotify() const
{
    if (alphaHint != -1) Resource::OnLoadNotify();
}

// 0x10008660: the observer's OnLoad. The bitmap resource has just loaded.
void __cdecl BitmapTextureResource::OnLoad(const Resource*) const
{
    if (alphaHint == -1) UpdateAlphaHint();
    UpdateAverageColor();
    if (alphaHint == -1) alphaHint = 0;
    if (GetStatus() == STATUS_AVAILABLE) OnLoadNotify();
}

// 0x10008950: ALPHA_HINT_DEFAULT unless it is "work it out" (3): only an ARGB bitmap is inspected, a partly
// transparent pixel means 1, only 0/255 pixels mean 0 when all are opaque and 2 otherwise.
void __cdecl BitmapTextureResource::UpdateAlphaHint() const
{
    const TextureAlpha byDefault = GetDefaultAlphaHint();
    if (byDefault != static_cast<TextureAlpha>(3)) {
        alphaHint = static_cast<int>(byDefault);
        return;
    }
    const Result r = bitmapResource->Lock();
    if (r.code != 0) return;  // the original leaves -1 here
    const Bitmap* const source = BitmapOf(bitmapResource);
    if (source->GetFormat() != Bitmap::ARGB) {
        bitmapResource->Unlock();
        alphaHint = 0;
        return;
    }
    bool opaque = true;
    const unsigned char* row = source->GetPtr() + 3;
    for (int y = 0; y < source->height; ++y, row += source->GetPitch()) {
        const unsigned char* pixel = row;
        for (int x = 0; x < source->width; ++x, pixel += 4) {
            const unsigned int alpha = *pixel;
            if (alpha >= 0xff) continue;
            opaque = false;
            if (alpha > 0) {
                alphaHint = 1;
                bitmapResource->Unlock();
                return;
            }
        }
    }
    bitmapResource->Unlock();
    alphaHint = opaque ? 0 : 2;
}

// 0x10008a60: only while the colour is still all zero. The original sums the first pixel of every row (the
// offsets are its quirk for ARGB and 24-bit alike) and divides by width * height.
void __cdecl BitmapTextureResource::UpdateAverageColor() const
{
    if (averageColor.r > 0 || averageColor.g > 0 || averageColor.b > 0 || averageColor.a > 0) return;
    if (!bitmapResource) return;
    const Result r = bitmapResource->Lock();
    if (r.code != 0) return;
    const Bitmap* const source = BitmapOf(bitmapResource);
    const Bitmap::Format format = source->GetFormat();
    if (format != Bitmap::ARGB && format != Bitmap::XRGB && format != Bitmap::RGB) {
        bitmapResource->Unlock();
        return;
    }
    unsigned int sumR = 0, sumG = 0, sumB = 0;
    const unsigned char* row = source->GetPtr();
    for (int y = 0; y < source->height; ++y, row += source->GetPitch()) {
        sumR += row[0];
        sumG += row[1];
        sumB += row[2];
    }
    const int pixels = source->width * source->height;
    bitmapResource->Unlock();
    const float inverse = 1.0f / static_cast<float>(pixels);
    averageColor.r = static_cast<float>(sumR) * inverse;
    averageColor.g = static_cast<float>(sumG) * inverse;
    averageColor.b = static_cast<float>(sumB) * inverse;
    averageColor.a = 1.0f;
}

// ---------------------------------------------------------------------------------------------------------------
// the fields the texture API reads

// 0x10008620
bool __cdecl BitmapTextureResource::IsAvailable() const
{
    return bitmapResource && bitmapResource->GetStatus() == STATUS_AVAILABLE;
}

NormalMapHint __cdecl BitmapTextureResource::GetNormalMapHint() const { return static_cast<NormalMapHint>(0); }  // 0x1001dec0
TextureAlpha __cdecl BitmapTextureResource::GetAlphaHint() const { return static_cast<TextureAlpha>(alphaHint); }  // 0x10008e40
Color __cdecl BitmapTextureResource::GetAverageColor() const { return averageColor; }                            // 0x10008e50
TextureDimension __cdecl BitmapTextureResource::GetDimension() const { return static_cast<TextureDimension>(0); }  // 0x1001dec0
unsigned int __cdecl BitmapTextureResource::GetDepth() const { return 1; }                                       // 0x10023b90

// 0x100088d0: the bitmap's size, halved until width * height fits the streaming limit's square
Size __cdecl BitmapTextureResource::GetSize() const
{
    Size size{0, 0};
    const Result r = bitmapResource->Lock();
    if (r.code == 0) {
        const Bitmap* const source = BitmapOf(bitmapResource);
        size.w = source->width;
        size.h = source->height;
        bitmapResource->Unlock();
    }
    const int limit = TextureManager::MAX_STREAMING_TEXTURE_SIZE * TextureManager::MAX_STREAMING_TEXTURE_SIZE;
    while (size.w * size.h > limit) {
        size.w >>= 1;
        size.h >>= 1;
    }
    return size;
}

float __cdecl BitmapTextureResource::GetAnisotropyFilterLevel() const  // 0x10008e80
{
    return ANISOTROPY_DEFAULT_TOKEN_VALUE;
}

void __cdecl BitmapTextureResource::GetFileCompression(char* const out) const  // 0x10008e90
{
    GetDefaultFileCompression(out);
}

// 0x10008c10: XRGB stays XRGB, every other bitmap format comes out as DFORMAT_DEFAULT
DFormatId __cdecl BitmapTextureResource::GetImageFormatId() const
{
    DFormatId format = static_cast<DFormatId>(1);
    const Result r = bitmapResource->Lock();
    if (r.code == 0) {
        const Bitmap* const source = BitmapOf(bitmapResource);
        const Bitmap::Format sourceFormat = source->GetFormat();
        bitmapResource->Unlock();
        if (sourceFormat != Bitmap::ARGB && sourceFormat == Bitmap::XRGB) format = static_cast<DFormatId>(3);
    }
    return format;
}

// The texture defaults stand in for a TextureInfo this resource does not have (slots 34..41).
TextureHint __cdecl BitmapTextureResource::GetHint() const { return GetDefaultTextureHint(); }
TextureWrap __cdecl BitmapTextureResource::GetWrapS() const { return GetDefaultWrapS(); }
TextureWrap __cdecl BitmapTextureResource::GetWrapT() const { return GetDefaultWrapT(); }
TextureFilter __cdecl BitmapTextureResource::GetMinFilter() const { return GetDefaultMinFilter(); }
TextureFilter __cdecl BitmapTextureResource::GetMagFilter() const { return GetDefaultMagFilter(); }
TextureFilter __cdecl BitmapTextureResource::GetMipFilter() const { return GetDefaultMipFilter(); }
TextureAutoMip __cdecl BitmapTextureResource::GetAutoMipHint() const { return GetDefaultAutoMip(); }

// 0x10008df0: log2 of the longer side, at least one level
unsigned int __cdecl BitmapTextureResource::GetNumMipLevels() const
{
    const Size size = GetSize();
    int side = size.w > size.h ? size.w : size.h;
    if (side < 2) return 1;
    unsigned int levels = 0;
    while ((side /= 2) != 0) ++levels;
    return levels + 1;
}

// 0x10001580
unsigned int __cdecl BitmapTextureResource::CalculateSize() const { return sizeof(BitmapTextureResource); }
unsigned int __cdecl BitmapTextureResource::CalculateDataSize() const { return 0; }

// 0x100085d0: the bitmap resource is held locked across TextureResource::Save
bool __cdecl BitmapTextureResource::Save(const char* path, Display* display) const
{
    const Result locked = bitmapResource->Lock();
    if (locked.code != 0) return false;
    const bool saved = TextureResource::Save(path, display);
    bitmapResource->Unlock();
    return saved;
}

// 0x10008bd0 (see the header: unnamed in retail and without a caller there)
DisplayFormat& __cdecl BitmapTextureResource::SelectDisplayFormat(DisplayFormat& out) const
{
    out.Set(static_cast<DFormatId>(GetAlphaHint() == static_cast<TextureAlpha>(0) ? 3 : 1));
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// mips: the single bitmap, scaled by Bitmap::Scale

// 0x100086b0
bool __cdecl BitmapTextureResource::GetMipLock() const
{
    if (bitmap) return false;  // already held
    const Result r = bitmapResource->Lock();
    if (r.code != 0) return false;
    const Bitmap* const source = BitmapOf(bitmapResource);
    const Bitmap::Format format = source->GetFormat();
    if (format != Bitmap::ARGB && format != Bitmap::XRGB) {
        bitmapResource->Unlock();
        return false;
    }
    auto* const copy = new (Alloc::New(sizeof(Bitmap))) Bitmap();
    copy->Init(Size{source->width, source->height}, 0, format, source->GetPtr(), 0);
    bitmap = copy;
    return true;
}

// 0x100087f0: an aliased copy when the size matches, else Bitmap::Scale
const Bitmap* __cdecl BitmapTextureResource::GetMip(unsigned int, unsigned int, const Size& size) const
{
    if (!bitmap) return nullptr;
    if (size.w == bitmap->width && size.h == bitmap->height) {
        auto* const copy = new (Alloc::New(sizeof(Bitmap))) Bitmap();
        copy->Init(Size{bitmap->width, bitmap->height}, 0, bitmap->GetFormat(), bitmap->GetPtr(), 0);
        return copy;
    }
    return bitmap->Scale(size.w, size.h);
}

// 0x100087b0
bool __cdecl BitmapTextureResource::GetMipUnlock() const
{
    if (!bitmap) return false;
    DeleteBitmap(bitmap);
    bitmapResource->Unlock();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// the factory

// 0x1003b7c0
Resource* __cdecl BitmapTextureResourceType::Create(const PString& name, const ResourceDatabase* owner) const
{
    return new BitmapTextureResource(name, owner);
}

// 0x1003b880
unsigned int __cdecl BitmapTextureResourceType::GetResourceSize() const
{
    return sizeof(BitmapTextureResource);
}

// 0x10008eb0: a resource made on the fly only when the bitmap file itself exists. The name's last four
// characters ("-tex") go, the database turns the rest into a file name, and File::Exists decides.
const Resource* __cdecl BitmapTextureResourceType::CreateOnTheFly(const PString& name,
                                                                  const ResourceDatabase* owner) const
{
    PString bitmapName(name);
    const unsigned int length = bitmapName.Length();
    if (bitmapName.node) {
        if (length >= kSuffixLength)
            bitmapName.node = PString::Cache()->Del(bitmapName.node, length - kSuffixLength, kSuffixLength);
        const PString fileName = owner->GetFileName(bitmapName);
        if (fileName.node && !FileExists(fileName.Text())) return nullptr;
    }
    return new BitmapTextureResource(name, owner);
}

}  // namespace Jet
