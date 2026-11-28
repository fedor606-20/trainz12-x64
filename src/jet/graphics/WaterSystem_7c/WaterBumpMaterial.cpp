// WaterSystem_7c U8a: WaterBumpMaterial. Behaviour follows the original; the rendering paths keep the
// original's raw vtable calls because the RenderSystem classes they drive stay in RenderSystem_7c.
#include <jet/WaterMaterial.h>

#include "WaterMaterialInternal.h"

#include <jet/FrameAnimation.h>
#include <jet/Geometry.h>
#include <jet/GfxLanguage.h>
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>
#include <jet/Texture.h>
#include <jet/VCall.h>
#include <jet/WaterManagerConfig.h>
#include <jet/WaterTextureAnimation.h>

#include <windows.h>

#include <cmath>
#include <cstring>

namespace Jet {

// 0x10038fdc / 0x10038fec
const UId WaterBumpMaterial::uid("5EA5C88C-A2F1-423A-B5AB-FFB935941E25");
const PString WaterBumpMaterial::UNDECORATED_TYPE("WaterBumpMaterial");
const PString WaterBumpMaterial::DECORATED_TYPE("m.WaterBumpMaterial");

GeometryChunk* WsCreateBumpGeometryChunk(const WaterBumpMaterial& material, unsigned int numVertices,
                                         unsigned int numPrimitives, unsigned int numIndices,
                                         unsigned int numFaceNormals, Material::GeometryType type,
                                         GeometryChunkCreateHint* hint, bool waveVertexProgram);

namespace {

// a material vtable slot that RenderSystem calls with `this` on the stack (all its virtuals are __cdecl)
template <class R, class... A>
R MC(const void* obj, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const void*, A...);
    return (*reinterpret_cast<F* const*>(obj))[offset / sizeof(void*)](obj, args...);
}

// 0x1002f38c: the display frame SetDisplayState last ran the interpolation for (shared by every material)
unsigned int lastLerpFrame = 0xffffffffu;

Texture* TextureAt(Texture* texture) { return texture; }
void AddTexture(Texture* texture)
{
    if (texture) texture->AddReference();
}
void ReleaseTexture(Texture* texture)
{
    if (texture) texture->ReleaseReference();
}

Material::Result Code(int code) { return Material::Result(code); }

// Render3DCore_7c export: the camera's view transform (the RenderSystem shell has no Camera class)
const Matrix4* CameraViewTransform(const void* camera)
{
    using F = const Matrix4*(__cdecl*)(const void*);
    HMODULE module = GetModuleHandleA("Render3DCore_7c.dll");
    if (!module) return nullptr;
    F fn = reinterpret_cast<F>(GetProcAddress(module, "?GetViewTransform@Camera@Jet@@QBAABVMatrix4@2@XZ"));
    return fn ? fn(camera) : nullptr;
}

}  // namespace

// 0x100058e0: note the original leaves renderTarget, the animation pointers and the UV offsets untouched.
WaterBumpMaterial::WaterBumpMaterial(MaterialManager* manager)
    : WaterMaterial(manager)
    , reflectionTexture(nullptr)
    , vertexProgram(nullptr)
    , interpProgram(nullptr)
    , combiner(nullptr)
    , lerpCombiner(nullptr)
    , refractiveIndex(1.3333f)
    , color{0.1f, 0.2f, 0.3f, 1.0f}
    , vertexType(nullptr)
    , enableTextureInterpolation(false)
    , enableRefraction(true)
    , scaleNormXY(1.0f)
{
    WsInstallMaterialVtables(this, 1);
}

// 0x10005a10: the reference-counted programs, the render target, the reflection texture, then WaterMaterial.
WaterBumpMaterial::~WaterBumpMaterial()
{
    ReleaseTexture(reflectionTexture);
    if (vertexProgram) vertexProgram->ReleaseReference();
    if (interpProgram) interpProgram->ReleaseReference();
    if (combiner) combiner->ReleaseReference();
    if (lerpCombiner) lerpCombiner->ReleaseReference();
    if (renderTarget) VCall<void>(renderTarget, 0, 1u);
}

