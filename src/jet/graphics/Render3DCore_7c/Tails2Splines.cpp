// Render3DCore_7c wave 5 tails: SplineResource. The class derives KernelResource_7c's local Jet::Resource, so the
// constructor, assignment and destructor are just the base plus the SplineDefinition pointer at +0x5c. The
// original's copy constructor and assignment share that pointer (they do not clone the definition); the
// destructor deletes it through its virtual deleting destructor. LoadTxt reads the file through
// SplineDefinition::ReadTextFile, which is local.
#include "Tails2.h"

namespace Jet {

namespace {

bool ReadDefinitionText(SplineDefinition* definition, const ResourceDatabase* database, const char* name)
{
    return definition->ReadTextFile(database, name);
}

}  // namespace

// 0x1006ea20: Resource(name, owner), vftable, definition = null.
SplineResource::SplineResource(const PString& name, const ResourceDatabase* owner)
    : Resource(name, owner), definition(nullptr)
{
}

// 0x1005ffa0: the definition pointer is copied shallowly, as the original does.
SplineResource::SplineResource(const SplineResource& o) : Resource(o), definition(o.definition)
{
}

// 0x1006ea50: delete the definition through its vtable slot 0, clear the pointer, then Resource::~Resource.
SplineResource::~SplineResource()
{
    delete definition;
    definition = nullptr;
}

// 0x1005ffd0
SplineResource& SplineResource::operator=(const SplineResource& o)
{
    Resource::operator=(o);
    definition = o.definition;
    return *this;
}

// 0x1006eac0: slot 1 of the definition's vtable (SplineDefinition::CreateSpline).
Spline* __cdecl SplineResource::CreateInstance() const
{
    return definition ? definition->CreateSpline() : nullptr;
}

// 0x1006eae0: a fresh SplineDefinition (type Unknown, closed, no points), then its ReadTextFile; on failure the
// definition is destroyed and the result is 1 (Ready), otherwise 0.
Resource::Result __cdecl SplineResource::LoadTxt(const char* name)
{
    SplineDefinition* created = new SplineDefinition();
    definition = created;
    if (!ReadDefinitionText(created, GetOwner(), name)) {
        if (definition) {
            delete definition;
            definition = nullptr;
        }
        return Resource::Result(1);
    }
    return Resource::Result(0);
}

}  // namespace Jet
