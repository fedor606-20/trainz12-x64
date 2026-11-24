// RenderOpenGL_7c: the OpenGL display texture (unit 3 of the port).
#pragma once

#include <windows.h>

#include <GL/gl.h>

#include <jet/Bitmap.h>
#include <jet/RenderTypes.h>
#include <jet/Texture.h>

namespace Jet {

class DisplayOpenGL;
class DisplayInterfaceWGL;
struct PBufferContext;

// a display format as the GL backend consumes it
class OpenGLFormat {
public:
    unsigned char flags;      // bit 0 supported, bit 1 compressed, bit 2 depth
    unsigned char blockBytes; // compressed block size (0 for uncompressed)
    GLint internalFormat;
    GLenum format;
    GLenum type;
    DFormatId convertTo;      // the format this one is converted to when unsupported
    int minWidth;
    int minHeight;
};

class DisplayTextureOpenGL : public DisplayTexture {
public:
    explicit DisplayTextureOpenGL(Display* display);
    DisplayTextureOpenGL(const DisplayTextureOpenGL& o);
    ~DisplayTextureOpenGL() override;
    DisplayTextureOpenGL& operator=(const DisplayTextureOpenGL& o);

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
    bool __cdecl Bind(unsigned int unit, bool keep, float anisotropy, TextureWrap wrapS, TextureWrap wrapT,
                      TextureFilter minFilter, TextureFilter magFilter, TextureFilter mipFilter) override;
    bool __cdecl Bind(unsigned int unit) const override;
    void __cdecl CopyMipToMip(DisplayTexture* from, int fromLevel, DisplayTexture* to, int toLevel) override;
    DisplayTexture* __cdecl CreateInstance() const override;
    Bitmap* __thiscall GetBitmap(unsigned int level, unsigned int face, DisplayFormat& format,
                                 DFormatId as) const override;
    bool __cdecl GetDataLost() const override;
    DFormatId __cdecl GetJetFormat() const override;
    void __cdecl CopySamplerAttributesFrom(DisplayTexture* from) override;
    void __cdecl RefreshFromChangedSettings() override;
    void* __cdecl GetAPITexture() const override;

    PBufferContext* __cdecl GetRenderContext() const;

protected:
    DisplayOpenGL* __cdecl GetDisplay() const;
    DisplayInterfaceWGL* __cdecl GetWinInterface() const;
    Bitmap* __cdecl GetTextureBitmap(int level, unsigned int face, DisplayFormat& format, DFormatId as) const;
    OpenGLFormat* __cdecl CheckFormat(DisplayFormat& format, void*& data, int& pitch, int& a, int& b, int width,
                                      int height);
    void __cdecl BindTexture(unsigned int unit) const;
    void __cdecl UnbindRenderTexture(unsigned int unit) const;
    void __cdecl InitInstance(DisplayTextureOpenGL& to) const;
    void __cdecl SetBasicState() const;

    static bool anisotropySupport;
    static bool canAutoMip;
    static unsigned int numObjects;
    static const DisplayTextureOpenGL** bound;
    static unsigned maxTextureUnits;

public:
    // retail layout: DisplayTexture (0x38) + GL fields
    unsigned int apiTexture;      // +0x38
    int renderContext;            // +0x3c
    unsigned int firstLevel;      // +0x40
    TextureFilter minFilter;      // +0x44
    TextureFilter magFilter;      // +0x48
    TextureWrap wrapS;            // +0x4c
    TextureWrap wrapT;            // +0x50
    int unused54;                 // +0x54
    int unused58;                 // +0x58
    int formatId;                 // +0x5c
    mutable bool dirty;           // +0x60
    mutable bool bound61;         // +0x61
    mutable int bindCount;        // +0x64
    bool flag68;                  // +0x68
};
JET_LAYOUT32(sizeof(DisplayTextureOpenGL) == 0x6c);

}  // namespace Jet
