// RenderSystem_7c unit U6 (notes/RenderSystem.md), textures. U6a: TextureResource (the binary .texture resource,
// a JIRF file: an INFO block and one MMAP block per mip level) and BinaryTextureResourceExport (a texture resource
// over a Bitmap, written out with TextureResource::Save). Layouts and slot order follow the TS12 assembly
// (addresses per member in RenderSystem_7c/TextureResource.cpp).
#pragma once

#include <jet/Bitmap.h>
#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/GfxLanguage.h>
#include <jet/RenderTypes.h>
#include <jet/Resource.h>
#include <jet/SmallTypes.h>

namespace Jet {

class Display;
class DisplayTexture;
class JIRFFile;
class Texture;
class TextureResource;

enum TextureDimension {};  // 0 2D, 1 cube map (6 faces at least), ...
enum TextureHint {};
enum TextureWrap {};
enum TextureFilter {};     // 2: the default (TextureResource's *_FILTER_DEFAULT)
enum TextureAlpha {};      // 1 none, 2 one bit, else full
enum TextureAutoMip {};
enum NormalMapHint {};
enum ModifyMap {};         // 0 none, 1 flip green

class TextureManager;

class DisplayFormat;
class TextureManagerInfo;

// U6c2: a named texture of the TextureManager (ReferenceCount + 0x2c): it registers with the manager (its slots 4
// add, 6 remove, 9 activate; +0x28 the current frame) and wraps the API texture. TS12 layout and slot order.
class Texture : public ReferenceCount {
public:
    Texture(const PString& name, TextureManager* manager);
    Texture(const PString& name, TextureManager* manager, DisplayTexture* texture, TextureAlpha alpha);

protected:
    ~Texture() override;  // 1 (0 is ReferenceCount::DeleteThis)

public:
    virtual Texture* __cdecl CreateInstance() const;                 // 2
    virtual bool __cdecl IsAvailable() const;                        // 3
    virtual bool __cdecl IsLoadFailure() const;                      // 4: false
    virtual void __cdecl SetName(PString& name);                     // 5
    virtual const PString& __cdecl GetResourceName() const;          // 6: the name
    virtual const TextureResource* __cdecl GetResource() const;      // 7: null
    virtual unsigned long __cdecl GetMemorySize() const;             // 8
    virtual TextureAlpha __cdecl GetAlphaHint() const;               // 9
    virtual Color __cdecl GetAverageColor() const;                   // 10
    virtual TextureDimension __cdecl GetDimension() const;           // 11
    virtual unsigned int __cdecl GetDepth() const;                   // 12
    virtual Size __cdecl GetSize() const;                            // 13
    virtual Size __cdecl GetAbsoluteSize() const;                    // 14
    virtual void __cdecl Activate(int how);                          // 15
    virtual void __cdecl Update(float time);                         // 16: nothing
    virtual void __cdecl Bind(unsigned int unit) const;              // 17
    virtual void __cdecl SetResourcePriority(float priority) const;  // 18: nothing
    virtual bool __cdecl DynamicSubload(int x, int y, const Bitmap& bitmap, DisplayFormat& format);  // 19
    virtual void __cdecl SetDisplayTexture(DisplayTexture* texture);  // 20
    virtual DisplayTexture* __cdecl GetDisplayTexture() const;        // 21
    virtual TextureManagerInfo* __cdecl GetTextureManagerInfo();      // 22: null

    const PString& __cdecl GetName() const;
    PString __cdecl GetNonUniqueName() const;
    bool __cdecl GetActive() const;
    unsigned int __cdecl GetActiveFrame() const;
    void __cdecl SetActiveFrame(unsigned int frame);
    bool __cdecl GetRequiresUpdate() const;
    void SetAlphaHint(TextureAlpha alpha);  // __thiscall

    JET_HEAP_OBJECT

protected:
    friend class TextureManager;

    PString __cdecl GenerateUniqueName(const PString& name) const;
    TextureManager* __cdecl GetTextureManager() const;
    void __cdecl InitInstance(Texture& to) const;
    void __cdecl SetRequiresUpdate(bool on);

    static __declspec(dllimport) unsigned long uniqueTexId;  // stays the original's (U6d)

