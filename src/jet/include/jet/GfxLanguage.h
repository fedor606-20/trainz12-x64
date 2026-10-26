// RenderSystem_7c unit U2 (notes/RenderSystem.md), the shader-language front-end: the symbol records, register
// operands and instructions the vertex/texture program parsers build, and the preprocessor defines. Layouts follow
// the TS12 assembly (addresses per member in RenderSystem_7c/GfxSymbols.cpp).
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>

namespace Jet {

struct ApiDataBase;  // the renderer's per-symbol data (U9); deleted through its virtual destructor

enum SymbolType {};
enum VertexSymbolClass {};
enum TextureSymbolClass {};
enum VertexOpcode {};
enum TextureOpcode {};

// a named or anonymous symbol of a program (0x48 bytes). Symbols chain through `next` (AppendSymbol) and an alias
// points at the symbol it stands for (UnAlias); a symbol owns its chain, its API data and, with bit 2 of `flags`,
// its alias.
struct SymbolEntry {
    SymbolEntry(SymbolType type, bool referenced);
    SymbolEntry(const SymbolEntry& o);
    SymbolEntry& operator=(const SymbolEntry& o);
    ~SymbolEntry();

    void __cdecl SetName(const char* name);  // null: no name; either way the symbol is no longer anonymous
    const PString& __cdecl GetName() const;
    void __cdecl AddRef();  // on the symbol and every alias it stands for
    void __cdecl DecRef();
    bool __cdecl IsAnonymous() const;
    bool __cdecl IsReferenced() const;
    bool __cdecl LikeType(const SymbolEntry* o) const;
    void __cdecl AppendSymbol(SymbolEntry* s);  // at the end of the `next` chain
    SymbolEntry* __cdecl GetNextSymbol() const;
    const SymbolEntry* __cdecl UnAlias() const;
    void __cdecl SetApiData(ApiDataBase* d) const;
    ApiDataBase* __cdecl GetApiData() const;
    ApiDataBase* __cdecl GetAndZeroApiData() const;
    unsigned long __cdecl GetHint() const;
    void __cdecl SetHint(unsigned long h);

    JET_HEAP_OBJECT

    SymbolEntry* alias;  // +0
    SymbolType type;     // +4
    int field08, field0c, field10, field14;  // +8.. (-1 when new; LikeType compares +0xc..+0x14)
    unsigned long hint;  // +0x18
    int field1c;         // +0x1c (5 when new)
    unsigned char field20;  // +0x20
    unsigned char flags;    // +0x21 bit 0 anonymous, bit 1 (compared by LikeType), bit 2 owns the alias
    int field24, field28, field2c, field30;  // +0x24 (left by the constructors)
    int field34;                 // +0x34
    PString name;                // +0x38
    int refs;                    // +0x3c
    SymbolEntry* next;           // +0x40
    mutable ApiDataBase* api;    // +0x44
};
JET_LAYOUT32(sizeof(SymbolEntry) == 0x48);

// the two languages' symbols add their class (+0x48); they share their bodies in the original
struct VertexSymbolEntry : SymbolEntry {
    VertexSymbolEntry(VertexSymbolClass cls, SymbolType type, bool referenced);
    VertexSymbolEntry(const VertexSymbolEntry& o);
    VertexSymbolEntry& operator=(const VertexSymbolEntry& o);
    ~VertexSymbolEntry();
    bool __cdecl LikeType(const VertexSymbolEntry* o) const;  // the same class, then SymbolEntry::LikeType

    VertexSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(VertexSymbolEntry) == 0x4c);

struct TextureSymbolEntry : SymbolEntry {
    TextureSymbolEntry(TextureSymbolClass cls, SymbolType type, bool referenced);
    TextureSymbolEntry(const TextureSymbolEntry& o);
    TextureSymbolEntry& operator=(const TextureSymbolEntry& o);
    ~TextureSymbolEntry();

    TextureSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(TextureSymbolEntry) == 0x4c);

// the symbols of a vertex program: an Array<VertexSymbolEntry*, 128> it owns
struct VertexSymbolTable {
    VertexSymbolTable();
    VertexSymbolTable(const VertexSymbolTable& o);  // copies the pointers
    VertexSymbolTable& operator=(const VertexSymbolTable& o);
    ~VertexSymbolTable();  // deletes the symbols

