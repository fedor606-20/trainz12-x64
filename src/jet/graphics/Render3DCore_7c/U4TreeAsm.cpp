// U4 wave 4: exact transcriptions of the CacheBucket tree functions (0x10001190, 0x10002880, 0x10002900).
// The original's Insert is a top-down insert with inline rotations and its own head-as-root convention; C++ could
// not reproduce the topology for the differential test, so the instruction sequence is kept here.
//
// All three are __cdecl members (this is the first stack argument), as the mangling says.
#include <cstddef>

struct Rec;

extern "C" void __cdecl U4TreeSplitTree(void* bucket, Rec* p1, Rec* p2, Rec* p3, Rec* p4, Rec* p5);

// 0x10002900
extern "C" __declspec(naked) void* __cdecl U4TreeRotateTree(void* bucket, void* a, void* b)
{
    __asm {
        mov edx, [esp + 0xc]
        movss xmm1, dword ptr [edx + 0x20]
        push esi
        push edi
        mov edi, [esp + 0x10]
        movss xmm0, dword ptr [edi + 0x20]
        comiss xmm1, xmm0
        jbe done_left
        mov ecx, dword ptr [edx + 0x24]
        jmp have_child
    done_left:
        mov ecx, dword ptr [edx + 0x28]
    have_child:
        movss xmm1, dword ptr [ecx + 0x20]
        comiss xmm1, xmm0
        jbe child_right
        mov eax, dword ptr [ecx + 0x24]
        mov esi, dword ptr [eax + 0x28]
        mov dword ptr [ecx + 0x24], esi
        mov dword ptr [esi + 0x2c], ecx
        mov dword ptr [eax + 0x28], ecx
        jmp link_up
    child_right:
        mov eax, dword ptr [ecx + 0x28]
        mov esi, dword ptr [eax + 0x24]
        mov dword ptr [ecx + 0x28], esi
        mov dword ptr [esi + 0x2c], ecx
        mov dword ptr [eax + 0x24], ecx
    link_up:
        mov dword ptr [ecx + 0x2c], eax
        movss xmm0, dword ptr [edx + 0x20]
        comiss xmm0, dword ptr [edi + 0x20]
        pop edi
        pop esi
        jbe link_left
        mov dword ptr [edx + 0x24], eax
        mov dword ptr [eax + 0x2c], edx
        ret
    link_left:
        mov dword ptr [edx + 0x28], eax
        mov dword ptr [eax + 0x2c], edx
        ret
    }
}

// 0x10002880
extern "C" __declspec(naked) void __cdecl U4TreeSplitTree(void* bucket, Rec* p1, Rec* p2, Rec* p3, Rec* p4,
                                                          Rec* p5)
{
    __asm {
        mov eax, [esp + 0x18]
        mov ecx, dword ptr [eax + 0x24]
        mov edx, 1
        or byte ptr [eax + 0x30], dl
        and byte ptr [ecx + 0x30], 0xfe
        mov eax, dword ptr [eax + 0x28]
        mov ecx, [esp + 0x14]
        and byte ptr [eax + 0x30], 0xfe
        test byte ptr [ecx + 0x30], dl
        jz split_done
        mov eax, [esp + 0x10]
        or byte ptr [eax + 0x30], dl
        movss xmm1, dword ptr [eax + 0x20]
        push ebx
        push esi
        push edi
        mov edi, [esp + 0x14]
        movss xmm0, dword ptr [edi + 0x20]
        comiss xmm1, xmm0
        jbe first_ge
        mov ebx, edx
        jmp first_done
    first_ge:
        xor ebx, ebx
    first_done:
        movss xmm1, dword ptr [ecx + 0x20]
        comiss xmm1, xmm0
        jbe second_ge
        mov ecx, edx
        jmp second_done
    second_ge:
        xor ecx, ecx
    second_done:
        cmp bl, cl
        mov esi, [esp + 0x10]
        jz same_side
        push eax
        push edi
        push esi
        call U4TreeRotateTree
        add esp, 0xc
    same_side:
        mov eax, [esp + 0x18]
        push eax
        push edi
        push esi
        call U4TreeRotateTree
        add esp, 0xc
        and byte ptr [eax + 0x30], 0xfe
        pop edi
        pop esi
        pop ebx
    split_done:
        ret
    }
}

