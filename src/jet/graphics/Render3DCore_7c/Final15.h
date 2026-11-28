// Render3DCore_7c Final15: the last eleven multipass material texture virtuals and the four OrientedBox
// best-fit helpers. The material entries are plain namespace functions with the original cdecl ABI (`this` and
// the hidden ResultClass pointer first), exported under their original mangled names through
// implemented.d/final15.txt. Object layouts are the TS12 ones already documented in the U5b fragments:
//   MultiPassNoTextureMaterial  +0xa4 texture 0xc, +0xa8 texture 0xd
//   MultiPassOneTextureMaterial +0xc4 texture 1,  +0xc8 alpha hint
//   MultiPassBumpMaterial       +0xd0 texture 8,  +0xc4 inherited, +0xc8 alpha hint
#pragma once

#include <jet/Render3DCore.h>
#include <jet/Render3DCoreMaterials.h>

namespace Jet {
namespace Final15 {

void NoTextureDuplicateTexture(void* self, Material::Result* out, int type);
void NoTextureSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                           const PString& name, float amount, const String& request);
void NoTextureSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                            float amount);

void OneTextureDuplicateTexture(void* self, Material::Result* out, int type);
void OneTextureSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                            const PString& name, float amount, const String& request);
void OneTextureSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                             float amount);
void OneTextureIsLoaded(const void* self, Material::Result* out);

void BumpDuplicateTexture(void* self, Material::Result* out, int type);
void BumpSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db, const PString& name,
                      float amount, const String& request);
void BumpSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                       float amount);
void BumpIsLoaded(const void* self, Material::Result* out);

// ---- OrientedBox best-fit helpers (Final15OrientedBox.cpp)
void __cdecl MakeBestFitArray(void* self, unsigned int count, const Vector3* points);
void __cdecl MakeBestFitItems(void* self, unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);
float __cdecl BoxVolume(const float* angles, unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items);
void __cdecl MinimalBoxForAngles(void* self, int depth, VertexItemPtr<const Vector3, VIPConstHelper> items,
                                 float* angles);

}  // namespace Final15
}  // namespace Jet