    VertexSymbolEntry* __cdecl Find(const PString& name) const;
    VertexSymbolEntry* __cdecl FindLike(const VertexSymbolEntry* e) const;  // the first of the same class and type
    void __cdecl AddSymbol(VertexSymbolEntry* e);

    unsigned long count, capacity;  // +0, +4 (capacity in steps of 128)
    VertexSymbolEntry** data;       // +8
};
JET_LAYOUT32(sizeof(VertexSymbolTable) == 0xc);

// a modifier of a register operand (negation, write mask...): copies nothing
struct RegisterModifier {
    RegisterModifier& operator=(const RegisterModifier& o);
};

struct RegisterItem;

// an indexed operand: the index register and a constant offset
struct OffsetAddress {
    OffsetAddress(RegisterItem* item, int offset);
    ~OffsetAddress();  // deletes the index register
    OffsetAddress& operator=(const OffsetAddress& o);

    RegisterItem* item;  // +0
    int offset;          // +4
};

// one operand of an instruction (0x18 bytes); it owns its swizzle text, offset and modifiers
struct RegisterItem {
    RegisterItem(SymbolEntry* symbol, char* swizzle, bool flag, RegisterModifier* m1, RegisterModifier* m2);
    explicit RegisterItem(const RegisterItem* o);  // a copy without the modifiers; the offset's register is shared
    ~RegisterItem();
    RegisterItem& operator=(const RegisterItem& o);  // a plain copy of the six fields

    SymbolEntry* symbol;     // +0
    OffsetAddress* offset;   // +4
    char* swizzle;           // +8 (copies get 6 bytes)
    bool flag;               // +0xc
    RegisterModifier* m1;    // +0x10
    RegisterModifier* m2;    // +0x14
    JET_HEAP_OBJECT
};
JET_LAYOUT32(sizeof(RegisterItem) == 0x18);

using RegisterList = Array<RegisterItem*, 128>;

// an instruction: its operands (an array it owns, deleted with them). The constructors take the array as is.
struct Instruction {
    explicit Instruction(RegisterList* operands);
    Instruction(const Instruction& o);
    Instruction& operator=(const Instruction& o);
    virtual ~Instruction();

    RegisterList* operands;  // +4
    JET_HEAP_OBJECT
};

struct VertexInstruction : Instruction {
    VertexInstruction(VertexOpcode op, RegisterList* operands);
    VertexInstruction(const VertexInstruction& o);
    VertexInstruction& operator=(const VertexInstruction& o);
    ~VertexInstruction() override;

    VertexOpcode opcode;  // +8
};

struct TextureInstruction : Instruction {
    TextureInstruction(TextureOpcode op, RegisterList* operands);
    TextureInstruction(const TextureInstruction& o);
    TextureInstruction& operator=(const TextureInstruction& o);
    ~TextureInstruction() override;

    TextureOpcode opcode;  // +8
};

// a #define handed to a program: name and value
struct PreProcessorDefine {
    PreProcessorDefine();
    PreProcessorDefine(const AnsiString& name, const AnsiString& value);
    PreProcessorDefine(const PreProcessorDefine& o);
    ~PreProcessorDefine();
    PreProcessorDefine& operator=(const PreProcessorDefine& o);

    AnsiString name, value;
};

struct PreProcessorArray : Array<PreProcessorDefine, 3> {
    PreProcessorArray();
    PreProcessorArray(const PreProcessorArray& o);
    ~PreProcessorArray();
    PreProcessorArray& operator=(const PreProcessorArray& o);

    AnsiString __cdecl ConcatenateStrings() const;  // ":name:value" for each define
};

// ---- U2b (RenderSystem_7c/ExtendedGfxLanguage.cpp)

class Display;
class ResourceDatabase;

// what a program's symbol name stands for (GetSymbolId); `local` symbols are constants numbered by `number`
struct GfxSymbolId {
    bool local;  // +0 (GetLocalConstantNumber wants it false)
    int number;  // +4
    int extra;   // +8
};

// the base of the vertex programs and texture combiners (0x3c bytes with its virtual ReferenceCount at +0x34):
// the source text, names, a generation, the symbol names it declares, a user value and a fallback program.
// LoadProgram/LoadFromFile run the defines over the text and hand it to the derived class's Load.
class ExtendedGfxLanguage : public virtual ReferenceCount {
public:
    struct SymbolName {  // 0x10
        PString name;    // +0
        GfxSymbolId id;  // +4
    };

