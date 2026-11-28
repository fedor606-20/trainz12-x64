// Render3DCore_7c unit U8: the camera, the viewport and the debug viewport.
//
// Layouts and behaviour follow the TS12 assembly (addresses per member in graphics/Render3DCore_7c/Camera.cpp,
// ViewPort.cpp, DebugViewPort.cpp). 0x264 bytes for Camera:
//   +0x000 Particle, +0x12c transition blend, +0x130 width, +0x134 height, +0x138 width2, +0x13c height2,
//   +0x140 near, +0x144 far, +0x148 sx, +0x14c sy, +0x150 six 0x10-byte frustum planes,
//   +0x1b0 reflection cull, +0x1b4 projection mode, +0x1b8 cached view, +0x1f8 global view,
//   +0x238 cached orientation (column-major: 0x238..0x240 right, 0x244..0x24c up, 0x250..0x258 forward),
//   +0x25c dirty view, +0x25d dirty global, +0x25e dirty cone, +0x260 scene.
// ViewPort is 0xf4 bytes: Layer at +0, camera +0x20, rect +0x24, clear colour +0x34, clear depth +0x44,
// clear stencil +0x48, clear mode +0x4c, render flags +0x50, user data +0x54, viewport number +0x58,
// window viewport +0x5c, render hint +0x60, alpha-mask reference +0x64, flags +0x68, closest distance +0x6c,
// aspect +0x70/+0x74, visible list +0x78, embedded ViewPoint +0x84 (0x70 bytes).
// DebugViewPort adds the source viewport at +0xf4 and a flag word at +0xf8 (0xfc bytes).
//
// Ordinary members are __cdecl (this on the stack); constructors and the assignments are __thiscall. The embedded
// ViewPoint is another unit (U9 scene): only its 0x70-byte block is addressed here and its copy/assignment go to
// the original's exports until U9 moves. The bounding volume behind Particle is U3 and is likewise the original's.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/Math.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/Render3DCorePlane.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>

#include <cstddef>

namespace Jet {

enum RenderHint { RENDER_DEFAULT = 3 };

using ElementPtrList = FastDynamicArray<Element*>;

class Camera;
class DebugViewPort;
class LayerGroup;
class RenderKernel;
class Ray;
struct VisibleElement;

// One entry of the visible list: 0x48 bytes, the element's matrix followed by the element pointer and a key. The
// constructors are RenderSystem's (imported); only their layout matters here.
struct VisibleElement {
    Matrix4 matrix;              // +0x00
    Element* element;            // +0x40
    unsigned long key;           // +0x44
    VisibleElement(const Matrix4& m, Element* e);
    VisibleElement(const Matrix4& m, Element* e, unsigned long k);
};
JET_LAYOUT32(sizeof(VisibleElement) == 0x48);
JET_LAYOUT32(offsetof(VisibleElement, element) == 0x40);

class Camera : public Particle {
public:
    enum ProjectionMode { PERSPECTIVE = 0, TRANSITION = 1, ORTHOGRAPHIC = 2 };

    Camera();
    Camera(float dist, float height, float width);
    Camera(const Vector3& position, const Quaternion& orientation);
    Camera(const Vector3& position, const Matrix3& orientation);
    Camera(const Vector3& position, float rotateX, float rotateY, float rotateZ);
    Camera(const Vector3& position, const Quaternion& orientation, float dist, float height, float width);
    Camera(const Vector3& position, const Matrix3& orientation, float dist, float height, float width);
    Camera(const Vector3& position, float roll, float pitch, float yaw, float dist, float height, float width);
    Camera(const Camera& o);
    // portal / cell-space copies (U8 wave 6): the source fields, then a transform or a quad fit
    Camera(const Camera& o, const Matrix4& transform);
    Camera(const Camera& o, const Vector2& min, const Vector2& max, const Vector3& position, float nearDistance);
    Camera(const Camera& o, const Vector3* points, const Matrix4& transform);
    Camera(const Camera& o, const Vector3* points, const Matrix4& transform, const Vector3& position, bool reflection);
    virtual ~Camera();
    Camera& operator=(const Camera& o);

    // Particle's virtuals, overridden here
    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl SetPosition(const Vector3& position);
    virtual void __cdecl SetOrientation(const Quaternion& orientation);
    virtual void __cdecl SetReferenceFrame(const Vector3& position, const Quaternion& orientation);

    // Camera's own virtuals (slots 48..57 of the original vtable)
    virtual const Matrix4& __cdecl GetGlobalViewTransform() const;
    virtual void __cdecl Translate(const Vector3& delta);
    virtual void __cdecl Rotate(const Matrix3& rot);
    virtual void __cdecl Rotate(const Quaternion& rot);
    virtual void __cdecl SelfRotate(const Matrix3& rot);
    virtual void __cdecl SelfRotate(const Quaternion& rot);
    virtual void __cdecl ResetViewCone();
    virtual void __cdecl Cull(ViewPoint& view) const;
    virtual void __cdecl RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags) const;

