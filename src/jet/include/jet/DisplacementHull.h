// WaterSystem_7c unit U3 (notes/WaterSystem.md): a height field loaded from a bitmap, used to displace the
// physics water grid. Derives from WaterGrid<float>; layout and behaviour follow the TS12 assembly (addresses
// per function in DisplacementHull.cpp).
#pragma once

#include <jet/WaterGrid.h>

namespace Jet {

class PString;
class ResourceDatabase;

class DisplacementHull : public WaterGrid<float> {
public:
    DisplacementHull();
    DisplacementHull(const DisplacementHull& o);  // deep copy, as the base
    ~DisplacementHull();
    DisplacementHull& operator=(const DisplacementHull& o);

    // loads the heightmap: from fileName, resolved through db when given (the original calls db's vtable slot 3
    // with an out node); fills the grid from the bitmap's first channel, one byte per cell, scaled to 0..1
    bool __cdecl Initialize(const PString& fileName, const ResourceDatabase* db);
};

}  // namespace Jet