// 0x10005b10: Material::CreateInstance by uid, base InitInstance, then the reflection texture reference.
Material* WaterBumpMaterial::CreateInstance() const
{
    Material* const instance = Material::CreateInstance(uid, materialManager);
    if (instance) {
        Material::InitInstance(*instance);
        Texture* const source = const_cast<WaterBumpMaterial*>(this)->reflectionTexture;
        auto* const target = reinterpret_cast<Texture**>(reinterpret_cast<unsigned char*>(instance) + 0xe4);
        AddTexture(source);
        ReleaseTexture(*target);
        *target = source;
    }
    return instance;
}

// 0x100057e0
bool WaterBumpMaterial::operator==(const Material& other) const
{
    const auto* const material = dynamic_cast<const WaterBumpMaterial*>(&other);
    return material ? Material::Equals(other) : false;
}

// 0x10005810 (shared with Ocean)
void WaterBumpMaterial::SetResourcePriority(float priority) const
{
    auto* const self = const_cast<WaterBumpMaterial*>(this);
    self->resourcePriority = priority;
    if (self->reflectionTexture) VCall<void>(self->reflectionTexture, 0x48, priority);
}

// 0x10005840
const PString& WaterBumpMaterial::GetType() const { return UNDECORATED_TYPE; }

// 0x10014560
void WaterBumpMaterial::Activate(int detail)
{
    if (reflectionTexture) VCall<void>(reflectionTexture, 0x3c, detail);
}

// 0x10014120
void WaterBumpMaterial::Bind(Display* display, ViewPort* view, unsigned int pass)
{
    if (pass == 0) VCall<void>(display, 0xc8, view);
}

// 0x10005850
void WaterBumpMaterial::Unbind(Display* display, ViewPort* view, unsigned int pass)
{
    if (pass == 0) VCall<void>(display, 0xcc, view);
}

// 0x91d0 (shared no-op)
void WaterBumpMaterial::CacheChunk(Display*, ViewPort*, GeometryChunk*, const DisplayLightPtrList*,
                                           const Matrix4*) const
{
}
void WaterBumpMaterial::RenderNumChunks(Display*, ViewPort*, GeometryChunk**, const DisplayLightPtrList**,
                                                const Matrix4**, unsigned int, int) const
{
}
void WaterBumpMaterial::RenderPrimitives(Display*, ViewPort*, GeometryChunk*, const DisplayLightPtrList*,
                                                 const Matrix4*, unsigned int, const unsigned short*, unsigned int,
                                                 DPrimitive, int) const
{
}

// 0x10005b70
GeometryChunk* WaterBumpMaterial::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                              unsigned int numIndices, unsigned int numFaceNormals,
                                                              GeometryType type, GeometryChunkCreateHint* hint,
                                                              bool waveVertexProgram) const
{
    return WsCreateBumpGeometryChunk(*this, numVertices, numPrimitives, numIndices, numFaceNormals, type, hint,
                                     waveVertexProgram);
}

