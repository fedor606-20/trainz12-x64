// Unit U1 of WaterSystem_7c: the WaterGrid template. Behaviour follows the TS12 assembly (addresses per
// function); the two instantiations are the ones the original exports.
#include <jet/WaterGrid.h>

#include <cstring>  // memcpy

#include "WaterSystemInternal.h"  // GridNewBytes

namespace Jet {

namespace {

// The original copies single floats through x87 (FLD/FSTP), which quiet signaling NaNs; a plain C++ assignment
// keeps the payload. Reproduce the x87 load/store so the element bytes match even for arbitrary bit patterns.
float X87Store(float v)
{
    unsigned bits;
    std::memcpy(&bits, &v, sizeof bits);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0)
        bits |= 0x00400000u;
    std::memcpy(&v, &bits, sizeof bits);
    return v;
}

void StoreElement(float& dst, const float& src)
{
    dst = X87Store(src);
}

void StoreElement(Vector2& dst, const Vector2& src)
{
    dst.x = X87Store(src.x);
    dst.y = X87Store(src.y);
}

}  // namespace

template <class T>
WaterGrid<T>::WaterGrid() : data(nullptr)
{
}

template <class T>
WaterGrid<T>::WaterGrid(const Size& size)
{
    width = size.w;
    height = size.h;
    data = static_cast<T*>(Alloc::New(GridNewBytes(width * height, sizeof(T))));
}

template <class T>
WaterGrid<T>::WaterGrid(const WaterGrid& o)
{
    width = o.width;
    height = o.height;
    data = static_cast<T*>(Alloc::New(GridNewBytes(width * height, sizeof(T))));
    for (int i = 0; i < width * height; ++i)
        StoreElement(data[i], o.data[i]);
}

template <class T>
WaterGrid<T>::~WaterGrid()
{
    if (data)
        Alloc::Delete(data);
}

template <class T>
void WaterGrid<T>::operator=(const WaterGrid& o)
{
    if (data)
        Alloc::Delete(data);
    width = o.width;
    height = o.height;
    data = static_cast<T*>(Alloc::New(GridNewBytes(width * height, sizeof(T))));
    for (int i = 0; i < width * height; ++i)
        StoreElement(data[i], o.data[i]);
}

template <class T>
void WaterGrid<T>::Initialize(const Size& size)
{
    width = size.w;
    height = size.h;
    data = static_cast<T*>(Alloc::New(GridNewBytes(width * height, sizeof(T))));
}

template <class T>
const T& WaterGrid<T>::Get(int x, int y) const
{
    return data[x + y * width];
}

template <class T>
void WaterGrid<T>::Set(int x, int y, const T& v)
{
    StoreElement(data[x + y * width], v);
}

template <class T>
T* WaterGrid<T>::GetData()
{
    return data;
}

template class WaterGrid<float>;
template class WaterGrid<Vector2>;

}  // namespace Jet