// 0x10001190
extern "C" __declspec(naked) void* __cdecl U4TreeInsert(void* bucket, void* record)
{
    __asm {
        push ecx
        push ebx
        mov ebx, [esp + 0x10]
        push ebp
        mov ebp, [esp + 0x10]
        mov edx, dword ptr [ebp + 0x4]
        cmp edx, dword ptr [ebp + 0x8]
        push esi
        push edi
        mov edi, edx
        mov eax, edx
        mov esi, edx
        jz insert_link
    insert_loop:
        movss xmm0, dword ptr [esi + 0x20]
        comiss xmm0, dword ptr [ebx + 0x20]
        mov edx, edi
        mov edi, eax
        lea eax, [esi + 0x20]
        mov dword ptr [esp + 0x1c], esi
        mov dword ptr [esp + 0x10], eax
        jbe descend_right
        mov esi, dword ptr [esi + 0x24]
        jmp descend_done
    descend_right:
        mov esi, dword ptr [esi + 0x28]
    descend_done:
        mov eax, dword ptr [esi + 0x24]
        test byte ptr [eax + 0x30], 1
        jz next_iteration
        mov ecx, dword ptr [esi + 0x28]
        test byte ptr [ecx + 0x30], 1
        jz next_iteration
        or byte ptr [esi + 0x30], 1
        and byte ptr [eax + 0x30], 0xfe
        mov eax, dword ptr [esi + 0x28]
        and byte ptr [eax + 0x30], 0xfe
        mov eax, [esp + 0x1c]
        test byte ptr [eax + 0x30], 1
        jz next_iteration
        or byte ptr [edi + 0x30], 1
        movss xmm0, dword ptr [ebx + 0x20]
        movss xmm1, dword ptr [edi + 0x20]
        comiss xmm1, xmm0
        jbe side1_zero
        mov ecx, 1
        jmp side1_done
    side1_zero:
        xor ecx, ecx
    side1_done:
        mov eax, [esp + 0x10]
        movss xmm2, dword ptr [eax]
        comiss xmm2, xmm0
        jbe side2_zero
        mov eax, 1
        jmp side2_done
    side2_zero:
        xor eax, eax
    side2_done:
        cmp cl, al
        jz rotate_edx
        comiss xmm1, xmm0
        jbe edi_right
        mov eax, dword ptr [edi + 0x24]
        jmp edi_child
    edi_right:
        mov eax, dword ptr [edi + 0x28]
    edi_child:
        movss xmm1, dword ptr [eax + 0x20]
        comiss xmm1, xmm0
        jbe edi_child_right
        mov ecx, dword ptr [eax + 0x24]
        mov ebp, dword ptr [ecx + 0x28]
        mov dword ptr [eax + 0x24], ebp
        mov dword ptr [ebp + 0x2c], eax
        mov dword ptr [ecx + 0x28], eax
        jmp edi_link
    edi_child_right:
        mov ecx, dword ptr [eax + 0x28]
        mov ebp, dword ptr [ecx + 0x24]
        mov dword ptr [eax + 0x28], ebp
        mov dword ptr [ebp + 0x2c], eax
        mov dword ptr [ecx + 0x24], eax
    edi_link:
        mov dword ptr [eax + 0x2c], ecx
        movss xmm0, dword ptr [edi + 0x20]
        comiss xmm0, dword ptr [ebx + 0x20]
        jbe edi_link_right
        mov dword ptr [edi + 0x24], ecx
        jmp edi_link_done
    edi_link_right:
        mov dword ptr [edi + 0x28], ecx
    edi_link_done:
        mov dword ptr [ecx + 0x2c], edi
    rotate_edx:
        movss xmm0, dword ptr [ebx + 0x20]
        movss xmm1, dword ptr [edx + 0x20]
        comiss xmm1, xmm0
        jbe edx_right
        mov ecx, dword ptr [edx + 0x24]
        jmp edx_child
    edx_right:
        mov ecx, dword ptr [edx + 0x28]
    edx_child:
        movss xmm1, dword ptr [ecx + 0x20]
        comiss xmm1, xmm0
        jbe edx_child_right
        mov eax, dword ptr [ecx + 0x24]
        mov ebp, dword ptr [eax + 0x28]
        mov dword ptr [ecx + 0x24], ebp
        mov dword ptr [ebp + 0x2c], ecx
        mov dword ptr [eax + 0x28], ecx
        jmp edx_link
    edx_child_right:
        mov eax, dword ptr [ecx + 0x28]
        mov ebp, dword ptr [eax + 0x24]
        mov dword ptr [ecx + 0x28], ebp
        mov dword ptr [ebp + 0x2c], ecx
        mov dword ptr [eax + 0x24], ecx
    edx_link:
        mov dword ptr [ecx + 0x2c], eax
        movss xmm0, dword ptr [edx + 0x20]
        comiss xmm0, dword ptr [ebx + 0x20]
        jbe edx_link_right
        mov dword ptr [edx + 0x24], eax
        jmp edx_link_done
    edx_link_right:
        mov dword ptr [edx + 0x28], eax
    edx_link_done:
        and byte ptr [eax + 0x30], 0xfe
        mov ebp, [esp + 0x18]
        mov dword ptr [eax + 0x2c], edx
    next_iteration:
        cmp esi, dword ptr [ebp + 0x8]
        mov eax, [esp + 0x1c]
        jnz insert_loop
    insert_link:
        mov dword ptr [ebx + 0x2c], eax
        mov ecx, dword ptr [ebp + 0x8]
        mov dword ptr [ebx + 0x24], ecx
        mov ecx, dword ptr [ebp + 0x8]
        mov dword ptr [ebx + 0x28], ecx
        movss xmm0, dword ptr [eax + 0x20]
        comiss xmm0, dword ptr [ebx + 0x20]
        jbe insert_right
        mov dword ptr [eax + 0x24], ebx
        jmp insert_linked
    insert_right:
        mov dword ptr [eax + 0x28], ebx
    insert_linked:
        push ebx
        push eax
        push edi
        push edx
        push ebx
        push ebp
        call U4TreeSplitTree
        mov edx, dword ptr [ebp + 0x4]
        mov eax, dword ptr [edx + 0x28]
        and byte ptr [eax + 0x30], 0xfe
        add esp, 0x18
        add dword ptr [ebp], 1
        pop edi
        pop esi
        pop ebp
        mov eax, ebx
        pop ebx
        pop ecx
        ret
    }
}
