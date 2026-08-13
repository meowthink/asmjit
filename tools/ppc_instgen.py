#!/usr/bin/env python3
"""Generates asmjit/ppc/ppcinst.h, asmjit/ppc/ppcinstdb_p.h, and
asmjit/ppc/ppcemitter.h from the PPC64 assembler method list.

Every `ASMJIT_API Error <name>(...)` declared in ppcassembler.h becomes an
instruction id `kId<Name>` (except the non-instruction helpers listed below).
The instruction database records the operand signature of each instruction
for assembler validation, and the emitter header provides the instruction
methods shared by the compiler (and future builder). Only instructions that
have a case in the assembler's generic `emitInst()` switch are emitted, so the
compiler surface stays in sync with what generic emission can serialize.
Run from the repository root:

  python3 tools/ppc_instgen.py
"""

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "asmjit/ppc/ppcassembler.h")
ASSEMBLER_CPP = os.path.join(ROOT, "asmjit/ppc/ppcassembler.cpp")
OUTPUT = os.path.join(ROOT, "asmjit/ppc/ppcinst.h")
OUTPUT_DB = os.path.join(ROOT, "asmjit/ppc/ppcinstdb_p.h")
OUTPUT_EMITTER = os.path.join(ROOT, "asmjit/ppc/ppcemitter.h")
OUTPUT_EMITINST = os.path.join(ROOT, "asmjit/ppc/ppc_emitinst.inc")

# Non-instruction helpers and BaseAssembler overrides that must not become
# instruction ids.
EXCLUDED = {
    "bind", "on_attach", "on_detach", "emit32", "align", "embed", "_emit",
    "embed_label", "embed_label_delta",
    "prolog", "epilog", "call", "callDescriptor", "callHelper", "tailCall",
    "tailCallDescriptor", "bLong", "bailIfFPSpecial", "loadImm64",
}

# Parameter type -> operand kind.
KIND_BY_TYPE = {
    "Gp": "kGp",
    "Fp": "kFp",
    "Vr": "kVec",
    "Vsx": "kVec",
    "const Mem&": "kMem",
    "const Label&": "kLabel",
    "int16_t": "kImm",
    "uint16_t": "kImm",
    "uint8_t": "kImm",
    "int32_t": "kImm",
    "uint32_t": "kImm",
    "int64_t": "kImm",
    "uint64_t": "kImm",
    "bool": "kSkip",  # Trailing Rc/Oe flags, not exposed through generic emit().
}


def parse_params(args):
    """Returns (required, total, kinds) for an argument list."""
    required = 0
    total = 0
    kinds = []
    if not args.strip():
        return 0, 0, kinds
    for param in args.split(","):
        param = param.strip()
        has_default = "=" in param
        type_name = param.split("=")[0].strip().rsplit(" ", 1)[0]
        kind = KIND_BY_TYPE.get(type_name)
        if kind is None:
            raise SystemExit(f"unknown parameter type '{type_name}' in '{param}'")
        if kind == "kSkip":
            continue
        kinds.append(kind)
        total += 1
        if not has_default:
            required += 1
    return required, total, kinds


