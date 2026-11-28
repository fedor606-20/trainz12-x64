// Render3DCore_7c U5b wave 5 (U5bFinal): the multipass module's exported static data.
//
// This unit is the first Render3DCore slice to own the multipass static data objects. The contents are the
// original image's: the six material class vftables (the Material and PluginInstance halves), their shared
// vbtable, the six plugin vftables, the two bump-chunk template and base vftables, and the MultiPassLightRender /
// MPLightRender vftables, plus the six type-name PString pairs and the six UID objects.
//
// The vftable slots point at the original module's functions, exactly as the byte image does, so the tables are
// copied from the renamed original once at load. The material constructors (U5b wave 4) still install those
// original addresses; the local copies therefore stay byte-identical and the differential tests can compare them
// slot for slot. The type names and UIDs are plain objects built from their text and 16 bytes.
#include <jet/Render3DCoreMultiPass.h>

#include <jet/PString.h>
#include <jet/UId.h>

#include <cstring>

namespace Jet {

// The class declarations exist only to give the static members the original mangled names. U5a owns the real
// material classes; nothing here derives from anything, and no code outside this unit refers to these names.
class MultiPassNoTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};
class MultiPassOneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};
class MultiPassBumpMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};
class MultiPassBump2Material {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};
class MultiPassBumpSpecMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};
class MultiPassBumpSpec2Material {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
};

namespace MultiPass {
namespace FinalData {

// Sizes are the distances to the next vftable in the original image. A vftable's -4 entry (the complete object
// locator) is not part of the exported symbol.
struct Tables {
    static unsigned int NoTextureVtable[24];
    static unsigned int OneTextureVtable[26];
    static unsigned int BumpVtable[26];
    static unsigned int Bump2Vtable[26];
    static unsigned int BumpSpecVtable[26];
    static unsigned int BumpSpec2Vtable[26];

    static unsigned int NoTextureInstanceVtable[2];
    static unsigned int OneTextureInstanceVtable[2];
    static unsigned int BumpInstanceVtable[2];
    static unsigned int Bump2InstanceVtable[2];
    static unsigned int BumpSpecInstanceVtable[2];
    static unsigned int BumpSpec2InstanceVtable[2];

    static unsigned int NoTexturePluginVtable[7];
    static unsigned int OneTexturePluginVtable[7];
    static unsigned int BumpPluginVtable[7];
    static unsigned int Bump2PluginVtable[7];
    static unsigned int BumpSpecPluginVtable[7];
    static unsigned int BumpSpec2PluginVtable[7];

    static int NoTextureVbtable[2];
    static int OneTextureVbtable[2];
    static int BumpVbtable[2];

    static unsigned int BumpChunkBaseVtable[1];
    static unsigned int BumpChunkTVtable[73];
    static unsigned int BumpChunkTBaseVtable[1];
    static unsigned int BumpChunkT2Vtable[73];
    static unsigned int BumpChunkT2BaseVtable[1];