    Element* __cdecl GetScene();
    Plane* __cdecl GetFrustum();
    const Plane* __cdecl GetFrustum() const;
    float __cdecl GetNearClipDistance() const;
    float __cdecl GetFarClipDistance() const;
    float __cdecl GetNearClipHeight() const;
    float __cdecl GetNearClipWidth() const;
    Ray __cdecl GetRay(float x, float y) const;
    const Matrix3& __cdecl GetCachedOrientation() const;
    const Matrix4& __cdecl GetCellSpaceViewTransform() const;
    const Matrix4& __cdecl GetViewTransform() const;
    float __cdecl GetHorizontalFOV() const;
    float __cdecl GetVerticalFOV() const;
    Vector3 __cdecl GetRight() const;
    Vector3 __cdecl GetUp() const;
    Vector3 __cdecl GetForward() const;
    float __cdecl GetPitch() const;
    float __cdecl GetYaw() const;
    float __cdecl GetRoll() const;
    void __cdecl SetScene(Element* scene);
    ClipResult __cdecl Clip3D(const Vector3& center, float radius) const;
    ClipResult __cdecl Clip3D(const Vector3* array, int num) const;
    ClipResult __cdecl ClipNear(Vector3& position) const;
    bool __cdecl BigEnough(const Vector3& center, float radius) const;
    bool __thiscall IsReflectionCull() const;
    Matrix4 __cdecl GetPerspectiveTransform() const;
    void __cdecl SetNearDistance(float distance);
    void __cdecl SetFarDistance(float distance);
    void __cdecl SetPerspective(float vFov, float aspect);
    void __cdecl SetTransition(float blend, float vFov, float height, float aspect);
    void __cdecl SetOrthographic(float height, float aspect);
    ProjectionMode __cdecl GetProjectionMode() const;
    void __thiscall SetProjectionData(const Camera& o);
    void __cdecl GetClippingPlanes(float xL, float yT, float xR, float yB, PlaneList& planes) const;
    void __cdecl AlignToTarget(const Vector3& target, const Vector3& up);
    void __cdecl Orbit(const Vector3& target, float deltaYaw, float deltaPitch, const Vector3& up);
    void __cdecl Perspective(Vector3& v) const;
    void __cdecl TrainzUnPerspective(float x, float y, float z, Vector3& v) const;
    bool __cdecl Transform2D(Vector3& v, float tolerance) const;
    bool __thiscall TrainzTransform2D(Vector3& v, float tolerance) const;
    void __cdecl SetProjected3D(RenderKernel* kernel);
    void __cdecl SetProjected3DOffset(RenderKernel* kernel, float offset);
    void __cdecl SetRenderView(RenderKernel* kernel);

protected:
    virtual void __cdecl UpdateGlobalViewTransform() const;

    float __thiscall GetRelevantWidth() const;
    float __thiscall GetRelevantHeight() const;
    void __cdecl Init(float width, float height, float close);
    void __cdecl ResetView(float close, float width, float height, float width2, float height2);
    void __cdecl InitInstance(Camera& instance) const;
    void __cdecl UpdateViewCone() const;
    void __cdecl UpdateCachedViewTransform() const;

protected:
    static float pixelError;

    float transition;              // +0x12c
    float width;                   // +0x130
    float height;                  // +0x134
    float width2;                  // +0x138
    float height2;                 // +0x13c
    float close;                   // +0x140
    float distant;                 // +0x144
    float sx;                      // +0x148
    float sy;                      // +0x14c
    Plane frustum[6];              // +0x150
    unsigned char reflectionCull;  // +0x1b0
    unsigned char padding1b1[3];
    ProjectionMode projectionMode; // +0x1b4
    Matrix4 cachedViewTransform;        // +0x1b8
    Matrix4 globalViewTransform;        // +0x1f8
    Matrix3 cachedOrientation;          // +0x238
    mutable unsigned char dirtyViewTransform;   // +0x25c
    mutable unsigned char dirtyGlobalTransform; // +0x25d
    mutable unsigned char dirtyViewCone;        // +0x25e
    unsigned char padding25f;
    Element* scene;                // +0x260

private:
    // Particle's transform members are private in U9a's header; Camera owns the same offsets in the original.
    Vector3& PosRef() { return *reinterpret_cast<Vector3*>(reinterpret_cast<char*>(this) + 0x2c); }
    const Vector3& PosRef() const
    {
        return *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(this) + 0x2c);
    }
    Quaternion& OriRef() { return *reinterpret_cast<Quaternion*>(reinterpret_cast<char*>(this) + 0x38); }
    const Quaternion& OriRef() const
    {
        return *reinterpret_cast<const Quaternion*>(reinterpret_cast<const char*>(this) + 0x38);
    }
    void* BvRef() const { return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(this) + 0x4c); }
};
JET_LAYOUT32(sizeof(Camera) == 0x264);

class ViewPort : public Layer {
public:
    ViewPort();
    ViewPort(Camera* camera, LayerGroup* group);
    ViewPort(Camera* camera, const Rect& rect, LayerGroup* group);
    ViewPort(const ViewPort& o);
    virtual ~ViewPort();
    ViewPort& operator=(const ViewPort& o);

