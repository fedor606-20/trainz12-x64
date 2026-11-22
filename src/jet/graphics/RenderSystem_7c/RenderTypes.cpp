// Unit U1 of RenderSystem_7c: the display layer's small value types (DisplayFormat, DisplayLight, DisplayMaterial,
// RenderTextureConstruct, TextureArgs/TextureStage, DisplayDescriptor, Element, Projector). Behaviour follows the TS12
// assembly (addresses per function); several share one body in the original (identical-code folding).
#include <jet/RenderTypes.h>
extern "C" void RsMirrorSync(const char* name);  // U11: keep exported data mirrors in step
#include <jet/VCall.h>

#include <cstdio>
#include <cstring>

namespace Jet {

// ---------------------------------------------------------------------------------------------------------------
// DisplayFormat (0x10001000, 0x10001020, 0x1001d7d0..0x1001dd73, 0x10024b00)

namespace {

// per format id 1..28: bits, then shift and size of red, green, blue and alpha, then stencil and depth bits
const unsigned char kFormatLayouts[28][11] = {
    {32, 16, 8, 8, 8, 0, 8, 24, 8, 0, 0},     // 1 ARGB8888
    {32, 0, 8, 8, 8, 16, 8, 24, 8, 0, 0},     // 2 ABGR8888
    {32, 16, 8, 8, 8, 0, 8, 0, 0, 0, 0},      // 3 RGB0888
    {32, 0, 8, 8, 8, 16, 8, 0, 0, 0, 0},      // 4 BGR0888
    {24, 16, 8, 8, 8, 0, 8, 0, 0, 0, 0},      // 5 RGB888
    {24, 0, 8, 8, 8, 16, 8, 0, 0, 0, 0},      // 6 BGR888
    {16, 8, 4, 4, 4, 0, 4, 12, 4, 0, 0},      // 7 ARGB4444
    {16, 11, 5, 5, 6, 0, 5, 0, 0, 0, 0},      // 8 RGB565
    {16, 10, 5, 5, 5, 0, 5, 0, 0, 0, 0},      // 9 RGB555
    {32, 0, 8, 0, 8, 0, 8, 0, 1, 0, 0},       // 10 DXT1
    {32, 0, 8, 0, 8, 0, 8, 0, 8, 0, 0},       // 11 DXT3
    {32, 0, 8, 0, 8, 0, 8, 0, 8, 0, 0},       // 12 DXT5
    {8, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0},        // 13 L8
    {16, 0, 16, 0, 16, 0, 16, 0, 0, 0, 0},    // 14 L16
    {16, 0, 8, 0, 8, 0, 8, 8, 8, 0, 0},       // 15 AL88
    {8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0},        // 16 A8
    {16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16},      // 17 D16
    {24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24},      // 18 D24
    {32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32},      // 19 D32
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},        // 20 FORMAT_CUSTOM
    {32, 24, 8, 16, 8, 8, 8, 0, 8, 0, 0},     // 21 RGBA8888
    {16, 12, 4, 8, 4, 4, 4, 0, 4, 0, 0},      // 22 RGBA4444
    {32, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0},       // 23 PVRTC4
    {32, 0, 8, 0, 8, 0, 8, 0, 8, 0, 0},       // 24 PVRTC4A
    {32, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0},       // 25 ATC
    {32, 0, 8, 0, 8, 0, 8, 0, 8, 0, 0},       // 26 ATCE
    {32, 0, 8, 0, 8, 0, 8, 0, 8, 0, 0},       // 27 ATCI
    {24, 0, 8, 0, 8, 0, 8, 0, 0, 0, 0},       // 28 ETC1
};

const char* const kFormatNames[29] = {
    "FORMAT_UNKNOWN", "ARGB8888", "ABGR8888", "RGB0888", "BGR0888", "RGB888", "BGR888", "ARGB4444", "RGB565", "RGB555",
    "DXT1", "DXT3", "DXT5", "L8", "L16", "AL88", "A8", "D16", "D24", "D32", "FORMAT_CUSTOM", "RGBA8888", "RGBA4444",
    "PVRTC4", "PVRTC4A", "ATC", "ATCE", "ATCI", "ETC1"};

}  // namespace

