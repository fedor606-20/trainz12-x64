// WaterSystem_7c's view of the RenderSystem_7c classes it uses, until the real ones are reconstructed
// (RenderSystem units U5+). The members are the originals, imported by name through RenderSystem_7c.dll, so
// constructing one places the original's vftable into the object. Extend as the later units need more.
#pragma once

#include <jet/KernelStandard.h>  // ReferenceCount
#include <jet/PString.h>
#include <jet/Platform.h>

namespace Jet {

class TextureManager;
class DisplayTexture;
enum TextureAlpha {};
class Display;
class ViewPort;
class RenderKernel;

// one corner of the PCT vertex record (24 bytes); the water grid builds a four-corner quad record
struct PackedVertexPCT {
    Vector3 position;       // +0x00
    unsigned int color;     // +0x0c (0xffffffff: opaque white)
    Vector2 texCoord;       // +0x10
};

// a vertex buffer object of the renderer; the water code reaches its arrays through the raw vtable slots the
// original uses (49: the item's array as (stride, pointer), 47/51: item offsets, 4: set, 31: upload)
class __declspec(dllimport) GeometryChunk : public ReferenceCount {
public:
    unsigned short* __cdecl GetIndexArray();
};

// the hint handed to the material's CreateGeometryChunk (48 bytes, zeroed)
struct StandardChunkHint {
    unsigned int data[12];
    StandardChunkHint();
};

// 0x34 bytes in the original; the game calls its virtuals through the original's vftable
class __declspec(dllimport) Texture : public ReferenceCount {
public:
    Texture(const PString& name, TextureManager* manager, DisplayTexture* display, TextureAlpha alpha);
    virtual ~Texture();
};

}  // namespace Jet
