// Unit U9b of Render3DCore_7c: Portal, PhysicalPortal, VirtualPortal and Mirror.
//
// Layouts per Render3DCoreScene.h. The four new virtual slots after Particle's 48 are Traverse, Enter, Leave and
// CreateCamera. This file owns the object state: construction, copying, the portal counters, the stencil flag,
// sizes/destinations and the references. The camera/cull and display bodies live in U9bRem.cpp (wave 4); the
// display-stencil and virtual-camera bodies there still run the original's private implementation by RVA.
#include <jet/Render3DCoreScene.h>

namespace Jet {

unsigned int Portal::total = 0;
unsigned int Portal::traversed = 0;
bool Portal::enableStencilMode = true;  // the original's initial byte is 1


// ---- Portal --------------------------------------------------------------------------------------------------

Portal::Portal() : Particle()
{
    ++total;
}

Portal::Portal(const Portal& o) : Particle(o), enterCount(o.enterCount), stencil(o.stencil) {}

Portal::~Portal()
{
    --total;
}

Portal& Portal::operator=(const Portal& o)
{
    Particle::operator=(o);
    enterCount = o.enterCount;
    stencil = o.stencil;
    return *this;
}

void __cdecl Portal::Traverse(ViewPoint&, const Matrix4&, Camera*)
{
    ++traversed;
}

Camera* __cdecl Portal::CreateCamera(Camera*, const Vector3*, void*, Matrix4&) const
{
    return nullptr;
}

int __cdecl Portal::GetID() const
{
    return 13;
}

bool __cdecl Portal::IsOfType(int id) const
{
    return id == 13 || id == 1;
}

void __cdecl Portal::SetStencil(bool value)
{
    stencil = static_cast<unsigned char>(value);
}

bool __cdecl Portal::GetStencil() const
{
    return stencil != 0 && enableStencilMode;
}

void __cdecl Portal::SetEnableGlobalStencil(bool enable)
{
    enableStencilMode = enable;
}

bool __cdecl Portal::GetEnableGlobalStencil()
{
    return enableStencilMode;
}

unsigned int __cdecl Portal::GetTraversed()
{
    return traversed;
}

unsigned int __cdecl Portal::GetTotal()
{
    return total;
}

void __cdecl Portal::Reset()
{
    traversed = 0;
}

// ---- PhysicalPortal ------------------------------------------------------------------------------------------

PhysicalPortal::PhysicalPortal(Cell* destinationCell, const Vector2& portalSize) : Portal()
{
    size = portalSize;
    destination = destinationCell;
    traversalStamp = 0;
    nonReenterant = false;
    SetStencil(true);
}

PhysicalPortal::PhysicalPortal(const PhysicalPortal& o)
    : Portal(o),
      size(o.size),
      destination(o.destination),
      traversalStamp(o.traversalStamp),
      nonReenterant(o.nonReenterant)
{
}

PhysicalPortal::~PhysicalPortal() {}

PhysicalPortal& PhysicalPortal::operator=(const PhysicalPortal& o)
{
    Portal::operator=(o);
    size = o.size;
    destination = o.destination;
    traversalStamp = o.traversalStamp;
    nonReenterant = o.nonReenterant;
    return *this;
}

const Vector2& __cdecl PhysicalPortal::GetSize() const
{
    return size;
}

Cell* __cdecl PhysicalPortal::GetDestination()
{
    return destination;
}

const Cell* __cdecl PhysicalPortal::GetDestination() const
{
    return destination;
}

bool __cdecl PhysicalPortal::GetNonReenterant() const
{
    return nonReenterant;
}

void __cdecl PhysicalPortal::SetNonReenterant(bool value)
{
    nonReenterant = value;
}

int __cdecl PhysicalPortal::GetID() const
{
    return 14;
}

bool __cdecl PhysicalPortal::IsOfType(int id) const
{
    return id == 14 || id == 13 || id == 1;
}

}
