// RenderOpenGL_7c: extension resolution shared by the texture and display backends.
#include "OpenGLExtensions.h"

#include <cstring>

namespace Jet::GL {
namespace {
HMODULE OpenGL32()
{
    static HMODULE module = LoadLibraryA("opengl32.dll");
    return module;
}

FARPROC Find(const char* name)
{
    HMODULE const gl = OpenGL32();
    if (!gl) return nullptr;
    if (FARPROC p = wglGetProcAddress(name)) return p;
    return GetProcAddress(gl, name);
}

template <class T>
void Load(T& field, const char* name)
{
    field = reinterpret_cast<T>(Find(name));
}

bool loaded = false;
}  // namespace

Extensions ext{};

bool HasExtension(const char* list, const char* name)
{
    if (!list || !name || !*name) return false;
    const std::size_t length = std::strlen(name);
    const char* at = list;
    while ((at = std::strstr(at, name)) != nullptr) {
        if ((at == list || at[-1] == ' ') && (at[length] == ' ' || at[length] == '\0')) return true;
        at += length;
    }
    return false;
}

const char* ExtensionString()
{
    static const char* cached = nullptr;
    if (!cached) {
        using GetString = const GLubyte*(APIENTRY*)(GLenum);
        auto const getString = reinterpret_cast<GetString>(Find("glGetString"));
        if (getString) cached = reinterpret_cast<const char*>(getString(GL_EXTENSIONS));
    }
    return cached;
}

bool IsSupported(const char* name) { return HasExtension(ExtensionString(), name); }

void LoadExtensions()
{
    if (loaded) return;
    loaded = true;
    Load(ext.CompressedTexImage2D, "glCompressedTexImage2DARB");
    Load(ext.CompressedTexSubImage2D, "glCompressedTexSubImage2DARB");
    Load(ext.GetCompressedTexImage, "glGetCompressedTexImageARB");
    Load(ext.GenerateMipmap, "glGenerateMipmapEXT");
    if (!ext.GenerateMipmap) Load(ext.GenerateMipmap, "glGenerateMipmapSGIS");
    Load(ext.ActiveTexture, "glActiveTextureARB");
    Load(ext.ClientActiveTexture, "glClientActiveTextureARB");
    Load(ext.BlendEquation, "glBlendEquationEXT");
    Load(ext.StencilOpSeparate, "glStencilOpSeparateATI");
    Load(ext.StencilFuncSeparate, "glStencilFuncSeparateATI");
    Load(ext.StencilMaskSeparate, "glStencilMaskSeparateATI");
    Load(ext.BlendFuncSeparate, "glBlendFuncSeparateEXT");
    Load(ext.PointParameterf, "glPointParameterfEXT");
    Load(ext.PointParameterfv, "glPointParameterfvEXT");
    Load(ext.CreatePbuffer, "wglCreatePbufferARB");
    Load(ext.GetPbufferDC, "wglGetPbufferDCARB");
    Load(ext.ReleasePbufferDC, "wglReleasePbufferDCARB");
    Load(ext.DestroyPbuffer, "wglDestroyPbufferARB");
    Load(ext.BindTexImage, "wglBindTexImageARB");
    Load(ext.ReleaseTexImage, "wglReleaseTexImageARB");
    Load(ext.ChoosePixelFormatARB, "wglChoosePixelFormatARB");
    Load(ext.GetPixelFormatAttribivARB, "wglGetPixelFormatAttribivARB");
    Load(ext.GetExtensionsStringARB, "wglGetExtensionsStringARB");
    Load(ext.SwapInterval, "wglSwapIntervalEXT");
}

}  // namespace Jet::GL