# Curated per-instruction feature requirements (PowerISA opcode-map v-tags).
FEATURE_BY_NAME = {
    # ISA 3.0 (POWER9) additions.
    "modsd": "kISA_3_0", "modsw": "kISA_3_0", "modud": "kISA_3_0", "moduw": "kISA_3_0",
    "cnttzd": "kISA_3_0", "cnttzw": "kISA_3_0", "darn": "kISA_3_0", "scv": "kISA_3_0",
    "extswsli": "kISA_3_0", "wait": "kISA_3_0", "maddhd": "kISA_3_0", "maddhdu": "kISA_3_0",
    "maddld": "kISA_3_0", "cmprb": "kISA_3_0", "setb": "kISA_3_0",
    "vclzlsbb": "kISA_3_0", "vctzlsbb": "kISA_3_0",
    "vextublx": "kISA_3_0", "vextubrx": "kISA_3_0", "vextuhlx": "kISA_3_0", "vextuhrx": "kISA_3_0",
    "vextuwlx": "kISA_3_0", "vextuwrx": "kISA_3_0",
    "vabsdub": "kISA_3_0", "vabsduh": "kISA_3_0", "vabsduw": "kISA_3_0",
    "vnegd": "kISA_3_0", "vnegw": "kISA_3_0",
    "vmul10cuq": "kISA_3_0", "vmul10ecuq": "kISA_3_0", "vmul10euq": "kISA_3_0", "vmul10uq": "kISA_3_0",
    "vbpermd": "kISA_3_0",
    # VSX loads and stores (DS/DQ/X forms).
    "lxsd": "kISA_3_0", "lxssp": "kISA_3_0", "lxv": "kISA_3_0", "lxvx": "kISA_3_0",
    "stxsd": "kISA_3_0", "stxssp": "kISA_3_0", "stxv": "kISA_3_0", "stxvx": "kISA_3_0",
    "lxsdx": "kISA_2_06", "stxsdx": "kISA_2_06",
    "lxsspx": "kISA_2_07", "stxsspx": "kISA_2_07",
    "lxsiwzx": "kISA_2_07", "lxsiwax": "kISA_2_07", "stxsiwx": "kISA_2_07",
}


def feature_of(name):
    if name in FEATURE_BY_NAME:
        return FEATURE_BY_NAME[name]
    if name.startswith("v"):
        return "kAltivec"  # VMX.
    if name.startswith(("xs", "xv", "xx")):
        return "kVSX"
    return "kNone"


# Instructions that never write a register operand. This covers compares (the
# result goes to a CR field), traps, branches, moves-to-special-registers, CR
# field operations, and barriers / cache-maintenance instructions. Note that
# `cmpb` is NOT in this set: it writes its first GPR operand. Likewise
# `xscmpeqdp`/`xscmpgedp`/`xscmpgtdp` are NOT in this set: since Power ISA 3.0
# they write an all-ones/all-zeros mask to XT (only `xscmpodp`/`xscmpudp` and
# the `xstdivdp`/`xstsqrtdp` tests still write a CR field).
RW_NO_WRITE = {
    # Compares.
    "cmpd", "cmpld", "cmpdi", "cmp", "cmpl", "cmpi", "cmpli", "cmpldi",
    "cmpeqb", "cmprb",
    "fcmpu", "fcmpo", "ftdiv", "ftsqrt",
    "xscmpudp", "xscmpodp", "xstdivdp", "xstsqrtdp",
    # Traps.
    "tw", "twi", "td", "tdi",
    # Branches.
    "b", "beq", "bne", "blt", "bge", "bgt", "ble", "bc", "bcl", "bclr",
    "bclrl", "bcctr", "bcctrl", "bctr", "bctrl", "bl", "blr",
    # Moves to special registers (the source GPR is only read).
    "mtctr", "mtlr", "mtcrf", "mtocrf", "mtspr", "mtxer",
    "mtfsf", "mtfsb0", "mtfsb1", "mtfsfi",
    # CR field operations (fields are encoded as immediates).
    "mcrf", "mcrfs", "mcrxrx",
    "crand", "crandc", "crnor", "creqv", "crnand", "cror", "crorc", "crxor",
    "crclr", "crset", "crmove", "crnot",
    # Barriers and cache-maintenance instructions.
    "sync", "lwsync", "isync", "eieio", "wait", "sc", "scv",
    "dcbf", "dcbst", "dcbt", "dcbtst", "dcbz", "icbi", "icbt",
}