// 0x10005c30
void WaterBumpMaterial::CreateVertexType()
{
    if (vertexType) return;
    auto* const memory = static_cast<VertexType*>(Alloc::New(sizeof(VertexType)));
    if (memory) {
        ::new (memory) VertexType(static_cast<VertexTypeID>(99));
        vertexType = memory;
        vertexType->CreateStreams(1);
        vertexType->CreateItems(0, 3);
        const PString empty;
        vertexType->AddItem(0, static_cast<VertexItemType>(1), -1, static_cast<DVertexItemDataType>(0),
                            static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
        vertexType->AddItem(0, static_cast<VertexItemType>(2), -1, static_cast<DVertexItemDataType>(0),
                            static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
        vertexType->AddItem(0, static_cast<VertexItemType>(15), -1, static_cast<DVertexItemDataType>(0),
                            static_cast<VertexItemSemanticType>(1), empty, -1, static_cast<VertexType::BoolEnum>(-1));
    }
}

// 0x10005df0 (shared with Ocean)
Material::Result WaterBumpMaterial::SetTexture(int type, const ResourceDatabase* database,
                                                       const PString& name, float amount, const String& request)
{
    if (type != 9) return Code((type == 1 || type == 6) ? 0 : 2);
    auto* const manager = reinterpret_cast<TextureManager*>(
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(materialManager) + 0xc));
    using CreateFn = Texture*(__cdecl*)(TextureManager*, const ResourceDatabase*, const PString&, const String&);
    auto* const table = *reinterpret_cast<CreateFn* const*>(manager);
    Texture* texture = table[3](manager, database, name, request);
    ReleaseTexture(reflectionTexture);
    reflectionTexture = texture;
    if (!texture) return Code(3);
    VCall<void>(texture, 0x48, resourcePriority);
    SetTextureDefinition(9, name, amount);
    return Code(0);
}

// 0x10005eb0 (shared with Ocean)
Material::Result WaterBumpMaterial::SetTexture(int type, const AutoReference<Texture>& texture, float amount)
{
    if (type != 9) return Code((type == 1 || type == 6) ? 0 : 2);
    Texture* const source = texture.Get();
    if (!source) return Code(3);
    AddTexture(source);
    ReleaseTexture(reflectionTexture);
    reflectionTexture = source;
    VCall<void>(reflectionTexture, 0x48, resourcePriority);
    SetTextureDefinition(9, *reinterpret_cast<const PString*>(reinterpret_cast<unsigned char*>(reflectionTexture) + 0x10),
                         amount);
    return Code(0);
}

// 0x10014500 (shared)
Material::Result WaterBumpMaterial::DuplicateTexture(int type)
{
    if (type != 9) return Code(2);
    if (!reflectionTexture) return Code(3);
    Texture* const copy = VCall<Texture*>(reflectionTexture, 8);
    ReleaseTexture(reflectionTexture);
    reflectionTexture = copy;
    return Code(0);
}

// 0x10014810 (shared)
Texture* WaterBumpMaterial::GetTexture(int type) { return type == 9 ? reflectionTexture : nullptr; }
const Texture* WaterBumpMaterial::GetTexture(int type) const { return type == 9 ? reflectionTexture : nullptr; }

// 0x10005870 (shared)
Material::Result WaterBumpMaterial::IsLoaded() const
{
    if (reflectionTexture && VCall<bool>(reflectionTexture, 0xc)) return Code(0);
    return Code(4);
}

// 0x10005710 / 0x10005730
void WaterBumpMaterial::SetScaleNormXY(float value) { scaleNormXY = value; }
float WaterBumpMaterial::GetScaleNormXY() const { return scaleNormXY; }

// 0x100058d0
void WaterBumpMaterial::EnableTextureInterpolation(bool value) { enableTextureInterpolation = value; }

// 0x10058b0
void WaterBumpMaterial::SetRefractiveIndex(float value) { refractiveIndex = value; }

// 0x10013fb0 / 0x10013fc0
bool WaterBumpMaterial::SetReflectionType(int) { return false; }
int WaterBumpMaterial::GetReflectionType() const { return 1; }

// 0x100144f0
bool WaterBumpMaterial::Initialize(const ResourceDatabase*, const WaterManagerConfig*, FrameAnimation*,
                                   WaterTextureAnimation*, int)
{
    return false;
}

// 0x10014100
bool WaterBumpMaterial::Equals(const WaterBumpMaterial& other) const { return Material::Equals(other); }

// 0x100144a0 (shared with Ocean): base state and the reflection texture reference.
void WaterBumpMaterial::InitInstance(WaterBumpMaterial& instance) const
{
    Material::InitInstance(instance);
    Texture* const source = const_cast<WaterBumpMaterial*>(this)->reflectionTexture;
    AddTexture(source);
    ReleaseTexture(instance.reflectionTexture);
    instance.reflectionTexture = source;
}

// 0x10005f60
void WaterBumpMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            int pass) const
{
    VCall<void>(display, 0xc8, view);
    const VertexProgram* const program = VCall<const VertexProgram*>(chunk, 0xf0, 0);
    VCall<void>(view, 0x54, pass);
    const Matrix4* viewMatrix = lights ? CameraViewTransform(lights) : nullptr;
    const float opacity = *reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(display) + 0x80) *
                          *reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(chunk) + 0x3c);
    const_cast<WaterBumpMaterial*>(this)->SetLightingState(display, modelView, chunk, program, lights, viewMatrix,
                                                           opacity, false);
    VCall<void>(display, 0xc4, view, *reinterpret_cast<void* const*>(reinterpret_cast<unsigned char*>(chunk) + 0x20),
                modelView);
    VCall<void>(combiner, 0x14);
    VCall<void>(vertexProgram, 0x4c);
    VCall<void>(vertexProgram, 0x3c, 1);
    VCall<void>(chunk, 0x38, display, view, program, 1, 0);
    VCall<void>(display, 0xcc, view);
}