    int unused08;              // +0x08
    unsigned char flags;       // +0x0c bit 0 requires an update (the rest is not written)
    PString name;              // +0x10 "%08x%%name" for an instance
    unsigned int activeFrame;  // +0x14
    DisplayTexture* texture;   // +0x18 owned
    TextureAlpha alpha;        // +0x1c -1 unknown
    Color averageColor;        // +0x20
    TextureManager* manager;   // +0x30 holds a reference
};
JET_LAYOUT32(sizeof(Texture) == 0x34);

// The texture a TextureResource makes (0x60; its code stays the original's until U6d1b). Only what the
// TextureManager reads: the streaming request (+0x4c, its state at +0x14: 1 down size, 2 async load) and the mip
// levels on disk (+0x50).
class TextureRes : public Texture, public ResourceObserver {
public:
    __declspec(dllimport) TextureRes(const PString& name, TextureManager* manager, const TextureResource* resource);

private:
    __declspec(dllimport) void __cdecl OnLoad(const Resource* resource) const override;

    friend class TextureManager;
    unsigned char body38[0x14];
    void* asyncInfo;          // +0x4c
    int mipLevelsOnDisk;      // +0x50
    unsigned char body54[0xc];
};
JET_LAYOUT32(sizeof(TextureRes) == 0x60);

class TextureResource : public Resource {
public:
    // the loaded texture (0x68): the INFO block, then the mip levels on disk and the pixel blocks
    struct TextureInfo {
        TextureInfo();
        ~TextureInfo();
        void FreeBlocks();  // 0x10047010

        unsigned int version;          // +0x00 (0x108 once read)
        TextureDimension dimension;    // +0x04
        unsigned int mipLevels;        // +0x08
        unsigned int depth;            // +0x0c faces
        DFormatId imageFormat;         // +0x10
        Size size;                     // +0x14
        TextureHint hint;              // +0x1c
        TextureWrap wrapS, wrapT;      // +0x20
        int unused28;                  // +0x28
        TextureFilter minFilter;       // +0x2c
        TextureFilter magFilter;       // +0x30
        TextureFilter mipFilter;       // +0x34
        TextureAlpha alphaHint;        // +0x38
        float anisotropy;              // +0x3c
        TextureAutoMip autoMip;        // +0x40
        char compression[8];           // +0x44 (not always terminated)
        NormalMapHint normalMapHint;   // +0x4c
        Color averageColor;            // +0x50
        int levelsOnDisk;              // +0x60
        unsigned int** blocks;         // +0x64 levels x depth, each {size, bytes}; largest level first
        JET_HEAP_OBJECT
    };

    TextureResource(const PString& name, const ResourceDatabase* owner);
    TextureResource(const TextureResource& o);
    TextureResource& operator=(const TextureResource& o);

    Result __cdecl Lock() const override;                  // 0: reloads unloaded data
    bool __cdecl GetMustResetWhenFinished() const override;  // 7: true

    // (new virtuals take their slots by where the name first appears in the class: Load's override comes after
    // the new Load, which belongs at 28)
    virtual bool __cdecl IsAvailable() const;              // 18
    virtual NormalMapHint __cdecl GetNormalMapHint() const;
    virtual TextureAlpha __cdecl GetAlphaHint() const;     // 20
    virtual Color __cdecl GetAverageColor() const;
    virtual TextureDimension __cdecl GetDimension() const;
    virtual unsigned int __cdecl GetDepth() const;
    virtual Size __cdecl GetSize() const;                  // 24
    virtual float __cdecl GetAnisotropyFilterLevel() const;
    virtual void __cdecl GetFileCompression(char* const out) const;
    virtual unsigned int __cdecl GetFileCompressionRatio() const;  // 27: 5
    virtual Result __cdecl Load(DisplayTexture* texture) const;   // 28: into the API texture
    Result __cdecl Load(const char* name) override;               // 10
    virtual Texture* __cdecl CreateInstance(const PString& name, TextureManager* manager) const;  // 29
    virtual bool __cdecl Save(const char* path, Display* display) const;  // 30
    virtual DFormatId __cdecl GetFormatId() const;         // 31: slot 32

protected:
    virtual DFormatId __cdecl GetImageFormatId() const;    // 32
    virtual DFormatId __cdecl GetDisplayFormatId() const;  // 33
    virtual TextureHint __cdecl GetHint() const;           // 34
    virtual TextureWrap __cdecl GetWrapS() const;
    virtual TextureWrap __cdecl GetWrapT() const;
    virtual TextureFilter __cdecl GetMinFilter() const;    // 37
    virtual TextureFilter __cdecl GetMagFilter() const;
    virtual TextureFilter __cdecl GetMipFilter() const;
    virtual unsigned int __cdecl GetNumMipLevels() const;  // 40
    virtual TextureAutoMip __cdecl GetAutoMipHint() const;

private:
    virtual bool __cdecl GetMipLock() const;               // 42: true
    virtual const Bitmap* __cdecl GetMip(unsigned int level, unsigned int face, const Size& size) const;  // 43
    virtual bool __cdecl GetMipUnlock() const;             // 44: true

public:
    ~TextureResource() override;