def rw_class_of(name):
    """Returns the read/write behavior class of an instruction name.

    PPC instructions are regular: the destination register comes first and
    everything else is read. The exceptions are stores (the first register is
    only read and the memory operand is written), update-form loads/stores
    (the memory base register is also written), and instructions that never
    write a register (compares, branches, traps, ...).
    """
    if name in RW_NO_WRITE:
        return "kNoWrite"

    if name.startswith("st"):
        return "kStoreUpdate" if (name.endswith("u") or name.endswith("ux")) else "kStore"

    if name.startswith("l") and (name.endswith("u") or name.endswith("ux")):
        return "kLoadUpdate"

    return "kDefault"


# ---------------------------------------------------------------------------
# Generic emit (Assembler::_emit) generation.
#
# This section is self-contained: it only needs the assembler method list, so
# it can be kept (and was originally introduced) as an independent block.
# ---------------------------------------------------------------------------

#! Instructions whose VSX register operands also accept FPR operands (both
#! are valid VSRs, and the register allocator mirrors FPR bits to GPRs with
#! `mfvsrd`).
EMIT_VEC_OR_FP = {"mfvsrd", "mfvsrld", "mfvsrwz"}

EMIT_OP_CHECK = {
    "Gp": "opGp(ops[{i}])",
    "Fp": "opFp(ops[{i}])",
    "Vr": "opVec(ops[{i}])",
    "Vsx": "opVec(ops[{i}])",
    "const Mem&": "opMem(ops[{i}])",
    "const Label&": "opLabel(ops[{i}])",
    "int16_t": "opImm(ops[{i}])",
    "uint16_t": "opImm(ops[{i}])",
    "uint8_t": "opImm(ops[{i}])",
    "int32_t": "opImm(ops[{i}])",
    "uint32_t": "opImm(ops[{i}])",
    "int64_t": "opImm(ops[{i}])",
    "uint64_t": "opImm(ops[{i}])",
}

EMIT_OP_CAST = {
    "Gp": "ops[{i}].as<Gp>()",
    "Fp": "ops[{i}].as<Fp>()",
    "Vr": "ops[{i}].as<Vr>()",
    "Vsx": "ops[{i}].as<Vsx>()",
    "const Mem&": "ops[{i}].as<Mem>()",
    "const Label&": "ops[{i}].as<Label>()",
    "int16_t": "immAs<int16_t>(ops[{i}])",
    "uint16_t": "immAs<uint16_t>(ops[{i}])",
    "uint8_t": "immAs<uint8_t>(ops[{i}])",
    "int32_t": "immAs<int32_t>(ops[{i}])",
    "uint32_t": "immAs<uint32_t>(ops[{i}])",
    "int64_t": "immAs<int64_t>(ops[{i}])",
    "uint64_t": "immAs<uint64_t>(ops[{i}])",
}


def emitinst_parse_params(args):
    """Returns [(cpp_type, default_expr)] for the non-bool params of `args`."""
    result = []
    for param in args.split(","):
        param = param.strip()
        if not param:
            continue
        has_default = "=" in param
        lhs = param.split("=", 1)[0].strip()
        default = param.split("=", 1)[1].strip() if has_default else None
        type_name = lhs.rsplit(" ", 1)[0].strip()
        if type_name == "bool":
            continue  # Trailing Rc/Oe flags are not exposed through generic emit().
        result.append((type_name, default))
    return result


def gen_emit_inst_case(name, args):
    """Generates the Assembler::_emit() dispatch case for `name`.

    The case validates the operands against the generated signature and calls
    the dedicated assembler method. Optional trailing immediates (C++ default
    arguments such as `cmpd`'s `bf`) accept a shorter operand list.
    """
    plist = emitinst_parse_params(args)
    required = sum(1 for _, default in plist if default is None)
    total = len(plist)

    checks = [f"op_count < {required}", f"op_count > {total}"]
    for i, (t, default) in enumerate(plist):
        if t == "Vsx" and name in EMIT_VEC_OR_FP:
            check = f"(opFp(ops[{i}]) || opVec(ops[{i}]))"
        else:
            check = EMIT_OP_CHECK[t].format(i=i)
        if i >= required:
            check = f"(op_count > {i} && !{check})"
        else:
            check = f"!{check}"
        checks.append(check)

    args_list = []
    for i, (t, default) in enumerate(plist):
        cast = EMIT_OP_CAST[t].format(i=i)
        if i >= required:
            args_list.append(f"op_count > {i} ? {cast} : {default if default is not None else '0'}")
        else:
            args_list.append(cast)

    id_name = f"kId{name[0].upper()}{name[1:]}"
    cond = " || ".join(checks)
    return "\n".join([
        f"    case Inst::{id_name}:",
        f"      if (ASMJIT_UNLIKELY({cond}))",
        "        return emitInvalidOperand();",
        f"      return asm_.{name}({', '.join(args_list)});",
    ])