// 0x10006070
void WaterBumpMaterial::SetDisplayState(Display* display) const
{
    const unsigned int frame =
        *reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(display) + 0x3c);
    if (frame != lastLerpFrame) {
        if (textureAnimation && enableTextureInterpolation) CalculateLerpTexture(display);
        lastLerpFrame = frame;
    }
    display->SetCullMode((flags & 1) ? static_cast<DCull>(0) : static_cast<DCull>(1));
    if (reflectionTexture) VCall<void>(reflectionTexture, 0x44, 3);
    if (!textureAnimation) return;
    if (!enableTextureInterpolation) {
        Texture* const texture = textureAnimation->GetTexture(frameAnimation->GetCurrentFrame());
        VCall<void>(texture, 0x44, 0);
    } else if (renderTarget) {
        VCall<void>(renderTarget, 0x20, 0);
    }
}

// 0x10006160
void WaterBumpMaterial::ClearDisplayState(Display* display) const
{
    for (int i = 3; i >= 0; --i) VCall<void>(display, 0x12c, i);
    if (combiner) VCall<void>(combiner, 0x18);
    if (vertexProgram) VCall<void>(vertexProgram, 0x18);
    display->SetCullMode(static_cast<DCull>(1));
}

// 0x10006840
void WaterBumpMaterial::CalculateLerpTexture(Display* display) const
{
    FrameAnimation* const animation = frameAnimation;
    display->SetRenderTarget(static_cast<DisplayTexture*>(renderTarget), 0);
    Texture* const first = textureAnimation->GetTexture(animation->GetCurrentFrame());
    Texture* const second =
        textureAnimation->GetTexture((animation->GetCurrentFrame() + 1) % animation->GetNumFrames());

    const float delta = animation->GetDeltaFrame();
    const Vector4 lerp{delta, delta, delta, delta};
    VCall<void>(lerpCombiner, 0x30, 0, &lerp);

    const Vector4 texSize{uvScrollX * 0.5f, uvScrollY * 0.5f, 0.0f, 0.0f};
    const PString directX("DirectX");
    if (VCall<bool>(interpProgram, 0x1c, &directX)) {
        const int reg = interpProgram->GetLocalConstantNumber(PString("texSize"));
        VCall<void>(interpProgram, 0x5c, reg, &texSize);
    }
    VCall<void>(lerpCombiner, 0x14);
    VCall<void>(interpProgram, 0x3c, 1);
    VCall<void>(display, 0xac, 1);
    display->SetDepthTestMode(false);
    VCall<void>(first, 0x44, 0);
    VCall<void>(second, 0x44, 1);
    RenderSquare(display);
    VCall<void>(lerpCombiner, 0x18);
    VCall<void>(interpProgram, 0x18);
    display->SetRenderTarget(nullptr, 0);
    display->SetDepthTestMode(true);
    for (int i = 0; i < 4; ++i) VCall<void>(display, 0x12c, i);
}

// 0x10006b20
void WaterBumpMaterial::RenderSquare(Display* display) const
{
    static float vertices[4][5];
    static bool built;
    if (!built) {
        for (int i = 0; i < 4; ++i) {
            vertices[i][0] = (i == 0 || i == 3) ? -1.0f : 1.0f;
            vertices[i][1] = i < 2 ? -1.0f : 1.0f;
            vertices[i][2] = 0.0f;
            vertices[i][3] = (i == 0 || i == 3) ? 0.0f : 1.0f;
            vertices[i][4] = i < 2 ? 1.0f : 0.0f;
        }
        built = true;
    }
    using DeclFn = int(__cdecl*)(Display*, int, int, int, const void*, int);
    using DrawFn = void(__cdecl*)(Display*, int);
    auto* const table = *reinterpret_cast<void* const* const*>(display);
    const int declaration = reinterpret_cast<DeclFn>(table[0x10c / 4])(display, 4, 5, 2, vertices, 4);
    reinterpret_cast<DrawFn>(table[0xe4 / 4])(display, declaration);
}