    const Bitmap* __cdecl GetBitmap(unsigned int level, unsigned int face, const Size& size) const;
    bool __cdecl CanAsyncLoadMips() const;
    int __cdecl GetMipLevelCountOnDisk() const;
    void __cdecl UnloadDataOnly();
    Size GetMipSizeAtReverseLevel(int level) const;  // __thiscall

    static Size __cdecl GetMipSizeAtReverseLevel(int level, int width, int height);
    static Size __cdecl GetMipSizeAtReverseLevel(int level, Size size);
    static Result __cdecl Load(const ResourceDatabase* db, const char* name, const String& key, int& first,
                               int& last, TextureInfo* info);
    static Result __cdecl LoadMipLevels(TextureInfo* info, DisplayTexture* texture, int first, int last);

    static TextureWrap __cdecl GetDefaultWrapS();
    static void __cdecl SetDefaultWrapS(TextureWrap w);
    static TextureWrap __cdecl GetDefaultWrapT();
    static void __cdecl SetDefaultWrapT(TextureWrap w);
    static TextureFilter __cdecl GetDefaultMinFilter();
    static void __cdecl SetDefaultMinFilter(TextureFilter f);
    static TextureFilter __cdecl GetDefaultMagFilter();
    static void __cdecl SetDefaultMagFilter(TextureFilter f);
    static TextureFilter __cdecl GetDefaultMipFilter();
    static void __cdecl SetDefaultMipFilter(TextureFilter f);
    static TextureAutoMip __cdecl GetDefaultAutoMip();
    static void __cdecl SetDefaultAutoMip(TextureAutoMip m);
    static TextureAlpha __cdecl GetDefaultAlphaHint();
    static void __cdecl SetDefaultAlphaHint(TextureAlpha a);
    static TextureHint __cdecl GetDefaultTextureHint();
    static void __cdecl SetDefaultTextureHint(TextureHint h);
    static DFormatId __cdecl GetDefaultDisplayFormatId();
    static void __cdecl SetDefaultDisplayFormatId(DFormatId f);
    static float __cdecl GetDefaultAnisotropyFilterLevel();
    static void __cdecl SetDefaultAnisotropyFilterLevel(float a);
    static void __cdecl GetDefaultFileCompression(char* const out);
    static void __cdecl SetDefaultFileCompression(char* name);

    static __declspec(dllimport) float ANISOTROPY_DEFAULT_TOKEN_VALUE;

protected:
    unsigned int __cdecl CalculateSize() const override;      // 15: 100
    unsigned int __cdecl CalculateDataSize() const override;  // 16
    bool SaveLockedInternal(JIRFFile& file, Display* display) const;  // __thiscall

private:
    bool __cdecl Unload() override;  // 14

    unsigned char* __cdecl GetData(unsigned int& size, unsigned int level, unsigned int face) const;
    bool __cdecl Save(JIRFFile& file, Display* display) const;
    bool __cdecl SaveMipMap(JIRFFile& file, int level, int levels, const Size& size, const Size& top,
                            Display* display) const;
    static const Bitmap* __cdecl GetMip(unsigned int level, unsigned int face, const Size& size, TextureInfo* info);

    // the defaults stay the original's until the RenderKernel (U9) moves; the TextureManager sets them in Init
    friend class TextureManager;
    static __declspec(dllimport) TextureWrap WRAP_S_DEFAULT;
    static __declspec(dllimport) TextureWrap WRAP_T_DEFAULT;
    static __declspec(dllimport) TextureFilter MIN_FILTER_DEFAULT;
    static __declspec(dllimport) TextureFilter MAG_FILTER_DEFAULT;
    static __declspec(dllimport) TextureFilter MIP_FILTER_DEFAULT;
    static __declspec(dllimport) TextureAutoMip AUTO_MIP_DEFAULT;
    static __declspec(dllimport) TextureAlpha ALPHA_HINT_DEFAULT;
    static __declspec(dllimport) TextureHint HINT_DEFAULT;
    static __declspec(dllimport) DFormatId DISPLAYFORMATID_DEFAULT;
    static __declspec(dllimport) float ANISOTROPY_DEFAULT;
    static __declspec(dllimport) char* FILE_COMPRESSION_DEFAULT;  // (its 8 bytes are used as a char[8])
    static __declspec(dllimport) NormalMapHint NORMAL_MAP_HINT_DEFAULT;

protected:
    TextureInfo* info;   // +0x5c
    bool dataUnloaded;   // +0x60 (UnloadDataOnly; Lock loads again)
};
JET_LAYOUT32(sizeof(TextureResource) == 0x64);

// A texture resource over one Bitmap, set up field by field and written with Save (0xa8)
class BinaryTextureResourceExport : public TextureResource {
public:
    explicit BinaryTextureResourceExport(const PString& name);
    BinaryTextureResourceExport(const BinaryTextureResourceExport& o);
    BinaryTextureResourceExport& operator=(const BinaryTextureResourceExport& o);
    ~BinaryTextureResourceExport() override;

