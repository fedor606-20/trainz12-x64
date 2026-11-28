// Render3DCore_7c wave 5 tails: classes that were not declared by their workstream headers.
// SplineResource derives KernelResource_7c's Jet::Resource (0x5c bytes) and owns the SplineDefinition at +0x5c;
// only this unit's translation files need the declaration.
#pragma once

#include <jet/Resource.h>
#include <jet/Render3DCoreSplines.h>

namespace Jet {

class SplineResource : public Resource {
public:
    SplineResource(const PString& name, const ResourceDatabase* owner);
    SplineResource(const SplineResource& o);
    virtual ~SplineResource();
    SplineResource& operator=(const SplineResource& o);

    // The original appends this virtual after Resource's 18 slots (slot 18 of ??_7SplineResource).
    virtual Spline* __cdecl CreateInstance() const;

protected:
    virtual Resource::Result __cdecl LoadTxt(const char* name);

public:
    SplineDefinition* definition;  // +0x5c, shared by the copy constructor/assignment as in the original
};
JET_LAYOUT32(sizeof(SplineResource) == 0x60);

}  // namespace Jet
