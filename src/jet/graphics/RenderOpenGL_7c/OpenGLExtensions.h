// RenderOpenGL_7c: the GL entry points that opengl32.dll does not export itself.
#pragma once

#include <windows.h>

#include <GL/gl.h>

namespace Jet::GL {

using CompressedTexImage2D = void(APIENTRY*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*);
using CompressedTexSubImage2D = void(APIENTRY*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei,
                                                const void*);
using GetCompressedTexImage = void(APIENTRY*)(GLenum, GLint, void*);
using GenerateMipmap = void(APIENTRY*)(GLenum);
using ActiveTexture = void(APIENTRY*)(GLenum);
using ClientActiveTexture = void(APIENTRY*)(GLenum);
using BlendEquation = void(APIENTRY*)(GLenum);
using StencilOpSeparate = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using StencilFuncSeparate = void(APIENTRY*)(GLenum, GLenum, GLint, GLuint);
using StencilMaskSeparate = void(APIENTRY*)(GLenum, GLuint);
using BlendFuncSeparate = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using PointParameterf = void(APIENTRY*)(GLenum, GLfloat);
using PointParameterfv = void(APIENTRY*)(GLenum, const GLfloat*);

using CreatePbuffer = HDC(WINAPI*)(HDC, int, const int*, const FLOAT*, UINT, int*);
using GetPbufferDC = HDC(WINAPI*)(void*);
using ReleasePbufferDC = int(WINAPI*)(void*, HDC);
using DestroyPbuffer = BOOL(WINAPI*)(void*);
using BindTexImage = BOOL(WINAPI*)(HDC, int, int);
using ReleaseTexImage = BOOL(WINAPI*)(HDC, int, int);
using ChoosePixelFormatARB = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
using GetPixelFormatAttribivARB = BOOL(WINAPI*)(HDC, int, int, UINT, const int*, int*);
using GetExtensionsStringARB = const char*(WINAPI*)(HDC);
using SwapInterval = BOOL(WINAPI*)(int);

struct Extensions {
    CompressedTexImage2D CompressedTexImage2D;
    CompressedTexSubImage2D CompressedTexSubImage2D;
    GetCompressedTexImage GetCompressedTexImage;
    GenerateMipmap GenerateMipmap;
    ActiveTexture ActiveTexture;
    ClientActiveTexture ClientActiveTexture;
    BlendEquation BlendEquation;
    StencilOpSeparate StencilOpSeparate;
    StencilFuncSeparate StencilFuncSeparate;
    StencilMaskSeparate StencilMaskSeparate;
    BlendFuncSeparate BlendFuncSeparate;
    PointParameterf PointParameterf;
    PointParameterfv PointParameterfv;

    CreatePbuffer CreatePbuffer;
    GetPbufferDC GetPbufferDC;
    ReleasePbufferDC ReleasePbufferDC;
    DestroyPbuffer DestroyPbuffer;
    BindTexImage BindTexImage;
    ReleaseTexImage ReleaseTexImage;
    ChoosePixelFormatARB ChoosePixelFormatARB;
    GetPixelFormatAttribivARB GetPixelFormatAttribivARB;
    GetExtensionsStringARB GetExtensionsStringARB;
    SwapInterval SwapInterval;
};

extern Extensions ext;

// Resolves the pointers for the current context. Safe to call more than once.
void LoadExtensions();
bool HasExtension(const char* list, const char* name);
bool IsSupported(const char* name);
const char* ExtensionString();

}  // namespace Jet::GL
