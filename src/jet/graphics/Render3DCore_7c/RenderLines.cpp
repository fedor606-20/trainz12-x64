// Unit U10 of Render3DCore_7c: RenderLines, the global debug line/point/polygon collector. It derives Particle and
// keeps five {count, capacity, data} pointer pools (spheres, this frame's lines and their pool, this frame's
// polygons and their pool) plus a live count and frame stamp beside each. Object layout in Render3DCorePrimitives.h.
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

void EnsureCapacity(RenderLines::Pool& pool, unsigned int needed)
{
    if (needed <= pool.capacity) return;
    const unsigned int items = (needed + 127u) / 128u * 128u;
    void** const block = static_cast<void**>(Alloc::New(items * 4));
    if (pool.data) {
        std::memcpy(block, pool.data, pool.count * 4);
        Alloc::Delete(pool.data);
    }
    pool.capacity = items;
    pool.data = block;
}

// the original's array assignment (0x2ef90): a deep copy into the destination's block
void CopyPool(RenderLines::Pool& to, const RenderLines::Pool& from)
{
    to.count = 0;
    if (!from.data) return;
    if (to.capacity < from.capacity) {
        if (to.data) Alloc::Delete(to.data);
        to.data = static_cast<void**>(Alloc::New(from.capacity * 4));
        to.capacity = from.capacity;
    }
    to.count = from.count;
    if (from.count) std::memcpy(to.data, from.data, from.count * 4);
}

// 0x66f70: delete every live item, keep the block
void FreePoolItems(RenderLines::Pool& pool)
{
    for (unsigned int i = 0; i < pool.count; ++i) {
        void* const item = pool.data[i];
        if (!item) continue;
        using Fn = void(__thiscall*)(void*, int);
        reinterpret_cast<Fn>(*reinterpret_cast<void**>(item))(item, 1);
    }
    pool.count = 0;
}

}  // namespace

bool RenderLines::enabled = false;

// 0x66320
RenderLines::RenderLines()
    : Particle(), spheres(), sphereCount(0), sphereCap(0), lines(), linePool(), lineCount(0), lineCap(0), polygons(),
      polygonPool(), polygonCount(0), polygonCap(0), lastFrame(0), kernel(nullptr)
{
    std::memset(&spheres, 0, sizeof(Pool));
    std::memset(&lines, 0, sizeof(Pool));
    std::memset(&linePool, 0, sizeof(Pool));
    std::memset(&polygons, 0, sizeof(Pool));
    std::memset(&polygonPool, 0, sizeof(Pool));
    Sphere sphere;
    sphere.center.x = 0.0f;
    sphere.center.y = 0.0f;
    sphere.center.z = 0.0f;
    sphere.radius = 2.0f;
    void* const volume = *reinterpret_cast<void* const*>(reinterpret_cast<char*>(this) + 0x4c);
    if (volume) {
        using Fn = void(__cdecl*)(void*, const Sphere*);
        void** const table = *reinterpret_cast<void***>(volume);
        reinterpret_cast<Fn>(table[0x1c / 4])(volume, &sphere);
    }
}

// 0x65d10
RenderLines::RenderLines(const RenderLines& o)
    : Particle(o), spheres(), sphereCount(0), sphereCap(0), lines(), linePool(), lineCount(0), lineCap(0), polygons(),
      polygonPool(), polygonCount(0), polygonCap(0), lastFrame(0), kernel(nullptr)
{
    std::memset(&spheres, 0, sizeof(Pool));
    std::memset(&lines, 0, sizeof(Pool));
    std::memset(&linePool, 0, sizeof(Pool));
    std::memset(&polygons, 0, sizeof(Pool));
    std::memset(&polygonPool, 0, sizeof(Pool));
    CopyPool(spheres, o.spheres);
    sphereCount = o.sphereCount;
    sphereCap = o.sphereCap;
    CopyPool(lines, o.lines);
    CopyPool(linePool, o.linePool);
    lineCount = o.lineCount;
    lineCap = o.lineCap;
    CopyPool(polygons, o.polygons);
    CopyPool(polygonPool, o.polygonPool);
    polygonCount = o.polygonCount;
    polygonCap = o.polygonCap;
    lastFrame = o.lastFrame;
    kernel = o.kernel;
}

// 0x66440
RenderLines::~RenderLines()
{
    DeleteAll();
    if (spheres.data) Alloc::Delete(spheres.data);
    if (lines.data) Alloc::Delete(lines.data);
    if (linePool.data) Alloc::Delete(linePool.data);
    if (polygons.data) Alloc::Delete(polygons.data);
    if (polygonPool.data) Alloc::Delete(polygonPool.data);
    spheres.data = lines.data = linePool.data = polygons.data = polygonPool.data = nullptr;
}

// 0x65e70
RenderLines& RenderLines::operator=(const RenderLines& o)
{
    Particle::operator=(o);
    CopyPool(spheres, o.spheres);
    sphereCount = o.sphereCount;
    sphereCap = o.sphereCap;
    CopyPool(lines, o.lines);
    CopyPool(linePool, o.linePool);
    lineCount = o.lineCount;
    lineCap = o.lineCap;
    CopyPool(polygons, o.polygons);
    CopyPool(polygonPool, o.polygonPool);
    polygonCount = o.polygonCount;
    polygonCap = o.polygonCap;
    lastFrame = o.lastFrame;
    kernel = o.kernel;
    return *this;
}

// 0x668d0
void RenderLines::RemoveAll()
{
    if (kernel) {
        const unsigned int frame =
            *reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(kernel->GetDisplay()) + 0x3c);
        if (frame != lastFrame) {
            FreePoolItems(linePool);
            FreePoolItems(polygonPool);
            lastFrame = frame;
        }
    }
    for (unsigned int i = 0; i < lines.count; ++i) {
        EnsureCapacity(linePool, linePool.count + 1);
        linePool.data[linePool.count++] = lines.data[i];
    }
    lines.count = 0;
    for (unsigned int i = 0; i < polygons.count; ++i) {
        EnsureCapacity(polygonPool, polygonPool.count + 1);
        polygonPool.data[polygonPool.count++] = polygons.data[i];
    }
    polygons.count = 0;
    polygonCount = 0;
    polygonCap = 0;
    lineCount = 0;
    lineCap = 0;
}

// 0x66ec0
void RenderLines::DeleteAll()
{
    FreePoolItems(linePool);
    FreePoolItems(lines);
    lineCount = 0;
    lineCap = 0;
    FreePoolItems(polygonPool);
    FreePoolItems(polygons);
    polygonCount = 0;
    polygonCap = 0;
    FreePoolItems(spheres);
    sphereCount = 0;
    sphereCap = 0;
}

// 0x66f40
void RenderLines::SetState(bool state)
{
    enabled = state;
}

// 0x66f50
bool RenderLines::GetState()
{
    return enabled;
}

// 0x66f60
bool RenderLines::Switch()
{
    enabled = !enabled;
    return enabled;
}

}  // namespace Jet
