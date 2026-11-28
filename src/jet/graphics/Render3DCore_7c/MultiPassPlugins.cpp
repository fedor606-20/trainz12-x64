// Unit U5b of Render3DCore_7c: the multipass material plugins' IsSupported, transcribed from 0x369a0, 0x39910 and
// 0x33010.
//
// Three capability codes (0x15, 0x11, 0x1b) are asked of the Display's virtual slot 0x1c, short-circuiting on the
// first false. On failure the caller's PString is set to the plugin family's fallback material type; the original
// writes the interned node by hand (destroy the old node, then lock, ++node refs, ++cache live refs, unlock), which
// this unit repeats through the same PStringCache so the cache accounting stays identical. The fallback strings are
// the module's exported UNDECORATED_TYPE objects, resolved from the renamed original at run time.
#include <jet/Render3DCoreMultiPass.h>

#include <jet/PString.h>

namespace Jet {
namespace MultiPass {

namespace {

constexpr const char* kFallbackNoTexture = "?UNDECORATED_TYPE@NoTextureMaterial@Jet@@2VPString@2@B";
constexpr const char* kFallbackOneTexture = "?UNDECORATED_TYPE@OneTextureMaterial@Jet@@2VPString@2@B";
constexpr const char* kFallbackBump = "?UNDECORATED_TYPE@MultiPassBumpMaterial@Jet@@2VPString@2@B";

bool DisplaySupports(void* display, int capability)
{
    void** const vtable = *reinterpret_cast<void***>(display);
    return reinterpret_cast<bool(__cdecl*)(void*, int)>(vtable[0x1c / 4])(display, capability);
}

// The original's inline PString assignment: the old node is destroyed even when the new one is the same node.
void AssignType(PString& out, const PString& type)
{
    if (out.node && PString::Cache()) PString::Cache()->Destroy(out.node);
    PStringCache::Node* const node = type.node;
    if (node) {
        PString::Cache()->Lock();
        ++node->refs;
        ++PString::Cache()->liveRefs;
        PString::Cache()->Unlock();
    }
    out.node = node;
}

bool IsSupported(const char* fallback, void* display, PString& out)
{
    if (DisplaySupports(display, 0x15) && DisplaySupports(display, 0x11) && DisplaySupports(display, 0x1b))
        return true;
    const PString* const type = static_cast<const PString*>(OriginalExport(fallback));
    if (type) AssignType(out, *type);
    return false;
}

}  // namespace

}  // namespace MultiPass
}  // namespace Jet

namespace Jet {
namespace MultiPass {
namespace Leaves {

bool r3d_u5b_supported_notexture(void* self, void* display, Jet::PString& out)
{
    (void)self;
    return Jet::MultiPass::IsSupported(Jet::MultiPass::kFallbackNoTexture, display, out);
}

// 0x39910 is both MultiPassBumpMaterialPlugin::IsSupported and MultiPassOneTextureMaterialPlugin::IsSupported, and
// falls back to OneTextureMaterial's type.
bool r3d_u5b_supported_onetexture(void* self, void* display, Jet::PString& out)
{
    (void)self;
    return Jet::MultiPass::IsSupported(Jet::MultiPass::kFallbackOneTexture, display, out);
}

bool r3d_u5b_supported_bumpspec(void* self, void* display, Jet::PString& out)
{
    (void)self;
    return Jet::MultiPass::IsSupported(Jet::MultiPass::kFallbackBump, display, out);
}
}  // namespace Leaves
}  // namespace MultiPass
}  // namespace Jet