def parse_emitter_params(args):
    """Returns [(cpp_type, param_name, default_expr)] for non-bool params."""
    result = []
    for param in args.split(","):
        param = param.strip()
        if not param:
            continue
        has_default = "=" in param
        lhs = param.split("=", 1)[0].strip()
        default = param.split("=", 1)[1].strip() if has_default else None
        parts = lhs.rsplit(" ", 1)
        type_name = parts[0].strip()
        pname = parts[1].strip() if len(parts) > 1 else ""
        if type_name == "bool":
            continue  # Trailing Rc/Oe flags are not exposed through generic emit().
        result.append((type_name, pname, default))
    return result


EMITTER_TYPE_MAP = {
    "Gp": "const Gp&",
    "Fp": "const Fp&",
    "Vr": "const Vr&",
    "Vsx": "const Vsx&",
    "const Mem&": "const Mem&",
    "const Label&": "const Label&",
    "int16_t": "const Imm&",
    "uint16_t": "const Imm&",
    "uint8_t": "const Imm&",
    "int32_t": "const Imm&",
    "uint32_t": "const Imm&",
    "int64_t": "const Imm&",
    "uint64_t": "const Imm&",
}


def gen_emitter_method(name, params):
    """Generates a single compiler emitter method body."""
    args = [(pname or f"o{i}", EMITTER_TYPE_MAP[t]) for i, (t, pname, _) in enumerate(params)]
    sig_parts = []
    for i, ((_, _, default), (aname, cpp_type)) in enumerate(zip(params, args)):
        sig = f"{cpp_type} {aname}"
        if default is not None:
            sig += f" = imm({default})"
        sig_parts.append(sig)
    call_args = ", ".join(aname for aname, _ in args)
    id_name = f"kId{name[0].upper()}{name[1:]}"
    if call_args:
        call = f"_emitI(Inst::{id_name}, {call_args})"
    else:
        call = f"_emitI(Inst::{id_name})"
    return f"  inline Error {name}({', '.join(sig_parts)}) {{ return _emitter()->{call}; }}"


def emit_inst_supported_names():
    """Returns the set of instruction names that `Assembler::_emit()` supports.

    Since `emitInst` is generated from the same method list, every instruction
    is supported; this returns `None` to express that.
    """
    return None


