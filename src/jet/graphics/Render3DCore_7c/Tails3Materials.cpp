// Render3DCore_7c wave 6 tails (Tails3): the AutoTex plugin's key function and the two material chunk paths.
//
// AutoTexMaterialPlugin has no constructor of its own in this image (the plugin thunks build it by name), so the
// destructor is the class's key function; defining it out of line makes VS18 emit ??_7AutoTexMaterialPlugin with the
// same seven slots. It only releases the three StdMaterialPlugin cache pointers, which the local StdMaterialPlugin
// destructor already does.
//
// ReflectiveMaterial and GenericShaderMaterial normally select their own U6 chunk/shader caches. Their
// CreateGeometryChunk exports build the one-texture/no-texture chunk classes directly so the exports resolve in this
// image; the vertex programs those two materials install afterwards are unchanged.
#include <jet/Render3DCoreGeometry.h>
#include <jet/Render3DCoreMaterials.h>

#include <new>

namespace Jet {

namespace {

template <class Chunk>
GeometryChunk* MakeMaterialChunk(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                 unsigned int faceNormals, Material::GeometryType type,
                                 GeometryChunkCreateHint* hint)
{
    const unsigned int flags = Material::MakeGeometryChunkFlags(type, hint);
    void* const memory = Alloc::New(sizeof(Chunk));
    if (!memory)
        return nullptr;
    return ::new (memory) Chunk(vertices, primitives, indices, faceNormals, flags);
}

}  // namespace

AutoTexMaterialPlugin::~AutoTexMaterialPlugin() = default;

Material* __cdecl AutoTexMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    const auto* self = reinterpret_cast<const StdMaterialPluginTemplate<AutoTexMaterial>*>(this);
    return self->StdMaterialPluginTemplate<AutoTexMaterial>::CreateInstance(manager, kernel);
}

GeometryChunk* __cdecl ReflectiveMaterial::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                               unsigned int numIndices, unsigned int numFaceNormals,
                                                               GeometryType type,
                                                               GeometryChunkCreateHint* hint) const
{
    return MakeMaterialChunk<PackedVertexPCTGeometryNmlChunk>(numVertices, numPrimitives, numIndices, numFaceNormals,
                                                              type, hint);
}

GeometryChunk* __cdecl GenericShaderMaterial::CreateGeometryChunk(unsigned int numVertices,
                                                                  unsigned int numPrimitives,
                                                                  unsigned int numIndices, unsigned int numFaceNormals,
                                                                  GeometryType type,
                                                                  GeometryChunkCreateHint* hint) const
{
    return MakeMaterialChunk<PackedVertexPNGeometryChunk>(numVertices, numPrimitives, numIndices, numFaceNormals, type,
                                                          hint);
}

}  // namespace Jet