DisplayFormat::DisplayFormat() : id(0), bits(0) {}
DisplayFormat::DisplayFormat(DFormatId f) { Set(f); }
DisplayFormat& DisplayFormat::operator=(const DisplayFormat& o) = default;

int DisplayFormat::GetBytes() const { return bits >> 3; }

// the id is kept as a byte; unknown ids clear the layout
void DisplayFormat::Set(DFormatId f)
{
    id = static_cast<unsigned char>(f);
    if (id >= 1 && id <= 28) {
        const unsigned char* v = kFormatLayouts[id - 1];
        SetValues(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
    } else {
        SetValues(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void DisplayFormat::SetValues(unsigned char bits_, unsigned char rs, unsigned char rb, unsigned char gs,
                              unsigned char gb, unsigned char bs, unsigned char bb, unsigned char as, unsigned char ab,
                              unsigned char stencil, unsigned char depth)
{
    bits = bits_;
    redShift = rs;
    redBits = rb;
    greenShift = gs;
    greenBits = gb;
    blueShift = bs;
    blueBits = bb;
    alphaShift = as;
    alphaBits = ab;
    stencilBits = stencil;
    depthBits = depth;
}

void DisplayFormat::SetDepthValues(unsigned char bits_, unsigned char stencil, unsigned char depth)
{
    SetValues(bits_, 0, 0, 0, 0, 0, 0, 0, 0, stencil, depth);
}

bool DisplayFormat::Compare(const DisplayFormat& o) const
{
    return bits == o.bits && redShift == o.redShift && greenShift == o.greenShift && blueShift == o.blueShift &&
           alphaShift == o.alphaShift && stencilBits == o.stencilBits && redBits == o.redBits &&
           greenBits == o.greenBits && blueBits == o.blueBits && alphaBits == o.alphaBits && depthBits == o.depthBits;
}

// formats with a known id compare by id; unknown (0) and custom (20) ones by layout
bool DisplayFormat::operator==(const DisplayFormat& o) const
{
    if (id == DFORMAT_UNKNOWN || id == DFORMAT_CUSTOM || o.id == DFORMAT_UNKNOWN || o.id == DFORMAT_CUSTOM)
        return Compare(o);
    return id == o.id;
}

void DisplayFormat::Classify() const
{
    if (id != DFORMAT_UNKNOWN) return;
    for (unsigned char f = 1; f < 29; ++f) {
        DisplayFormat probe;
        probe.Set(static_cast<DFormatId>(f));
        if (Compare(probe)) {
            id = f;
            return;
        }
    }
    id = DFORMAT_CUSTOM;
}

// the lowest set bit of the mask and the run of set bits from there
void DisplayFormat::GetMaskPlacement(unsigned long mask, unsigned char& shift, unsigned char& bitCount)
{
    shift = 0;
    bitCount = 0;
    if (mask == 0) return;
    // x86 shifts count mod 32 (a mask of all ones never ends, as in the original)
    while (!(mask & (1u << (shift & 31)))) ++shift;
    while (mask & (1u << ((shift + bitCount) & 31))) ++bitCount;
}

const char* DisplayFormat::GetFormatIDString(DFormatId f)
{
    return static_cast<unsigned int>(f) <= 28 ? kFormatNames[f] : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// RenderTextureConstruct (0x10004640..0x10004728, 0x100106c0, 0x1003e930)

RenderTextureConstruct::RenderTextureConstruct() : shared(nullptr), activeDepthLevel(0) { target = TextureTarget(); }

RenderTextureConstruct::RenderTextureConstruct(TextureTarget t, bool useMain, bool create, bool sharedPB,
                                               const SharedContext* s)
    : shared(s), target(t), useMainContext(useMain), createContext(create), sharedPBuffer(sharedPB),
      activeDepthLevel(0)
{
    if (shared) reinterpret_cast<const ReferenceCount*>(shared)->AddReference();
}

RenderTextureConstruct::RenderTextureConstruct(const RenderTextureConstruct& o)
    : shared(o.shared), target(o.target), useMainContext(o.useMainContext), createContext(o.createContext),
      sharedPBuffer(o.sharedPBuffer), activeDepthLevel(o.activeDepthLevel)
{
    if (shared) reinterpret_cast<const ReferenceCount*>(shared)->AddReference();
}

RenderTextureConstruct::~RenderTextureConstruct()
{
    if (shared) reinterpret_cast<const ReferenceCount*>(shared)->ReleaseReference();
}

// a plain copy: the shared context's count is not touched
RenderTextureConstruct& RenderTextureConstruct::operator=(const RenderTextureConstruct& o)
{
    std::memcpy(this, &o, sizeof *this);
    return *this;
}

const SharedContext* RenderTextureConstruct::GetSharedContext() const { return shared; }
TextureTarget RenderTextureConstruct::GetRenderType() const { return target; }
bool RenderTextureConstruct::GetUseMainContext() const { return useMainContext; }
bool RenderTextureConstruct::GetCreateContext() const { return createContext; }
bool RenderTextureConstruct::GetSharedPBuffer() const { return sharedPBuffer; }
int RenderTextureConstruct::GetActiveDepthLevel() const { return activeDepthLevel; }
void RenderTextureConstruct::SetActiveDepthLevel(int level) { activeDepthLevel = level; }

// ---------------------------------------------------------------------------------------------------------------
// DisplayMaterial (0x10001e40..0x1000201a); colours compare channel by channel, NaN never equal

namespace {
bool Differs(const Color& a, const Color& b) { return !(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a); }
}  // namespace

DisplayMaterial::DisplayMaterial()
{
    ambient = {0, 0, 0, 1};
    diffuse = {0, 0, 0, 1};
    specular = {0, 0, 0, 1};
    emissive = {0, 0, 0, 1};
    power = 0;
}
DisplayMaterial::DisplayMaterial(const DisplayMaterial& o) = default;
DisplayMaterial& DisplayMaterial::operator=(const DisplayMaterial& o) = default;

bool DisplayMaterial::operator!=(const DisplayMaterial& o)
{
    return Differs(ambient, o.ambient) || Differs(diffuse, o.diffuse) || Differs(specular, o.specular) ||
           Differs(emissive, o.emissive) || !(power == o.power);
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayLight (0x1000b8e0..0x1000be80, 0x1000c520, 0x1001d560, 0x10021870, 0x10021b10, 0x1003f270)

DisplayLight::DisplayLight() : DisplayLight(DLIGHT_DEFAULT) {}

DisplayLight::DisplayLight(DLightTypes t)
    : type(t), ambient{0, 0, 0, 0}, diffuse{0, 0, 0, 0}, specular{0, 0, 0, 0}, constantAttenuation(1.0f),
      linearAttenuation(0), quadraticAttenuation(0), spotExponent(0), spotCutoff(180.0f), range(0)
{
}

DisplayLight::DisplayLight(const DisplayLight& o) { o.InitInstance(*this); }
DisplayLight::~DisplayLight() {}

void DisplayLight::BatchCalculateDiffuse(Display*, int, VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>>,
                                         VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>>,
                                         VertexItemPtr<unsigned long, VertexItemPtr<const unsigned long, VIPConstHelper>>,
                                         const Matrix4&, const Matrix4&, Material*) const
{
}
int DisplayLight::Slot2() const { return 0; }
int DisplayLight::Slot3() const { return 0; }
int DisplayLight::Slot4() const { return 0; }
int DisplayLight::Slot5() const { return 0; }

bool DisplayLight::operator==(const DisplayLight& o) const
{
    return type == o.type && !Differs(ambient, o.ambient) && !Differs(diffuse, o.diffuse) &&
           !Differs(specular, o.specular) && constantAttenuation == o.constantAttenuation &&
           linearAttenuation == o.linearAttenuation && quadraticAttenuation == o.quadraticAttenuation &&
           spotExponent == o.spotExponent && spotCutoff == o.spotCutoff && range == o.range;
}

DisplayLight& DisplayLight::operator=(const DisplayLight& o)
{
    o.InitInstance(*this);
    return *this;
}

void DisplayLight::InitInstance(DisplayLight& o) const
{
    o.type = type;
    o.ambient = ambient;
    o.diffuse = diffuse;
    o.specular = specular;
    o.constantAttenuation = constantAttenuation;
    o.linearAttenuation = linearAttenuation;
    o.quadraticAttenuation = quadraticAttenuation;
    o.spotExponent = spotExponent;
    o.spotCutoff = spotCutoff;
    o.range = range;
}

DLightTypes DisplayLight::GetLightType() const { return type; }
const Color& DisplayLight::GetAmbient() const { return ambient; }
const Color& DisplayLight::GetDiffuse() const { return diffuse; }
const Color& DisplayLight::GetSpecular() const { return specular; }
void DisplayLight::SetAmbient(const Color& c) { ambient = c; }
void DisplayLight::SetDiffuse(const Color& c) { diffuse = c; }
void DisplayLight::SetSpecular(const Color& c) { specular = c; }
Vector3 DisplayLight::GetAttenuation() const
{
    return Vector3(constantAttenuation, linearAttenuation, quadraticAttenuation);
}
float DisplayLight::GetConstantAttenuation() const { return constantAttenuation; }
float DisplayLight::GetLinearAttenuation() const { return linearAttenuation; }
float DisplayLight::GetQuadraticAttenuation() const { return quadraticAttenuation; }
void DisplayLight::SetConstantAttenuation(float v) { constantAttenuation = v; }
void DisplayLight::SetLinearAttenuation(float v) { linearAttenuation = v; }
void DisplayLight::SetQuadraticAttenuation(float v) { quadraticAttenuation = v; }
float DisplayLight::GetSpotExponent() const { return spotExponent; }
float DisplayLight::GetSpotCutoff() const { return spotCutoff; }
float DisplayLight::GetRange() const { return range; }
void DisplayLight::SetSpotExponent(float v) { spotExponent = v; }
void DisplayLight::SetSpotCutoff(float v) { spotCutoff = v; }
void DisplayLight::SetRange(float v) { range = v; }

// ---------------------------------------------------------------------------------------------------------------
// Element (0x1000c9a0..0x1000cacb, 0x1000f5e0); Display +0x3c is its frame counter

Element::Element() : visited(0), lastCulledFrame(0) {}
Element::Element(const Element& o) : visited(o.visited), lastCulledFrame(o.lastCulledFrame) {}
Element::~Element() {}
Element& Element::operator=(const Element& o)
{
    visited = o.visited;
    lastCulledFrame = o.lastCulledFrame;
    return *this;
}
void Element::SetVisited() const { visited = currentVisited; }
void Element::SetLastCulledFrame(unsigned int frame) { lastCulledFrame = frame; }
void Element::Cull(ViewPoint&, const Matrix4&) {}
void Element::Render(Display*, ViewPort&, Matrix4&) {}
void Element::RenderDebug(Display*, DebugViewPort&, Matrix4&, unsigned long) {}
bool Element::GetCulled(const Display* display) const
{
    return lastCulledFrame != *reinterpret_cast<const unsigned int*>(reinterpret_cast<const char*>(display) + 0x3c);
}
bool Element::IsVisited() const { return visited == currentVisited; }
unsigned int Element::GetLastCulledFrame() const { return lastCulledFrame; }
void Element::ResetVisited()
{
    ++currentVisited;
    RsMirrorSync("?currentVisited@Element@Jet@@0IA");
}

// ---------------------------------------------------------------------------------------------------------------
// Projector (0x10038c40..0x10038cd0, 0x10021870)

Projector::Projector() {}
Projector::Projector(const Projector&) {}
Projector::~Projector() {}
Projector& Projector::operator=(const Projector&) { return *this; }
int Projector::Slot1() const { return 0; }
void Projector::InitInstance(Projector&) const {}

// ---------------------------------------------------------------------------------------------------------------
// TextureArgs (0x1000f060, 0x1000f0e0, 0x1000fc00, 0x100219b0, 0x1003f9c0..0x1003fd2f)

namespace {

struct Token {
    const char* name;
    int value;  // what Decode stores
    int id;     // what GetNextToken answers
};
const Token kTokens[] = {
    {"COLOR", 0, 0},        {"ALPHA", 1, 1},     {"REPLACE", 0, 2},       {"MODULATE", 1, 3},
    {"ADD", 2, 4},          {"ADD_SIGNED", 3, 5}, {"MULTIPLY_ADD", 4, 6},  {"COMBINE_4", 5, 7},
    {"SUBTRACT", 6, 8},     {"INTERPOLATE", 7, 9}, {"DOT3_RGB", 8, 10},    {"DOT3_RGBA", 9, 11},
    {"TEX", 2, 12},         {"CONST", 3, 13},    {"PRIM", 1, 14},         {"PREV", 0, 15},
    {"MINUS", 16, 16},      {"#", 17, 17},       {"STAGE", 2, 18},
};
enum TokenId { TK_COLOR = 0, TK_ALPHA = 1, TK_REPLACE = 2, TK_DOT3_RGBA = 11, TK_TEX = 12, TK_CONST = 13, TK_PRIM = 14,
               TK_PREV = 15, TK_MINUS = 16, TK_STAGE = 18, TK_NONE = 19 };

// arguments taken by each combiner function (the original's table also holds the function's own number)
const int kFunctionArgs[10] = {1, 2, 2, 2, 3, 4, 2, 3, 2, 2};

}  // namespace

TextureArgs::TextureArgs() : mode(MODE_NONE), stage(0), multiplier(1)
{
    for (TexArg& a : args) {
        a.minus = false;
        a.mode = MODE_NONE;
        a.loc = static_cast<Source>(3);
    }
}

TextureArgs::TextureArgs(int stage_, unsigned int multiplier_, ModeType mode_, DTexFuncType fn_, const TexArg& a1,
                         const TexArg& a2, const TexArg& a3, const TexArg& a4)
    : mode(mode_), fn(fn_), stage(stage_), multiplier(multiplier_)
{
    for (TexArg& a : args) {
        a.minus = false;
        a.mode = MODE_NONE;
        a.loc = static_cast<Source>(3);
    }
    numArgs = kFunctionArgs[fn];
    if (multiplier != 1 && multiplier != 2 && multiplier != 4) multiplier = 1;
    args[0] = a1;
    // an argument the function takes last that has no mode switches the whole stage off
    if ((numArgs == 2 && a2.mode == MODE_NONE) || (numArgs == 3 && a3.mode == MODE_NONE) ||
        (numArgs == 4 && a4.mode == MODE_NONE)) {
        mode = MODE_NONE;
        return;
    }
    if (numArgs > 1) args[1] = a2;
    if (numArgs > 2) args[2] = a3;
    if (numArgs > 3) args[3] = a4;
}

TextureArgs::TextureArgs(const TextureArgs& o)
    : mode(o.mode), fn(o.fn), numArgs(o.numArgs), stage(o.stage), multiplier(o.multiplier)
{
    for (int i = 0; i < 4; ++i) {
        args[i].minus = o.args[i].minus;
        args[i].mode = o.args[i].mode;
        args[i].loc = o.args[i].loc;
    }
}

TextureArgs& TextureArgs::operator=(const TextureArgs& o)
{
    std::memcpy(this, &o, sizeof *this);
    return *this;
}

// the next token of text (strtok; text is cleared for the following calls); TK_NONE when it is unknown
int TextureArgs::GetNextToken(char*& text, bool keepUnderscore)
{
    const char* token = std::strtok(text, keepUnderscore ? " \r" : " _\r");
    text = nullptr;
    for (const Token& t : kTokens)
        if (std::strcmp(t.name, token) == 0) return t.id;
    return TK_NONE;
}

bool TextureArgs::Decode(char* text)
{
    char* p = _strupr(text);
    int token = GetNextToken(p, false);
    if (token != TK_STAGE && token != TK_COLOR && token != TK_ALPHA) return false;
    mode = static_cast<ModeType>(kTokens[token].value);
    if (token == TK_STAGE) {
        const char* rest = std::strtok(p, "");
        if (!rest) return false;
        std::sscanf(rest, "%d", &stage);
        return true;
    }
    token = GetNextToken(p, true);
    if (token < TK_REPLACE || token > TK_DOT3_RGBA) return false;
    fn = static_cast<DTexFuncType>(kTokens[token].value);
    numArgs = kFunctionArgs[fn];
    bool next = true;  // false: the token after the last argument's mode starts this one
    for (int i = 0; i < numArgs; ++i) {
        if (next) token = GetNextToken(p, false);
        if (token != TK_PREV && token != TK_PRIM && token != TK_TEX && token != TK_CONST) return false;
        args[i].loc = static_cast<Source>(kTokens[token].value);
        token = GetNextToken(p, false);
        if (token != TK_COLOR && token != TK_ALPHA) return false;
        args[i].mode = static_cast<ModeType>(kTokens[token].value);
        token = GetNextToken(p, false);
        next = token == TK_MINUS;
        args[i].minus = next;
    }
    return true;
}

ModeType TextureArgs::GetMode() const { return mode; }
DTexFuncType TextureArgs::GetFn() const { return fn; }
int TextureArgs::GetNumArgs() const { return numArgs; }
int TextureArgs::GetStage() const { return stage; }
unsigned int TextureArgs::GetMultiplier() const { return multiplier; }
bool TextureArgs::IsArgMinus(int i) const { return args[i].minus; }
Source TextureArgs::GetLoc(int i) const { return args[i].loc; }
ModeType TextureArgs::GetArgMode(int i) const { return args[i].mode; }

// ---------------------------------------------------------------------------------------------------------------
// TextureStage (0x10021870, 0x10048ef0..0x100490b6)

TextureStage::TextureStage(Display* d) : unknown8C(0), display(d) {}
TextureStage::TextureStage(const TextureStage& o)
    : color(o.color), alpha(o.alpha), stage(o.stage), unknown8C(o.unknown8C), display(o.display)
{
}
TextureStage::~TextureStage() {}
TextureStage& TextureStage::operator=(const TextureStage& o)
{
    std::memcpy(this, &o, sizeof *this);
    return *this;
}

void TextureStage::CreateStageSegment(int stage_, unsigned int multiplier, ModeType mode, DTexFuncType fn,
                                      const TexArg& a1, const TexArg& a2, const TexArg& a3, const TexArg& a4)
{
    stage = stage_;
    if (mode == MODE_COLOR)
        color = TextureArgs(stage_, multiplier, MODE_COLOR, fn, a1, a2, a3, a4);
    else if (mode == MODE_ALPHA)
        alpha = TextureArgs(stage_, multiplier, MODE_ALPHA, fn, a1, a2, a3, a4);
}

void* TextureStage::Build()
{
    return VCall<void*>(display, 0x11c, stage, this, &alpha);
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayDescriptor (0x1000ccb0, 0x1001d290..0x1001d7cd, 0x1003ea80, 0x1003f270)

DisplayDescriptor::DisplayDescriptor(APIType type) : apiType(type), apiVersionNumber(0) {}
DisplayDescriptor::DisplayDescriptor(const DisplayDescriptor& o) = default;
DisplayDescriptor::~DisplayDescriptor() {}
DisplayDescriptor& DisplayDescriptor::operator=(const DisplayDescriptor& o) = default;

void DisplayDescriptor::SetData(const PString& name, const PString& version, const PString& vendor, const PString& card,
                                float number)
{
    apiName = name;
    apiVersion = version;
    cardVendorName = vendor;
    cardName = card;
    apiVersionNumber = number;
}

DisplayDescriptor::APIType DisplayDescriptor::GetApiType() const { return apiType; }
const PString& DisplayDescriptor::GetApiName() const { return apiName; }
const PString& DisplayDescriptor::GetApiVersionString() const { return apiVersion; }
float DisplayDescriptor::GetApiVersionNumber() const { return apiVersionNumber; }
const PString& DisplayDescriptor::GetCardVendorName() const { return cardVendorName; }
const PString& DisplayDescriptor::GetCardName() const { return cardName; }

}  // namespace Jet