    bool __cdecl GetMustResetWhenFinished() const override;  // false
    NormalMapHint __cdecl GetNormalMapHint() const override;  // 0
    TextureAlpha __cdecl GetAlphaHint() const override;
    TextureDimension __cdecl GetDimension() const override;
    unsigned int __cdecl GetDepth() const override;
    Size __cdecl GetSize() const override;  // the bitmap's
    float __cdecl GetAnisotropyFilterLevel() const override;
    void __cdecl GetFileCompression(char* const out) const override;
    unsigned int __cdecl GetFileCompressionRatio() const override;
    DFormatId __cdecl GetImageFormatId() const override;
    DFormatId __cdecl GetDisplayFormatId() const override;
    TextureHint __cdecl GetHint() const override;
    TextureWrap __cdecl GetWrapS() const override;
    TextureWrap __cdecl GetWrapT() const override;
    TextureFilter __cdecl GetMinFilter() const override;
    TextureFilter __cdecl GetMagFilter() const override;
    TextureFilter __cdecl GetMipFilter() const override;
    unsigned int __cdecl GetNumMipLevels() const override;
    TextureAutoMip __cdecl GetAutoMipHint() const override;
    const Bitmap* __cdecl GetMip(unsigned int level, unsigned int face, const Size& size) const override;

    void __cdecl SetNumMipLevels(unsigned int n);
    void __cdecl SetDimension(TextureDimension d);
    void __cdecl SetDisplayFormatId(DFormatId f);
    void __cdecl SetImageFormatId(DFormatId f);
    void __cdecl SetDepth(unsigned int d);
    void __cdecl SetHint(TextureHint h);
    void __cdecl SetWrapS(TextureWrap w);
    void __cdecl SetWrapT(TextureWrap w);
    void __cdecl SetMinFilter(TextureFilter f);
    void __cdecl SetMagFilter(TextureFilter f);
    void __cdecl SetMipFilter(TextureFilter f);
    void __cdecl SetAlphaHint(TextureAlpha a);
    void __cdecl SetAnisotropyFilterLevel(float a);
    void __cdecl SetAutoMipHint(TextureAutoMip m);
    void __cdecl SetFileCompression(const AnsiString& name);
    void __cdecl SetFileCompressionRatio(unsigned int r);
    void __cdecl SetData(Bitmap* bitmap, DFormatId format);  // both formats

private:
    DFormatId imageFormat;     // +0x64
    DFormatId displayFormat;   // +0x68
    Bitmap* bitmap;            // +0x6c
    unsigned int mipLevels;    // +0x70
    unsigned int depth;        // +0x74
    unsigned int ratio;        // +0x78
    float anisotropy;          // +0x7c
    TextureDimension dimension;  // +0x80
    TextureHint hint;          // +0x84
    TextureWrap wrapS, wrapT;  // +0x88
    TextureFilter minFilter;   // +0x90
    TextureFilter magFilter;   // +0x94
    TextureFilter mipFilter;   // +0x98
    TextureAlpha alphaHint;    // +0x9c
    TextureAutoMip autoMip;    // +0xa0
    AnsiString compression;    // +0xa4
};
JET_LAYOUT32(sizeof(BinaryTextureResourceExport) == 0xa8);

class BitmapResource;
class ConfigData;

// U6c1: the API texture base (0x38) every renderer's texture derives from (RenderOpenGL / RenderDirectX9 inline
// its fields), and the null renderer's one (0x70). Slot order and layouts follow TS12 (addresses in
// RenderSystem_7c/DisplayTexture.cpp). The pure slots answer 0 through the module's _purecall, as in TS12.
class DisplayTexture {
public:
    struct DisplayTextureStats;
    struct DisplayTextureFrameStats;

    DisplayTexture(const DisplayTexture& o);
    DisplayTexture& operator=(const DisplayTexture& o);
    virtual ~DisplayTexture();  // 0

