// Unit U1 of WaterSystem_7c: the WaterGrid template. Behaviour follows the TS12 assembly (addresses per
// function); the two instantiations are the ones the original exports.
#include <jet/WaterGrid.h>

#include "WaterSystemInternal.h"  // GridNewBytes

namespace Jet {

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
        data[i] = o.data[i];
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
        data[i] = o.data[i];
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
    data[x + y * width] = v;
}

template <class T>
T* WaterGrid<T>::GetData()
{
    return data;
}

template class WaterGrid<float>;
template class WaterGrid<Vector2>;

}  // namespace Jet
