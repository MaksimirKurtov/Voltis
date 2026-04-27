#include "abi.h"

#include <utility>

namespace {

CallingConventionDescriptor msX64() {
    return CallingConventionDescriptor{
        "ms_x64_abi",
        {"rcx", "rdx", "r8", "r9"},
        {"xmm0", "xmm1", "xmm2", "xmm3"},
        "rax",
        "xmm0",
        {"rbx", "rbp", "rdi", "rsi", "r12", "r13", "r14", "r15", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"},
        {"rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"},
        16,
        true
    };
}

CallingConventionDescriptor sysvAmd64() {
    return CallingConventionDescriptor{
        "sysv_amd64",
        {"rdi", "rsi", "rdx", "rcx", "r8", "r9"},
        {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"},
        "rax",
        "xmm0",
        {"rbx", "rbp", "r12", "r13", "r14", "r15"},
        {"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"},
        16,
        false
    };
}

CallingConventionDescriptor msX86() {
    return CallingConventionDescriptor{
        "__stdcall/__fastcall/cdecl",
        {"ecx", "edx"},
        {},
        "eax",
        "st0",
        {"ebx", "esi", "edi", "ebp"},
        {"eax", "ecx", "edx"},
        16,
        false
    };
}

CallingConventionDescriptor sysvI386() {
    return CallingConventionDescriptor{
        "sysv_i386",
        {},
        {},
        "eax",
        "st0",
        {"ebx", "esi", "edi", "ebp"},
        {"eax", "ecx", "edx"},
        16,
        false
    };
}

CallingConventionDescriptor aapcs64() {
    return CallingConventionDescriptor{
        "aapcs64",
        {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"},
        {"v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7"},
        "x0",
        "v0",
        {"x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15"},
        {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7"},
        16,
        false
    };
}

CallingConventionDescriptor aapcs32() {
    return CallingConventionDescriptor{
        "aapcs",
        {"r0", "r1", "r2", "r3"},
        {"s0", "s1", "s2", "s3"},
        "r0",
        "s0",
        {"r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "sp"},
        {"r0", "r1", "r2", "r3", "r12", "lr"},
        8,
        false
    };
}

CallingConventionDescriptor riscvLp64() {
    return CallingConventionDescriptor{
        "riscv_lp64",
        {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"},
        {"fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7"},
        "a0",
        "fa0",
        {"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "sp"},
        {"ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3", "t4", "t5", "t6"},
        16,
        false
    };
}

CallingConventionDescriptor riscvIlp32() {
    CallingConventionDescriptor cc = riscvLp64();
    cc.name = "riscv_ilp32";
    cc.stackAlignment = 16;
    return cc;
}

CallingConventionDescriptor mipsO32() {
    return CallingConventionDescriptor{
        "mips_o32",
        {"$a0", "$a1", "$a2", "$a3"},
        {"$f12", "$f14"},
        "$v0",
        "$f0",
        {"$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7", "$sp", "$fp", "$ra"},
        {"$v0", "$v1", "$a0", "$a1", "$a2", "$a3", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9"},
        8,
        false
    };
}

CallingConventionDescriptor avrAbi() {
    return CallingConventionDescriptor{
        "avr_abi",
        {"r24", "r22", "r20", "r18"},
        {},
        "r24",
        "",
        {"r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r28", "r29"},
        {"r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r30", "r31"},
        2,
        false
    };
}

CallingConventionDescriptor ppc64ElfV2() {
    return CallingConventionDescriptor{
        "ppc64_elfv2",
        {"r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10"},
        {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12", "f13"},
        "r3",
        "f1",
        {"r1", "r2", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"},
        {"r0", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12"},
        16,
        false
    };
}

CallingConventionDescriptor s390xElfAbi() {
    return CallingConventionDescriptor{
        "s390x_elf",
        {"r2", "r3", "r4", "r5", "r6"},
        {"f0", "f2", "f4", "f6"},
        "r2",
        "f0",
        {"r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"},
        {"r0", "r1", "r2", "r3", "r4", "r5", "r14"},
        8,
        false
    };
}

CallingConventionDescriptor xtensaCall0() {
    return CallingConventionDescriptor{
        "xtensa_call0",
        {"a2", "a3", "a4", "a5", "a6", "a7"},
        {},
        "a2",
        "",
        {"a12", "a13", "a14", "a15", "a1"},
        {"a0", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10", "a11"},
        16,
        false
    };
}

} // namespace

std::optional<CallingConventionDescriptor> callingConventionForTarget(const TargetDescription& target) {
    switch (target.triple.arch) {
        case TargetArch::X64:
            if (target.triple.os == TargetOs::Windows) {
                return msX64();
            }
            return sysvAmd64();
        case TargetArch::X86:
            if (target.triple.os == TargetOs::Windows) {
                return msX86();
            }
            return sysvI386();
        case TargetArch::Arm64:
            return aapcs64();
        case TargetArch::Armv7:
        case TargetArch::Armv6:
            return aapcs32();
        case TargetArch::Riscv32:
            return riscvIlp32();
        case TargetArch::Riscv64:
            return riscvLp64();
        case TargetArch::Mips:
        case TargetArch::Mips64:
            return mipsO32();
        case TargetArch::PowerPc64:
            return ppc64ElfV2();
        case TargetArch::S390x:
            return s390xElfAbi();
        case TargetArch::Xtensa:
            return xtensaCall0();
        case TargetArch::Avr:
            return avrAbi();
        case TargetArch::Unknown:
            return std::nullopt;
    }
    return std::nullopt;
}
