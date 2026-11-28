// Render3DCore_7c wave 6: Jet::ViewPoint (0x70 bytes), transcribed from the TS12 assembly.
//
// Addresses: 0x1000a830 public ctor, 0x100072a40 private ctor, 0x100072b80 destructor, 0x10000a770 assignment,
// 0x10000a7d0/0x10000a8f0 Add, 0x10000a880 AddPortal, 0x100072c70 CalcWantedMipLevelsForCull, 0x100006970
// GetCamera, 0x100054e60 GetPortal, 0x10000a870 GetRenderKernel, 0x10005ff70 GetViewPort, 0x100072aa0 RemoveAll,
// 0x100072bc0 Render, 0x100072b50 TraversePortal, 0x100072940 Update.
//
// The children are a singly linked list at +0x10 (each child's +0x0c is the next sibling). RemoveAll deletes the
// trees without running destructors (helper 0x100072b20); the destructor does run them. The visible list is an
// Array<VisibleElement,128>; the ViewPoint owns a window [visibleFirst, visibleFirst+visibleCount).
#include "SimpleRemain.h"

#include <windows.h>

#include <cmath>
#include <cstring>
#include <new>

namespace Jet {

namespace {

// 0x10008ab0: the original returns |v| in st(0) without rounding to double. SolidsDetail::Length stores it to a
// double, which differs in the last bits and can flip a CeilPow2 boundary in CalcWantedMipLevelsForCull, so the
// sequence is repeated here and left in st(0).
const unsigned int* g_sqrtLut;
float g_half = 0.5f;
double g_threeHalves = 1.5;

void InitLengthLut()
{
    HMODULE module = GetModuleHandleA("KernelStandard_7c.dll");
    if (module == nullptr) return;
    auto symbol = reinterpret_cast<const unsigned int*>(GetProcAddress(module, "?s_sqrtLUT@StdMath@Jet@@0PAIA"));
    if (symbol == nullptr) return;
    if (symbol[0] >= 0x800000u) {
        const unsigned int* deref = *reinterpret_cast<const unsigned int* const*>(symbol);
        if (deref != nullptr) symbol = deref;
    }
    g_sqrtLut = symbol;
}

#if defined(_M_IX86)
__declspec(naked) double __cdecl LengthExtended(const Vector3* v)
{
    __asm {
        push ebp
        mov ebp, esp
        mov eax, dword ptr [ebp + 8]
        movss xmm2, dword ptr [eax + 4]
        movss xmm1, dword ptr [eax + 8]
        movss xmm0, dword ptr [eax]
        movaps xmm3, xmm2
        mulss xmm3, xmm2
        mulss xmm0, xmm0
        movaps xmm2, xmm1
        addss xmm0, xmm3
        mulss xmm2, xmm1
        addss xmm0, xmm2
        sub esp, 8
        movss dword ptr [esp + 4], xmm0
        mov eax, dword ptr [esp + 4]
        mov ecx, eax
        shr ecx, 017h
        and ecx, 0FFh
        mov edx, 17Ch
        sub edx, ecx
        mov ecx, dword ptr [g_sqrtLut]
        shl edx, 016h
        shr eax, 0Fh
        and edx, 0FF800000h
        and eax, 1FFh
        or edx, dword ptr [ecx + eax * 4]
        movss dword ptr [esp], xmm0
        fld dword ptr [esp]
        mov dword ptr [esp + 4], edx
        fld dword ptr [g_half]
        fmul st, st(1)
        fld dword ptr [esp + 4]
        fld st(0)
        fmul st, st(1)
        fmul st, st(2)
        fld qword ptr [g_threeHalves]
        fld st(0)
        fsubrp st(2), st(0)
        fxch st(1)
        fmulp st(2), st(0)
        fld st(1)
        fmul st, st(2)
        fmulp st(3), st(0)
        fsubrp st(2), st(0)
        fmulp st(1), st(0)
        fmulp st(1), st(0)
        add esp, 8
        leave
        ret
    }
}
#endif

float DistanceX87(const Vector3& delta, float radius)
{
#if defined(_M_IX86)
    static const bool initialized = [] { InitLengthLut(); return true; }();
    (void)initialized;
    float result;
    const Vector3* argument = &delta;
    __asm {
        push argument
        call LengthExtended
        add esp, 4
        fsub dword ptr [radius]
        fstp dword ptr [result]
    }
    return result;
#else
    return static_cast<float>(SolidsDetail::Length(delta) - static_cast<double>(radius));
#endif
}

// 0x100072b20: delete a child tree (children and siblings) without destructors.
void DeleteTree(ViewPoint* point)
{
    if (point == nullptr) return;
    DeleteTree(point->child);
    DeleteTree(point->next);
    Alloc::Delete(point);
}

// 0x100072940's pixel size: atan/tan round trip in x87, exactly as the original;
// (width * 0.5) / near is SSE, the rest is x87 with a 0.5 * width multiply on the way out.
float PixelSizeX87(int width, float scale, float nearDistance)
{
#if defined(_M_IX86)
    static const float kHalf = 0.5f;
    volatile float angle = 0.0f;
    float value = 0.0f;
    {
        float a = scale * kHalf;
        a = a / nearDistance;
        angle = a;
    }
    __asm {
        fild dword ptr [width]
        fmul dword ptr [kHalf]
        fstp dword ptr [value]
        fld dword ptr [angle]
        fld1
        fpatan
        fadd st, st
        fmul dword ptr [kHalf]
        fstp dword ptr [angle]
        fld dword ptr [angle]
        fptan
        fstp st(0)
        fdivr dword ptr [value]
        fstp dword ptr [value]
    }
    return value;
#else
    float a = scale * 0.5f / nearDistance;
    float value = static_cast<float>(width) * 0.5f;
    return value / std::tan(static_cast<float>(std::atan(a)));
#endif
}

}  // namespace

ViewPoint::ViewPoint(Array<VisibleElement, 128>* visibleList_, RenderKernel* kernel, const ViewPort* port)
{
    renderKernel = kernel;
    camera = nullptr;
    visibleList = visibleList_;
    next = nullptr;
    child = nullptr;
    portal = nullptr;
    viewPort = port;
    depthArgument = 0;
    visibleFirst = 0;
    visibleCount = 0;
    depth = 0;
    pixelSize = 0.0f;
}

ViewPoint::ViewPoint(ViewPoint& parent, Portal* portal_, const Matrix4& matrix_, unsigned int depth_,
                     ViewPoint* nextPoint)
{
    renderKernel = parent.renderKernel;
    camera = nullptr;
    visibleList = parent.visibleList;
    next = nextPoint;
    child = nullptr;
    portal = portal_;
    viewPort = parent.viewPort;
    std::memcpy(matrix.m, matrix_.m, sizeof matrix.m);
    depthArgument = depth_;
    visibleFirst = 0;
    visibleCount = 0;
    depth = parent.depth + 1;
}

ViewPoint::~ViewPoint()
{
    if (child != nullptr) {
        child->~ViewPoint();
        Alloc::Delete(child);
    }
    if (next != nullptr) {
        next->~ViewPoint();
        Alloc::Delete(next);
    }
}

ViewPoint& ViewPoint::operator=(const ViewPoint& o)
{
    renderKernel = o.renderKernel;
    camera = o.camera;
    visibleList = o.visibleList;
    next = o.next;
    child = o.child;
    portal = o.portal;
    viewPort = o.viewPort;
    // the original copies the 0x40-byte matrix with x87 loads/stores (quieting a signalling NaN)
    for (int i = 0; i < 16; ++i) SolidsDetail::X87Copy(matrix.m[i], o.matrix.m[i]);
    depthArgument = o.depthArgument;
    visibleFirst = o.visibleFirst;
    visibleCount = o.visibleCount;
    depth = o.depth;
    SolidsDetail::X87Copy(pixelSize, o.pixelSize);
    return *this;
}

void __cdecl ViewPoint::Add(Element* element, const Matrix4& matrix_)
{
    VisibleElement item(matrix_, element);
    visibleList->Reserve(visibleList->count + 1);
    visibleList->data[visibleList->count] = item;
    ++visibleList->count;
    ++visibleCount;
}

void __cdecl ViewPoint::Add(Element* element, const Matrix4& matrix_, unsigned long key)
{
    VisibleElement item(matrix_, element, key);
    visibleList->Reserve(visibleList->count + 1);
    visibleList->data[visibleList->count] = item;
    ++visibleList->count;
    ++visibleCount;
}

void __cdecl ViewPoint::AddPortal(Portal* portal_, const Matrix4& matrix_, unsigned int depth_)
{
    void* memory = Alloc::New(sizeof(ViewPoint));
    if (memory == nullptr) {
        child = nullptr;
        return;
    }
    ViewPoint* point = new (memory) ViewPoint(*this, portal_, matrix_, depth_, child);
    child = point;
}

const Camera* __cdecl ViewPoint::GetCamera() const
{
    return camera;
}

const Portal* __cdecl ViewPoint::GetPortal() const
{
    return portal;
}

RenderKernel* __cdecl ViewPoint::GetRenderKernel() const
{
    return renderKernel;
}

const ViewPort* __cdecl ViewPoint::GetViewPort() const
{
    return viewPort;
}

void __cdecl ViewPoint::RemoveAll()
{
    visibleList->count = 0;
    visibleCount = 0;
    if (child != nullptr) {
        DeleteTree(child->child);
        DeleteTree(child->next);
        Alloc::Delete(child);
        child = nullptr;
    }
    if (next != nullptr) {
        DeleteTree(next->child);
        DeleteTree(next->next);
        Alloc::Delete(next);
        next = nullptr;
    }
}

void __cdecl ViewPoint::Render(ViewPort& port)
{
    Display* display = renderKernel->GetDisplay();
    if (portal != nullptr) portal->Enter(display, port, matrix, depth, depthArgument);
    for (unsigned int i = 0; i < visibleCount; ++i) {
        VisibleElement& item = visibleList->data[visibleFirst + i];
        item.element->Render(display, port, item.matrix);
    }
    for (ViewPoint* point = child; point != nullptr; point = point->next) point->Render(port);
    if (portal != nullptr) portal->Leave(display, port, matrix, depth, depthArgument);
}

void __cdecl ViewPoint::TraversePortal(Camera* camera_)
{
    if (portal != nullptr) portal->Traverse(*this, matrix, camera_);
}

void __cdecl ViewPoint::Update(Camera* camera_)
{
    const int width = viewPort->GetWidth();
    float scale = 0.0f;
    switch (camera_->GetProjectionMode()) {
    case Camera::PERSPECTIVE:
        scale = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x130);
        break;
    case Camera::TRANSITION: {
        const float transition = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x12c);
        const float width1 = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x130);
        const float width2 = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x138);
        scale = SolidsDetail::AddSS(SolidsDetail::MulSS(SolidsDetail::SubSS(1.0f, transition), width1),
                                    SolidsDetail::MulSS(width2, transition));
        break;
    }
    case Camera::ORTHOGRAPHIC:
        scale = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x138);
        break;
    default:
        scale = 0.0f;
        break;
    }
    const float nearDistance = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(camera_) + 0x140);
    pixelSize = PixelSizeX87(width, scale, nearDistance);

    camera = camera_;
    visibleFirst = visibleList->count;
    camera_->Cull(*this);
    for (ViewPoint* point = child; point != nullptr; point = point->next) {
        point->portal->Traverse(*point, point->matrix, camera_);
    }
}

