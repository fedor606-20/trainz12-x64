#pragma once

// Wave 1 scaffold for InterfaceResources_7c (Jet UI, Trainz 2012 x64 port).
// Evidence: D:\trainz12-decomp\exports\InterfaceResources_7c.tsv (169 exports),
// ghidra_maps/jet/ui/InterfaceResources_7c.dll.functions.tsv,
// ghidra_maps/jet/ui/vtnames_InterfaceResources_7c.dll.tsv,
// notes/agents/InterfaceResources_7c.md, InterfaceSystem/KernelResource/KernelResources headers.
// STATUS: placeholders only. ABI/layout/vtable slot order/RTTI/resource formats and
// plugin registration contracts are NOT verified here. Do not treat declarations as final.
//
// Boundary assumptions (TODO: verify with decomp/differential tests):
// - InterfaceResources owns loadable/renderable UI asset primitives (borders, cursors,
//   fonts/font effects, icons) and resource/type registration for them.
// - InterfaceSystem_7c consumes these primitives (MODLOG: InterfaceSystem -> InterfaceResources 32);
//   UI wrappers (UI::Border/Cursor/IFont/Icon, OSD fonts) depend on compatible lookup,
//   instance init, measurement/print/render, rect/offset, and tag/color semantics.
// - Resource lifetime/lookup/lock/ref/observer/status/name/size cache semantics come from
//   Jet::Resource/ResourceDatabase/ResourceTypeBase in KernelResource_7c (see Resource.h).
// - Bitmap-backed assets (cursor/border/icon/bitmap fonts) likely interoperate with
//   KernelResources_7c BitmapResource/BitmapBaseInstance and Jet::Bitmap APIs; rendering
//   targets Jet::Bitmap via Render2DCore/bitmap primitives, not direct 3D device ownership.
// - Parsing/caching/GPU preparation responsibilities are unclear from exports alone:
//   treat load/parse/cache as module-local behavior to be recovered from original.

#include <jet/Bitmap.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelResources.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Plugin.h>
#include <jet/Resource.h>
#include <jet/Result.h>

