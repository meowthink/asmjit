// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC) && !defined(ASMJIT_NO_COMPILER)

#include <asmjit/ppc/ppcassembler.h>
#include <asmjit/ppc/ppccompiler.h>
#include <asmjit/ppc/ppcemithelper_p.h>
#include <asmjit/ppc/ppcrapass_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

// ppc::Compiler - Construction & Destruction
// ==========================================

Compiler::Compiler(CodeHolder* code) noexcept : BaseCompiler() {
  _arch_mask = (uint64_t(1) << uint32_t(Arch::kPPC64_LE)) |
               (uint64_t(1) << uint32_t(Arch::kPPC64_BE));
  init_emitter_funcs(this);

  if (code) {
    code->attach(this);
  }
}

Compiler::~Compiler() noexcept {}

// ppc::Compiler - Events
// ======================

Error Compiler::on_attach(CodeHolder& code) noexcept {
  ASMJIT_PROPAGATE(Base::on_attach(code));

  Error err = add_pass<PPCRAPass>();
  if (ASMJIT_UNLIKELY(err != Error::kOk)) {
    on_detach(code);
    return err;
  }

  _instruction_alignment = uint8_t(4);
  return Error::kOk;
}

Error Compiler::on_detach(CodeHolder& code) noexcept {
  return Base::on_detach(code);
}

Error Compiler::on_reinit(CodeHolder& code) noexcept {
  Error err = Base::on_reinit(code);
  if (err == Error::kOk) {
    err = add_pass<PPCRAPass>();
  }
  return err;
}

// ppc::Compiler - Finalize
// ========================

Error Compiler::finalize() {
  ASMJIT_PROPAGATE(run_passes());
  Assembler a(_code);
  a.add_encoding_options(encoding_options());
  a.add_diagnostic_options(diagnostic_options());
  return serialize_to(&a);
}

// ppc::Compiler - Moves
// =====================

Error Compiler::mov(const Gp& o0, const Gp& o1) {
  // `ori ra, rs, 0` is the canonical register move (mr) idiom.
  return ori(o0, o1, imm(0));
}

Error Compiler::mov(const Gp& o0, const Imm& o1_) {
  // Mirrors the assembler's loadImm64(): fast paths for common constants,
  // a 5-instruction sequence for general 64-bit values.
  const uint64_t val = uint64_t(o1_.value());

  if (val <= 0x7FFFu || val >= 0xFFFFFFFFFFFF8000ull)
    return li(o0, Imm(int16_t(val)));

  if (val <= 0xFFFFFFFFull) {
    if (val <= 0x7FFFFFFFull) {
      ASMJIT_PROPAGATE(addis(o0, r0, Imm(int16_t(uint32_t(val) >> 16))));
      return ori(o0, o0, Imm(uint16_t(val)));
    }
    ASMJIT_PROPAGATE(li(o0, imm(0)));
    ASMJIT_PROPAGATE(oris(o0, o0, Imm(uint16_t(val >> 16))));
    return ori(o0, o0, Imm(uint16_t(val)));
  }

  if (val >= 0xFFFFFFFF80000000ull) {
    ASMJIT_PROPAGATE(addis(o0, r0, Imm(int16_t(uint32_t(val) >> 16))));
    return addi(o0, o0, Imm(int16_t(uint32_t(val))));
  }

  ASMJIT_PROPAGATE(addis(o0, r0, Imm(int16_t(val >> 48))));
  ASMJIT_PROPAGATE(ori(o0, o0, Imm(uint16_t(val >> 32))));
  ASMJIT_PROPAGATE(sldi(o0, o0, imm(32)));
  ASMJIT_PROPAGATE(oris(o0, o0, Imm(uint16_t(val >> 16))));
  return ori(o0, o0, Imm(uint16_t(val)));
}

Error Compiler::mov(const Gp& o0, const Mem& o1) {
  return ld(o0, o1);
}

Error Compiler::mov(const Mem& o0, const Gp& o1) {
  return std(o1, o0);
}

Error Compiler::mov(const Fp& o0, const Fp& o1) {
  return fmr(o0, o1);
}

Error Compiler::mov(const Vr& o0, const Vr& o1) {
  // `vor vrt, vra, vra` copies a VMX register.
  return vor(o0, o1, o1);
}

Error Compiler::mov(const Vsx& o0, const Vsx& o1) {
  // `xxlor xt, xa, xa` copies a VSX register.
  return xxlor(o0, o1, o1);
}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC && !ASMJIT_NO_COMPILER
