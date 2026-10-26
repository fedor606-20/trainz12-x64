// WaterSystem_7c unit U2 (notes/WaterSystem.md): the water manager's config model. Four POD property structs
// (animation, material, tile, physics settings) and the WaterManagerConfig that holds them and loads them from a
// config node or a file. Layouts and behaviour follow the TS12 assembly (addresses per member in
// WaterManagerConfig.cpp).
#pragma once

#include <jet/Math.h>        // Size, Vector2, Vector3
#include <jet/Platform.h>
#include <jet/PString.h>
#include <jet/SmallTypes.h>  // Color

namespace Jet {

class ConfigData;
class File;
class ResourceDatabase;

// ---------------------------------------------------------------------------------------------------------------
// one animation's settings (0x60): the surface or detail animation of the water

struct AnimProp {
    AnimProp();
    AnimProp(const AnimProp& o);
    ~AnimProp();
    AnimProp& operator=(const AnimProp& o);

    void __cdecl Load(const ConfigData& config);
    bool __cdecl LoadFile(File& file);              // the binary "WANI" form
    bool __cdecl LoadFile(const PString& fileName);  // WANI, or a config file when the binary read fails
    bool __cdecl SaveFile(File& file, unsigned char* frameData) const;

    PString animName;         // +0x00 "WaterAnimationPN"
    Size animSize;            // +0x04 {128, 128}
    float animWorldSize;      // +0x0c 450.0
    float animMaxHeight;      // +0x10 3.0
    float animScaleNormXY;    // +0x14 1.0
    int animSampleRate;       // +0x18 10
    int animPeriod;           // +0x1c 5
    float animSpeed;          // +0x20 5.0
    int animGeometry;         // +0x24 1
    float animPerlinFreq;     // +0x28 15.0
    float animPerlinSpeed;    // +0x2c 10.0
    int animPerlinOctaves;    // +0x30 3
    float animFFTA;           // +0x34 0.03
    Vector2 animFFTWindVec;   // +0x38 {-15.0, 5.0}
    int animFFTSeed;          // +0x40 0
    float animPhillipsA;      // +0x44 0.001
    float acrossCosMax;       // +0x48 -1.1
    float acrossCosMim;       // +0x4c 1.1
    float acrossRatio;        // +0x50
    float oppositeCos;        // +0x54
    float oppositeRatio;      // +0x58
    void* frameData;          // +0x5c the WANI frame bytes (shared on copy, as the original)
};
JET_LAYOUT32(sizeof(AnimProp) == 0x60);

// ---------------------------------------------------------------------------------------------------------------
// the water material's settings (0x50)

struct MaterialProp {
    MaterialProp();
    MaterialProp(const MaterialProp& o);
    ~MaterialProp();
    MaterialProp& operator=(const MaterialProp& o);

    void __cdecl Load(const ConfigData& config);

    PString materialName;   // +0x00 "Default.m.WaterMaterialFresWave"
    float materialRI;       // +0x04 1.3333 (refractive index)
    Color materialColor;    // +0x08 {0.1, 0.2, 0.3, 1}
    Color ambientColor;     // +0x18 {0.1, 0.1, 0.1, 0}
    Color lightColor;       // +0x28 {1, 1, 1, 1}
    Vector3 lightDir;       // +0x38 {0, 0.7071, -0.7071}
    int fres;               // +0x44 1
    PString opacityTex;     // +0x48
    float opacityAmount;    // +0x4c
};
JET_LAYOUT32(sizeof(MaterialProp) == 0x50);

// ---------------------------------------------------------------------------------------------------------------
// the tile's settings (0x24)

struct TileProp {
    TileProp();
    TileProp(const TileProp& o);
    ~TileProp();
    TileProp& operator=(const TileProp& o);

    void __cdecl Load(const ConfigData& config);

    float reserved;       // +0x00 never set by the original's ctor or Load
    PString tileName;     // +0x04 "WaterStdTile"
    Size tileSize;        // +0x08 {2, 2}
    Vector3 tileScale;    // +0x10 {1, 1, 1}
    Vector2 tileUVScale;  // +0x1c {1, 1}
};
JET_LAYOUT32(sizeof(TileProp) == 0x24);

// ---------------------------------------------------------------------------------------------------------------
// the physics settings (0x0c)

struct PhysicsProp {
    PhysicsProp();
    PhysicsProp& operator=(const PhysicsProp& o);

    void __cdecl Load(const ConfigData& config);

    float waveSpeed;  // +0x00 100.0
    float viscosity;  // +0x04 0.5
    float deltaTime;  // +0x08 0.02
};
JET_LAYOUT32(sizeof(PhysicsProp) == 0x0c);

// ---------------------------------------------------------------------------------------------------------------
// the whole water config (0x160)

class WaterManagerConfig {
public:
    WaterManagerConfig();
    WaterManagerConfig(const WaterManagerConfig& o);
    ~WaterManagerConfig();
    WaterManagerConfig& operator=(const WaterManagerConfig& o);

    // vtable slots 0 and 1, in declaration order as in the original
    virtual bool __cdecl Load(const ConfigData& config, const ResourceDatabase* db, const PString& path);
    virtual bool __cdecl Load(const PString& fileName, const ResourceDatabase* db);

    const PString& __cdecl GetManagerName() const;   // +0x04
    const AnimProp& __cdecl GetSurfaceAnimation() const;  // +0x08
    const AnimProp& __cdecl GetDetailAnimation() const;   // +0x68
    AnimProp& __cdecl GetDetailAnimation();               // (the non-const overload)
    const PString* __cdecl GetCubeTextures() const;       // +0x148, six entries posX negX negZ posZ posY negY
    const MaterialProp& __cdecl GetMaterialProperties() const;  // +0xc8
    const TileProp& __cdecl GetTileProperties() const;          // +0x118
    const PhysicsProp& __cdecl GetPhysicsProperties() const;    // +0x13c

private:
    PString managerName;                 // +0x04
    AnimProp surfaceAnimation;           // +0x08
    AnimProp detailAnimation;            // +0x68
    MaterialProp materialProperties;     // +0xc8
    TileProp tileProperties;             // +0x118
    PhysicsProp physicsProperties;       // +0x13c
    PString cubeTextures[6];             // +0x148
};
JET_LAYOUT32(sizeof(WaterManagerConfig) == 0x160);

}  // namespace Jet