    virtual bool __cdecl SupportedByPlatform(float generation);  // slot 0: false
    virtual bool __cdecl LoadProgram(const PString& name, const char* source, const PreProcessorArray* defines);
    virtual bool __cdecl LoadFromFile(const ResourceDatabase* db, const PString& name, const PString& file,
                                      const PreProcessorArray* defines);
    virtual bool __cdecl LoadNativeFormat(const PString& name, const char* source);  // true
    virtual bool __cdecl CompileProgram();                                           // true
    virtual bool __cdecl Bind() const;                                               // false
    virtual bool __cdecl End() const;                                                // false
    // slot 7: does the program's API define `name` (#if in the pre-parser)? false in every class seen here
    virtual bool __cdecl Defines(const PString& name) const;

    Display* __cdecl GetDisplay() const;
    const AnsiString& __cdecl GetProgramString() const;
    const PString& __cdecl GetFilename() const;
    const PString& __cdecl GetProgramName() const;
    const PString& __cdecl GetRegisteredName() const;
    void __cdecl SetFilename(const PString& f);  // a trailing ".txt" is cut off
    void __cdecl SetProgramName(const PString& n);
    void __cdecl SetRegisteredName(const PString& n);
    float __cdecl GetGeneration() const;
    void __cdecl SetGeneration(float g);
    unsigned long __cdecl GetUserData() const;
    void __cdecl SetUserData(unsigned long d) const;
    const ExtendedGfxLanguage* __cdecl GetFallbackProgram() const;
    void __cdecl SetFallbackProgram(const ExtendedGfxLanguage* p) const;  // referenced
    bool __cdecl GetSymbolId(const PString& name, GfxSymbolId& id) const;
    int __cdecl GetLocalConstantNumber(const PString& name) const;  // -1 when not a local symbol
    const Array<SymbolName, 3>& GetSymbolNameList() const;

    JET_HEAP_OBJECT

protected:
    explicit ExtendedGfxLanguage(Display* display);
    virtual ~ExtendedGfxLanguage();

    virtual bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName);  // slot 8: false

    void __cdecl InitInstance(ExtendedGfxLanguage& copy) const;  // copies everything into `copy`
    void __cdecl ProcessDefines(AnsiString& text, const PreProcessorArray* defines) const;
    void __cdecl AddSymbolName(const PString& name, int number, bool local, int extra);

    Display* display;                             // +8
    AnsiString source;                            // +0xc
    PString filename;                             // +0x10
    PString programName;                          // +0x14
    PString registeredName;                       // +0x18
    float generation;                             // +0x1c (1)
    Array<SymbolName, 3> symbolNames;             // +0x20
    mutable unsigned long userData;               // +0x2c
    mutable const ExtendedGfxLanguage* fallback;  // +0x30
};
JET_LAYOUT32(sizeof(ExtendedGfxLanguage) == 0x3c);

// a singly linked list of parse records through each item's `next` (0xc)
template <class T>
struct ParseList {
    T* head;
    T* tail;
    int count;
    JET_HEAP_OBJECT
};

// ---- U3a (RenderSystem_7c/VertexProgram.cpp, TextureCombiner.cpp)

class TextureCombinerManager;
class VertexProgramManager;
class VertexType;

using DisplayLightList = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

// A texture combiner (pixel program) of the API modules (0x6c bytes with its virtual ReferenceCount at +0x64): the
// constants its program declares (by register, 16 at most), the bump-environment settings per stage and its
// manager. The API module's class fills the slots that answer 0 here.
class TextureCombiner : public ExtendedGfxLanguage {
public:
    enum TextureOpcode {};
    enum TextureAddressType {};
    enum RegType {};
    enum ModifierType {};

    struct ModifierItem {  // 8
        explicit ModifierItem(ModifierType t);
        ModifierItem& operator=(const ModifierItem& o);
        ModifierType type;
        ModifierItem* next;
        JET_HEAP_OBJECT
    };

    // an operand (0x14): the register and two lists of modifiers it owns
    struct RegisterItem {
        RegisterItem(RegType type, int index);
        explicit RegisterItem(const RegisterItem* o);  // copies both lists, not `next`
        ~RegisterItem();
        RegisterItem& operator=(const RegisterItem& o);  // the five fields as they are

