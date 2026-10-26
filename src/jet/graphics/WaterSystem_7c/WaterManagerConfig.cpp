// Unit U2 of WaterSystem_7c: the water manager's config model. Behaviour follows the TS12 assembly (addresses
// per function). The config keys are module statics interning the same strings the original creates at load
// time (init function 0x100198e0..0x10019cc0); they are not exported, only our own Load functions use them.
#include <jet/WaterManagerConfig.h>

#include <jet/Config.h>
#include <jet/KernelStandard.h>  // Alloc, File
#include <jet/Resource.h>

#include <cstring>

#include "WaterSystemInternal.h"  // CallDbSlot3

namespace Jet {

namespace {

// config keys, interned like the original's statics
const PString kWaterManager("WaterManager");
const PString kWaterTile("WaterTile");
const PString kTileName("TileName");
const PString kTileSize("TileSize");
const PString kTileScale("TileScale");
const PString kTileUVScale("TileUVScale");
const PString kWaterMaterial("WaterMaterial");
const PString kMaterialName("MaterialName");
const PString kMaterialColor("MaterialColor");
const PString kMaterialRI("MaterialRI");
const PString kMaterialFres("MaterialFres");
const PString kMaterialLightDir("MaterialLightDir");
const PString kMaterialLightColor("MaterialLightColor");
const PString kMaterialAmbientColor("MaterialAmbientColor");
const PString kOpacityTex("OpacityTex");
const PString kOpacityAmount("OpacityAmount");
const PString kCubeTextures("CubeTextures");
const PString kWaterAnim("WaterAnim");
const PString kDetailAnimFile("DetailAnimFile");
const PString kDetailAnim("DetailAnim");
const PString kAnimName("AnimName");
const PString kAnimSize("AnimSize");
const PString kAnimWorldSize("AnimWorldSize");
const PString kAnimMaxHeight("AnimMaxHeight");
const PString kAnimScaleNormXY("AnimScaleNormXY");
const PString kAnimSampleRate("AnimSampleRate");
const PString kAnimPeriod("AnimPeriod");
const PString kAnimSpeed("AnimSpeed");
const PString kAnimGeometry("AnimGeometry");
const PString kPerlin("Perlin");
const PString kAnimPerlinFreq("AnimPerlinFreq");
const PString kAnimPerlinSpeed("AnimPerlinSpeed");
const PString kAnimPerlinOctaves("AnimPerlinOctaves");
const PString kFFT("FFT");
const PString kAnimFFTA("AnimFFTA");
const PString kAnimFFTWindVec("AnimFFTWindVec");
const PString kAnimFFTSeed("AnimFFTSeed");
const PString kAnimPhillipsA("AnimPhillipsA");
const PString kAcrossCosMax("AcrossCosMax");
const PString kAcrossCosMim("AcrossCosMim");
const PString kAcrossRatio("AcrossRatio");
const PString kOppositeCos("OppositeCos");
const PString kOppositeRatio("OppositeRatio");
const PString kWaterPhysics("WaterPhysics");
const PString kWaveSpeed("WaveSpeed");
const PString kViscosity("Viscosity");
const PString kDeltaTime("DeltaTime");

// the child `key` of config, ConfigData::unknownItem when missing
const ConfigData& Child(const ConfigData& config, const PString& key)
{
    const ConfigData& c = config(key);
    return &c != &ConfigData::unknownItem ? c : ConfigData::unknownItem;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// AnimProp (0x10009730, 0x10009830, 0x10002a90, 0x10002b60)

AnimProp::AnimProp() : frameData(nullptr)
{
    animName = PString("WaterAnimationPN");
    animSize = Size{128, 128};
    animWorldSize = 450.0f;
    animMaxHeight = 3.0f;
    animScaleNormXY = 1.0f;
    animSampleRate = 10;
    animPeriod = 5;
    animSpeed = 5.0f;
    animGeometry = 1;
    animPerlinFreq = 15.0f;
    animPerlinSpeed = 10.0f;
    animPerlinOctaves = 3;
    animFFTA = 0.03f;
    animFFTWindVec = Vector2{-15.0f, 5.0f};
    animFFTSeed = 0;
    animPhillipsA = 0.001f;
    acrossCosMax = -1.1f;
    acrossCosMim = 1.1f;
    acrossRatio = 1.0f;
    oppositeCos = -1.1f;
    oppositeRatio = 1.0f;
}

AnimProp::~AnimProp()
{
    if (frameData)
        Alloc::Delete(frameData);
}

// 0x10002a90: the frame buffer is shared, not copied (the original's shallow copy)
AnimProp::AnimProp(const AnimProp& o) : animName(o.animName), animSize(o.animSize),
    animWorldSize(o.animWorldSize), animMaxHeight(o.animMaxHeight), animScaleNormXY(o.animScaleNormXY),
    animSampleRate(o.animSampleRate), animPeriod(o.animPeriod), animSpeed(o.animSpeed), animGeometry(o.animGeometry),
    animPerlinFreq(o.animPerlinFreq), animPerlinSpeed(o.animPerlinSpeed), animPerlinOctaves(o.animPerlinOctaves),
    animFFTA(o.animFFTA), animFFTWindVec(o.animFFTWindVec), animFFTSeed(o.animFFTSeed),
    animPhillipsA(o.animPhillipsA), acrossCosMax(o.acrossCosMax), acrossCosMim(o.acrossCosMim),
    acrossRatio(o.acrossRatio), oppositeCos(o.oppositeCos), oppositeRatio(o.oppositeRatio),
    frameData(o.frameData)
{
}

// 0x10002b60
AnimProp& AnimProp::operator=(const AnimProp& o)
{
    animName = o.animName;
    animSize = o.animSize;
    animWorldSize = o.animWorldSize;
    animMaxHeight = o.animMaxHeight;
    animScaleNormXY = o.animScaleNormXY;
    animSampleRate = o.animSampleRate;
    animPeriod = o.animPeriod;
    animSpeed = o.animSpeed;
    animGeometry = o.animGeometry;
    animPerlinFreq = o.animPerlinFreq;
    animPerlinSpeed = o.animPerlinSpeed;
    animPerlinOctaves = o.animPerlinOctaves;
    animFFTA = o.animFFTA;
    animFFTWindVec = o.animFFTWindVec;
    animFFTSeed = o.animFFTSeed;
    animPhillipsA = o.animPhillipsA;
    acrossCosMax = o.acrossCosMax;
    acrossCosMim = o.acrossCosMim;
    acrossRatio = o.acrossRatio;
    oppositeCos = o.oppositeCos;
    oppositeRatio = o.oppositeRatio;
    frameData = o.frameData;
    return *this;
}

// 0x10009c20
void AnimProp::Load(const ConfigData& config)
{
    Child(config, kAnimName) >> animName;
    Child(config, kAnimSampleRate) >> animSampleRate;
    Child(config, kAnimPeriod) >> animPeriod;
    Child(config, kAnimWorldSize) >> animWorldSize;
    Child(config, kAnimMaxHeight) >> animMaxHeight;
    Child(config, kAnimScaleNormXY) >> animScaleNormXY;
    Child(config, kAnimSize) >> animSize;
    Child(config, kAnimSpeed) >> animSpeed;
    Child(config, kAnimGeometry) >> animGeometry;
    const ConfigData& perlin = config(kPerlin);
    if (&perlin != &ConfigData::unknownItem) {
        Child(perlin, kAnimPerlinFreq) >> animPerlinFreq;
        Child(perlin, kAnimPerlinSpeed) >> animPerlinSpeed;
        Child(perlin, kAnimPerlinOctaves) >> animPerlinOctaves;
    }
    const ConfigData& fft = config(kFFT);
    if (&fft != &ConfigData::unknownItem) {
        Child(fft, kAnimFFTA) >> animFFTA;
        Child(fft, kAnimFFTWindVec) >> animFFTWindVec;
        Child(fft, kAnimFFTSeed) >> animFFTSeed;
        Child(fft, kAnimPhillipsA) >> animPhillipsA;
        Child(fft, kAcrossCosMax) >> acrossCosMax;
        Child(fft, kAcrossCosMim) >> acrossCosMim;
        Child(fft, kAcrossRatio) >> acrossRatio;
        Child(fft, kOppositeCos) >> oppositeCos;
        Child(fft, kOppositeRatio) >> oppositeRatio;
    }
}

// 0x10009e70: the binary "WANI" form. A file that opens but has other magic reads as "not binary" (false) so the
// caller falls back to the config form.
bool AnimProp::LoadFile(File& file)
{
    unsigned char magic[4];
    if (file.Read(magic, 4, 1) != 4)
        return false;
    if (std::memcmp(magic, "WANI", 4) != 0)
        return false;
    if (!file.Read(animName) || !file.Read32(animSize.w) || !file.Read32(animSize.h)
        || !file.Read(animWorldSize) || !file.Read(animMaxHeight) || !file.Read(animScaleNormXY)
        || !file.Read32(animSampleRate) || !file.Read32(animPeriod) || !file.Read(animSpeed)
        || !file.Read32(animGeometry) || !file.Read(animPerlinFreq) || !file.Read(animPerlinSpeed)
        || !file.Read32(animPerlinOctaves) || !file.Read(animFFTA) || !file.Read(animFFTWindVec.x)
        || !file.Read(animFFTWindVec.y) || !file.Read32(animFFTSeed) || !file.Read(animPhillipsA)
        || !file.Read(acrossCosMax) || !file.Read(acrossCosMim) || !file.Read(acrossRatio)
        || !file.Read(oppositeCos) || !file.Read(oppositeRatio))
        return false;
    if (frameData)
        Alloc::Delete(frameData);
    unsigned int bytes = 4 * animSize.w * animSize.h * animSampleRate * animPeriod;
    frameData = Alloc::New(bytes);
    return file.Read(frameData, bytes, 1) == bytes;
}

// 0x1000a1a0
bool AnimProp::LoadFile(const PString& fileName)
{
    File file;
    PString base;  // the original leaves this stack PString unset; a null base resolves the same way
    const char* path = fileName.node ? fileName.node->text : nullptr;
    bool needConfig = file.Open(base, path, 0x10, 0).code != 0;
    if (!needConfig) {
        needConfig = !LoadFile(file);
        file.Close();
    }
    if (!needConfig)
        return true;
    // the binary form failed (or the file did not open): parse it as a config file
    File::Path configPath(path);
    if (!configPath.IsExt(".txt"))
        configPath.Add(".txt");
    ConfigParser parser;
    if (parser.Parse(configPath).code != 0)
        return false;
    const ConfigData& asset = Child(*parser.GetConfig(), kDetailAnim);
    if (&asset != &ConfigData::unknownItem)
        Load(asset);
    return true;
}

// 0x1000a370
bool AnimProp::SaveFile(File& file, unsigned char* frameData_) const
{
    if (file.Write("WANI", 4, 1) != 4 || !file.Write(animName) || !file.Write32(animSize.w)
        || !file.Write32(animSize.h) || !file.Write(animWorldSize) || !file.Write(animMaxHeight)
        || !file.Write(animScaleNormXY) || !file.Write32(animSampleRate) || !file.Write32(animPeriod)
        || !file.Write(animSpeed) || !file.Write32(animGeometry) || !file.Write(animPerlinFreq)
        || !file.Write(animPerlinSpeed) || !file.Write32(animPerlinOctaves) || !file.Write(animFFTA)
        || !file.Write(animFFTWindVec.x) || !file.Write(animFFTWindVec.y) || !file.Write32(animFFTSeed)
        || !file.Write(animPhillipsA) || !file.Write(acrossCosMax) || !file.Write(acrossCosMim)
        || !file.Write(acrossRatio) || !file.Write(oppositeCos) || !file.Write(oppositeRatio))
        return false;
    unsigned int bytes = 4 * animSize.w * animSize.h * animSampleRate * animPeriod;
    return file.Write(frameData_, bytes, 1) == bytes;
}

// ---------------------------------------------------------------------------------------------------------------
// MaterialProp (0x10009870, 0x10002c50, 0x10002ce0, 0x10002e00, 0x1000a5f0)

MaterialProp::MaterialProp()
{
    materialName = PString("Default.m.WaterMaterialFresWave");
    materialRI = 1.3333f;
    materialColor = Color{0.1f, 0.2f, 0.3f, 1.0f};
    ambientColor = Color{0.1f, 0.1f, 0.1f, 0.0f};
    lightColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
    lightDir = Vector3{0.0f, 0.70710677f, -0.70710677f};
    fres = 1;
}

MaterialProp::~MaterialProp() = default;

MaterialProp::MaterialProp(const MaterialProp& o) : materialName(o.materialName), materialRI(o.materialRI),
    materialColor(o.materialColor), ambientColor(o.ambientColor), lightColor(o.lightColor), lightDir(o.lightDir),
    fres(o.fres), opacityTex(o.opacityTex), opacityAmount(o.opacityAmount)
{
}

MaterialProp& MaterialProp::operator=(const MaterialProp& o)
{
    materialName = o.materialName;
    materialRI = o.materialRI;
    materialColor = o.materialColor;
    ambientColor = o.ambientColor;
    lightColor = o.lightColor;
    lightDir = o.lightDir;
    fres = o.fres;
    opacityTex = o.opacityTex;
    opacityAmount = o.opacityAmount;
    return *this;
}

void MaterialProp::Load(const ConfigData& config)
{
    Child(config, kMaterialName) >> materialName;
    Child(config, kMaterialColor) >> materialColor;
    Child(config, kMaterialRI) >> materialRI;
    Child(config, kMaterialFres) >> fres;
    Child(config, kMaterialLightDir) >> lightDir;
    Child(config, kMaterialLightColor) >> lightColor;
    Child(config, kMaterialAmbientColor) >> ambientColor;
    Child(config, kOpacityTex) >> opacityTex;
    Child(config, kOpacityAmount) >> opacityAmount;
}

// ---------------------------------------------------------------------------------------------------------------
// TileProp (0x10009960, 0x10002f30, 0x10002f50, 0x10002fc0, 0x1000a6f0)

TileProp::TileProp()
{
    tileName = PString("WaterStdTile");
    tileSize = Size{2, 2};
    tileScale = Vector3{1.0f, 1.0f, 1.0f};
    tileUVScale = Vector2{1.0f, 1.0f};
}

TileProp::~TileProp() = default;

TileProp::TileProp(const TileProp& o) : reserved(o.reserved), tileName(o.tileName), tileSize(o.tileSize),
    tileScale(o.tileScale), tileUVScale(o.tileUVScale)
{
}

TileProp& TileProp::operator=(const TileProp& o)
{
    reserved = o.reserved;
    tileName = o.tileName;
    tileSize = o.tileSize;
    tileScale = o.tileScale;
    tileUVScale = o.tileUVScale;
    return *this;
}

void TileProp::Load(const ConfigData& config)
{
    Child(config, kTileName) >> tileName;
    Child(config, kTileSize) >> tileSize;
    Child(config, kTileScale) >> tileScale;
    Child(config, kTileUVScale) >> tileUVScale;
}

// ---------------------------------------------------------------------------------------------------------------
// PhysicsProp (0x10009930, 0x10002f10, 0x1000a770)

PhysicsProp::PhysicsProp() : waveSpeed(100.0f), viscosity(0.5f), deltaTime(0.02f)
{
}

PhysicsProp& PhysicsProp::operator=(const PhysicsProp& o)
{
    waveSpeed = o.waveSpeed;
    viscosity = o.viscosity;
    deltaTime = o.deltaTime;
    return *this;
}

void PhysicsProp::Load(const ConfigData& config)
{
    Child(config, kWaveSpeed) >> waveSpeed;
    Child(config, kViscosity) >> viscosity;
    Child(config, kDeltaTime) >> deltaTime;
}

// ---------------------------------------------------------------------------------------------------------------
// WaterManagerConfig (0x100099b0, 0x10003050, 0x10003180, 0x100032a0, 0x100033d0..0x10003430, 0x1000aa70, 0x1000a7d0)

namespace {

const PString kManagerNoLod("WaterManagerNoLod");

}  // namespace

WaterManagerConfig::WaterManagerConfig() : managerName(kManagerNoLod)
{
    // six interned empty strings, as the original's init creates them
    for (int i = 0; i < 6; ++i)
        cubeTextures[i] = PString("");
}

WaterManagerConfig::~WaterManagerConfig() = default;

WaterManagerConfig::WaterManagerConfig(const WaterManagerConfig& o) : managerName(o.managerName),
    surfaceAnimation(o.surfaceAnimation), detailAnimation(o.detailAnimation),
    materialProperties(o.materialProperties), tileProperties(o.tileProperties),
    physicsProperties(o.physicsProperties)
{
    for (int i = 0; i < 6; ++i)
        cubeTextures[i] = o.cubeTextures[i];
}

WaterManagerConfig& WaterManagerConfig::operator=(const WaterManagerConfig& o)
{
    managerName = o.managerName;
    surfaceAnimation = o.surfaceAnimation;
    detailAnimation = o.detailAnimation;
    materialProperties = o.materialProperties;
    tileProperties = o.tileProperties;
    physicsProperties = o.physicsProperties;
    for (int i = 0; i < 6; ++i)
        cubeTextures[i] = o.cubeTextures[i];
    return *this;
}

// 0x1000aa70: vtable slot 0
bool WaterManagerConfig::Load(const ConfigData& config, const ResourceDatabase* db, const PString& path)
{
    managerName = config.GetName();
    const ConfigData& tile = Child(config, kWaterTile);
    if (&tile != &ConfigData::unknownItem)
        tileProperties.Load(tile);
    const ConfigData& material = Child(config, kWaterMaterial);
    if (&material != &ConfigData::unknownItem)
        materialProperties.Load(material);
    const ConfigData& physics = Child(config, kWaterPhysics);
    if (&physics != &ConfigData::unknownItem)
        physicsProperties.Load(physics);
    const ConfigData& cube = Child(config, kCubeTextures);
    if (&cube != &ConfigData::unknownItem) {
        static const char* const kFaces[6] = {"posX", "negX", "negZ", "posZ", "posY", "negY"};
        for (int i = 0; i < 6; ++i) {
            PString key(kFaces[i]);
            Child(cube, key) >> cubeTextures[i];
        }
    }
    const ConfigData& surface = Child(config, kWaterAnim);
    if (&surface != &ConfigData::unknownItem)
        surfaceAnimation.Load(surface);

    const ConfigData& detailFile = Child(config, kDetailAnimFile);
    if (&detailFile == &ConfigData::unknownItem) {
        const ConfigData& detail = Child(config, kDetailAnim);
        if (&detail != &ConfigData::unknownItem)
            detailAnimation.Load(detail);
        return true;
    }

    PString animFile;
    detailFile >> animFile;
    bool absolute = animFile.node && PString::Cache()->Find(animFile.node, ':', 0) != -1;
    const PStringCache::Node* pathNode = path.node;
    if (pathNode == nullptr || pathNode->length == 0
        || (animFile.node && (animFile.node->text[0] == '\\' || animFile.node->text[0] == '/')) || absolute) {
        // use the name as it is
    } else {
        // a relative name: try it under the config's own path, through the database when one is given
        File::Path combined(pathNode->text);
        if (animFile.node)
            combined.AddPath(animFile.node->text);
        PString combinedName(combined);
        if (db) {
            PStringCache::Node* out = nullptr;
            const PString* found = CallDbSlot3(db, &out, combinedName);
            PString resolved = *found;
            if (out && PString::Cache())
                PString::Cache()->Destroy(out);
            if (detailAnimation.LoadFile(resolved))
                return true;
        } else {
            if (detailAnimation.LoadFile(combinedName))
                return true;
        }
    }
    // the path form did not load: resolve the raw name through the database (unless absolute) and try again
    if (db && !absolute) {
        PStringCache::Node* out = nullptr;
        const PString* found = CallDbSlot3(db, &out, animFile);
        animFile = *found;
        if (out && PString::Cache())
            PString::Cache()->Destroy(out);
    }
    return detailAnimation.LoadFile(animFile);
}

// 0x1000a7d0: vtable slot 1
bool WaterManagerConfig::Load(const PString& fileName, const ResourceDatabase* db)
{
    ConfigParser parser;
    File::Path dir(fileName.node ? fileName.node->text : nullptr);
    dir.DelName(false);
    PString name;
    if (db && (fileName.node == nullptr || PString::Cache()->Find(fileName.node, ':', 0) == -1)) {
        PStringCache::Node* out = nullptr;
        const PString* found = CallDbSlot3(db, &out, fileName);
        name = *found;
        if (out && PString::Cache())
            PString::Cache()->Destroy(out);
    } else {
        name = fileName;
        dir = "";
    }
    if (parser.Parse(name.node ? name.node->text : nullptr).code != 0)
        return false;
    const ConfigData& water = Child(*parser.GetConfig(), kWaterManager);
    if (&water == &ConfigData::unknownItem)
        return false;
    PString basePath(dir);
    return Load(water, db, basePath);
}

const PString& WaterManagerConfig::GetManagerName() const
{
    return managerName;
}

const AnimProp& WaterManagerConfig::GetSurfaceAnimation() const
{
    return surfaceAnimation;
}

const AnimProp& WaterManagerConfig::GetDetailAnimation() const
{
    return detailAnimation;
}

AnimProp& WaterManagerConfig::GetDetailAnimation()
{
    return detailAnimation;
}

const PString* WaterManagerConfig::GetCubeTextures() const
{
    return cubeTextures;
}

const MaterialProp& WaterManagerConfig::GetMaterialProperties() const
{
    return materialProperties;
}

const TileProp& WaterManagerConfig::GetTileProperties() const
{
    return tileProperties;
}

const PhysicsProp& WaterManagerConfig::GetPhysicsProperties() const
{
    return physicsProperties;
}

}  // namespace Jet
