// The x86 WaterMaterial state methods. Lifetime and vtables still belong to the original.
#include <cstddef>

class WsWaterMaterialState {
public:
    bool GetDetailAnim() const;
    bool GetUseVertexProgram();
    void IgnoreBool(bool);
    void IgnoreFloat(float);
    void SetWaterColor(const float* source);
    const float* GetWaterColor() const;
    void SetLightColor(const float* source);
    const float* GetLightColor() const;
    void SetAmbientColor(const float* source);
    const float* GetAmbientColor() const;
    void SetLightDir(const float* source);
    const float* GetLightDir() const;
    void SetUVScrollVelocity(float x, float y);
    float* GetUVScrollVelocity(float* result) const;
    void SetReflectEyeVector(bool value);
    bool IsReflectEyeVector() const;

    unsigned char material[0xa4];
    float waterColor[4];
    float lightColor[4];
    float ambientColor[4];
    float lightDir[3];
    bool reflectEyeVector;
};

static_assert(offsetof(WsWaterMaterialState, reflectEyeVector) == 0xe0);

namespace {
void CopyComponents(float* destination, const float* source, unsigned count)
{
    // The original uses x87 loads and stores, which quiet signaling NaNs.
    for (unsigned i = 0; i < count; ++i) {
        __asm {
            mov eax, source
            mov edx, destination
            mov ecx, i
            fld dword ptr [eax+ecx*4]
            fstp dword ptr [edx+ecx*4]
        }
    }
}
}

bool WsWaterMaterialState::GetDetailAnim() const { return false; }
bool WsWaterMaterialState::GetUseVertexProgram() { return true; }
void WsWaterMaterialState::IgnoreBool(bool) {}
void WsWaterMaterialState::IgnoreFloat(float) {}
void WsWaterMaterialState::SetWaterColor(const float* source) { CopyComponents(waterColor, source, 4); }
const float* WsWaterMaterialState::GetWaterColor() const { return waterColor; }
void WsWaterMaterialState::SetLightColor(const float* source) { CopyComponents(lightColor, source, 4); }
const float* WsWaterMaterialState::GetLightColor() const { return lightColor; }
void WsWaterMaterialState::SetAmbientColor(const float* source) { CopyComponents(ambientColor, source, 4); }
const float* WsWaterMaterialState::GetAmbientColor() const { return ambientColor; }
void WsWaterMaterialState::SetLightDir(const float* source) { CopyComponents(lightDir, source, 3); }
const float* WsWaterMaterialState::GetLightDir() const { return lightDir; }
void WsWaterMaterialState::SetUVScrollVelocity(float, float) {}

// Spell out the hidden return pointer used by the original Vector2 return.
float* WsWaterMaterialState::GetUVScrollVelocity(float* result) const
{
    result[0] = 0.0f;
    result[1] = 0.0f;
    return result;
}

void WsWaterMaterialState::SetReflectEyeVector(bool value) { reflectEyeVector = value; }
bool WsWaterMaterialState::IsReflectEyeVector() const { return reflectEyeVector; }