    // overrides of Layer's virtuals (slots 0..10)
    virtual void __cdecl RenderLayer(SystemResourceViewerList& list);
    virtual bool __cdecl Collides(const Vector2& p) const;
    virtual bool __cdecl Collides(const Vector2& a, const Vector2& b) const;
    virtual void __cdecl Render();
    virtual void __cdecl Update(float dt);
    virtual void __cdecl Begin();
    virtual void __cdecl End();
    virtual bool __cdecl CanRender();

    // viewport virtuals (slots 11, 12)
    virtual Element* __cdecl GetElementAt(const Vector2& point);
    virtual void __cdecl GetAllWithin(const Vector2& a, const Vector2& b, ElementPtrList& elements);

    Camera* __cdecl GetCamera() const;
    void __cdecl SetCamera(Camera* camera);
    bool __cdecl GetLetterBoxMode() const;
    void __cdecl AdjustAspectRatio(float& x, float& y) const;
    void __cdecl WindowSpaceConvert(Rect& rect) const;
    void __cdecl SetUserData(unsigned long value);
    unsigned long __cdecl GetUserData() const;
    void __cdecl SetRenderHint(RenderHint hint);
    RenderHint __cdecl GetRenderHint() const;
    void __cdecl SetRenderFlags(unsigned long flags);
    unsigned long __cdecl GetRenderFlags() const;
    unsigned long __cdecl GetViewportNumber() const;
    void __cdecl SetWindowViewport(bool enabled);
    bool __cdecl GetWindowViewport() const;
    void __cdecl SetAlphaMaskReference(float value);
    float __cdecl GetAlphaMaskReference() const;
    static unsigned long __cdecl GetTraversalNumber();
    void __cdecl SetLocalPosition(int x, int y);
    void __cdecl GetLocalPosition(int& x, int& y) const;
    void __cdecl GetPosition(int& x, int& y) const;
    void __cdecl SetSize(int w, int h);
    void __cdecl GetSize(int& w, int& h) const;
    void __cdecl SetLetterBoxMode(bool enabled);
    void __cdecl Translate(int dx, int dy);
    void __cdecl Scale(float scale);
    void __cdecl Scale(float sx, float sy);
    void __cdecl Transform(Vector3& v);
    void __cdecl ViewTransformPoint(Vector3& point);
    void __cdecl Clear();
    Rect __cdecl CalculateSphereSize(const Vector3& center, float radius, const Matrix4& view) const;
    Rect __cdecl CalculateOrientedBoxSize(const OrientedBox& box, const Matrix4& view) const;
    void __cdecl SetClearColor(const Color& color);
    void __cdecl SetClearDepth(float depth);
    void __cdecl SetClearStencil(long value);
    void __cdecl SetClearMode(unsigned long mode);

    const Rect& __cdecl GetLocalExtents() const;
    void __cdecl GetExtents(Rect& rect) const;
    int __cdecl GetLocalX() const;
    int __cdecl GetX() const;
    int __cdecl GetLocalY() const;
    int __cdecl GetY() const;
    int __cdecl GetWidth() const;
    int __cdecl GetHeight() const;
    const Array<VisibleElement, 128>& __thiscall GetVisibleList() const;
    const Color& __cdecl GetClearColor() const;
    float __cdecl GetClearDepth() const;
    unsigned long __cdecl GetClearStencil() const;
    unsigned long __cdecl GetClearMode() const;

private:
    void __cdecl Initialize(Camera* camera, const Rect* rect);

protected:
    void InitializeViewPoint();

protected:
    static unsigned long numViewports;
    static unsigned long traversalNumber;

    Camera* camera;               // +0x20
    Rect extents;                 // +0x24
    Color clearColor;             // +0x34
    float clearDepth;             // +0x44
    unsigned long clearStencil;   // +0x48
    unsigned long clearMode;      // +0x4c
    unsigned long renderFlags;    // +0x50
    unsigned long userData;       // +0x54
    unsigned long viewportNumber; // +0x58
    bool windowViewport;          // +0x5c
    unsigned char padding5d[3];
    RenderHint renderHint;        // +0x60
    float alphaMaskReference;     // +0x64
    unsigned long viewportFlags;  // +0x68
    float closestDistance;        // +0x6c
    float aspectX;                // +0x70
    float aspectY;                // +0x74
    Array<VisibleElement, 128> visibleList;  // +0x78
    unsigned char viewPoint[0x70];           // +0x84
};
JET_LAYOUT32(sizeof(ViewPort) == 0xf4);

class DebugViewPort : public ViewPort {
public:
    DebugViewPort(ViewPort* source);
    DebugViewPort(const DebugViewPort& o);
    virtual ~DebugViewPort();
    DebugViewPort& operator=(const DebugViewPort& o);

    virtual void __cdecl Render();
    virtual void __cdecl Update(float dt);

    ViewPort* __cdecl GetViewPort() const;

    ViewPort* sourceViewPort;  // +0xf4
    unsigned long debugFlags;  // +0xf8
};
JET_LAYOUT32(sizeof(DebugViewPort) == 0xfc);

}  // namespace Jet
