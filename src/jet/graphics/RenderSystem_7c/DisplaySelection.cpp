// U9: base display selector and the device-independent format negotiation step.
#include <jet/RenderSystem.h>
#include <jet/Window.h>
#include <cstring>

namespace Jet {
namespace {
struct VideoMode {
    unsigned width, height, colorBits, frequency;
};

struct VideoFormat {
    unsigned id, auxiliary, colorBits, depthBits, stencilBits, alphaBits, samples, flags;
};

template<class T>
struct VideoList {
    unsigned count, capacity;
    const T* entries;
};

template<class T>
T DisplayValue(const Display* display, unsigned offset)
{
    return *reinterpret_cast<const T*>(reinterpret_cast<const unsigned char*>(display) + offset);
}

constexpr unsigned Any = ~0u;

template<class T>
T& DisplaySlot(Display* display, unsigned offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(display) + offset);
}

void CloseContextWindow(Window* window)
{
    window->SetContextOnly(true);
    window->Close();
}
}

void __cdecl RsDisplaySetSelector(Display*, void*);

bool __cdecl RsDisplayPreOpen(Display* display, const Options* options)
{
    auto* window = DisplayValue<Window*>(display, 0x40);
    if (!window) return false;
    char title[260]{};
    strcpy_s(title, window->GetInternalName());
    if (options) {
        PString custom;
        if (options->Get("WindowTitle", custom)) {
            const char* text = custom.Text();
            if (text && custom.Length() <= 245) strcpy_s(title, text);
        }
        bool showApi;
        options->Get("ShowRenderAPI", showApi, false);
        if (showApi) {
            strcat_s(title, " [");
            strcat_s(title, VCall<const char*>(display, 0x18));
            strcat_s(title, "]");
        }
    }
    if (!window->Open(title, options)) return false;
    if (!DisplayValue<void*>(display, 0x288)) RsDisplaySetSelector(display, nullptr);
    if (!VCall<bool>(display, 0x24c) || !VCall<bool>(display, 0x250)) {
        CloseContextWindow(window);
        return false;
    }
    VCall<void>(DisplayValue<void*>(display, 0x288), 0x08, options, window,
                &DisplaySlot<VideoMode>(display, 0x24c), &DisplaySlot<int>(display, 0x22c));
    return true;
}

bool __cdecl RsDisplayOpen(Display* display, const Options* options, bool selectFormatFirst)
{
    if (!VCall<bool>(display, 0x244, options)) return false;
    VideoFormat format{};
    if (selectFormatFirst && !VCall<bool>(display, 0x248, options, &format)) return false;

    auto* window = DisplayValue<Window*>(display, 0x40);
    auto* selector = DisplayValue<void*>(display, 0x288);
    const int mode = DisplayValue<int>(display, 0x22c);
    auto& original = DisplaySlot<VideoMode>(display, 0x24c);
    auto& selected = DisplaySlot<VideoMode>(display, 0x23c);
    bool created;
    if (mode == 2) {
        VideoMode fullscreen{};
        if (!VCall<bool>(selector, 0x14, options, &DisplaySlot<VideoList<VideoMode>>(display, 0x230), &fullscreen) ||
            !VCall<bool>(display, 0x258, &fullscreen)) {
            CloseContextWindow(window);
            return false;
        }
        selected = fullscreen;
        created = window->CreateFullscreenHost(fullscreen.width, fullscreen.height, const_cast<Options*>(options));
    } else {
        selected = original;
        if (mode == 0) {
            Rect area;
            VCall<void>(selector, 0x10, options, window, &original, &area);
            created = window->CreateWindowedHost(area.x, area.y, area.w, area.h, const_cast<Options*>(options));
        } else {
            created = window->CreateFullscreenHost(original.width, original.height, const_cast<Options*>(options));
        }
    }
    if (!created) {
        CloseContextWindow(window);
        return false;
    }
    if (!selectFormatFirst && !VCall<bool>(display, 0x248, options, &format)) return false;
    if (!VCall<bool>(display, 0x25c, &format)) {
        CloseContextWindow(window);
        if (DisplayValue<int>(display, 0x22c) == 2) VCall<bool>(display, 0x258, &original);
        return false;
    }

    auto& selectedFormat = DisplaySlot<VideoFormat>(display, 0x268);
    selectedFormat = format;
    const auto capability = VCall<unsigned>(display, 0x8c, &selectedFormat, &format);
    DisplaySlot<unsigned>(display, 0x280) = capability;
    if (auto* state = DisplayValue<unsigned char*>(display, 0x54))
        state[0xa1] = static_cast<unsigned char>((state[0xa1] & ~0x10) | (capability ? 0x10 : 0));
    const Rect client = window->GetClientArea();
    DisplaySlot<int>(display, 0x28c) = client.w;
    DisplaySlot<int>(display, 0x290) = client.h;
    options->Get("PhysicalScreenRatio", DisplaySlot<float>(display, 0x6c),
                 static_cast<float>(client.w) / static_cast<float>(client.h));
    return true;
}

