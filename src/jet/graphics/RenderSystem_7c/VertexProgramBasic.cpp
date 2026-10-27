// Unit U3b1 of RenderSystem_7c: VertexProgramBasic (0x1004a820..0x1004e5c0), the text program generator under the
// API modules' vertex programs: LoadSymbolTable sorts the parsed symbols into constant lists and register maps,
// LoadInstructions writes each instruction as assembly text (macros for the opcodes the API has no name for) and
// puts the API's header and the declarations in front. Behaviour follows the TS12 assembly (addresses per
// function). Its statics (limits, symbol names, opcode tables) are set by the API module, so they stay the
// original's; so does ApiDataBase's vftable use here (VPSymbol below derives from it).
#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <cstring>

// VS2005 mangled the `T const * const` statics as P..B, today's compiler as Q..B: the imports take the old names
#define JET_VPB_OLD_NAME(name, type) \
    __pragma(comment(linker, "/alternatename:__imp_?" #name "@VertexProgramBasic@Jet@@1Q" type "B=__imp_?" #name \
                             "@VertexProgramBasic@Jet@@1P" type "B"))
JET_VPB_OLD_NAME(vertexSymbol, "BD")
JET_VPB_OLD_NAME(resultSymbol, "BD")
JET_VPB_OLD_NAME(addressSymbol, "BD")
JET_VPB_OLD_NAME(tempSymbol, "BD")
JET_VPB_OLD_NAME(instSeperator, "BD")
JET_VPB_OLD_NAME(logInstr, "BD")
JET_VPB_OLD_NAME(expInstr, "BD")
JET_VPB_OLD_NAME(opcodeMap, "BUOpcodeMap@12@")
JET_VPB_OLD_NAME(vertexMap, "BUVertexMap@12@")
JET_VPB_OLD_NAME(resultMap, "BUResultMap@12@")