    virtual bool __cdecl Create(const Size& size, const DisplayFormat& format, TextureDimension dimension,
                                unsigned int mips, TextureAutoMip autoMip, unsigned int depth, float anisotropy,
                                TextureHint hint, TextureFilter minFilter, TextureFilter magFilter,
                                TextureFilter mipFilter, TextureWrap wrapS, TextureWrap wrapT,
                                const RenderTextureConstruct& construct) = 0;  // 1
    virtual void __cdecl Destroy() = 0;                                         // 2
    virtual bool __cdecl Load(unsigned int level, DisplayFormat& format, const Bitmap& bitmap,
                              unsigned int face) = 0;                           // 3
    virtual bool __cdecl SubLoad(unsigned int level, const Rect& area, DisplayFormat& format, const Bitmap& bitmap,
                                 unsigned int face) = 0;                        // 4
    virtual bool __cdecl SubloadFromFrame(unsigned int level, const Point& at, const Rect& area,
                                          unsigned int face);                   // 5: true
    virtual bool __cdecl CreateRenderContext();                                 // 6: true
    // (overloaded virtuals take their slots in reverse declaration order: 7 is the long Bind, 8 this one)
    virtual bool __cdecl Bind(unsigned int unit) const = 0;
    virtual bool __cdecl Bind(unsigned int unit, bool keep, float anisotropy, TextureWrap wrapS, TextureWrap wrapT,
                              TextureFilter minFilter, TextureFilter magFilter, TextureFilter mipFilter) = 0;
    virtual void __cdecl CopyMipToMip(DisplayTexture* from, int fromLevel, DisplayTexture* to, int toLevel) = 0;  // 9
    virtual DisplayTexture* __cdecl CreateInstance() const = 0;                 // 10
    virtual Bitmap* __thiscall GetBitmap(unsigned int level, unsigned int face, DisplayFormat& format,
                                         DFormatId as) const = 0;               // 11
    virtual bool __cdecl GetDataLost() const;                                   // 12
    virtual DFormatId __cdecl GetJetFormat() const = 0;                         // 13
    virtual void __cdecl CopySamplerAttributesFrom(DisplayTexture* from) = 0;  // 14
    virtual void __cdecl RefreshFromChangedSettings();                         // 15: nothing
    virtual void* __cdecl GetAPITexture() const;                               // 16: null

    bool __cdecl SubLoad(const Bitmap& bitmap, const Rect& area, DisplayFormat& format, unsigned int face);
    Display* __cdecl GetDisplay() const;
    const Size& __cdecl GetSize() const;
    TextureDimension __cdecl GetDimension() const;
    unsigned int __cdecl GetDepthLevels() const;
    unsigned int __cdecl GetNumMipLevels() const;
    unsigned int __cdecl GetMemorySize() const;
    void __cdecl SetActiveRenderLevel(unsigned int level);
    unsigned int __cdecl GetActiveRenderLevel() const;
    TextureTarget __cdecl GetRenderTargetType() const;
    void __cdecl SetDataLost(bool lost);
    bool __cdecl GetCanMip() const;
    bool __cdecl GetAutoMip() const;
    TextureAutoMip __cdecl GetAutoMipHint() const;
    float __cdecl GetAnisotropy() const;
    static int __cdecl GetNumMipLevels(const Size& size);
    static Size __cdecl GetMipLevelSize(const Size& size, unsigned int level);  // (exported with a hidden pointer)
    static const DisplayTextureStats& __cdecl GetTextureStats();
    static void __cdecl EndFrameStats();

    JET_HEAP_OBJECT

protected:
    explicit DisplayTexture(Display* display);
    bool __cdecl CheckInitialized(const char* what) const;
    void __cdecl InitInstance(DisplayTexture& to) const;
    void __cdecl LogInitFailure(const char* what) const;

    static __declspec(dllimport) DisplayTextureStats textureStats;
    static __declspec(dllimport) DisplayTextureFrameStats frameStats;

    int state;                         // +0x04 1 when made (CheckInitialized)
    unsigned int memorySize;           // +0x08
    Display* display;                  // +0x0c
    Size size;                         // +0x10
    TextureDimension dimension;        // +0x18
    unsigned int depthLevels;          // +0x1c
    unsigned int mipLevels;            // +0x20
    float requestedAnisotropy;         // +0x24
    float anisotropy;                  // +0x28
    unsigned char flags;               // +0x2c bit 0 compressed, 1 can mip, 2 data lost (the rest is not written)
    unsigned char pad2d[3];
    char autoMip;                     // +0x30 a TextureAutoMip
    RenderTextureConstruct* construct; // +0x34 a render target's
};
JET_LAYOUT32(sizeof(DisplayTexture) == 0x38);

class DisplayNull;
class NULLFormat;
class OPENGL_INTERFACE_CLASS;
struct PBufferContext;

class DisplayTextureNull : public DisplayTexture {
public:
    explicit DisplayTextureNull(Display* display);
    DisplayTextureNull(const DisplayTextureNull& o);
    DisplayTextureNull& operator=(const DisplayTextureNull& o);
    ~DisplayTextureNull() override;