// 0x100061d0: the camera basis, the view matrix, the projection constants, then the refraction pair.
void WaterBumpMaterial::SetupVertexProgramConstants(Display*, Camera* camera, const Matrix4* modelView) const
{
    const float* const basisSource = reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(camera) + 0x238);
    Matrix4 basis = Matrix4{};
    basis.m[0] = basisSource[0];
    basis.m[1] = basisSource[1];
    basis.m[2] = basisSource[2];
    basis.m[4] = basisSource[3];
    basis.m[5] = basisSource[4];
    basis.m[6] = basisSource[5];
    basis.m[8] = basisSource[6];
    basis.m[9] = basisSource[7];
    basis.m[10] = basisSource[8];
    basis.m[15] = 1.0f;

    // the original's basis flip: x -> -x, z -> -y (the m6/m9 pair is not a diagonal, as in the binary)
    Matrix4 flip = Matrix4{};
    flip.m[0] = -1.0f;
    flip.m[6] = -1.0f;
    flip.m[9] = 1.0f;
    flip.m[15] = 1.0f;

    Matrix4 product = flip;
    WsMultiplyMatrix(product, basis, flip);

    Matrix4 view = *modelView;
    view.m[12] = view.m[13] = view.m[14] = 0.0f;
    view.m[15] = 1.0f;
    Matrix4 reverse = product;
    WsMultiplyMatrix(reverse, view, product);
    reverse.Invert();
    reverse.m[3] = reverse.m[7] = reverse.m[11] = 0.0f;

    Matrix4 identity = Matrix4{};
    identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1.0f;
    // the original copies from [esp+0x12c] to [esp+0x16c]: its first float stays uninitialised stack.
    // We leave it zero; it reaches only the first vector of the 0x2b upload (the test normalises that vector).
    Matrix4 shifted = Matrix4{};
    for (int i = 1; i < 16; ++i) shifted.m[i] = product.m[i];
    Matrix4 shiftedProduct = identity;
    WsMultiplyMatrix(shiftedProduct, identity, shifted);
    Matrix4 second = shiftedProduct;
    WsMultiplyMatrix(second, *modelView, shiftedProduct);
    Matrix4 transposed = Matrix4{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) transposed.m[row * 4 + col] = second.m[col * 4 + row];

    if (vertexProgram) {
        VCall<void>(vertexProgram, 0x64, 0x14, reverse.m, 3);
        VCall<void>(vertexProgram, 0x64, 0x2b, transposed.m, 3);
        const Vector4 xAxis{1.0f, 0.0f, 0.0f, 0.0f};
        const Vector4 yAxis{0.0f, 1.0f, 0.0f, 0.0f};
        const Vector4 zAxis{0.0f, 0.0f, 1.0f, 0.0f};
        VCall<void>(vertexProgram, 0x64, 0x0f, &xAxis, 1);
        VCall<void>(vertexProgram, 0x64, 0x10, &yAxis, 1);
        VCall<void>(vertexProgram, 0x64, 0x11, &zAxis, 1);
        if (enableRefraction) {
            const double r = refractiveIndex;
            const float numerator = static_cast<float>(std::pow(1.0 - r, 2.0));
            const double denominator = std::pow(1.0 + r, 2.0);
            const float ratio = static_cast<float>(static_cast<double>(numerator) / denominator);
            const Vector4 refraction{0.0f, 0.0f, 0.0f, ratio};
            if (combiner) VCall<void>(combiner, 0x30, 0, &refraction);
            vertexProgram->SetLocalConstant(7, Color{color.r, color.g, color.b, 1.0f});
        }
    }
}

}  // namespace Jet