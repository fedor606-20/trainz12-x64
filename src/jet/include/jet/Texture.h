// RenderSystem_7c unit U6 (notes/RenderSystem.md), textures. U6a: TextureResource (the binary .texture resource,
// a JIRF file: an INFO block and one MMAP block per mip level) and BinaryTextureResourceExport (a texture resource
// over a Bitmap, written out with TextureResource::Save). Layouts and slot order follow the TS12 assembly
// (addresses per member in RenderSystem_7c/TextureResource.cpp).
#pragma once

#include <jet/Bitmap.h>
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

// What the streaming manager decides about texture sizes (U6d; its statics stay the original's until then)
class TextureManager {
public:
    static __declspec(dllimport) int MIN_STREAMING_TEXTURE_MIP_COUNT;
    static __declspec(dllimport) int MAX_STREAMING_TEXTURE_SIZE;
};

// The texture a TextureResource makes (U6c; its constructor stays the original's until then)
class TextureRes {
public:
    __declspec(dllimport) TextureRes(const PString& name, TextureManager* manager, const TextureResource* resource);
    JET_HEAP_OBJECT

private:
    unsigned char body[0x60];
};

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

    // the defaults stay the original's until the TextureManager (U6d) and the RenderKernel (U9) move
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

}  // namespace Jet