    bool __cdecl Create(const Size& size, const DisplayFormat& format, TextureDimension dimension, unsigned int mips,
                        TextureAutoMip autoMip, unsigned int depth, float anisotropy, TextureHint hint,
                        TextureFilter minFilter, TextureFilter magFilter, TextureFilter mipFilter, TextureWrap wrapS,
                        TextureWrap wrapT, const RenderTextureConstruct& construct) override;
    void __cdecl Destroy() override;
    bool __cdecl Load(unsigned int level, DisplayFormat& format, const Bitmap& bitmap, unsigned int face) override;
    bool __cdecl SubLoad(unsigned int level, const Rect& area, DisplayFormat& format, const Bitmap& bitmap,
                         unsigned int face) override;
    bool __cdecl SubloadFromFrame(unsigned int level, const Point& at, const Rect& area, unsigned int face) override;
    bool __cdecl CreateRenderContext() override;
    bool __cdecl Bind(unsigned int unit) const override;
    bool __cdecl Bind(unsigned int unit, bool keep, float anisotropy, TextureWrap wrapS, TextureWrap wrapT,
                      TextureFilter minFilter, TextureFilter magFilter, TextureFilter mipFilter) override;
    void __cdecl CopyMipToMip(DisplayTexture* from, int fromLevel, DisplayTexture* to, int toLevel) override;
    DisplayTexture* __cdecl CreateInstance() const override;
    Bitmap* __thiscall GetBitmap(unsigned int level, unsigned int face, DisplayFormat& format,
                                 DFormatId as) const override;
    bool __cdecl GetDataLost() const override;
    DFormatId __cdecl GetJetFormat() const override;
    void __cdecl CopySamplerAttributesFrom(DisplayTexture* from) override;

    PBufferContext* __cdecl GetRenderContext() const;

protected:
    DisplayNull* __cdecl GetDisplay() const;
    OPENGL_INTERFACE_CLASS* __cdecl GetWinInterface() const;
    Bitmap* __cdecl GetTextureBitmap(int level, unsigned int face, DisplayFormat& format, DFormatId as) const;
    NULLFormat* __cdecl CheckFormat(DisplayFormat& format, void*& data, int& pitch, int& a, int& b, int width,
                                    int height);
    void __cdecl BindTexture(unsigned int unit) const;
    void __cdecl UnbindRenderTexture(unsigned int unit) const;
    void __cdecl InitInstance(DisplayTextureNull& to) const;
    void __cdecl SetBasicState() const;

    // the null display's statics stay the original's until the Display unit (U9)
    static __declspec(dllimport) bool anisotropySupport;
    static __declspec(dllimport) bool canAutoMip;
    static __declspec(dllimport) unsigned int numObjects;
    static __declspec(dllimport) const DisplayTextureNull** bound;  // 1000 texture units

    int unused38;                  // +0x38 -1
    int renderContext;             // +0x3c
    unsigned int firstLevel;       // +0x40 the smallest level loaded, -1 none
    TextureFilter minFilter;       // +0x44
    TextureFilter magFilter;       // +0x48
    TextureWrap wrapS;             // +0x4c
    TextureWrap wrapT;             // +0x50
    int unused54;                  // +0x54 (never written)
    int unused58;                  // +0x58
    int formatId;                  // +0x5c
    mutable bool dirty;            // +0x60
    mutable bool bound61;          // +0x61
    mutable int bindCount;         // +0x64 units holding it
    bool flag68;                   // +0x68 cleared for a depth format
    Bitmap* image;                 // +0x6c the level 0 pixels kept
};
JET_LAYOUT32(sizeof(DisplayTextureNull) == 0x70);

}  // namespace Jet

struct EnumCheck;  // {const char* name; int value} (global, as TS12 mangles it)

