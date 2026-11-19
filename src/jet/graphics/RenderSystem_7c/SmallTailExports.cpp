// Small/tail exports for RenderSystem_7c (value types).
// Behaviour/layout follows TS12 exports/decomp/original bytes.
#include <jet/Math.h>
#include <jet/Platform.h>

#include <cstring>

namespace Jet {

class Element {};

// VisibleElement: {Matrix4 transform, Element* element, unsigned long value}.
struct VisibleElement {
    Matrix4 transform;    // +0x00
    Element* element;     // +0x40
    unsigned long value;  // +0x44

    VisibleElement() {}  // 0x10024c50 (shared body)

    // 0x1000a970
    VisibleElement(const Matrix4& m, Element* e)
    {
        std::memcpy(&transform, &m, sizeof(Matrix4));
        element = e;
        value = 0;
    }

    // 0x1000a9a0
    VisibleElement(const Matrix4& m, Element* e, unsigned long v)
    {
        std::memcpy(&transform, &m, sizeof(Matrix4));
        element = e;
        value = v;
    }

    // 0x1000a940
    VisibleElement& __thiscall operator=(const VisibleElement& o)
    {
        std::memcpy(&transform, &o.transform, sizeof(Matrix4));
        element = o.element;
        value = o.value;
        return *this;
    }
};

JET_LAYOUT32(sizeof(VisibleElement) == 0x48);

class Display {
public:
    struct VideoMode {
        unsigned int width;
        unsigned int height;
        unsigned int bpp;
        unsigned int refresh;

        VideoMode()  // 0x100560c0 (shared body)
        {
            width = 0;
            height = 0;
            bpp = 0;
            refresh = 0;
        }

        // 0x1001c500
        VideoMode(unsigned int w, unsigned int h, unsigned int b, unsigned int r)
        {
            width = w;
            height = h;
            bpp = b;
            refresh = r;
        }

        // 0x1001c590
        VideoMode& __cdecl operator=(const VideoMode& o)
        {
            width = o.width;
            height = o.height;
            bpp = o.bpp;
            refresh = o.refresh;
            return *this;
        }

        // 0x1001c520
        bool __thiscall operator==(const VideoMode& o) const
        {
            return width == o.width && height == o.height && bpp == o.bpp && refresh == o.refresh;
        }

        // 0x1001c550
        bool __thiscall operator!=(const VideoMode& o) const
        {
            if (width == o.width && height == o.height && bpp == o.bpp && refresh == o.refresh) return false;
            return true;
        }

        // 0x1001c5b0
        bool __thiscall operator<(const VideoMode& o) const
        {
            if (o.width <= width && (width != o.width || o.height <= height) &&
                (width != o.width || height != o.height || o.bpp <= bpp) &&
                (width != o.width || height != o.height || bpp != o.bpp || o.refresh <= refresh)) {
                return false;
            }
            return true;
        }
    };

    struct VideoFormat {
        unsigned int v[8];

        VideoFormat()  // 0x1001c610
        {
            v[0] = 0; v[1] = 0; v[2] = 0; v[3] = 0;
            v[4] = 0; v[5] = 0; v[6] = 0; v[7] = 0;
        }

        // 0x1001c630: params (a,b,c,d,e,f,g,h) -> (a,b,c,e,f,d,g,h)
        VideoFormat(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
                    unsigned int e, unsigned int f, unsigned int g, unsigned int h)
        {
            v[0] = a;
            v[1] = b;
            v[2] = c;
            v[3] = e;
            v[4] = f;
            v[5] = d;
            v[6] = g;
            v[7] = h;
        }

        // 0x1001c760
        VideoFormat& __cdecl operator=(const VideoFormat& o)
        {
            v[0] = o.v[0]; v[1] = o.v[1]; v[2] = o.v[2]; v[3] = o.v[3];
            v[4] = o.v[4]; v[5] = o.v[5]; v[6] = o.v[6]; v[7] = o.v[7];
            return *this;
        }

        // 0x1001c670
        bool __thiscall operator==(const VideoFormat& o) const
        {
            return v[0] == o.v[0] && v[1] == o.v[1] && v[2] == o.v[2] && v[3] == o.v[3] &&
                   v[4] == o.v[4] && v[5] == o.v[5] && v[6] == o.v[6] && v[7] == o.v[7];
        }

        // 0x1001c6c0
        bool __thiscall operator!=(const VideoFormat& o) const
        {
            if (v[0] == o.v[0] && v[1] == o.v[1] && v[2] == o.v[2] && v[3] == o.v[3] &&
                v[4] == o.v[4] && v[5] == o.v[5] && v[6] == o.v[6] && v[7] == o.v[7]) {
                return false;
            }
            return true;
        }

        // 0x1001c720
        bool __thiscall ComparePixelFormat(const VideoFormat& o) const
        {
            return v[2] == o.v[2] && v[3] == o.v[3] && v[4] == o.v[4] &&
                   v[5] == o.v[5] && v[7] == o.v[7];
        }
    };
};

}  // namespace Jet
