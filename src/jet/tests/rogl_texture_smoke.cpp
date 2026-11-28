// RenderOpenGL_7c: standalone smoke test of the local display texture over a real GL context.
#include "DisplayTextureOpenGL.h"

#include <jet/Bitmap.h>
#include <jet/RenderSystem.h>

#include <windows.h>

#include <cstdio>

using namespace Jet;

namespace {
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }

struct TestContext {
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;

    bool Create()
    {
        WNDCLASSA wc{};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "RoglTextureSmoke";
        RegisterClassA(&wc);
        window = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                               wc.hInstance, nullptr);
        if (!window) return false;
        dc = GetDC(window);
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof pfd;
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        int const format = ChoosePixelFormat(dc, &pfd);
        if (!format || !SetPixelFormat(dc, format, &pfd)) return false;
        rc = wglCreateContext(dc);
        return rc && wglMakeCurrent(dc, rc);
    }

    void Destroy()
    {
        if (rc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(rc);
        }
        if (dc && window) ReleaseDC(window, dc);
        if (window) DestroyWindow(window);
    }
};
}  // namespace

int main()
{
    TestContext context;
    if (!context.Create()) {
        std::printf("context failed\n");
        return 1;
    }

    // The base DisplayTexture only stores this pointer; the smoke path does not dispatch through it.
    Display* display = nullptr;
    DisplayTextureOpenGL texture(display);
    DisplayFormat format(static_cast<DFormatId>(1));
    RenderTextureConstruct construct;
    Size const size{16, 16};
    if (!texture.Create(size, format, static_cast<TextureDimension>(0), 1, static_cast<TextureAutoMip>(0), 1, 1.0f,
                        static_cast<TextureHint>(0), static_cast<TextureFilter>(GL_NEAREST),
                        static_cast<TextureFilter>(GL_NEAREST), static_cast<TextureFilter>(GL_NEAREST),
                        static_cast<TextureWrap>(GL_REPEAT), static_cast<TextureWrap>(GL_REPEAT), construct)) {
        std::printf("create failed\n");
        context.Destroy();
        return 2;
    }

    Bitmap bitmap(size, Bitmap::ARGB, nullptr);
    for (int y = 0; y < size.h; ++y)
        for (int x = 0; x < size.w; ++x) bitmap.Pixel(Point{x, y}, PackedColor{0xff000000u | static_cast<unsigned>(x * 17) << 16 | static_cast<unsigned>(y * 17)});

    bool const loaded = texture.Load(0, format, bitmap, 0);
    bool const bound = texture.Bind(0);
    DisplayFormat readFormat;
    Bitmap* read = texture.GetBitmap(0, 0, readFormat, static_cast<DFormatId>(1));
    std::printf("api=%u loaded=%d bound=%d read=%p size=%dx%d glerr=%u\n", texture.apiTexture, loaded, bound,
                static_cast<void*>(read), read ? static_cast<Rect>(*read).w : 0,
                read ? static_cast<Rect>(*read).h : 0, glGetError());
    delete read;
    texture.Destroy();
    context.Destroy();
    return loaded && bound ? 0 : 3;
}