    static unsigned int LightRenderVtable[2];
    static unsigned int MPLightRenderVtable[2];
};

unsigned int Tables::NoTextureVtable[24];
unsigned int Tables::OneTextureVtable[26];
unsigned int Tables::BumpVtable[26];
unsigned int Tables::Bump2Vtable[26];
unsigned int Tables::BumpSpecVtable[26];
unsigned int Tables::BumpSpec2Vtable[26];
unsigned int Tables::NoTextureInstanceVtable[2];
unsigned int Tables::OneTextureInstanceVtable[2];
unsigned int Tables::BumpInstanceVtable[2];
unsigned int Tables::Bump2InstanceVtable[2];
unsigned int Tables::BumpSpecInstanceVtable[2];
unsigned int Tables::BumpSpec2InstanceVtable[2];
unsigned int Tables::NoTexturePluginVtable[7];
unsigned int Tables::OneTexturePluginVtable[7];
unsigned int Tables::BumpPluginVtable[7];
unsigned int Tables::Bump2PluginVtable[7];
unsigned int Tables::BumpSpecPluginVtable[7];
unsigned int Tables::BumpSpec2PluginVtable[7];
int Tables::NoTextureVbtable[2];
int Tables::OneTextureVbtable[2];
int Tables::BumpVbtable[2];
unsigned int Tables::BumpChunkBaseVtable[1];
unsigned int Tables::BumpChunkTVtable[73];
unsigned int Tables::BumpChunkTBaseVtable[1];
unsigned int Tables::BumpChunkT2Vtable[73];
unsigned int Tables::BumpChunkT2BaseVtable[1];
unsigned int Tables::LightRenderVtable[2];
unsigned int Tables::MPLightRenderVtable[2];

namespace {

struct Copy {
    void* destination;
    const char* name;
    unsigned int bytes;
};

const Copy kCopies[] = {
    {Tables::NoTextureVtable, "??_7MultiPassNoTextureMaterial@Jet@@6BMaterial@1@@", sizeof Tables::NoTextureVtable},
    {Tables::OneTextureVtable, "??_7MultiPassOneTextureMaterial@Jet@@6BMaterial@1@@", sizeof Tables::OneTextureVtable},
    {Tables::BumpVtable, "??_7MultiPassBumpMaterial@Jet@@6BMaterial@1@@", sizeof Tables::BumpVtable},
    {Tables::Bump2Vtable, "??_7MultiPassBump2Material@Jet@@6BMaterial@1@@", sizeof Tables::Bump2Vtable},
    {Tables::BumpSpecVtable, "??_7MultiPassBumpSpecMaterial@Jet@@6BMaterial@1@@", sizeof Tables::BumpSpecVtable},
    {Tables::BumpSpec2Vtable, "??_7MultiPassBumpSpec2Material@Jet@@6BMaterial@1@@", sizeof Tables::BumpSpec2Vtable},
    {Tables::NoTextureInstanceVtable, "??_7MultiPassNoTextureMaterial@Jet@@6BPluginInstance@1@@",
     sizeof Tables::NoTextureInstanceVtable},
    {Tables::OneTextureInstanceVtable, "??_7MultiPassOneTextureMaterial@Jet@@6BPluginInstance@1@@",
     sizeof Tables::OneTextureInstanceVtable},
    {Tables::BumpInstanceVtable, "??_7MultiPassBumpMaterial@Jet@@6BPluginInstance@1@@",
     sizeof Tables::BumpInstanceVtable},
    {Tables::Bump2InstanceVtable, "??_7MultiPassBump2Material@Jet@@6BPluginInstance@1@@",
     sizeof Tables::Bump2InstanceVtable},
    {Tables::BumpSpecInstanceVtable, "??_7MultiPassBumpSpecMaterial@Jet@@6BPluginInstance@1@@",
     sizeof Tables::BumpSpecInstanceVtable},
    {Tables::BumpSpec2InstanceVtable, "??_7MultiPassBumpSpec2Material@Jet@@6BPluginInstance@1@@",
     sizeof Tables::BumpSpec2InstanceVtable},
    {Tables::NoTexturePluginVtable, "??_7MultiPassNoTextureMaterialPlugin@Jet@@6B@", sizeof Tables::NoTexturePluginVtable},
    {Tables::OneTexturePluginVtable, "??_7MultiPassOneTextureMaterialPlugin@Jet@@6B@", sizeof Tables::OneTexturePluginVtable},
    {Tables::BumpPluginVtable, "??_7MultiPassBumpMaterialPlugin@Jet@@6B@", sizeof Tables::BumpPluginVtable},
    {Tables::Bump2PluginVtable, "??_7MultiPassBump2MaterialPlugin@Jet@@6B@", sizeof Tables::Bump2PluginVtable},
    {Tables::BumpSpecPluginVtable, "??_7MultiPassBumpSpecMaterialPlugin@Jet@@6B@", sizeof Tables::BumpSpecPluginVtable},
    {Tables::BumpSpec2PluginVtable, "??_7MultiPassBumpSpec2MaterialPlugin@Jet@@6B@", sizeof Tables::BumpSpec2PluginVtable},
    {Tables::NoTextureVbtable, "??_8MultiPassNoTextureMaterial@Jet@@7B@", sizeof Tables::NoTextureVbtable},
    {Tables::OneTextureVbtable, "??_8MultiPassOneTextureMaterial@Jet@@7B@", sizeof Tables::OneTextureVbtable},
    {Tables::BumpVbtable, "??_8MultiPassBumpMaterial@Jet@@7B@", sizeof Tables::BumpVbtable},
    {Tables::BumpChunkBaseVtable, "??_7MultiPassBumpChunkBase@Jet@@6B@", sizeof Tables::BumpChunkBaseVtable},
    {Tables::BumpChunkTVtable,
     "??_7MultiPassBumpChunkT@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTA@Jet@@VGeometryChunkSingleStream@2@@1@@",
     sizeof Tables::BumpChunkTVtable},
    {Tables::BumpChunkTBaseVtable, "??_7MultiPassBumpChunkT@Jet@@6BMultiPassBumpChunkBase@1@@",
     sizeof Tables::BumpChunkTBaseVtable},
    {Tables::BumpChunkT2Vtable,
     "??_7MultiPassBumpChunkT2@Jet@@6B?$GeometryChunkTemplate@UPackedVertexPNTTA@Jet@@VGeometryChunkSingleStream@2@@1@@",
     sizeof Tables::BumpChunkT2Vtable},
    {Tables::BumpChunkT2BaseVtable, "??_7MultiPassBumpChunkT2@Jet@@6BMultiPassBumpChunkBase@1@@",
     sizeof Tables::BumpChunkT2BaseVtable},
    {Tables::LightRenderVtable, "??_7MultiPassLightRender@Jet@@6B@", sizeof Tables::LightRenderVtable},
    {Tables::MPLightRenderVtable, "??_7MPLightRender@MultiPassRenderState@Jet@@6B@",
     sizeof Tables::MPLightRenderVtable},
};

struct Initialise {
    Initialise()
    {
        for (const Copy& copy : kCopies) {
            if (const void* const source = OriginalExport(copy.name))
                std::memcpy(copy.destination, source, copy.bytes);
        }
    }
};

Initialise initialise;

}  // namespace

}  // namespace FinalData
}  // namespace MultiPass

