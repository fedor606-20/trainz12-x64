// WaterSystem_7c unit U1 (notes/WaterSystem.md): Perlin noise with fractional Brownian motion (turbulence).
// The object is a 0x383C-byte POD with the permutation and gradient tables laid out exactly as in the TS12
// binary, so it can be instantiated and copied across module boundaries like the original.
#pragma once

#include <jet/Platform.h>

namespace Jet {

class PNoise {
public:
    PNoise();  // builds the tables from CRT rand(); the object must not be read before this
    ~PNoise();  // a no-op in the original (shared empty body)
    PNoise& operator=(const PNoise& o);  // memcpy of the whole object, as the original

    void __cdecl SetNumOctaves(int numOctaves);

    float __cdecl Noise1(float x) const;
    float __cdecl Noise2(const float* p) const;
    float __cdecl Noise3(const float* p) const;

    float __cdecl Turbulence1(float x) const;
    float __cdecl Turbulence2(const float* p) const;  // doubles p[0..1] in place
    float __cdecl Turbulence2(float x, float y) const;
    float __cdecl Turbulence3(const float* p) const;  // doubles p[0..1] in place (not p[2])
    float __cdecl Turbulence3(float x, float y, float z) const;
    float __cdecl TurbulenceTiled(float x, float y, float z, float period) const;
    float __cdecl TurbulenceLoopingTiled(float x, float y, float z, float period, float loopPeriod) const;

private:
    // byte-exact TS12 layout; the hash chains index the permutation 1-based (perm[1..256]) and wrap into the
    // -Wrapped copies, and perm[0] is this very field -- the original reads numOctaves there
    int numOctaves;                 // +0
    int perm[256];                  // +4
    int permWrapped[258];           // +1028
    float grad3D[768];              // +2060  256 unit vectors, 3 components
    float grad3DWrapped[774];       // +5120
    float grad2D[512];              // +8228  256 unit vectors, 2 components
    float grad2DWrapped[516];       // +10276
    float grad1D[256];              // +12340 256 unit scalars
    float grad1DWrapped[258];       // +13364

public:
    // The original computes in x87 at the CRT's 53-bit precision, so these work in double and round to float
    // only where the assembly stores to a float (the public float wrappers round at the export boundary).
    // WaterAnimationPN uses them the way the original's callers use the unrounded st0 return of the exports.
    double Noise1D(float x) const;
    double Noise2D(const float* p) const;
    double Noise3D(const float* p) const;
    double Turbulence1D(float x) const;
    double Turbulence2D(const float* p) const;
    double Turbulence3D(const float* p) const;
    double TurbulenceTiledD(float x, float y, float z, float period) const;
    double TurbulenceLoopingTiledD(float x, float y, float z, float period, float loopPeriod) const;

private:
    int Perm(int k) const;                  // permutation entry k, 1-based, wrapped
    const float* Grad2(int entry) const;    // entry < 256
    const float* Grad3(int entry) const;    // entry < 511 (wraps into Grad3Wrapped)
};

JET_LAYOUT32(sizeof(PNoise) == 0x383C);

}  // namespace Jet