int __cdecl ViewPoint::CalcWantedMipLevelsForCull(ViewPoint& view, Particle& particle, float f)
{
    ParticleBoundingVolume* volume = particle.GetBoundingVolume();
    if ((volume->dirtyFlags & 3) != 0) volume->GetGlobalBoundingSphereCenter(true);
    Vector3 position{0.0f, 0.0f, 0.0f};
    view.camera->LocalToGlobal(position);
    if ((volume->dirtyFlags & 2) != 0) volume->RecalculateSphereVolume();
    Vector3 delta;
    delta.x = SolidsDetail::SubSS(position.x, volume->globalSphereCenter.x);
    delta.y = SolidsDetail::SubSS(position.y, volume->globalSphereCenter.y);
    delta.z = SolidsDetail::SubSS(position.z, volume->globalSphereCenter.z);
    const float distance = DistanceX87(delta, volume->sphere.radius);
    if (!(distance > 0.0f)) return -1;
    if (f == 0.0f) return -1;  // the original's NaN test continues for a NaN f, only a zero returns -1
    const float ratio = view.pixelSize / distance;
    const float scaled = SolidsDetail::MulSS(SolidsDetail::MulSS(ratio, f), 0.5f);
    const int levels = CeilPow2(static_cast<int>(scaled));
    const float levelsFloat = static_cast<float>(levels);
    unsigned int bits;
    std::memcpy(&bits, &levelsFloat, sizeof bits);
    int exponent = static_cast<int>((bits >> 23) & 0xffu) - 0x7f;
    if (exponent <= 0) exponent = 0;
    return exponent + 1;
}

}  // namespace Jet