namespace Jet {

// U6b: a texture made of bitmap resources, described by a .texture.txt (ConfigData "Texture { primary ...
// alpha ... }", or the obsolete "key=value" lines). The bitmaps are other resources of the same database: the
// texture watches them (ResourceObserver) and builds its mip 0 images from them under GetMipLock (0xd8).
class AsciiTextureResource : public TextureResource, public ResourceObserver {
public:
    AsciiTextureResource(const AsciiTextureResource& o);
    AsciiTextureResource& operator=(const AsciiTextureResource& o);
    AsciiTextureResource(const PString& name, const ResourceDatabase* owner);

protected:
    Result __cdecl Lock() const override;  // 0

public:
    bool __cdecl LoadAll() const override;                   // 2
    Result __cdecl IsAllLoaded() const override;             // 3
    bool __cdecl GetMustResetWhenFinished() const override;  // 7: false
    ~AsciiTextureResource() override;                        // 8

protected:
    Result __cdecl LoadTxt(const char* name) override;  // 11
    Result __cdecl LoadComplete() override;             // 12
    void __cdecl OnLoadNotify() const override;         // 13
    unsigned int __cdecl CalculateSize() const override;      // 15: 0xd8
    unsigned int __cdecl CalculateDataSize() const override;  // 16: 0

public:
    bool __cdecl IsAvailable() const override;                // 18
    NormalMapHint __cdecl GetNormalMapHint() const override;  // 19
    TextureAlpha __cdecl GetAlphaHint() const override;
    Color __cdecl GetAverageColor() const override;
    TextureDimension __cdecl GetDimension() const override;
    unsigned int __cdecl GetDepth() const override;
    Size __cdecl GetSize() const override;                    // 24
    float __cdecl GetAnisotropyFilterLevel() const override;
    void __cdecl GetFileCompression(char* const out) const override;
    bool __cdecl Save(const char* path, Display* display) const override;  // 30
    DFormatId __cdecl GetImageFormatId() const override;      // 32
    DFormatId __cdecl GetDisplayFormatId() const override;
    TextureHint __cdecl GetHint() const override;             // 34
    TextureWrap __cdecl GetWrapS() const override;
    TextureWrap __cdecl GetWrapT() const override;
    TextureFilter __cdecl GetMinFilter() const override;
    TextureFilter __cdecl GetMagFilter() const override;
    TextureFilter __cdecl GetMipFilter() const override;
    unsigned int __cdecl GetNumMipLevels() const override;    // 40
    TextureAutoMip __cdecl GetAutoMipHint() const override;

protected:
    bool __cdecl GetMipLock() const override;  // 42
    const Bitmap* __cdecl GetMip(unsigned int level, unsigned int face, const Size& size) const override;
    bool __cdecl GetMipUnlock() const override;  // 44

public:
    virtual ModifyMap __cdecl GetModifyMap() const;  // 45
    virtual bool __cdecl UseMipMaps() const;         // 46

protected:
    virtual bool __cdecl LoadConfigData(const ConfigData& config);    // 47
    virtual const Bitmap* __cdecl GetImage(unsigned int face) const;  // 48
    void __cdecl OnLoad(const Resource* resource) const override;     // the observer's 0

public:
    static void __cdecl SetUsePath(bool on);

protected:
    bool __cdecl AllValidBitmapsAvailable() const;
    bool __cdecl ParseObsoleteTxtFormat(const char* name, ConfigData& config);
    bool __cdecl LoadTextureOptions(const ConfigData& config);
    bool __cdecl LookUpEnum(String& text, const EnumCheck* table, unsigned int count, int& value);
    const Bitmap* __cdecl GetBitmapImage(const Bitmap* image, const Bitmap* alpha) const;
    void __cdecl UpdateAlphaHint() const;
    void __cdecl UpdateAlphaHintStrict() const;
    void __cdecl UpdateAverageColor() const;
    void CopyFields(const AsciiTextureResource& o);  // (inlined by the original)

    // written by RenderKernel::Init (U9): stays the original's until then
    static __declspec(dllimport) bool noPath;

    // flat {count, capacity, items} arrays growing in steps of 6 (0x10007a40 / 0x10007af0 / copy 0x10007a70)
    mutable Array<const BitmapResource*, 6> primary;  // +0x68 one per face
    mutable Array<const BitmapResource*, 6> alpha;    // +0x74 none, or one per face
    mutable Array<Bitmap*, 6> images;                 // +0x80 the combined images while GetMipLock holds them
    char wrapS, wrapT;                            // +0x8c
    char minFilter, magFilter, mipFilter;         // +0x8e
    mutable char alphaHint;                       // +0x91 -1 until worked out from the bitmaps
    char hint;                                    // +0x92
    char compression;                             // +0x93 none, dxt1, dxt3, dxt5, atc, atce, atci, (7 unset), etc1 9
    char autoMip;                                 // +0x94
    char pad95[3];                                // (never written, nor copied)
    bool useMipMaps;                              // +0x98
    char pad99[3];
    char dimension;                               // +0x9c two, cube, volume, one
    char pad9d[3];
    unsigned int depth;                         // +0xa0
    float anisotropy;                             // +0xa4
    char fileCompression[8];                      // +0xa8
    NormalMapHint normalMapHint;                  // +0xb0
    ModifyMap modifyMap;                          // +0xb4
    mutable Color averageColor;                   // +0xb8 zero until worked out from the bitmaps
    mutable bool locked;                          // +0xc8 GetMipLock's figures below are valid
    mutable Size lockedSize;                      // +0xcc
    mutable unsigned int lockedMips;              // +0xd4
};
JET_LAYOUT32(sizeof(AsciiTextureResource) == 0xd8);


// ---- U6d1a (RenderSystem_7c/TextureManager.cpp)

class RenderKernel;
class ResourceDatabase;

// Hands out the named Textures (ReferenceCount + 0xd0): a hash table by name, the textures needing an update each
// frame, the frame's active textures with their memory, and the streaming queues (U6d1b; their code stays the
// original's until then). TS12 layout and slot order (vftable 0x10072338).
class TextureManager : public ReferenceCount {
public:
    TextureManager();
    ~TextureManager() override;  // 1 (0 is ReferenceCount::DeleteThis)