def main():
    with open(HEADER) as f:
        text = f.read()

    decls = re.findall(r"ASMJIT_API Error ([A-Za-z_][A-Za-z0-9_]*)\(([^)]*)\)", text)
    names = [n for n, _ in decls if n not in EXCLUDED]
    params = {n: a for n, a in decls if n not in EXCLUDED}
    supported = emit_inst_supported_names()

    lines = [
        "// This file is part of AsmJit project <https://asmjit.com>",
        "//",
        "// See <asmjit/core.h> or LICENSE.md for license and copyright information",
        "// SPDX-License-Identifier: Zlib",
        "",
        "// Generated by tools/ppc_instgen.py - do not edit manually.",
        "",
        "#ifndef ASMJIT_PPC_PPCINST_H_INCLUDED",
        "#define ASMJIT_PPC_PPCINST_H_INCLUDED",
        "",
        "ASMJIT_BEGIN_SUB_NAMESPACE(ppc)",
        "",
        "//! Instruction.",
        "//!",
        "//! \\note Only used to hold PPC-specific instruction identifiers.",
        "namespace Inst {",
        "  //! Instruction id.",
        "  enum Id : uint32_t {",
        "    kIdNone = 0,                         //!< Invalid instruction id.",
    ]
    for name in names:
        id_name = f"kId{name[0].upper()}{name[1:]}"
        lines.append(f"    {id_name}, {' ' * (42 - len(id_name))}//!< Instruction '{name}'.")
    lines += [
        "",
        "    _kIdCount",
        "  };",
        "} // {Inst}",
        "",
        "ASMJIT_END_SUB_NAMESPACE",
        "",
        "#endif // ASMJIT_PPC_PPCINST_H_INCLUDED",
        "",
    ]

    with open(OUTPUT, "w") as f:
        f.write("\n".join(lines))

    db = [
        "// This file is part of AsmJit project <https://asmjit.com>",
        "//",
        "// See <asmjit/core.h> or LICENSE.md for license and copyright information",
        "// SPDX-License-Identifier: Zlib",
        "",
        "// Generated by tools/ppc_instgen.py - do not edit manually.",
        "",
        "#ifndef ASMJIT_PPC_PPCINSTDB_P_H_INCLUDED",
        "#define ASMJIT_PPC_PPCINSTDB_P_H_INCLUDED",
        "",
        "#include <asmjit/ppc/ppcinst.h>",
        "#include <asmjit/core/cpuinfo.h>",
        "",
        "ASMJIT_BEGIN_SUB_NAMESPACE(ppc)",
        "",
        "namespace Inst {",
        "  //! Maximum number of instruction operands.",
        "  static inline constexpr uint32_t kMaxOperands = 6;",
        "",
        "  //! Operand kind.",
        "  enum class Kind : uint8_t {",
        "    kNone = 0, kGp, kFp, kVec, kMem, kLabel, kImm",
        "  };",
        "",
        "  //! Instruction operand signature.",
        "  struct Signature {",
        "    uint8_t required;",
        "    uint8_t count;",
        "    Kind kinds[kMaxOperands];",
        "  };",
        "",
        "  //! Instruction signatures indexed by Inst::Id.",
        "  static constexpr Signature inst_signatures[_kIdCount] = {",
        "    { 0, 0, { Kind::kNone, Kind::kNone, Kind::kNone, Kind::kNone, Kind::kNone, Kind::kNone } }, // kIdNone",
    ]
    for name in names:
        required, total, kinds = parse_params(params[name])
        kinds += ["kNone"] * (6 - len(kinds))
        db.append(f"    {{ {required}, {total}, {{ {', '.join('Kind::' + k for k in kinds)} }} }}, // {name}")
    db += [
        "  };",
        "",
        "  //! Required feature of each instruction (CpuFeatures::PPC::kNone = none).",
        "  static constexpr CpuFeatures::PPC::Id inst_features[_kIdCount] = {",
        "    CpuFeatures::PPC::kNone, // kIdNone",
    ]
    for name in names:
        db.append(f"    CpuFeatures::PPC::{feature_of(name)}, // {name}")
    db += [
        "  };",
        "",
        "  //! Read/Write behavior class of each instruction (see InstAPI::query_rw_info()).",
        "  enum class RWClass : uint8_t {",
        "    //! First register operand is written, all other register operands are read.",
        "    kDefault = 0,",
        "    //! Store: all register operands are read, the memory operand (if any) is written.",
        "    kStore = 1,",
        "    //! Load with update: first register operand is written, the memory base is also written.",
        "    kLoadUpdate = 2,",
        "    //! Store with update: all register operands are read, the memory base is also written.",
        "    kStoreUpdate = 3,",
        "    //! No register operand is written (compares, branches, moves-to-special-registers, ...).",
        "    kNoWrite = 4",
        "  };",
        "",
        "  //! Read/Write class of each instruction indexed by Inst::Id.",
        "  static constexpr RWClass inst_rw_classes[_kIdCount] = {",
        "    RWClass::kDefault, // kIdNone",
    ]
    for name in names:
        db.append(f"    RWClass::{rw_class_of(name)}, // {name}")
    db += [
        "  };",
        "} // {Inst}",
        "",
        "ASMJIT_END_SUB_NAMESPACE",
        "",
        "#endif // ASMJIT_PPC_PPCINSTDB_P_H_INCLUDED",
        "",
    ]

    with open(OUTPUT_DB, "w") as f:
        f.write("\n".join(db))

    emitter = [
        "// This file is part of AsmJit project <https://asmjit.com>",
        "//",
        "// See <asmjit/core.h> or LICENSE.md for license and copyright information",
        "// SPDX-License-Identifier: Zlib",
        "",
        "// Generated by tools/ppc_instgen.py - do not edit manually.",
        "",
        "#ifndef ASMJIT_PPC_PPCEMITTER_H_INCLUDED",
        "#define ASMJIT_PPC_PPCEMITTER_H_INCLUDED",
        "",
        "#include <asmjit/core/emitter.h>",
        "#include <asmjit/ppc/ppcassembler.h>",
        "#include <asmjit/ppc/ppcinst.h>",
        "",
        "ASMJIT_BEGIN_SUB_NAMESPACE(ppc)",
        "",
        "//! Instruction emitter methods shared by the PPC64 compiler.",
        "//!",
        "//! The method set mirrors the instructions supported by the assembler's",
        "//! generic `_emit()` path, so everything emitted here can be serialized.",
        "template<typename This>",
        "struct EmitterExplicitT {",
        "  //! \\cond",
        "  ASMJIT_ATTRIBUTE_NO_SANITIZE_UNDEF ASMJIT_INLINE_NODEBUG This* _emitter() noexcept { return static_cast<This*>(this); }",
        "  ASMJIT_ATTRIBUTE_NO_SANITIZE_UNDEF ASMJIT_INLINE_NODEBUG const This* _emitter() const noexcept { return static_cast<const This*>(this); }",
        "  //! \\endcond",
        "",
        "  //! \\name Instruction Emission",
        "  //! \\{",
        "",
    ]
    emitted = 0
    for name in names:
        if supported is not None and name not in supported:
            continue
        emitter.append(gen_emitter_method(name, parse_emitter_params(params[name])))
        emitted += 1
    emitter += [
        "",
        "  //! \\}",
        "};",
        "",
        "ASMJIT_END_SUB_NAMESPACE",
        "",
        "#endif // ASMJIT_PPC_PPCEMITTER_H_INCLUDED",
        "",
    ]

    with open(OUTPUT_EMITTER, "w") as f:
        f.write("\n".join(emitter))

    emitinst = [
        "// This file is part of AsmJit project <https://asmjit.com>",
        "//",
        "// See <asmjit/core.h> or LICENSE.md for license and copyright information",
        "// SPDX-License-Identifier: Zlib",
        "",
        "// Generated by tools/ppc_instgen.py - do not edit manually.",
        "//",
        "// Instruction dispatch cases for Assembler::_emit(), included by",
        "// asmjit/ppc/ppcassembler.cpp. Derived from the same method list that",
        "// produces the instruction ids and the instruction database, so the",
        "// generic emission surface can never drift from the instruction set.",
        "",
    ]
    emitted_cases = 0
    for name in names:
        emitinst.append(gen_emit_inst_case(name, params[name]))
        emitted_cases += 1
    emitinst.append("")

    with open(OUTPUT_EMITINST, "w") as f:
        f.write("\n".join(emitinst))

    print(f"generated {len(names)} instruction ids -> {OUTPUT}, {OUTPUT_DB}")
    print(f"generated {emitted} emitter methods -> {OUTPUT_EMITTER}")
    print(f"generated {emitted_cases} emitInst cases -> {OUTPUT_EMITINST}")


if __name__ == "__main__":
    main()