bool __cdecl RsDisplayParseTextureStage(Display* display, void* stage, File* file)
{
    TextureArgs args;
    char line[256];
    if (!file->ReadLine(line, sizeof line, 0)) return false;
    if (args.Decode(line)) VCall<void>(display, 0x17c, stage, &args);
    return true;
}

void __cdecl RsSelectResolution(void*, const Options* options, unsigned& width, unsigned& height)
{
    struct Preset { const char* name; unsigned width, height; };
    static constexpr Preset presets[]{
        {"320", 320, 240}, {"400", 400, 300}, {"640", 640, 480}, {"800", 800, 600},
        {"1024", 1024, 768}, {"1280", 1280, 960}, {"1600", 1600, 1200}, {"1920", 1920, 1440},
    };
    width = 1024;
    height = 768;
    for (const auto& preset : presets) {
        if (options->Get(preset.name)) {
            width = preset.width;
            height = preset.height;
        }
    }
    options->Get("Width", width, width);
    options->Get("Height", height, height);
}

void __cdecl RsSelectScreenMode(void* selector, const Options* options, Window* window, const void* video, int& out)
{
    out = options->Get("Fullscreen") ? 2 : 0;
    if (window->GetParent()) {
        out = 0;
        return;
    }
    // This legacy call preserves the selector hook's side effects. The dimensions are discarded.
    unsigned width = reinterpret_cast<unsigned>(video), height = reinterpret_cast<unsigned>(&out);
    VCall<void>(selector, 0x0c, options, &width, &height);
}

void __cdecl RsSelectWindowArea(void* selector, const Options* options, Window* window, const void* video, Rect& out)
{
    if (window->GetParent()) {
        out = window->GetParentArea();
        out.x = out.y = 0;
        return;
    }
    constexpr int DefaultPos = (-2147483647 - 1);
    const auto& mode = *static_cast<const VideoMode*>(video);
    out.x = out.y = DefaultPos;
    unsigned width = reinterpret_cast<unsigned>(video), height = 0;
    VCall<void>(selector, 0x0c, options, &width, &height);
    out.w = static_cast<int>(width);
    out.h = static_cast<int>(height);
    if (options->Get("WindowCentered") || options->Get("WindowHorizontalCentered") ||
        options->Get("WindowVerticalCentered")) {
        out.x = static_cast<int>((mode.width - width) >> 1);
        out.y = static_cast<int>((mode.height - height) >> 1);
    }
    options->Get("WindowPosX", out.x, out.x);
    options->Get("WindowPosY", out.y, out.y);
    if (out.x == DefaultPos && out.y != DefaultPos) out.x = 0;
    if (out.y == DefaultPos && out.x != DefaultPos) out.y = 0;
}

