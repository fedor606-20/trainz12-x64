// Unit U3 of WaterSystem_7c: the displacement hull. Behaviour follows the TS12 assembly (addresses per
// function). Initialize resolves the file name through the database exactly as the original does: a raw
// __cdecl call of db's vtable slot 3 with (db, &out node, name) on the stack, both outputs used, the out node
// released like a PString node. (ResourceDatabase's virtuals are __cdecl with `this` pushed, per Resource.h;
// slot 3 is GetFileName, whose by-value PString return slot the caller reuses -- the call shape here matches
// the assembly either way.)
#include <jet/DisplacementHull.h>

#include <jet/Bitmap.h>
#include <jet/KernelStandard.h>  // Alloc
#include <jet/PString.h>
#include <jet/Resource.h>

#include "WaterSystemInternal.h"

namespace Jet {

namespace {

// a bitmap channel byte to the 0..1 height it stands for (the original's .rdata constant, float bits 0x3b808081)
float HeightFromByte(unsigned char b)
{
    return static_cast<float>(b) * 0.0039215689f;
}

}  // namespace

// 0x10001070 (the base default constructor leaves the size unset; only the vftable and the null data pointer)
DisplacementHull::DisplacementHull() : WaterGrid<float>()
{
}

// 0x10001030
DisplacementHull::DisplacementHull(const DisplacementHull& o) : WaterGrid<float>(o)
{
}

// 0x10015ce0 (shares its body with ~WaterGrid<float> and ~WaterSurface: set the base vftable, free the block)
DisplacementHull::~DisplacementHull() = default;

// 0x10001050
DisplacementHull& DisplacementHull::operator=(const DisplacementHull& o)
{
    WaterGrid<float>::operator=(o);
    return *this;
}

// 0x10001080
bool DisplacementHull::Initialize(const PString& fileName, const ResourceDatabase* db)
{
    Bitmap bitmap;
    bool failed;
    if (db == nullptr) {
        failed = !bitmap.Load(fileName, PString(), false);
    } else {
        PStringCache::Node* node = nullptr;
        const PString* name = CallDbSlot3(db, &node, fileName);
        failed = !bitmap.Load(*name, PString(), false);
        if (node && PString::Cache())
            PString::Cache()->Destroy(node);
    }
    if (failed)
        return false;

    Rect rect = bitmap;  // x, y, w, h
    width = rect.w;
    height = rect.h;
    data = static_cast<float*>(Alloc::New(GridNewBytes(width * height, sizeof(float))));
    const unsigned char* p = bitmap.GetPtr();
    unsigned long step = bitmap.GetBytesPerPixel();
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x, p += step)
            data[x + y * width] = HeightFromByte(*p);
    return true;
}

}  // namespace Jet