    virtual bool __cdecl Init(RenderKernel* kernel);                                                   // 2
    virtual Texture* __cdecl Create(const ResourceDatabase* db, const PString& name, const String& key);  // 3
    virtual void __cdecl Add(Texture* texture);                                                        // 4
    virtual void __cdecl RegisterStreamableTexture(TextureRes* texture);                               // 5
    virtual void __cdecl RemoveTexture(Texture* texture);                                              // 6
    virtual Texture* __cdecl Find(const PString& name);                                                // 7
    virtual void __cdecl Begin();                                                                      // 8
    virtual void __cdecl Activate(Texture* texture, int mips);                                         // 9
    virtual void __cdecl End(float time);                                                              // 10

    unsigned long __cdecl GetTotalTextureMemory() const;
    unsigned long __cdecl GetActiveTextureMemory() const;
    RenderKernel* __cdecl GetRenderKernel();
    void __cdecl UpSizeTexture(const TextureRes* texture, int size);  // U6d1b: the original's

    static void __cdecl SetMinSizeTextureStreaming(int size);
    static void __cdecl SetMaxSizeTextureStreaming(int size);
    static void __cdecl SetEnableTextureStreaming(bool on);

    // the original's until the RenderKernel (U9) moves
    static __declspec(dllimport) int MIN_STREAMING_TEXTURE_MIP_COUNT;
    static __declspec(dllimport) int MAX_STREAMING_TEXTURE_MIP_COUNT;
    static __declspec(dllimport) int MIN_STREAMING_TEXTURE_SIZE;
    static __declspec(dllimport) int MAX_STREAMING_TEXTURE_SIZE;
    static __declspec(dllimport) bool TEXTURE_STREAMING_ENABLED;

    JET_HEAP_OBJECT

protected:
    friend class Texture;

    void __cdecl SetTextureUpdate(Texture* texture, bool on);
    Texture* __cdecl GetSubstituteTexture();
    unsigned int __cdecl GetActiveFrame() const;
    // U6d1b: the original's
    void __cdecl Update();
    bool __cdecl RemovedAsyncTextureLoad(const TextureRes* texture);
    bool __cdecl RemovedTextureDownSize(const TextureRes* texture);
    void CleanupAsyncTextureLoad();  // __thiscall

    static __declspec(dllimport) unsigned int globalSkipLevels;
    static __declspec(dllimport) unsigned int globalSkipThreshold;

    struct TextureHash {};
    // the hash table by name node (0x10042d80 init, 0x10035db0 clear, its buckets freed last)
    struct Textures : HashTable<PString, Texture*, TextureHash, 13> {
        Textures() { Init(); }
        ~Textures() { Destroy(); }
    };
    // an intrusive ring of streaming requests (0x10042d00 / 0x10042d30); a node's `owner` is its list's head
    struct ListHead {
        ListHead();
        ~ListHead();
        ListHead* owner;
        ListHead* prev;
        ListHead* next;
        int unused;
    };

    Textures textures;                                   // +0x08
    RenderKernel* kernel;                                // +0x14
    FastDynamicArray<Texture*, 13> updates;              // +0x18 their slot 16 Update runs in End
    Array<Texture*, 256> active;                         // +0x1c this frame's (0x10043130 grows by 256)
    unsigned int frame;                                  // +0x28
    unsigned long activeMemory;                          // +0x2c KB
    bool useSubstitute;                                  // +0x30
    FastDynamicArray<void*, 13> queue34;                 // +0x34 (U6d1b)
    int field38;                                         // +0x38
    TextureResource::TextureInfo info;                   // +0x3c (U6d1b)
    int fieldA4;                                         // +0xa4
    int fieldA8;                                         // +0xa8
    FastDynamicArray<void*, 13> queueAC;                 // +0xac (U6d1b)
    ListHead lists[2];                                   // +0xb0
    int listIndex;                                       // +0xd0 the list RegisterStreamableTexture links into
    int fieldD4;                                         // +0xd4 1
};
JET_LAYOUT32(sizeof(TextureManager) == 0xd8);

}  // namespace Jet