namespace Jet {

// ---- Resource type / plugin registration (evidence: exports 169 + vtnames/fmap) ----
// TODO(ABI): recover factory/type IDs, parent/group IDs, and exact registration side effects
// performed by JetGetPluginsInterfaceResources. vtnames show ResourceType<BitmapBorderResource>,
// ResourceType<CursorResource>, ResourceType<IconResource>, and FontResourceType vtables.
// Do NOT register with guessed IDs or guessed group membership.
class BitmapBorderResource;  // internal/concrete, non-exported by name
class CursorResource;
class IconResource;
// using BitmapBorderResourceType = ResourceType<BitmapBorderResource>; // TODO: confirm specialization/IDs
// using CursorResourceType = ResourceType<CursorResource>;
// using IconResourceType = ResourceType<IconResource>;
// class FontResourceType : public ResourceTypeBase { /* TODO: vf3/vf5 behavior, IDs */ };

// ---- Borders ----
// Exports: BorderResource ctor/dtor/copy/assign/vftable/Render (virtual, __thiscall, const).
// vtnames: BorderResource shares Resource-like base slots and has vf18/vf19 specializations;
// BitmapBorderResource has heavy vf11/vf14/vf18..vf20 render/load logic (internal).
// TODO(layout/vtable): verify inheritance (Resource-derived?), member layout, all slot mappings,
// and exact Render contract (DB/bitmap/area/clip semantics, 9-slice/compositing format).
class BorderResource /* : public Resource */ {
public:
    // TODO: BorderResource(const PString&, const ResourceDatabase*);
    // TODO: virtual void Render(const ResourceDatabase* db, Bitmap& dest, const Rect& area, const Rect& clip) const;
    // TODO: copy/assign/dtor and Resource overrides per ABI.
};

// ---- Cursors ----
// Exports: CursorResource ctor/dtor/copy/assign/vftable, GetBitmap/GetArea/GetOffset/IsAllLoaded/LoadTxt.
// CursorInstance is a small exported value/holder (ctor/dtor/copy/assign).
// TODO(layout/vtable): CursorResource adds vf18..vf20 in vnames; verify IsAllLoaded/LoadTxt overrides,
// text cursor format parsed by LoadTxt (~1129 B), bitmap/area/offset availability semantics.
class CursorResource /* : public Resource */ {
public:
    // TODO: const Bitmap* GetBitmap() const; const Rect GetArea() const; const Point GetOffset() const;
    // TODO: Result IsAllLoaded() const override?; protected LoadTxt(const char*) override?
};

class CursorInstance {
public:
    // TODO: confirm fields/ownership (resource instance/ref?) and any conversion/init API used by InterfaceSystem.
};

// ---- Fonts / font resources / font effects ----
// Exports show large Font surface (Print/Printf/GetW/H/WH/Calc/char/cache/fx mgmt), FontInstance wrapper,
// FontInformation (tags/colors), FontBaseResource/FontResource (load/create/unload/size/on-the-fly/delegation),
// and FontFx hierarchy: FontFx base (interface), FontFxBitmap, FontFxTT.
// TODO(ABI): FontFxBitmap/FontFxTT have vbtable + dual vftables (FontFx and PluginInstance subobjects)
// indicating multiple/virtual inheritance; do not infer layout or slot order here.
// TODO(behavior): font tag extraction (FontInformation::ExtractTags), charset init, ABC/char params,
// layout/justification/wrapping, color lists, bitmap font loading/caching, TT loading/caching/raw image,
// and Print pipeline are high-risk and must be recovered from original behavior.
struct FontColor;  // used with FastDynamicArray<FontColor,...> in exported signatures; TODO: define when known.
class FontFx {
public:
    // TODO: virtual interface (Init/Add/Remove?/cache/ABC/height/char-pos/raw-image variants) per subclass evidence.
};

class FontFxBitmap /* : public FontFx, public virtual PluginInstance? */ {
public:
    // TODO: Load/LoadDR/Init/Done/GetABC/GetCharPos/GetCacheBitmap/GetHeight; verify MI/vbase layout.
};

class FontFxTT /* : public FontFx, public virtual PluginInstance? */ {
public:
    // TODO: Load/Init/BeginCacheChars/CacheChar/EndCacheChars/GetABC/GetRawImage/GetMaxCharWidth/GetHeight.
};

class FontInformation {
public:
    // TODO: ctor/dtor/copy/assign, ExtractTags(String&), GetColorList() ABI/layout.
};

class Font {
public:
    // TODO: vftable (vf0 @ 0x10002760 per vnames), Init/InitialCharset/Done/AddFx/RemoveFx,
    // measurement/print/printf overloads, protected layout/calc/cache helpers; very large behavior surface.
};

class FontBaseResource /* : public Resource */ {
public:
    // TODO: SetOnTheFly(bool); verify extended vf18..vf32 mapping and Resource overrides.
};

class FontResource /* : public FontBaseResource or Resource-derived chain - TODO verify */ {
public:
    // TODO: Create/Load/Unload/CalculateDataSize/CalculateSize, delegated Get*/Print* overrides.
};

class FontInstance {
public:
    // TODO: Init/GetName/GetPoints/GetColor/GetFlags/Get*/Print* delegation and ownership/lifetime.
};

// ---- Icons ----
// Exports: IconInstance ctor/dtor/copy/assign/op==, GetAutoHeight/GetAutoSize/GetW/Init/Render overloads.
// fmap/vtnames: IconResource internal with sizable vf8/vf11/vf14/vf15/vf18/vf19; Render/measure depend on it.
// TODO(layout): verify IconInstance holds ref/resource instance to IconResource and text/icon render semantics.
class IconResource /* : public Resource */ {
public:
    // TODO: internal load/render/measure vfuncs and asset format.
};

class IconInstance {
public:
    // TODO: Init(db,name[,bool]), Render/GetAutoSize/GetAutoHeight/GetW; confirm equality/copy semantics.
};

}  // namespace Jet

// Plugin entry point (exported). Evidence: ordinal 169, RVA 0x9360, unmangled extern "C" name.
// TODO(ABI): confirm return/value and any parameters beyond __cdecl no-arg (evidence suggests void()).
// Must register InterfaceResources plugin/type factories in Jet plugin infrastructure when known.
extern "C" void __cdecl JetGetPluginsInterfaceResources();