        RegType type;                     // +0
        int index;                        // +4
        ParseList<ModifierItem>* mods1;   // +8
        ParseList<ModifierItem>* mods2;   // +0xc
        RegisterItem* next;               // +0x10
        JET_HEAP_OBJECT
    };

    struct Instruction {  // 0x2c
        Instruction(TextureOpcode op, ParseList<RegisterItem>* operands, ParseList<ModifierItem>* mods,
                    TextureAddressType address, float value, bool flag);
        Instruction(const Instruction& o);           // shallow: the same lists
        explicit Instruction(const Instruction* o);  // deep copy of both lists; +0x18..+0x24 left as they are
        Instruction& operator=(const Instruction& o);
        virtual ~Instruction();

        TextureOpcode opcode;                    // +4
        ParseList<RegisterItem>* operands;       // +8
        ParseList<ModifierItem>* mods;           // +0xc
        TextureAddressType address;              // +0x10
        bool flag;                               // +0x14
        int field18, field1c, field20, field24;  // +0x18 (only the copy constructor and operator= touch them)
        float value;                             // +0x28
        JET_HEAP_OBJECT
    };

    struct Constant {  // a tracked constant register (0x14)
        int index;
        Vector4 value;
    };
    struct BumpEnv {  // a stage's bump scale and bias (0x14; the last two are never set)
        int stage;
        float scale, bias, unused0c, unused10;
    };
    struct BumpMatrix {  // 0x14
        int stage;
        Vector4 matrix;
    };

    static bool __cdecl SupportedByPlatform(Display* display, float generation);
    bool __cdecl SupportedByPlatform(float generation) override;  // the display's capability 0x15, asked once

    virtual void __cdecl SetCombinerManager(TextureCombinerManager* m);  // 9
    virtual TextureCombinerManager* __cdecl GetCombinerManager();         // 10
    virtual bool __cdecl AddInstruction(const Instruction* i);           // 11 (the program parser's lines): false
    virtual bool __cdecl SetConstantRegister(int index, const Vector4& v);  // 12: false when not tracked
    // 13: a new constant register (index < 16, not tracked yet) and its symbol name when it has one
    virtual bool __cdecl TrackConstant(int index, const AnsiString& name, const Vector4& v);
    virtual void __cdecl SetBumpEnvScale(int stage, float scale, bool apply);           // 14
    virtual void __cdecl SetBumpEnvBias(int stage, float bias, bool apply);             // 15
    virtual void __cdecl SetBumpEnvMatrix(int stage, const Vector4& matrix, bool apply);  // 16
    virtual int __cdecl GetApi() const;  // 17: handed to the program parser (0 here)

    bool __cdecl SetConstantRegister(int index, const Color& c);
    bool __cdecl SetConstantRegister(int index, const Vector3& v);  // w = 1

protected:
    explicit TextureCombiner(Display* display);
    ~TextureCombiner() override;

    bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName) override;

    Array<Constant*, 128> constants;       // +0x34
    Constant** constantTable;              // +0x40 (16 slots, by register)
    Array<BumpEnv*, 128> bumpEnv;          // +0x44
    Array<BumpMatrix*, 128> bumpMatrices;  // +0x50
    bool bumpChanged;                      // +0x5c
    TextureCombinerManager* manager;       // +0x60

    static __declspec(dllimport) bool supportChecked;  // the original's until U9
    static __declspec(dllimport) bool supported;
};
JET_LAYOUT32(sizeof(TextureCombiner) == 0x6c);

// A vertex program of the API modules (0x50 bytes with its virtual ReferenceCount at +0x48): its vertex type, the
// registers where skinning matrices go and its manager. Load runs the pre-parser, then the vertex program parser
// over its output. The slots that answer 0 here are the API module's.
class VertexProgram : public ExtendedGfxLanguage {
public:
    // a constant register the program asked for (+0 register, +4 kind: 4 is a render state with its id at +8;
    // lights by index +0x10 among those of type +0x1c)
    struct ConstantData {
        int reg;         // +0
        int kind;        // +4
        int state;       // +8
        int field0c;     // +0xc
        int lightIndex;  // +0x10
        int field14, field18;
        int lightType;   // +0x1c
    };
    struct MatrixMap {
        const Matrix4* matrix;  // +0
    };

