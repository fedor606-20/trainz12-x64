// WaterSystem_7c unit U1 (notes/WaterSystem.md): a rectangular grid of T held in one Alloc block, as the water
// animation frames use it. Layout and behaviour follow the TS12 assembly (addresses per member in WaterGrid.cpp).
#pragma once

#include <jet/KernelStandard.h>  // Alloc
#include <jet/Math.h>            // Size
#include <jet/Platform.h>

namespace Jet {

template <class T>
class WaterGrid {
public:
    WaterGrid();                      // only the vftable and a null data pointer are set
    explicit WaterGrid(const Size& size);  // allocates, contents untouched
    WaterGrid(const WaterGrid& o);    // deep copy
    ~WaterGrid();                     // frees the block
    void operator=(const WaterGrid& o);  // deep copy (the original returns void)

    virtual void __cdecl Initialize(const Size& size);  // (re)allocates, contents untouched

    const T& __cdecl Get(int x, int y) const;
    void __cdecl Set(int x, int y, const T& v);
    T* __cdecl GetData();

    JET_HEAP_OBJECT  // the original allocates the grids through Jet::Alloc

protected:
    // derived classes (DisplacementHull) fill the grid directly, as the original does
    int width;
    int height;
    T* data;
};

JET_LAYOUT32(sizeof(WaterGrid<float>) == 0x10);

}  // namespace Jet