// The exported type names and identifiers. The texts are the original's (probe of the live module): the
// decorated names are the shortened "m." forms used by the material file syntax.
const PString MultiPassNoTextureMaterial::DECORATED_TYPE("m.mpnotex");
const PString MultiPassNoTextureMaterial::UNDECORATED_TYPE("mpnotex");
const PString MultiPassOneTextureMaterial::DECORATED_TYPE("m.mponetex");
const PString MultiPassOneTextureMaterial::UNDECORATED_TYPE("mponetex");
const PString MultiPassBumpMaterial::DECORATED_TYPE("m.mpbumptex");
const PString MultiPassBumpMaterial::UNDECORATED_TYPE("mpbumptex");
const PString MultiPassBump2Material::DECORATED_TYPE("m.mpbump2tex");
const PString MultiPassBump2Material::UNDECORATED_TYPE("mpbump2tex");
const PString MultiPassBumpSpecMaterial::DECORATED_TYPE("m.mpbumpspec");
const PString MultiPassBumpSpecMaterial::UNDECORATED_TYPE("mpbumpspec");
const PString MultiPassBumpSpec2Material::DECORATED_TYPE("m.mpbumpspec2");
const PString MultiPassBumpSpec2Material::UNDECORATED_TYPE("mpbumpspec2");

namespace {

const unsigned char kNoTextureUid[8] = {0x97, 0xd6, 0x74, 0x91, 0x4f, 0xb1, 0x70, 0x67};
const unsigned char kOneTextureUid[8] = {0x9c, 0x88, 0x23, 0xe9, 0x5c, 0xec, 0x1c, 0x9a};
const unsigned char kBumpUid[8] = {0xb1, 0xc0, 0x09, 0x62, 0x1e, 0x8f, 0x8a, 0xbe};
const unsigned char kBump2Uid[8] = {0xa1, 0xc1, 0x07, 0x52, 0x1e, 0x8f, 0xfd, 0xbc};
const unsigned char kBumpSpecUid[8] = {0xb1, 0xc0, 0x0a, 0x62, 0x1e, 0x8f, 0x8a, 0xce};
const unsigned char kBumpSpec2Uid[8] = {0xa1, 0xc1, 0x01, 0x52, 0x1e, 0x8f, 0xcd, 0xbc};

}  // namespace

const UId MultiPassNoTextureMaterial::UID(0x876282da, 0x2417, 0x4cc8, kNoTextureUid);
const UId MultiPassOneTextureMaterial::UID(0x6bc3a665, 0x6d12, 0x4f1c, kOneTextureUid);
const UId MultiPassBumpMaterial::UID(0xc39022f3, 0xc601, 0x40ba, kBumpUid);
const UId MultiPassBump2Material::UID(0xf39522a3, 0xc601, 0x52ca, kBump2Uid);
const UId MultiPassBumpSpecMaterial::UID(0xd39022f3, 0xc601, 0x40ba, kBumpSpecUid);
const UId MultiPassBumpSpec2Material::UID(0xe39522a3, 0xc601, 0x52ca, kBumpSpec2Uid);

}  // namespace Jet