    static bool __cdecl SupportedByPlatform(Display* display, float generation);  // generations 1..2, asked once
    bool __cdecl SupportedByPlatform(float generation) override;
    bool __cdecl LoadNativeFormat(const PString& name, const char* source) override;

    virtual int __cdecl Slot9();                                         // 0
    virtual void __cdecl SetProgramManager(VertexProgramManager* m);     // 10
    virtual VertexProgramManager* __cdecl GetProgramManager();           // 11
    virtual int __cdecl Slot12();                                        // the API module's: 0
    virtual int __cdecl Slot13();
    virtual int __cdecl Slot14();
    virtual int __cdecl Slot15();
    virtual bool __cdecl RestoreProgram();                               // 16: true
    virtual void __cdecl SetProgramState(const DisplayLightList& lights, Color ambient,
                                         const DisplayMaterial& material, const Matrix4& view) const;  // 17
    virtual void __cdecl LoadInfluences(const Array<MatrixMap*, 128>& matrices) const;  // 18
    virtual void __cdecl LoadConstantRegisters() const;                  // 19
    virtual int __cdecl Slot20();
    virtual int __cdecl Slot21();
    virtual int __cdecl Slot22();
    virtual bool __cdecl SetLocalConstant(int reg, const Vector4& v) const;        // 23
    virtual bool __cdecl SetLocalConstantDirect(int reg, const Vector4& v) const;  // 24
    virtual int __cdecl Slot25();
    virtual bool __cdecl SetLocalConstants(int reg, const Vector4* v, int count) const;  // 26
    virtual bool __cdecl SetEnvConstant(int reg, const Vector4& v) const;          // 27
    virtual bool __cdecl SetVertexType(VertexType* type);                // 28: false when unchanged
    virtual VertexType* __cdecl GetVertexType() const;                   // 29
    virtual int __cdecl GetMaxLocalRegisters() const;                    // 30
    virtual int __cdecl GetMaxEnvRegisters() const;                      // 31
    virtual int __cdecl GetMaxTotalRegisters() const;                    // 32
    virtual int __cdecl GetApi() const;                                  // 33: handed to the pre-parser
    virtual bool __cdecl LoadNativeProgram(AnsiString& text);           // 34

    // the display's limits, read once from a program it makes
    static int __cdecl GetMaxLocalRegisters(Display* display);
    static int __cdecl GetMaxEnvRegisters(Display* display);
    static int __cdecl GetMaxTotalRegisters(Display* display);

    bool __cdecl SetLocalConstant(int reg, const Vector3& v) const;  // w = 1
    bool __cdecl SetLocalConstant(int reg, const Color& c) const;
    bool __cdecl SetLocalConstantDirect(int reg, const Vector3& v) const;
    bool __cdecl SetLocalConstantDirect(int reg, const Color& c) const;
    bool __cdecl SetEnvConstant(int reg, const Vector3& v) const;
    bool __cdecl SetEnvConstant(int reg, const Color& c) const;
    int __cdecl GetInfluenceBaseRegister() const;
    int __cdecl GetInfluenceMaxRegister() const;

protected:
    VertexProgram(Display* display, VertexType* type);
    ~VertexProgram() override;

    bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName) override;
    void __cdecl InitInstance(VertexProgram& copy) const;
    // the render-state constants (material, lights by type, fog...) through SetLocalConstantDirect
    void __cdecl LoadCommonStates(const Array<ConstantData*, 3>& constants, const DisplayLightList& lights,
                                  Color ambient, const DisplayMaterial& material, const Matrix4& view) const;
    static void __cdecl GetMaxRegisters(Display* display);

    VertexType* vertexType;          // +0x34 (referenced)
    int influenceBase;               // +0x38 (-1: none)
    int influenceMax;                // +0x3c
    bool field40;                    // +0x40
    VertexProgramManager* manager;   // +0x44

    static __declspec(dllimport) int maxLocalRegs;  // the original's until U9
    static __declspec(dllimport) int maxEnvRegs;
    static __declspec(dllimport) int maxTotalRegs;
    static __declspec(dllimport) bool supportChecked[4];  // by generation - 1
    static __declspec(dllimport) bool supported[4];
};
JET_LAYOUT32(sizeof(VertexProgram) == 0x50);

}  // namespace Jet
