// U16: the texture resource made from one bitmap resource (the "bmp-tex", "tga-tex", "bmframe-tex", "pcx-tex" and
// "jpg-tex" factories). Retail: 0x10008360..0x100090a8, vftable 0x1006ee84 (46 slots) with a ResourceObserver
// secondary table at 0x1006ef40, type 0x1003b7c0. 0x84 bytes: TextureResource (0x64) + the observer at +0x64, the
// referenced BitmapResource at +0x68, the worked-out alpha hint at +0x6c (int, -1 unknown), the level-0 bitmap
// made under GetMipLock at +0x70 and the average colour at +0x74.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Resource.h>
#include <jet/Texture.h>

namespace Jet {

class BitmapResource;
class DisplayFormat;

class BitmapTextureResource : public TextureResource, public ResourceObserver {
public:
    BitmapTextureResource(const PString& name, const ResourceDatabase* owner);
    ~BitmapTextureResource() override;
    JET_HEAP_OBJECT

    Result __cdecl Lock() const override;                    // 0: TextureResource's, then the bitmap resource's
    bool __cdecl LoadAll() const override;                   // 2 (folded with AsciiTextureResource::LoadAll)
    Result __cdecl IsAllLoaded() const override;             // 3
    bool __cdecl GetMustResetWhenFinished() const override;  // 7: false
    bool __cdecl IsAvailable() const override;               // 18: the bitmap resource is available

protected:
    Result __cdecl Load(const char* name) override;     // 10: the bitmap resource named without the "-tex" suffix
    Result __cdecl LoadComplete() override;             // 12: watch the bitmap resource
    void __cdecl OnLoadNotify() const override;         // 13: only once the alpha hint is known
    unsigned int __cdecl CalculateSize() const override;      // 15: 0x84
    unsigned int __cdecl CalculateDataSize() const override;  // 16: 0

public:
    NormalMapHint __cdecl GetNormalMapHint() const override;  // 19: 0
    TextureAlpha __cdecl GetAlphaHint() const override;       // 20: worked out from the bitmap
    Color __cdecl GetAverageColor() const override;           // 21
    TextureDimension __cdecl GetDimension() const override;   // 22: 0
    unsigned int __cdecl GetDepth() const override;           // 23: 1
    Size __cdecl GetSize() const override;                    // 24: the bitmap's, clamped by the streaming limit
    float __cdecl GetAnisotropyFilterLevel() const override;  // 25: ANISOTROPY_DEFAULT_TOKEN_VALUE
    void __cdecl GetFileCompression(char* const out) const override;  // 26: FILE_COMPRESSION_DEFAULT
    bool __cdecl Save(const char* path, Display* display) const override;  // 30: around TextureResource::Save

protected:
    DFormatId __cdecl GetImageFormatId() const override;      // 32
    TextureHint __cdecl GetHint() const override;             // 34: HINT_DEFAULT
    TextureWrap __cdecl GetWrapS() const override;            // 35: WRAP_S_DEFAULT
    TextureWrap __cdecl GetWrapT() const override;            // 36: WRAP_T_DEFAULT
    TextureFilter __cdecl GetMinFilter() const override;      // 37: MIN_FILTER_DEFAULT
    TextureFilter __cdecl GetMagFilter() const override;      // 38: MAG_FILTER_DEFAULT
    TextureFilter __cdecl GetMipFilter() const override;      // 39: MIP_FILTER_DEFAULT
    unsigned int __cdecl GetNumMipLevels() const override;    // 40: from the bitmap's longer side
    TextureAutoMip __cdecl GetAutoMipHint() const override;   // 41: AUTO_MIP_DEFAULT
    bool __cdecl GetMipLock() const override;                 // 42: the bitmap as an aliased Bitmap
    const Bitmap* __cdecl GetMip(unsigned int level, unsigned int face, const Size& size) const override;  // 43
    bool __cdecl GetMipUnlock() const override;               // 44

public:
    // Slot 45, the retail vtable's last entry (0x10008bd0): no export names it and the module has no caller.
    // It sets `out` to DFormatId 3 when the bitmap has no alpha and to DFORMAT_DEFAULT otherwise.
    // TODO: recover its original source name once a caller turns up.
    virtual DisplayFormat& __cdecl SelectDisplayFormat(DisplayFormat& out) const;

    void __cdecl OnLoad(const Resource* resource) const override;  // the observer's 0

private:
    void __cdecl UpdateAlphaHint() const;     // 0x10008950
    void __cdecl UpdateAverageColor() const;  // 0x10008a60

    mutable const BitmapResource* bitmapResource;  // +0x68 referenced
    mutable int alphaHint;                         // +0x6c -1 until the bitmap is observed
    mutable Bitmap* bitmap;                        // +0x70 the level-0 bitmap while GetMipLock holds it
    mutable Color averageColor;                    // +0x74
};
JET_LAYOUT32(sizeof(BitmapTextureResource) == 0x84);

// The retail factory (0x1003b7c0 Create, 0x1003b880 GetResourceSize, 0x10008eb0 CreateOnTheFly): it makes a local
// BitmapTextureResource for each of the five bitmap extensions. Local vtable and RTTI.
class BitmapTextureResourceType : public ResourceTypeBase {
public:
    BitmapTextureResourceType(const PString& extension, const UId& id, const UId& parentId)
        : ResourceTypeBase(extension, id, parentId)
    {
    }
    ~BitmapTextureResourceType() override {}
    Resource* __cdecl Create(const PString& name, const ResourceDatabase* owner) const override;  // 3
    unsigned int __cdecl GetResourceSize() const override;                                        // 4: 0x84
    const Resource* __cdecl CreateOnTheFly(const PString& name, const ResourceDatabase* owner) const override;  // 5
};

}  // namespace Jet
