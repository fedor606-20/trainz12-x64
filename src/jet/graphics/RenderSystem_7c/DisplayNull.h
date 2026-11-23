// U16: the private null renderer of RenderSystem_7c (retail Jet::DisplayNull, constructor 0x1001e130,
// object size 0xf28). The class is not exported; RenderKernel constructs it for the "NullRender" option and
// the null texture class reads the format table it fills.
#pragma once

#include <jet/Platform.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>

namespace Jet {

class Display;
class DisplayDescriptor;
class DisplayTexture;

// One row of the null display's format table (retail RVA 0xb8228, 36 bytes per DFormatId). DisplayNull's
// format initialiser fills it: an unsupported id is converted to the format its `link` names.
class NULLFormat {
public:
    unsigned int flags;       // +0x00 bit 0 supported as is, bit 1 compressed
    int unused[3];            // +0x04
    int blockBits;            // +0x10 bits per 4x4 block (compressed formats)
    NULLFormat* link;         // +0x14 the format it is converted to
    DisplayFormat format;     // +0x18
};
JET_LAYOUT32(sizeof(NULLFormat) == 0x24);

constexpr unsigned int kNullFormatCount = 29;

// The rebuild's own table. `RsNullFormatEntry` is safe for any id below kNullFormatCount.
NULLFormat* __cdecl NullFormatTable();
NULLFormat* __cdecl NullFormatEntry(unsigned int id);

// Fills (or refills) the table from the retail seeds and mirrors it into the renamed original while that
// module is loaded, so code that still reads the original's copy behaves as before.
void __cdecl NullFormatInitialize();

class DisplayNull {
public:
    // Retail object layout: Display at +0 (its virtual base at +0xf04), the descriptor at +0xf00, and the
    // PluginInstance/ReferenceCount virtual base at +0xf1c/+0xf20.
    static constexpr unsigned int kObjectSize = 0xf28;
    static constexpr unsigned int kDescriptorOffset = 0xf00;
    static constexpr unsigned int kPluginOffset = 0xf1c;
    static constexpr unsigned int kReferenceOffset = 0xf20;

    static Display* __cdecl Create();              // 0x1001e130, createContext = true
    static void __cdecl Destroy(Display* display); // 0x1001e270 + Display::~Display

private:
    DisplayNull();
};

}  // namespace Jet