bool __cdecl RsSelectVideoMode(void* selector, const Options* options, const void* list, void* result)
{
    unsigned width = reinterpret_cast<unsigned>(selector), height = 0;
    VCall<void>(selector, 0x0c, options, &width, &height);
    int frequency = -1, colorBits = -1;
    options->Get("Frequency", frequency, frequency);
    options->Get("ColorBits", colorBits, colorBits);

    const auto& modes = *static_cast<const VideoList<VideoMode>*>(list);
    const VideoMode* best = nullptr;
    for (unsigned i = 0; i < modes.count; ++i) {
        const auto& mode = modes.entries[i];
        if (mode.width != width || mode.height != height ||
            (frequency != -1 && mode.frequency != static_cast<unsigned>(frequency)) ||
            (colorBits != -1 && mode.colorBits < static_cast<unsigned>(colorBits))) continue;
        if (!best || (colorBits == -1 ? mode.colorBits > best->colorBits
            : colorBits >= 0 && mode.colorBits < best->colorBits)) best = &mode;
    }
    if (!best) return false;
    auto& out = *static_cast<VideoMode*>(result);
    out = *best;
    if (frequency == -1) out.frequency = 0;
    return true;
}

bool __cdecl RsSelectVideoFormat(void*, const Options* options, const void* list, void* result)
{
    int color = -1, depth = -1, alpha = -1, samples = 4, stencil = -1;
    options->Get("ColorBits", color, color);
    options->Get("DepthBits", depth, depth);
    options->Get("AlphaBits", alpha, alpha);
    options->Get("multiSample", samples, samples);
    if (!options->Get("DisableStencil")) options->Get("StencilBits", stencil, stencil);
    const auto permits = [](int requested, unsigned bits) {
        return requested == -1 || static_cast<unsigned>(requested) <= bits;
    };

    const auto& formats = *static_cast<const VideoList<VideoFormat>*>(list);
    const VideoFormat* best = nullptr;
    unsigned bestColor = 0, bestDepth = 0, bestSamples = 0, bestFlag = 1;
    for (unsigned i = 0; i < formats.count; ++i) {
        const auto& format = formats.entries[i];
        if (!permits(color, format.colorBits) || !permits(depth, format.depthBits) ||
            !permits(stencil, format.stencilBits) || !permits(alpha, format.alphaBits) ||
            format.samples > static_cast<unsigned>(samples)) continue;

        const unsigned flag = format.flags & 1;
        bool take = !best || (bestFlag && !flag);
        if (flag == bestFlag) {
            const bool sameColor = format.colorBits == bestColor;
            const bool moreSamples = format.samples > bestSamples;
            const bool betterColor = color == -1 ? format.colorBits > bestColor
                : color >= 0 && format.colorBits < bestColor;
            if ((betterColor || (sameColor && moreSamples)) &&
                (bestDepth == 0 || depth != -1 || format.depthBits != 0)) take = true;
            const bool betterDepth = depth == -1 ? format.depthBits > bestDepth
                : depth >= 0 && format.depthBits < bestDepth;
            if (sameColor && (betterDepth || (moreSamples && format.depthBits == bestDepth))) take = true;
        }
        if (take) {
            best = &format;
            bestFlag = flag;
            bestColor = format.colorBits;
            bestDepth = format.depthBits;
            bestSamples = format.samples;
        }
    }
    if (!best) return false;
    *static_cast<VideoFormat*>(result) = *best;
    return true;
}

bool __cdecl RsSetupVideoFormat(Display* display, const Options* options, void* result)
{
    auto* bytes = reinterpret_cast<unsigned char*>(display);
    if (VCall<bool>(display, 0x254) &&
        VCall<bool>(DisplayValue<void*>(display, 0x288), 0x18, options, bytes + 0x25c, result)) return true;
    DisplayValue<Window*>(display, 0x40)->SetContextOnly(true);
    DisplayValue<Window*>(display, 0x40)->Close();
    if (DisplayValue<int>(display, 0x22c) == 2)
        VCall<void>(display, 0x258, bytes + 0x24c);
    return false;
}
}