namespace Jet {

namespace {

// an AnsiString's length (its block: {refs, length, chars})
unsigned int Length(const AnsiString& s) { return *reinterpret_cast<const unsigned int*>(*reinterpret_cast<char* const*>(&s) + 4); }
void Append(AnsiString& s, const char* text) { s.Ins(text, Length(s)); }

// 0x100217b0: a + b through a copy
AnsiString Plus(const AnsiString& a, const AnsiString& b)
{
    AnsiString t(a);
    t += b;
    return AnsiString(t);
}

const char kComma[] = ", ";
const char kXyzw[] = "xyzw";

// the n-th operand as the original walks its Array iterator (no bounds: the parser gives each opcode its operands)
const RegisterItem* Operand(const VertexInstruction* i, unsigned int n) { return i->operands->data[n]; }

// 0x1004e1f0: d = a * d, in the original's order of operations
void MulInto(float* d, const float* a)
{
    float t[16];
    std::memcpy(t, d, sizeof t);
    d[0] = (((a[1] * t[4]) + (a[3] * t[12])) + (a[2] * t[8])) + (a[0] * t[0]);
    d[1] = (((a[1] * t[5]) + (a[3] * t[13])) + (a[2] * t[9])) + (a[0] * t[1]);
    d[2] = (((a[1] * t[6]) + (a[3] * t[14])) + (a[2] * t[10])) + (a[0] * t[2]);
    d[3] = (((a[1] * t[7]) + (a[3] * t[15])) + (a[2] * t[11])) + (a[0] * t[3]);
    d[4] = (((a[7] * t[12]) + (a[6] * t[8])) + (a[4] * t[0])) + (t[4] * a[5]);
    d[5] = (((a[7] * t[13]) + (a[6] * t[9])) + (a[4] * t[1])) + (t[5] * a[5]);
    d[6] = (((a[7] * t[14]) + (a[6] * t[10])) + (a[4] * t[2])) + (t[6] * a[5]);
    d[7] = (((a[7] * t[15]) + (a[6] * t[11])) + (a[4] * t[3])) + (t[7] * a[5]);
    d[8] = (((a[11] * t[12]) + (a[10] * t[8])) + (a[8] * t[0])) + (t[4] * a[9]);
    d[9] = (((a[11] * t[13]) + (a[10] * t[9])) + (a[8] * t[1])) + (t[5] * a[9]);
    d[10] = (((a[11] * t[14]) + (a[10] * t[10])) + (a[8] * t[2])) + (t[6] * a[9]);
    d[11] = (((a[11] * t[15]) + (a[10] * t[11])) + (a[8] * t[3])) + (t[7] * a[9]);
    d[12] = (((a[15] * t[12]) + (a[14] * t[8])) + (a[12] * t[0])) + (t[4] * a[13]);
    d[13] = (((a[15] * t[13]) + (a[14] * t[9])) + (a[12] * t[1])) + (t[5] * a[13]);
    d[14] = (((a[15] * t[14]) + (a[14] * t[10])) + (a[12] * t[2])) + (t[6] * a[13]);
    d[15] = (((a[15] * t[15]) + (a[14] * t[11])) + (a[12] * t[3])) + (t[7] * a[13]);
}

void Transpose(float* m)
{
    const int pairs[6][2] = {{1, 4}, {2, 8}, {3, 12}, {6, 9}, {7, 13}, {11, 14}};
    for (const auto& p : pairs) {
        const float v = m[p[0]];
        m[p[0]] = m[p[1]];
        m[p[1]] = v;
    }
}

// a VPSymbol made by FUN_1004a820 (the heap's block, null when it has none)
VertexProgramBasic::VPSymbol* NewSymbol() { return new VertexProgramBasic::VPSymbol(); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// ApiDataBase (0x1000f100..0x1000f120)

ApiDataBase::ApiDataBase() {}
ApiDataBase::ApiDataBase(const ApiDataBase&) {}
ApiDataBase::~ApiDataBase() {}
ApiDataBase& ApiDataBase::operator=(const ApiDataBase&) { return *this; }

// 0x1004a820 / 0x1004a920: a register's text for DecodeRegister ({vftable, indexed, name, constant number})
VertexProgramBasic::VPSymbol::VPSymbol() : indexed(false), number(0) {}
VertexProgramBasic::VPSymbol::~VPSymbol() {}

// ---------------------------------------------------------------------------------------------------------------
// VertexProgramBasic

// 0x1004aa10
VertexProgramBasic::VertexProgramBasic(Display* d, VertexType* type)
    : VertexProgram(d, type), localRegMap(nullptr), localMin(999999), localMax(0), envRegMap(nullptr),
      envMin(999999), envMax(0), field48(-1), field4c(0), nextTemp(0), systemTemp(-1), used(nullptr), field5c(false)
{
}

// 0x1004ab30: the three constant lists, both register maps and the used-register table; the last program takes
// the shared env parameters with it
VertexProgramBasic::~VertexProgramBasic()
{
    for (unsigned int i = 0; i < constants.count; ++i) {
        Alloc::Delete(constants.data[i]);
        constants.data[i] = nullptr;
    }
    for (unsigned int i = 0; i < stateConstants.count; ++i) {
        Alloc::Delete(stateConstants.data[i]);
        stateConstants.data[i] = nullptr;
    }
    for (unsigned int i = 0; i < matrixConstants.count; ++i) {
        Alloc::Delete(matrixConstants.data[i]);
        matrixConstants.data[i] = nullptr;
    }
    if (localRegMap) Alloc::Delete(localRegMap);
    localRegMap = nullptr;
    if (envRegMap) Alloc::Delete(envRegMap);
    envRegMap = nullptr;
    if (used) Alloc::Delete(used);
    used = nullptr;
    if (--staticParameterInit == 0) {
        numEnvParameters = 0;
        if (envParameters) Alloc::Delete(envParameters);
        envParameters = nullptr;
    }
    // (the three arrays' blocks and the declarations go with the members, the last list first)
}

VertexProgram::ConstantData** __cdecl VertexProgramBasic::LocalRegMap(int reg) const { return &localRegMap[reg - localMin]; }
void __cdecl VertexProgramBasic::SetLocalRegMap(int reg, ConstantData* c) { localRegMap[reg - localMin] = c; }
VertexProgram::ConstantData* __cdecl VertexProgramBasic::EnvRegMap(int reg) const { return envRegMap[reg - envMin]; }
void __cdecl VertexProgramBasic::SetEnvRegMap(int reg, ConstantData* c) { envRegMap[reg - envMin] = c; }

// 0x1004acc0: everything into `copy`; the constant lists are copied item by item and the register maps point at
// the copies (a local or env constant at the first slot that held the original)
void __cdecl VertexProgramBasic::InitInstance(VertexProgramBasic& copy) const
{
    VertexProgram::InitInstance(copy);
    copy.declarations = declarations;
    copy.field48 = field48;
    copy.nextTemp = nextTemp;
    copy.systemTemp = systemTemp;
    if (localMin <= localMax) {
        copy.localRegMap = static_cast<ConstantData**>(Alloc::New((localMax - localMin + 1) * 4));
        std::memset(copy.localRegMap, 0, (localMax - localMin) * 4 + 4);
        copy.localMax = localMax;
        copy.localMin = localMin;
    }
    if (envMin <= envMax) {
        copy.envRegMap = static_cast<ConstantData**>(Alloc::New((envMax - envMin + 1) * 4));
        std::memset(copy.envRegMap, 0, (envMax - envMin) * 4 + 4);
        copy.envMax = envMax;
        copy.envMin = envMin;
    }
    for (unsigned int i = 0; i < constants.count; ++i) {
        auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
        if (c) *c = *constants.data[i];
        copy.constants.Add(c);
        if (c->kind == 1) {
            const int n = localMax - localMin + 1;
            for (int k = 0; k < n; ++k)
                if (localRegMap[k] == constants.data[i]) {
                    copy.localRegMap[k] = c;
                    break;
                }
        } else if (c->kind == 2) {
            const int n = envMax - envMin + 1;
            for (int k = 0; k < n; ++k)
                if (envRegMap[k] == constants.data[i]) {
                    copy.envRegMap[k] = c;
                    break;
                }
        }
    }
    for (unsigned int i = 0; i < stateConstants.count; ++i) {
        auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
        if (c) *c = *stateConstants.data[i];
        copy.stateConstants.Add(c);
    }
    for (unsigned int i = 0; i < matrixConstants.count; ++i) {
        auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
        if (c) *c = *matrixConstants.data[i];
        copy.matrixConstants.Add(c);
    }
}

// 0x1004b160: the register ranges first (local and env parameters; with `native` every symbol counts, else only
// referenced ones, unreferenced locals still marking their registers), then each symbol by class. A native table
// keeps only program parameters, states and Jet symbols.
bool __cdecl VertexProgramBasic::LoadSymbolTable(const VertexSymbolTable& table, bool native)
{
    used = static_cast<bool*>(Alloc::New(numEnvParameters));
    std::memset(used, 0, numEnvParameters);
    for (unsigned int i = 0; i < table.count; ++i) {
        const VertexSymbolEntry* s = table.data[i];
        if (s->alias) s = static_cast<const VertexSymbolEntry*>(s->alias->UnAlias());
        if (s->refs == 0 && !native) {
            if (s->hint & 2)
                for (const VertexSymbolEntry* c = s; c; c = static_cast<const VertexSymbolEntry*>(c->next)) {
                    const int cls = c->symbolClass;
                    if (cls == 1 || (cls == 10 && c->type == 0x2b))
                        for (int r = c->field10; r <= c->field14; ++r) used[r] = true;
                }
            continue;
        }
        for (const VertexSymbolEntry* c = s; c; c = static_cast<const VertexSymbolEntry*>(c->next)) {
            const int cls = c->symbolClass;
            if (cls == 1 || (cls == 10 && c->type == 0x2b)) {
                if (c->field10 < 0 || c->field14 >= numLocalParameters) return false;
                if (c->field10 < localMin) localMin = c->field10;
                if (c->field14 > localMax) localMax = c->field14;
                for (int r = c->field10; r <= c->field14; ++r) used[r] = true;
            } else if (cls == 2) {
                if (c->field10 < 0 || c->field14 >= numEnvParameters) return false;
                if (c->field10 < envMin) envMin = c->field10;
                if (c->field14 > envMax) envMax = c->field14;
            }
        }
    }
    if (localMin <= localMax) {
        localRegMap = static_cast<ConstantData**>(Alloc::New((localMax - localMin + 1) * 4));
        std::memset(localRegMap, 0, (localMax - localMin) * 4 + 4);
    }
    if (envMin <= envMax) {
        envRegMap = static_cast<ConstantData**>(Alloc::New((envMax - envMin + 1) * 4));
        std::memset(envRegMap, 0, (envMax - envMin) * 4 + 4);
    }
    if (!native) {
        for (unsigned int i = 0; i < table.count; ++i) {
            const VertexSymbolEntry* s = table.data[i];
            if (s->refs == 0) continue;
            for (; s; s = static_cast<const VertexSymbolEntry*>(s->next)) {
                const int cls = s->symbolClass;
                bool ok = true;
                switch (cls) {
                case 1:
                case 2: ok = ProcessProgramSymbol(s, cls == 1, used); break;
                case 3: ok = ProcessLocalConstSymbol(s, used); break;
                case 4: ok = ProcessStateSymbol(s, used); break;
                case 5: ok = ProcessAddressSymbol(s); break;
                case 6: ok = ProcessVertexSymbol(s); break;
                case 7: ok = ProcessResultSymbol(s); break;
                case 8: ok = ProcessAliasSymbol(s); break;
                case 9: ok = ProcessTempSymbol(s, nextTemp); break;
                case 10: ok = ProcessJetSymbol(s, used); break;
                default: break;
                }
                if (!ok) return false;
            }
        }
        return true;
    }
    for (unsigned int i = 0; i < table.count; ++i) {
        const VertexSymbolEntry* s = table.data[i];
        if (s->alias) s = static_cast<const VertexSymbolEntry*>(s->alias->UnAlias());
        const int cls = s->symbolClass;
        if (cls == 1 || cls == 2) {
            if (!ProcessProgramSymbol(s, cls == 1, used)) return false;
        } else if (cls == 10) {
            if (!ProcessJetSymbol(s, used)) return false;
        } else if (cls == 4) {
            // a state as the native program names it: the register is the table's own symbol's (+0xc/+0x10 only
            // for the matrices 0xc..0x11, left as the heap had them otherwise)
            auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
            c->reg = table.data[i]->field10;
            c->kind = 4;
            c->state = s->type;
            c->field14 = s->field10;
            c->field18 = s->field14;
            c->lightType = s->field1c;
            if (s->type >= 0xc && s->type <= 0x11) {
                c->lightIndex = s->field0c;
                c->field0c = s->field34;
                matrixConstants.Add(c);
            } else {
                stateConstants.Add(c);
            }
        } else {
            return false;
        }
    }
    return true;
}

// 0x1004b620: each instruction (all must decode), then the program is the API's header, the declarations and the
// instructions; the declarations keep one character
bool __cdecl VertexProgramBasic::LoadInstructions(const Array<VertexInstruction*, 128>& list)
{
    if (list.count == 0) return false;
    for (unsigned int i = 0; i < list.count; ++i)
        if (!AddSingleInstruction(list.data[i])) return false;
    AnsiString header;
    GetProgramHeader(header);
    source = Plus(Plus(header, declarations), source);
    declarations.SetLength(1);
    return true;
}

// 0x1004b760: the opcode's name and its operands, or a macro for the opcodes the API names none for
bool __cdecl VertexProgramBasic::AddSingleInstruction(const VertexInstruction* in)
{
    AnsiString line;
    const int op = in->opcode;
    if (const char* name = opcodeMap[op].name) {
        Append(line, name);
    } else {
        switch (op) {
        case 1: return DecodeAbsolute(source, in);
        case 6: return DecodeHomoDot(source, in);
        case 8:
        case 0xb:
        case 0xc: return false;
        case 0xa: return DecodeFloor(source, in);
        case 0xf: return DecodeLrp(source, in);
        case 0x15: return DecodeNrm(source, in);
        case 0x16: return DecodePower(source, in);
        case 0x1a: return DecodeSign(source, in);
        case 0x1c: return DecodeSub(source, in);
        case 0x1d: return DecodeSwizzle(source, in);
        case 0x1e: return DecodeXProd(source, in);
        case 0x1f:
        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
            if (in->operands->count != 3) return false;
            return CreateMatrixMacroInstructions(source, in);
        default: break;  // no name, no macro: the operands alone
        }
    }
    const RegisterList* ops = in->operands;
    for (unsigned int i = 0; i < ops->count; ++i) {
        DecodeRegister(line, ops->data[i], true);
        if (i + 1 < ops->count) Append(line, kComma);
    }
    Append(line, instSeperator);
    source += line;
    return true;
}

// 0x1004dcb0 / 0x1004dcd0: temporaries after the program's own; the system temporary is taken once
int __cdecl VertexProgramBasic::NextFreeTemp() const
{
    const int t = nextTemp;
    if (t < numTmpRegisters) {
        nextTemp = t + 1;
        return t;
    }
    return -1;
}
int __cdecl VertexProgramBasic::GetSystemTemp() const
{
    if (systemTemp < 0) systemTemp = NextFreeTemp();
    return systemTemp;
}

// 0x1004ba70: SWZ through a mad with the API's constant register (x 0, y 1, z -1 and w as the API sets them)
bool __cdecl VertexProgramBasic::DecodeSwizzle(AnsiString& out, const VertexInstruction* in)
{
    if (GetSwizzleConstant() < 0) return false;
    AnsiString line("mad ");
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* src = Operand(in, 1);
    DecodeRegister(line, dst, true);
    Append(line, kComma);
    const char* s = src->swizzle;
    char select[5], scale[5], bias[5];
    select[4] = scale[4] = bias[4] = 0;
    for (int i = 0; i < 4; ++i, s += 3) {
        const bool neg = s[0] == '-';
        if (s[1] < 'a') {
            select[i] = 'x';
            scale[i] = 'x';
            bias[i] = s[1] == '0' ? 'x' : static_cast<char>('y' + neg);
        } else {
            select[i] = s[1];
            scale[i] = static_cast<char>('y' + neg);
            bias[i] = 'x';
        }
    }
    DecodeRegister(line, src, false);
    line.Addf(".%s,  c[%d].%s,  c[%d].%s;", select, field48, scale, field48, bias);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004bc30: POW as log, mul and exp through the system temporary's z
bool __cdecl VertexProgramBasic::DecodePower(AnsiString& out, const VertexInstruction* in) const
{
    const int t = GetSystemTemp();
    if (t < 0) return false;
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* a = Operand(in, 1);
    const RegisterItem* b = Operand(in, 2);
    AnsiString line;
    line.Setf("%s  r%d.z, ", logInstr, t);
    DecodeRegister(line, a, true);
    line.Addf(";mul r%d.z, r%d.z, ", t, t);
    DecodeRegister(line, b, true);
    line.Addf(";%s", expInstr);
    if (!dst->swizzle || dst->swizzle[0] != 'z') line.Addf(" r%d.z, r%d.z;mov ", t, t);
    DecodeRegister(line, dst, true);
    line.Addf(", r%d.z;", t);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004bdd0: ABS as max(x, -x)
bool __cdecl VertexProgramBasic::DecodeAbsolute(AnsiString& out, const VertexInstruction* in) const
{
    AnsiString line("max ");
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* src = Operand(in, 1);
    DecodeRegister(line, dst, true);
    Append(line, kComma);
    DecodeRegister(line, src, true);
    RegisterItem neg(src);
    neg.flag = !neg.flag;
    Append(line, kComma);
    DecodeRegister(line, &neg, true);
    Append(line, instSeperator);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004bf40: DPH as dp3 plus the second operand's w
bool __cdecl VertexProgramBasic::DecodeHomoDot(AnsiString& out, const VertexInstruction* in) const
{
    const int t = GetSystemTemp();
    if (t < 0) return false;
    AnsiString line;
    line.Setf("dp3 r%d, ", t);
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* a = Operand(in, 1);
    const RegisterItem* b = Operand(in, 2);
    DecodeRegister(line, a, true);
    Append(line, kComma);
    DecodeRegister(line, b, true);
    Append(line, ";add ");
    DecodeRegister(line, dst, true);
    line.Addf(", r%d, ", t);
    DecodeRegister(line, b, false);
    Append(line, ".w;");
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004c100: FLR as x - frc(x). A one-component source goes through the system temporary's y; otherwise each
// written component in turn (w first), and a source that is also the destination is copied to a new temporary
// first. The source's stack copy shares the operand's pointers and its destructor frees them, as in the original.
bool __cdecl VertexProgramBasic::DecodeFloor(AnsiString& out, const VertexInstruction* in) const
{
    const int t = GetSystemTemp();
    AnsiString line;
    if (t < 0) return false;
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* src = Operand(in, 1);
    if (src->swizzle && src->swizzle[1] == 0) {
        line.Addf("%s r%d.y, ", expInstr, t);
        DecodeRegister(line, src, true);
        const char* d = dst->swizzle;
        if (d && d[0] == 'y' && d[1] == 0) {
            Append(line, ";add ");
            DecodeRegister(line, dst, true);
            Append(line, kComma);
            DecodeRegister(line, src, true);
            line.Addf(", -r%d.y\n", t);
        } else {
            line.Addf(";add r%d.y, ", t);
            DecodeRegister(line, src, true);
            line.Addf(", -r%d.y;mov ", t);
            DecodeRegister(line, dst, true);
            line.Addf(", r%d.y;", t);
        }
        FinishLine(line);
        out += line;
        return true;
    }
    RegisterItem copy(*src, RegisterItem::Shallow);
    const RegisterItem* from = src;
    bool owned = false;
    if (dst->symbol == src->symbol) {
        const int t2 = nextTemp;
        if (t2 >= numTmpRegisters) return false;  // (copy's destructor runs)
        nextTemp = t2 + 1;
        if (t2 < 0) return false;
        line.Addf("mov r%d, ", t2);
        DecodeRegister(line, src, false);
        Append(line, instSeperator);
        auto* sym = new VertexSymbolEntry(static_cast<VertexSymbolClass>(9), static_cast<SymbolType>(2), true);
        copy.symbol = sym;
        owned = true;
        VPSymbol* api = NewSymbol();
        api->name.Setf("r%d", t2);
        sym->api = api;
        from = &copy;
    }
    for (int i = 3; i >= 0; --i) {
        if (dst->swizzle && !std::strchr(dst->swizzle, kXyzw[i])) continue;
        line.Addf("%s r%d.y, ", expInstr, t);
        DecodeRegister(line, from, false);
        const int c = from->swizzle ? from->swizzle[i] : kXyzw[i];
        line.Addf(".%c;add ", c);
        DecodeRegister(line, dst, false);
        line.Addf(".%c, ", dst->swizzle ? dst->swizzle[i] : kXyzw[i]);
        DecodeRegister(line, from, false);
        line.Addf(".%c, -r%d.y;", c, t);
    }
    copy.swizzle = nullptr;
    if (owned) {
        if (copy.symbol) {
            copy.symbol->~SymbolEntry();
            Alloc::Delete(copy.symbol);
        }
        copy.symbol = nullptr;
    }
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004c5e0: SUB as add with the second source negated
bool __cdecl VertexProgramBasic::DecodeSub(AnsiString& out, const VertexInstruction* in) const
{
    AnsiString line("add ");
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* a = Operand(in, 1);
    RegisterItem neg(Operand(in, 2));
    neg.flag = !neg.flag;
    DecodeRegister(line, dst, true);
    Append(line, kComma);
    DecodeRegister(line, a, true);
    Append(line, kComma);
    DecodeRegister(line, &neg, true);
    Append(line, instSeperator);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004c750: XPD as mul and mad through the system temporary (the negated copy of the second source shares its
// pointers and frees them, as in the original)
bool __cdecl VertexProgramBasic::DecodeXProd(AnsiString& out, const VertexInstruction* in) const
{
    const int t = GetSystemTemp();
    if (t < 0) return false;
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* a = Operand(in, 1);
    const RegisterItem* b = Operand(in, 2);
    AnsiString line;
    line.Setf("mul r%d, ", t);
    DecodeRegister(line, a, false);
    Append(line, ".yzxw, ");
    DecodeRegister(line, b, false);
    Append(line, ".zxyw;mad ");
    DecodeRegister(line, dst, true);
    Append(line, kComma);
    RegisterItem neg(*b, RegisterItem::Shallow);
    neg.flag = !neg.flag;
    DecodeRegister(line, &neg, false);
    Append(line, ".yzxw, ");
    DecodeRegister(line, a, false);
    line.Addf(".zxyw, r%d;", t);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004c970: LRP (four operands) as add and mad through the system temporary
bool __cdecl VertexProgramBasic::DecodeLrp(AnsiString& out, const VertexInstruction* in) const
{
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* f = Operand(in, 1);
    AnsiString line;
    if (in->operands->count != 4) return false;
    const int t = GetSystemTemp();
    line.Addf("add r%d,", t);
    const RegisterItem* a = Operand(in, 2);
    RegisterItem b(Operand(in, 3));
    b.flag = !b.flag;
    const bool negated = b.flag;
    DecodeRegister(line, a, false);
    Append(line, kComma);
    DecodeRegister(line, &b, false);
    Append(line, instSeperator);
    b.flag = !negated;
    line.Addf("mad  ");
    DecodeRegister(line, dst, false);
    Append(line, kComma);
    DecodeRegister(line, f, false);
    line.Addf(", r%d, ", t);
    DecodeRegister(line, &b, false);
    Append(line, instSeperator);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004cbd0: NRM as dp3, rsq and mul through the system temporary's w
bool __cdecl VertexProgramBasic::DecodeNrm(AnsiString& out, const VertexInstruction* in) const
{
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* src = Operand(in, 1);
    AnsiString line;
    if (in->operands->count != 2) return false;
    const int t = GetSystemTemp();
    line.Addf("dp3 r%d.w,", t);
    DecodeRegister(line, src, false);
    Append(line, kComma);
    DecodeRegister(line, src, false);
    Append(line, instSeperator);
    line.Addf("rsq  r%d.w, r%d.w;", t, t);
    Append(line, "mul  ");
    DecodeRegister(line, dst, false);
    line.Addf(",  r%d.w, ", t);
    DecodeRegister(line, src, false);
    Append(line, instSeperator);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004cdb0: SGN as sge and mad with the API's constant register
bool __cdecl VertexProgramBasic::DecodeSign(AnsiString& out, const VertexInstruction* in)
{
    const RegisterItem* dst = Operand(in, 0);
    const RegisterItem* src = Operand(in, 1);
    if (in->operands->count != 2) return false;
    if (GetSwizzleConstant() < 0) return false;
    AnsiString line("sge ");
    DecodeRegister(line, dst, false);
    Append(line, kComma);
    DecodeRegister(line, src, false);
    line.Addf(",c[%d].x;mad ", field48);
    DecodeRegister(line, dst, false);
    Append(line, kComma);
    DecodeRegister(line, dst, false);
    line.Addf(", c[%d].w, c[%d].y;", field48, field48);
    FinishLine(line);
    out += line;
    return true;
}

// 0x1004cf20: M4x4..M3x2 as one dp3/dp4 per written row, the matrix operand's offset stepping through the rows
bool __cdecl VertexProgramBasic::CreateMatrixMacroInstructions(AnsiString& out, const VertexInstruction* in) const
{
    AnsiString dstText;
    AnsiString srcText;
    const char* op = nullptr;
    int rows = 0;
    switch (in->opcode) {
    case 0x1f: op = "DP3  "; rows = 2; break;
    case 0x20: op = "DP3  "; rows = 3; break;
    case 0x21: op = "DP3  "; rows = 4; break;
    case 0x22: op = "DP4  "; rows = 3; break;
    case 0x23: op = "DP4  "; rows = 4; break;
    default: break;
    }
    RegisterItem dst(Operand(in, 0));
    char* const mask = dst.swizzle;
    dst.swizzle = nullptr;
    DecodeRegister(dstText, &dst, true);
    DecodeRegister(srcText, Operand(in, 1), true);
    RegisterItem* m = const_cast<RegisterItem*>(Operand(in, 2));
    bool owned;
    int keep;
    if (m->offset) {
        keep = m->offset->offset;
        owned = false;
    } else {
        auto* a = static_cast<OffsetAddress*>(Alloc::New(sizeof(OffsetAddress)));
        if (a) {
            a->item = nullptr;
            a->offset = 0;
        }
        m->offset = a;
        owned = true;
        keep = 9;
    }
    for (int i = 0; i < rows; ++i) {
        const char c = kXyzw[i];
        if (!mask || std::strchr(mask, c)) {
            const char* s = srcText;
            const char* d = dstText;
            out.Addf("%s  %s.%c,  %s,  ", op, d, c, s);
            DecodeRegister(out, m, true);
            Append(out, instSeperator);
        }
        ++m->offset->offset;
    }
    if (owned) {
        if (OffsetAddress* a = m->offset) {
            delete a->item;
            a->item = nullptr;
            Alloc::Delete(a);
        }
        m->offset = nullptr;
    } else {
        m->offset->offset = keep;
    }
    if (mask) Alloc::Delete(mask);
    return true;
}

// 0x1004d260: an operand's text: the sign, the register's API name (an indexed constant as c[...] with its
// offset), then the swizzle when asked
void __cdecl VertexProgramBasic::DecodeRegister(AnsiString& out, const RegisterItem* r, bool swizzle) const
{
    if (r->flag) Append(out, "-");
    const auto* api = static_cast<const VPSymbol*>(r->symbol->api);
    if (!api) {
        Append(out, "Unexpected Register");
    } else if (!api->indexed) {
        out += api->name;
    } else if (const OffsetAddress* a = r->offset) {
        int n = a->offset + api->number;
        if (a->item) {
            n += field4c;
            Append(out, "c[");
            DecodeRegister(out, a->item, true);
            if (n > 0) out.Addf(" + %d]", n);
            else if (n < 0) out.Addf("%d]", n);
            else out.Addf("]");
        } else {
            out.Addf("c[%d]", n);
        }
    } else {
        out.Addf("c[%d]", api->number);
    }
    if (r->swizzle && swizzle) {
        Append(out, ".");
        Append(out, r->swizzle);
    }
}

// 0x1004d3a0: a constant the program defines ("def c<n>, x, y, z, w" among the declarations)
bool __cdecl VertexProgramBasic::ProcessLocalConstSymbol(const VertexSymbolEntry* s, bool* used)
{
    const int r = GetFreeRegisters(1, s->name, used, false);
    if (r < 0) return false;
    VPSymbol* api = NewSymbol();
    api->number = r;
    api->indexed = true;
    const float* v = reinterpret_cast<const float*>(&s->field24);
    s->api = api;
    declarations.Addf("def c%d, %f, %f, %f, %f\n", r, static_cast<double>(v[0]), static_cast<double>(v[1]),
                      static_cast<double>(v[2]), static_cast<double>(v[3]));
    return true;
}

// 0x1004d490: program parameters: locals at their own registers (each a constant, skipped with hint bit 0),
// env parameters at free registers pointing into the shared env values
bool __cdecl VertexProgramBasic::ProcessProgramSymbol(const VertexSymbolEntry* s, bool local, bool* used)
{
    int first = s->field10;
    if (local) {
        for (int r = s->field10; r <= s->field14; ++r) {
            if (s->hint & 1) continue;
            auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
            c->reg = r;
            c->kind = 1;
            const float* v = reinterpret_cast<const float*>(&s->field24);
            float* d = reinterpret_cast<float*>(&c->state);
            d[0] = v[0];
            d[1] = v[1];
            d[2] = v[2];
            d[3] = v[3];
            constants.Add(c);
            localRegMap[r - localMin] = c;
        }
    } else {
        first = GetFreeRegisters(s->field14 - s->field10 + 1, s->name, used, false);
        if (first < 0) return false;
        for (int r = s->field10; r <= s->field14; ++r) {
            auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
            c->reg = r;
            c->kind = 2;
            c->state = static_cast<int>(reinterpret_cast<uintptr_t>(&envParameters[r]));
            constants.Add(c);
            envRegMap[r - envMin] = c;
        }
    }
    AddSymbolName(s->name, s->field10, !local, s->field14 - s->field10 + 1);
    VPSymbol* api = NewSymbol();
    if (s->field10 == s->field14) {
        api->name.Setf("c[%d]", first);
    } else {
        api->name.Setf("c");
        api->number = first;
        api->indexed = true;
    }
    s->api = api;
    return true;
}

// 0x1004d6e0: a Jet symbol: the skinning matrices (0x2b) set the influence registers
bool __cdecl VertexProgramBasic::ProcessJetSymbol(const VertexSymbolEntry* s, bool*)
{
    const int r = s->field10;
    if (s->type == 0x2b) {
        influenceBase = s->field10;
        influenceMax = s->field14;
    }
    AddSymbolName(s->name, s->field10, false, s->field14 - s->field10 + 1);
    VPSymbol* api = NewSymbol();
    if (s->field10 == s->field14) {
        api->name.Setf("c[%d]", r);
    } else {
        api->name.Setf("c");
        api->number = s->field10;
        api->indexed = true;
    }
    s->api = api;
    return true;
}

// 0x1004d7d0: a render state at free registers (a 4-row matrix aligned when the API wants it); matrices
// (0xc..0x11) go to their own list, the program matrix only where supported and 0x10 never
bool __cdecl VertexProgramBasic::ProcessStateSymbol(const VertexSymbolEntry* s, bool* used)
{
    auto* c = static_cast<ConstantData*>(Alloc::New(sizeof(ConstantData)));
    const int n = s->field14 - s->field10 + 1;
    const bool matrix = s->type >= 0xc && s->type <= 0x11;
    const bool aligned = needsAlignedMatrix && matrix && n == 4;
    c->reg = GetFreeRegisters(n, s->name, used, aligned);
    if (c->reg < 0) {
        Alloc::Delete(c);
        return false;
    }
    c->kind = 4;
    c->state = s->type;
    c->field14 = s->field10;
    c->field18 = s->field14;
    c->lightType = s->field1c;
    if (s->type >= 0x12 && s->type <= 0x16) c->lightIndex = (s->flags >> 1) & 1;
    else c->lightIndex = s->field0c;
    if (matrix) {
        if (s->type == 0x10 || (s->type == 0x11 && !supportsProgramMatrix)) {
            Alloc::Delete(c);
            return false;
        }
        c->field0c = s->field34;
        matrixConstants.Add(c);
    } else {
        stateConstants.Add(c);
    }
    VPSymbol* api = NewSymbol();
    api->number = c->reg;
    api->indexed = true;
    s->api = api;
    return true;
}

// 0x1004d960: a vertex input by the API's map (indexed ones add the symbol's index)
bool __cdecl VertexProgramBasic::ProcessVertexSymbol(const VertexSymbolEntry* s)
{
    const int k = s->type - 3;
    VPSymbol* api = NewSymbol();
    const VertexMap& m = vertexMap[k];
    api->name.Setf(vertexSymbol, m.indexed ? s->field0c + m.reg : m.reg);
    s->api = api;
    return true;
}

// 0x1004da00: an output by the API's map; 0x2e marks the program as writing it (+0x5c)
bool __cdecl VertexProgramBasic::ProcessResultSymbol(const VertexSymbolEntry* s)
{
    const int k = s->type - 0x2c;
    VPSymbol* api = NewSymbol();
    const ResultMap& m = resultMap[k];
    if (m.indexed) api->name.Setf(resultSymbol, m.name, s->field0c);
    else Append(api->name, m.name);
    s->api = api;
    if (s->type == 0x2e) field5c = true;
    return true;
}

// 0x1004dac0
bool __cdecl VertexProgramBasic::ProcessAddressSymbol(const VertexSymbolEntry* s) const
{
    VPSymbol* api = NewSymbol();
    api->name.Setf(addressSymbol);
    s->api = api;
    return true;
}

// 0x1004db40: the text of the symbol the alias stands for; false when it has none
bool __cdecl VertexProgramBasic::ProcessAliasSymbol(const VertexSymbolEntry* s) const
{
    const SymbolEntry* t = s->alias ? s->alias->UnAlias() : s;
    VPSymbol* api = NewSymbol();
    const auto* to = static_cast<const VPSymbol*>(t->api);
    if (to->indexed) api->name.Setf("c[%d]", to->number);
    else api->name = to->name;
    s->api = api;
    return Length(api->name) != 0;
}

// 0x1004dc00: the next free temporary (the counter, not `next`, is what moves)
bool __cdecl VertexProgramBasic::ProcessTempSymbol(const VertexSymbolEntry* s, int&) const
{
    const int t = nextTemp;
    if (t >= numTmpRegisters) return false;
    nextTemp = t + 1;
    if (t < 0) return false;
    VPSymbol* api = NewSymbol();
    api->name.Setf(tempSymbol, t);
    s->api = api;
    return true;
}

// 0x1004dd00: the first run of `count` unused registers (aligned on 4 when asked), now marked used
int __cdecl VertexProgramBasic::GetFreeRegisters(int count, const PString&, bool* used, bool aligned) const
{
    const int step = aligned ? 4 : 1;
    const int limit = numEnvParameters - count + 1;
    for (int i = 0; i < limit; i += step) {
        if (used[i]) continue;
        const int start = i;
        const int end = start + count;
        int j = start + 1;
        while (j < end && !used[j]) ++j;
        if (j < end) {
            i = start;
            continue;
        }
        if (start < end) std::memset(used + start, 1, end - start);
        return start;
    }
    return -1;
}

// 0x1004dd80: the generated text as the API's native program (slot 34)
bool __cdecl VertexProgramBasic::CompileProgram() { return LoadNativeProgram(source); }

// 0x1004ddb0: each matrix state from the display (slots 25, 26, 28 and 31 by state; 0xc is model times view),
// transposed and/or inverted as the symbol asked, its rows into the local constants
void __cdecl VertexProgramBasic::LoadProgramMatrices() const
{
    for (unsigned int i = 0; i < matrixConstants.count; ++i) {
        const ConstantData* c = matrixConstants.data[i];
        float m[16];
        float m2[16];
        switch (c->state) {
        case 0xc:
            VCall<void>(display, 0x68, 0, m2);
            VCall<void>(display, 0x70, m);
            VCall<void>(display, 0x64, m);
            MulInto(m, m2);
            break;
        case 0xd: VCall<void>(display, 0x68, c->lightIndex, m); break;
        case 0xe:
            VCall<void>(display, 0x70, m);
            VCall<void>(display, 0x64, m);
            break;
        case 0xf: VCall<void>(display, 0x7c, c->lightIndex, m); break;
        default: break;  // (the matrix stays as the stack had it, as in the original)
        }
        switch (c->field0c) {
        case 0: Transpose(m); break;
        case 1:
            Transpose(m);
            reinterpret_cast<Matrix4*>(m)->Invert();
            break;
        case 3: reinterpret_cast<Matrix4*>(m)->Invert(); break;
        default: break;
        }
        SetLocalConstants(c->reg, reinterpret_cast<const Vector4*>(m) + c->field14, c->field18 - c->field14 + 1);
    }
}

// 0x1004e090: the render-state constants
void __cdecl VertexProgramBasic::SetProgramState(const DisplayLightList& lights, Color ambient,
                                                 const DisplayMaterial& material, const Matrix4& view) const
{
    LoadCommonStates(stateConstants, lights, ambient, material, view);
}

int __cdecl VertexProgramBasic::GetMaxLocalRegisters() const { return numLocalParameters; }
int __cdecl VertexProgramBasic::GetMaxEnvRegisters() const { return numEnvParameters; }
int __cdecl VertexProgramBasic::GetMaxTotalRegisters() const { return numTotalParameters; }

// the API module's slots (0x1001dec0 answers 0) and the macros it has to give (0x10009500: false)
void __cdecl VertexProgramBasic::GetProgramHeader(AnsiString&) const {}
int __cdecl VertexProgramBasic::GetSwizzleConstant() { return 0; }
void __cdecl VertexProgramBasic::FinishLine(AnsiString&) const {}
bool __cdecl VertexProgramBasic::DecodeExp(AnsiString&, const VertexInstruction*) const { return false; }
bool __cdecl VertexProgramBasic::DecodeLog(AnsiString&, const VertexInstruction*) const { return false; }
bool __cdecl VertexProgramBasic::DecodeFrac(AnsiString&, const VertexInstruction*) const { return false; }

}  // namespace Jet
