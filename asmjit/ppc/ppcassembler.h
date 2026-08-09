// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED
#define ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED

#include <asmjit/core/assembler.h>
#include <asmjit/ppc/ppcinst.h>

#include <cstdint>
#include <vector>

#if !defined(ASMJIT_NO_PPC)

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \addtogroup asmjit_ppc
//! \{

//! PowerPC general purpose register.
class Gp : public Reg {
public:
  ASMJIT_DEFINE_FINAL_REG(Gp, Reg, RegTraits<RegType::kGp64>)
};

//! GPR constants.
static constexpr Gp r0 { 0 };
static constexpr Gp r1 { 1 };
static constexpr Gp r2 { 2 };
static constexpr Gp r3 { 3 };
static constexpr Gp r4 { 4 };
static constexpr Gp r5 { 5 };
static constexpr Gp r6 { 6 };
static constexpr Gp r7 { 7 };
static constexpr Gp r8 { 8 };
static constexpr Gp r9 { 9 };
static constexpr Gp r10 { 10 };
static constexpr Gp r11 { 11 };
static constexpr Gp r12 { 12 };
static constexpr Gp r13 { 13 };
static constexpr Gp r14 { 14 };
static constexpr Gp r15 { 15 };
static constexpr Gp r16 { 16 };
static constexpr Gp r17 { 17 };
static constexpr Gp r18 { 18 };
static constexpr Gp r19 { 19 };
static constexpr Gp r20 { 20 };
static constexpr Gp r21 { 21 };
static constexpr Gp r22 { 22 };
static constexpr Gp r23 { 23 };
static constexpr Gp r24 { 24 };
static constexpr Gp r25 { 25 };
static constexpr Gp r26 { 26 };
static constexpr Gp r27 { 27 };
static constexpr Gp r28 { 28 };
static constexpr Gp r29 { 29 };
static constexpr Gp r30 { 30 };
static constexpr Gp r31 { 31 };

//! PowerPC floating-point register.
class Fp : public Reg {
public:
  ASMJIT_DEFINE_FINAL_REG(Fp, Reg, RegTraits<RegType::kVec64>)
};

//! FPR constants.
static constexpr Fp f0 { 0 };
static constexpr Fp f1 { 1 };
static constexpr Fp f2 { 2 };
static constexpr Fp f3 { 3 };
static constexpr Fp f4 { 4 };
static constexpr Fp f5 { 5 };
static constexpr Fp f6 { 6 };
static constexpr Fp f7 { 7 };
static constexpr Fp f8 { 8 };
static constexpr Fp f9 { 9 };
static constexpr Fp f10 { 10 };
static constexpr Fp f11 { 11 };
static constexpr Fp f12 { 12 };
static constexpr Fp f13 { 13 };
static constexpr Fp f14 { 14 };
static constexpr Fp f15 { 15 };
static constexpr Fp f16 { 16 };
static constexpr Fp f17 { 17 };
static constexpr Fp f18 { 18 };
static constexpr Fp f19 { 19 };
static constexpr Fp f20 { 20 };
static constexpr Fp f21 { 21 };
static constexpr Fp f22 { 22 };
static constexpr Fp f23 { 23 };
static constexpr Fp f24 { 24 };
static constexpr Fp f25 { 25 };
static constexpr Fp f26 { 26 };
static constexpr Fp f27 { 27 };
static constexpr Fp f28 { 28 };
static constexpr Fp f29 { 29 };
static constexpr Fp f30 { 30 };
static constexpr Fp f31 { 31 };

//! PowerPC VSX register (vs0..vs63; vs32..vs63 alias the vector registers).
class Vsx : public Reg {
public:
  ASMJIT_DEFINE_FINAL_REG(Vsx, Reg, RegTraits<RegType::kVec128>)
};

//! VSX register constants.
static constexpr Vsx vs0 { 0 };
static constexpr Vsx vs1 { 1 };
static constexpr Vsx vs2 { 2 };
static constexpr Vsx vs3 { 3 };
static constexpr Vsx vs4 { 4 };
static constexpr Vsx vs5 { 5 };
static constexpr Vsx vs6 { 6 };
static constexpr Vsx vs7 { 7 };
static constexpr Vsx vs8 { 8 };
static constexpr Vsx vs9 { 9 };
static constexpr Vsx vs10 { 10 };
static constexpr Vsx vs11 { 11 };
static constexpr Vsx vs12 { 12 };
static constexpr Vsx vs13 { 13 };
static constexpr Vsx vs14 { 14 };
static constexpr Vsx vs15 { 15 };
static constexpr Vsx vs16 { 16 };
static constexpr Vsx vs17 { 17 };
static constexpr Vsx vs18 { 18 };
static constexpr Vsx vs19 { 19 };
static constexpr Vsx vs20 { 20 };
static constexpr Vsx vs21 { 21 };
static constexpr Vsx vs22 { 22 };
static constexpr Vsx vs23 { 23 };
static constexpr Vsx vs24 { 24 };
static constexpr Vsx vs25 { 25 };
static constexpr Vsx vs26 { 26 };
static constexpr Vsx vs27 { 27 };
static constexpr Vsx vs28 { 28 };
static constexpr Vsx vs29 { 29 };
static constexpr Vsx vs30 { 30 };
static constexpr Vsx vs31 { 31 };
static constexpr Vsx vs32 { 32 };
static constexpr Vsx vs33 { 33 };
static constexpr Vsx vs34 { 34 };
static constexpr Vsx vs35 { 35 };
static constexpr Vsx vs36 { 36 };
static constexpr Vsx vs37 { 37 };
static constexpr Vsx vs38 { 38 };
static constexpr Vsx vs39 { 39 };
static constexpr Vsx vs40 { 40 };
static constexpr Vsx vs41 { 41 };
static constexpr Vsx vs42 { 42 };
static constexpr Vsx vs43 { 43 };
static constexpr Vsx vs44 { 44 };
static constexpr Vsx vs45 { 45 };
static constexpr Vsx vs46 { 46 };
static constexpr Vsx vs47 { 47 };
static constexpr Vsx vs48 { 48 };
static constexpr Vsx vs49 { 49 };
static constexpr Vsx vs50 { 50 };
static constexpr Vsx vs51 { 51 };
static constexpr Vsx vs52 { 52 };
static constexpr Vsx vs53 { 53 };
static constexpr Vsx vs54 { 54 };
static constexpr Vsx vs55 { 55 };
static constexpr Vsx vs56 { 56 };
static constexpr Vsx vs57 { 57 };
static constexpr Vsx vs58 { 58 };
static constexpr Vsx vs59 { 59 };
static constexpr Vsx vs60 { 60 };
static constexpr Vsx vs61 { 61 };
static constexpr Vsx vs62 { 62 };
static constexpr Vsx vs63 { 63 };

//! PowerPC VMX (Altivec) register.
class Vr : public Reg {
public:
  ASMJIT_DEFINE_FINAL_REG(Vr, Reg, RegTraits<RegType::kVec128>)
};

//! Vector register constants.
static constexpr Vr v0 { 0 };
static constexpr Vr v1 { 1 };
static constexpr Vr v2 { 2 };
static constexpr Vr v3 { 3 };
static constexpr Vr v4 { 4 };
static constexpr Vr v5 { 5 };
static constexpr Vr v6 { 6 };
static constexpr Vr v7 { 7 };
static constexpr Vr v8 { 8 };
static constexpr Vr v9 { 9 };
static constexpr Vr v10 { 10 };
static constexpr Vr v11 { 11 };
static constexpr Vr v12 { 12 };
static constexpr Vr v13 { 13 };
static constexpr Vr v14 { 14 };
static constexpr Vr v15 { 15 };
static constexpr Vr v16 { 16 };
static constexpr Vr v17 { 17 };
static constexpr Vr v18 { 18 };
static constexpr Vr v19 { 19 };
static constexpr Vr v20 { 20 };
static constexpr Vr v21 { 21 };
static constexpr Vr v22 { 22 };
static constexpr Vr v23 { 23 };
static constexpr Vr v24 { 24 };
static constexpr Vr v25 { 25 };
static constexpr Vr v26 { 26 };
static constexpr Vr v27 { 27 };
static constexpr Vr v28 { 28 };
static constexpr Vr v29 { 29 };
static constexpr Vr v30 { 30 };
static constexpr Vr v31 { 31 };

//! ELFv1 function descriptor
struct FunctionDescriptor {
  void* entry;
  void* toc;
  void* environment;
};


//! PowerPC64 memory operand.
//!
//! \note The base can be either a general purpose register or a \ref Label. An
//! optional index register is used by X-form load/store instructions.
class Mem : public BaseMem {
public:
  //! \name Construction & Destruction
  //! \{

  //! Construct a default `Mem` operand.
  ASMJIT_INLINE_CONSTEXPR Mem() noexcept
    : BaseMem() {}

  ASMJIT_INLINE_CONSTEXPR Mem(const Mem& other) noexcept
    : BaseMem(other) {}

  ASMJIT_INLINE_NODEBUG explicit Mem(Globals::NoInit_) noexcept
    : BaseMem(Globals::NoInit) {}

  //! Creates a memory operand with a `Label` base and an `off`set.
  ASMJIT_INLINE_CONSTEXPR explicit Mem(const Label& base, int32_t off = 0) noexcept
    : BaseMem(Signature::from_op_type(OperandType::kMem) |
              Signature::from_mem_base_type(RegType::kLabelTag),
              base.id(), 0, off) {}

  //! Creates a memory operand with a register `base` and an `off`set (D/DS-form).
  ASMJIT_INLINE_CONSTEXPR explicit Mem(const Reg& base, int32_t off = 0) noexcept
    : BaseMem(Signature::from_op_type(OperandType::kMem) |
              Signature::from_mem_base_type(base.reg_type()),
              base.id(), 0, off) {}

  //! Creates a memory operand with a register `base` and an `index` (X-form).
  ASMJIT_INLINE_CONSTEXPR Mem(const Reg& base, const Reg& index) noexcept
    : BaseMem(Signature::from_op_type(OperandType::kMem) |
              Signature::from_mem_base_type(base.reg_type()) |
              Signature::from_mem_index_type(index.reg_type()),
              base.id(), index.id(), 0) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  ASMJIT_INLINE_CONSTEXPR Mem& operator=(const Mem& other) noexcept {
    copy_from(other);
    return *this;
  }

  //! \}

  //! \name Clone
  //! \{

  //! Clones the memory operand.
  ASMJIT_INLINE_CONSTEXPR Mem clone() const noexcept { return Mem(*this); }

  //! Gets a new memory operand adjusted by `off`.
  ASMJIT_INLINE_CONSTEXPR Mem clone_adjusted(int64_t off) const noexcept {
    Mem result(*this);
    result.add_offset(off);
    return result;
  }

  //! \}
};

//! Creates a memory operand with a `base` register and an `offset` (D/DS-form).
[[nodiscard]]
static ASMJIT_INLINE_CONSTEXPR Mem ptr(const Gp& base, int32_t offset = 0) noexcept {
  return Mem(base, offset);
}

//! Creates a memory operand with `base` and `index` registers (X-form).
[[nodiscard]]
static ASMJIT_INLINE_CONSTEXPR Mem ptr(const Gp& base, const Gp& index) noexcept {
  return Mem(base, index);
}

//! Creates a memory operand with a `Label` base and an `offset`.
[[nodiscard]]
static ASMJIT_INLINE_CONSTEXPR Mem ptr(const Label& base, int32_t offset = 0) noexcept {
  return Mem(base, offset);
}

//! PowerPC64 assembler.
//!
//! AsmJit backend: derives from \ref BaseAssembler so it writes into
//! \ref CodeHolder and can be handed to \ref JitRuntime.
class ASMJIT_VIRTAPI Assembler : public BaseAssembler {
public:
  ASMJIT_NONCOPYABLE(Assembler)
  using Base = BaseAssembler;

  //! \name Construction & Destruction
  //! \{

  ASMJIT_API Assembler(CodeHolder* code = nullptr) noexcept;
  ASMJIT_API ~Assembler() noexcept override;

  //! \}

  //! \name Events
  //! \{

  ASMJIT_API Error on_attach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_detach(CodeHolder& code) noexcept override;

  //! \}

  //! \name Instruction Emission
  //! \{

  //! Emits an instruction by \ref Inst::Id (generic emission).
  ASMJIT_API Error _emit(InstId inst_id, const Operand_& o0, const Operand_& o1, const Operand_& o2, const Operand_* op_ext) override;

  //! \}

  //! \name Labels
  //! \{

  //! Binds a label and patches all branch references emitted so far.
  ASMJIT_API Error bind(const Label& label) override;

  //! \}

  //! \name Raw Emission
  //! \{

  //! Emits a raw 32-bit instruction word.
  ASMJIT_API Error emit32(uint32_t word);

  //! \}

  //! \name Integer Instructions
  //! \{

  //! `li rt, simm` (load immediate, 16-bit signed).
  ASMJIT_API Error li(Gp rt, int16_t simm);
  //! `addi rt, ra, simm`.
  ASMJIT_API Error addi(Gp rt, Gp ra, int16_t simm);
  //! `addis rt, ra, simm` (shifted immediate add).
  ASMJIT_API Error addis(Gp rt, Gp ra, int16_t simm);
  //! `ori ra, rs, uimm`.
  ASMJIT_API Error ori(Gp ra, Gp rs, uint16_t uimm);
  //! `oris ra, rs, uimm`.
  ASMJIT_API Error oris(Gp ra, Gp rs, uint16_t uimm);
  //! `sldi ra, rs, sh` (shift-left doubleword immediate).
  ASMJIT_API Error sldi(Gp ra, Gp rs, uint8_t sh);
  //! Materializes a full 64-bit immediate in 5 instructions.
  ASMJIT_API Error loadImm64(Gp rt, uint64_t imm);

  //! `add rt, ra, rb`.
  ASMJIT_API Error add(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `addc rt, ra, rb` (add with carry).
  ASMJIT_API Error addc(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `addze rt, ra` (add zero-extended).
  ASMJIT_API Error addze(Gp rt, Gp ra, bool oe = false, bool rc = false);
  //! `addme rt, ra` (add minus one extended).
  ASMJIT_API Error addme(Gp rt, Gp ra, bool oe = false, bool rc = false);
  //! `subf rt, ra, rb` (rt = rb - ra).
  ASMJIT_API Error subf(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `subfc rt, ra, rb` (subtract from with carry).
  ASMJIT_API Error subfc(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `subfze rt, ra`.
  ASMJIT_API Error subfze(Gp rt, Gp ra, bool oe = false, bool rc = false);
  //! `subfme rt, ra`.
  ASMJIT_API Error subfme(Gp rt, Gp ra, bool oe = false, bool rc = false);
  //! `neg rt, ra`.
  ASMJIT_API Error neg(Gp rt, Gp ra, bool oe = false, bool rc = false);
  //! `and ra, rs, rb`.
  ASMJIT_API Error and_(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `andc ra, rs, rb` (and with complement).
  ASMJIT_API Error andc(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `or ra, rs, rb`.
  ASMJIT_API Error or_(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `orc ra, rs, rb` (or with complement).
  ASMJIT_API Error orc(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `xor ra, rs, rb`.
  ASMJIT_API Error xor_(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `nand ra, rs, rb`.
  ASMJIT_API Error nand(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `nor ra, rs, rb`.
  ASMJIT_API Error nor(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `eqv ra, rs, rb` (equivalent, exclusive-nor).
  ASMJIT_API Error eqv(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `sld ra, rs, rb`.
  ASMJIT_API Error sld(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `srd ra, rs, rb`.
  ASMJIT_API Error srd(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `srad ra, rs, rb`.
  ASMJIT_API Error srad(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `slw ra, rs, rb` (shift-left word).
  ASMJIT_API Error slw(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `srw ra, rs, rb` (shift-right word).
  ASMJIT_API Error srw(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `sraw ra, rs, rb` (shift-right algebraic word).
  ASMJIT_API Error sraw(Gp ra, Gp rs, Gp rb, bool rc = false);
  //! `extsw ra, rs`.
  ASMJIT_API Error extsw(Gp ra, Gp rs, bool rc = false);
  //! `extswsli ra, rs, sh` (extend sign word and shift left immediate).
  ASMJIT_API Error extswsli(Gp ra, Gp rs, uint8_t sh, bool rc = false);
  //! `addic rt, ra, simm` (add immediate carrying).
  ASMJIT_API Error addic(Gp rt, Gp ra, int16_t simm, bool rc = false);
  //! `subfic rt, ra, simm` (subtract from immediate carrying).
  ASMJIT_API Error subfic(Gp rt, Gp ra, int16_t simm);
  //! `adde rt, ra, rb` (add extended).
  ASMJIT_API Error adde(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `subfe rt, ra, rb` (subtract from extended).
  ASMJIT_API Error subfe(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `addex rt, ra, rb, cy` (add extended using alternate carry bit).
  ASMJIT_API Error addex(Gp rt, Gp ra, Gp rb, uint32_t cy);
  //! `addpcis rt, d` (add PC immediate shifted).
  ASMJIT_API Error addpcis(Gp rt, int16_t d);
  //! `mulld rt, ra, rb`.
  ASMJIT_API Error mulld(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `mullw rt, ra, rb`.
  ASMJIT_API Error mullw(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `mulhdu rt, ra, rb`.
  ASMJIT_API Error mulhdu(Gp rt, Gp ra, Gp rb, bool rc = false);
  //! `mulhd rt, ra, rb`.
  ASMJIT_API Error mulhd(Gp rt, Gp ra, Gp rb, bool rc = false);
  //! `mulhw rt, ra, rb`.
  ASMJIT_API Error mulhw(Gp rt, Gp ra, Gp rb, bool rc = false);
  //! `mulhwu rt, ra, rb`.
  ASMJIT_API Error mulhwu(Gp rt, Gp ra, Gp rb, bool rc = false);
  //! `mulli rt, ra, simm`.
  ASMJIT_API Error mulli(Gp rt, Gp ra, int16_t simm);
  //! `maddhd rt, ra, rb, rc` (multiply-add high doubleword).
  ASMJIT_API Error maddhd(Gp rt, Gp ra, Gp rb, Gp rc0);
  //! `maddhdu rt, ra, rb, rc` (multiply-add high doubleword unsigned).
  ASMJIT_API Error maddhdu(Gp rt, Gp ra, Gp rb, Gp rc0);
  //! `maddld rt, ra, rb, rc` (multiply-add low doubleword).
  ASMJIT_API Error maddld(Gp rt, Gp ra, Gp rb, Gp rc0);
  //! `divd rt, ra, rb`.
  ASMJIT_API Error divd(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divw rt, ra, rb`.
  ASMJIT_API Error divw(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divdu rt, ra, rb`.
  ASMJIT_API Error divdu(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divwu rt, ra, rb`.
  ASMJIT_API Error divwu(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divde rt, ra, rb` (divide doubleword extended).
  ASMJIT_API Error divde(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divdeu rt, ra, rb` (divide doubleword extended unsigned).
  ASMJIT_API Error divdeu(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divwe rt, ra, rb` (divide word extended).
  ASMJIT_API Error divwe(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `divweu rt, ra, rb` (divide word extended unsigned).
  ASMJIT_API Error divweu(Gp rt, Gp ra, Gp rb, bool oe = false, bool rc = false);
  //! `modsd rt, ra, rb` (modulo signed doubleword).
  ASMJIT_API Error modsd(Gp rt, Gp ra, Gp rb);
  //! `modud rt, ra, rb` (modulo unsigned doubleword).
  ASMJIT_API Error modud(Gp rt, Gp ra, Gp rb);
  //! `modsw rt, ra, rb` (modulo signed word).
  ASMJIT_API Error modsw(Gp rt, Gp ra, Gp rb);
  //! `moduw rt, ra, rb` (modulo unsigned word).
  ASMJIT_API Error moduw(Gp rt, Gp ra, Gp rb);
  //! `darn rt, l` (deliver a random number).
  ASMJIT_API Error darn(Gp rt, uint8_t l);
  //! `srawi ra, rs, sh`.
  ASMJIT_API Error srawi(Gp ra, Gp rs, uint8_t sh, bool rc = false);
  //! `srdi ra, rs, sh`.
  ASMJIT_API Error srdi(Gp ra, Gp rs, uint8_t sh);
  //! `andi. ra, rs, uimm`.
  ASMJIT_API Error andi_(Gp ra, Gp rs, uint16_t uimm);
  //! `andis. ra, rs, uimm`.
  ASMJIT_API Error andis_(Gp ra, Gp rs, uint16_t uimm);
  //! `xori ra, rs, uimm`.
  ASMJIT_API Error xori(Gp ra, Gp rs, uint16_t uimm);
  //! `xoris ra, rs, uimm`.
  ASMJIT_API Error xoris(Gp ra, Gp rs, uint16_t uimm);

  //! \}

  //! \name Compare & Branch Instructions
  //! \{

  //! `cmpd ra, rb` (signed compare doubleword, CR0).
  ASMJIT_API Error cmpd(Gp ra, Gp rb, uint32_t bf = 0);
  //! `cmpld ra, rb` (unsigned compare doubleword, CR0).
  ASMJIT_API Error cmpld(Gp ra, Gp rb, uint32_t bf = 0);
  //! `cmpdi ra, simm`.
  ASMJIT_API Error cmpdi(Gp ra, int16_t simm, uint32_t bf = 0);
  //! `cmp bf, l, ra, rb` (signed compare).
  ASMJIT_API Error cmp(uint32_t bf, uint32_t l, Gp ra, Gp rb);
  //! `cmpl bf, l, ra, rb` (unsigned compare).
  ASMJIT_API Error cmpl(uint32_t bf, uint32_t l, Gp ra, Gp rb);
  //! `cmpi bf, l, ra, si` (signed compare immediate).
  ASMJIT_API Error cmpi(uint32_t bf, uint32_t l, Gp ra, int16_t si);
  //! `cmpli bf, l, ra, ui` (unsigned compare immediate).
  ASMJIT_API Error cmpli(uint32_t bf, uint32_t l, Gp ra, uint16_t ui);
  //! `cmpldi bf, ra, ui` (unsigned compare doubleword immediate).
  ASMJIT_API Error cmpldi(uint32_t bf, Gp ra, uint16_t ui);
  //! `cmpb ra, rs, rb` (compare bytes).
  ASMJIT_API Error cmpb(Gp ra, Gp rs, Gp rb);
  //! `cmpeqb bf, ra, rb` (compare equal byte).
  ASMJIT_API Error cmpeqb(uint32_t bf, Gp ra, Gp rb);
  //! `cmprb bf, l, ra, rb` (compare ranged byte).
  ASMJIT_API Error cmprb(uint32_t bf, uint32_t l, Gp ra, Gp rb);

  //! `tw to, ra, rb` (trap word).
  ASMJIT_API Error tw(uint32_t to, Gp ra, Gp rb);
  //! `twi to, ra, si` (trap word immediate).
  ASMJIT_API Error twi(uint32_t to, Gp ra, int16_t si);
  //! `td to, ra, rb` (trap doubleword).
  ASMJIT_API Error td(uint32_t to, Gp ra, Gp rb);
  //! `tdi to, ra, si` (trap doubleword immediate).
  ASMJIT_API Error tdi(uint32_t to, Gp ra, int16_t si);

  //! `isel rt, ra, rb, bc` (integer select).
  ASMJIT_API Error isel(Gp rt, Gp ra, Gp rb, uint32_t bc);
  //! `isellt rt, ra, rb` (select if less than).
  ASMJIT_API Error isellt(Gp rt, Gp ra, Gp rb);
  //! `iseleq rt, ra, rb` (select if equal).
  ASMJIT_API Error iseleq(Gp rt, Gp ra, Gp rb);
  //! `iselgt rt, ra, rb` (select if greater than).
  ASMJIT_API Error iselgt(Gp rt, Gp ra, Gp rb);

  ASMJIT_API Error beq(const Label& label);
  ASMJIT_API Error bne(const Label& label);
  ASMJIT_API Error blt(const Label& label);
  ASMJIT_API Error bge(const Label& label);
  ASMJIT_API Error bgt(const Label& label);
  ASMJIT_API Error ble(const Label& label);
  ASMJIT_API Error b(const Label& label);
  //! `bc bo, bi, label` (branch conditional, arbitrary BO/BI).
  ASMJIT_API Error bc(uint32_t bo, uint32_t bi, const Label& label);
  //! `bcl bo, bi, label` (branch conditional to link register).
  ASMJIT_API Error bcl(uint32_t bo, uint32_t bi, const Label& label);
  //! `bclr bo, bi, bh` (branch conditional to link register).
  ASMJIT_API Error bclr(uint32_t bo, uint32_t bi, uint32_t bh = 0);
  //! `bclrl bo, bi, bh` (branch conditional to link register and link).
  ASMJIT_API Error bclrl(uint32_t bo, uint32_t bi, uint32_t bh = 0);
  //! `bcctr bo, bi, bh` (branch conditional to count register).
  ASMJIT_API Error bcctr(uint32_t bo, uint32_t bi, uint32_t bh = 0);
  //! `bcctrl bo, bi, bh` (branch conditional to count register and link).
  ASMJIT_API Error bcctrl(uint32_t bo, uint32_t bi, uint32_t bh = 0);

  //! \}

  //! \name Control & Call Instructions
  //! \{

  //! `mtctr rs`.
  ASMJIT_API Error mtctr(Gp rs);
  //! `bctrl` (branch to CTR and link).
  ASMJIT_API Error bctrl();
  //! Calls the function at the given absolute address: materializes it in r12
  //! (1-5 instructions depending on the value) and emits `mtctr r12; bctrl`.
  //! The caller's r2 is not preserved; use `callDescriptor()` for ELFv1.
  ASMJIT_API Error call(uint64_t address);
  //! Calls an ELFv1 function descriptor: loads the entry and TOC pointers from
  //! the descriptor at `ptr` (`ld r12,0(r11); ld r2,8(r11); mtctr r12; bctrl`).
  ASMJIT_API Error callDescriptor(uint64_t ptr);
  //! PC-relative helper call: `addpcis r12,0; ld r12,disp(r12); mtctr r12;
  //! bctrl` followed by an 8-byte inline slot holding `address` in the target
  //! endianness. The slot is 8-byte aligned, so the sequence is self-contained
  //! and needs no external relocations.
  ASMJIT_API Error callHelper(uint64_t address);
  //! Tail call: materializes `address` in r12 and emits `mtctr r12; bctr`.
  ASMJIT_API Error tailCall(uint64_t address);
  //! ELFv1 tail call through a function descriptor: `ld r12,0(r11);
  //! ld r2,8(r11); mtctr r12; bctr`.
  ASMJIT_API Error tailCallDescriptor(uint64_t ptr);
  //! Long branch: PC-relative tail branch to any 64-bit address, using the
  //! same self-contained inline-slot sequence as `callHelper` (`addpcis r12,0;
  //! ld r12,disp(r12); mtctr r12; bctr` plus an 8-byte slot after the branch).
  ASMJIT_API Error bLong(uint64_t address);
  //! `bctr` (branch to CTR, no link).
  ASMJIT_API Error bctr();
  //! `mflr rt`.
  ASMJIT_API Error mflr(Gp rt);
  //! `mtlr rs`.
  ASMJIT_API Error mtlr(Gp rs);
  //! `blr` (branch to LR).
  ASMJIT_API Error blr();
  //! `nop`.
  ASMJIT_API Error nop();

  //! \}

  //! \name Memory Instructions
  //! \{

  //! `ld rt, ds(ra)` (doubleword load; DS-form, ds must be a multiple of 4).
  ASMJIT_API Error ld(Gp rt, const Mem& m);
  //! `std rs, ds(ra)` (doubleword store; DS-form, ds must be a multiple of 4).
  ASMJIT_API Error std(Gp rs, const Mem& m);
  //! `stdu rs, ds(ra)` (doubleword store with update).
  ASMJIT_API Error stdu(Gp rs, const Mem& m);
  //! `lwz rt, d(ra)` (word load, zero-extended).
  ASMJIT_API Error lwz(Gp rt, const Mem& m);
  //! `stw rs, d(ra)`.
  ASMJIT_API Error stw(Gp rs, const Mem& m);
  //! `lbz rt, d(ra)`.
  ASMJIT_API Error lbz(Gp rt, const Mem& m);
  //! `lbzu rt, d(ra)`.
  ASMJIT_API Error lbzu(Gp rt, const Mem& m);
  //! `stb rs, d(ra)`.
  ASMJIT_API Error stb(Gp rs, const Mem& m);
  //! `stbu rs, d(ra)`.
  ASMJIT_API Error stbu(Gp rs, const Mem& m);
  //! `lhz rt, d(ra)`.
  ASMJIT_API Error lhz(Gp rt, const Mem& m);
  //! `lhzu rt, d(ra)`.
  ASMJIT_API Error lhzu(Gp rt, const Mem& m);
  //! `lha rt, d(ra)` (word load, sign-extended).
  ASMJIT_API Error lha(Gp rt, const Mem& m);
  //! `lhau rt, d(ra)`.
  ASMJIT_API Error lhau(Gp rt, const Mem& m);
  //! `sth rs, d(ra)`.
  ASMJIT_API Error sth(Gp rs, const Mem& m);
  //! `sthu rs, d(ra)`.
  ASMJIT_API Error sthu(Gp rs, const Mem& m);
  //! `lwzu rt, d(ra)`.
  ASMJIT_API Error lwzu(Gp rt, const Mem& m);
  //! `stwu rs, d(ra)`.
  ASMJIT_API Error stwu(Gp rs, const Mem& m);
  //! `lwa rt, ds(ra)` (word load algebraic, DS-form).
  ASMJIT_API Error lwa(Gp rt, const Mem& m);
  //! `ldu rt, ds(ra)` (doubleword load with update, DS-form).
  ASMJIT_API Error ldu(Gp rt, const Mem& m);
  //! `lbzx rt, ra, rb` (byte load indexed).
  ASMJIT_API Error lbzx(Gp rt, const Mem& m);
  //! `lbzux rt, ra, rb` (byte load with update indexed).
  ASMJIT_API Error lbzux(Gp rt, const Mem& m);
  //! `lhzx rt, ra, rb` (halfword load indexed).
  ASMJIT_API Error lhzx(Gp rt, const Mem& m);
  //! `lhzux rt, ra, rb` (halfword load with update indexed).
  ASMJIT_API Error lhzux(Gp rt, const Mem& m);
  //! `lhax rt, ra, rb` (halfword load algebraic indexed).
  ASMJIT_API Error lhax(Gp rt, const Mem& m);
  //! `lhaux rt, ra, rb` (halfword load algebraic with update indexed).
  ASMJIT_API Error lhaux(Gp rt, const Mem& m);
  //! `lwzx rt, ra, rb` (word load indexed).
  ASMJIT_API Error lwzx(Gp rt, const Mem& m);
  //! `lwzux rt, ra, rb` (word load with update indexed).
  ASMJIT_API Error lwzux(Gp rt, const Mem& m);
  //! `lwax rt, ra, rb` (word load algebraic indexed).
  ASMJIT_API Error lwax(Gp rt, const Mem& m);
  //! `ldx rt, ra, rb` (doubleword load indexed).
  ASMJIT_API Error ldx(Gp rt, const Mem& m);
  //! `ldux rt, ra, rb` (doubleword load with update indexed).
  ASMJIT_API Error ldux(Gp rt, const Mem& m);
  //! `stbx rs, ra, rb` (byte store indexed).
  ASMJIT_API Error stbx(Gp rs, const Mem& m);
  //! `stbux rs, ra, rb` (byte store with update indexed).
  ASMJIT_API Error stbux(Gp rs, const Mem& m);
  //! `sthx rs, ra, rb` (halfword store indexed).
  ASMJIT_API Error sthx(Gp rs, const Mem& m);
  //! `sthux rs, ra, rb` (halfword store with update indexed).
  ASMJIT_API Error sthux(Gp rs, const Mem& m);
  //! `stwx rs, ra, rb` (word store indexed).
  ASMJIT_API Error stwx(Gp rs, const Mem& m);
  //! `stwux rs, ra, rb` (word store with update indexed).
  ASMJIT_API Error stwux(Gp rs, const Mem& m);
  //! `stdx rs, ra, rb` (doubleword store indexed).
  ASMJIT_API Error stdx(Gp rs, const Mem& m);
  //! `stdux rs, ra, rb` (doubleword store with update indexed).
  ASMJIT_API Error stdux(Gp rs, const Mem& m);

  //! `lhbrx rt, ra, rb` (load halfword byte-reversed indexed).
  ASMJIT_API Error lhbrx(Gp rt, const Mem& m);
  //! `lwbrx rt, ra, rb` (load word byte-reversed indexed).
  ASMJIT_API Error lwbrx(Gp rt, const Mem& m);
  //! `ldbrx rt, ra, rb` (load doubleword byte-reversed indexed).
  ASMJIT_API Error ldbrx(Gp rt, const Mem& m);
  //! `sthbrx rs, ra, rb` (store halfword byte-reversed indexed).
  ASMJIT_API Error sthbrx(Gp rs, const Mem& m);
  //! `stwbrx rs, ra, rb` (store word byte-reversed indexed).
  ASMJIT_API Error stwbrx(Gp rs, const Mem& m);
  //! `stdbrx rs, ra, rb` (store doubleword byte-reversed indexed).
  ASMJIT_API Error stdbrx(Gp rs, const Mem& m);

  //! `lmw rt, d(ra)` (load multiple words).
  ASMJIT_API Error lmw(Gp rt, const Mem& m);
  //! `stmw rs, d(ra)` (store multiple words).
  ASMJIT_API Error stmw(Gp rs, const Mem& m);
  //! `lswi rt, ra, nb` (load string word immediate).
  ASMJIT_API Error lswi(Gp rt, Gp ra, uint8_t nb);
  //! `lswx rt, ra, rb` (load string word indexed).
  ASMJIT_API Error lswx(Gp rt, const Mem& m);
  //! `stswi rs, ra, nb` (store string word immediate).
  ASMJIT_API Error stswi(Gp rs, Gp ra, uint8_t nb);
  //! `stswx rs, ra, rb` (store string word indexed).
  ASMJIT_API Error stswx(Gp rs, const Mem& m);

  //! `lwarx rt, ra, rb` (load word and reserve).
  ASMJIT_API Error lwarx(Gp rt, const Mem& m);
  //! `ldarx rt, ra, rb` (load doubleword and reserve).
  ASMJIT_API Error ldarx(Gp rt, const Mem& m);
  //! `lbarx rt, ra, rb, eh` (load byte and reserve).
  ASMJIT_API Error lbarx(Gp rt, const Mem& m, uint32_t eh = 0);
  //! `lharx rt, ra, rb, eh` (load halfword and reserve).
  ASMJIT_API Error lharx(Gp rt, const Mem& m, uint32_t eh = 0);
  //! `stwcx. rs, ra, rb` (store word conditional).
  ASMJIT_API Error stwcx_(Gp rs, const Mem& m);
  //! `stdcx. rs, ra, rb` (store doubleword conditional).
  ASMJIT_API Error stdcx_(Gp rs, const Mem& m);
  //! `stbcx. rs, ra, rb` (store byte conditional).
  ASMJIT_API Error stbcx_(Gp rs, const Mem& m);
  //! `sthcx. rs, ra, rb` (store halfword conditional).
  ASMJIT_API Error sthcx_(Gp rs, const Mem& m);

  //! \}

  //! \name Rotate Instructions
  //! \{

  //! `rldicl ra, rs, sh, mb`.
  ASMJIT_API Error rldicl(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc = false);
  //! `rldicr ra, rs, sh, me`.
  ASMJIT_API Error rldicr(Gp ra, Gp rs, uint8_t sh, uint8_t me, bool rc = false);
  //! `rldic ra, rs, sh, mb`.
  ASMJIT_API Error rldic(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc = false);
  //! `rlwinm ra, rs, sh, mb, me`.
  ASMJIT_API Error rlwinm(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me, bool rc = false);
  //! `rlwimi ra, rs, sh, mb, me`.
  ASMJIT_API Error rlwimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me, bool rc = false);
  //! `rldimi ra, rs, sh, mb`.
  ASMJIT_API Error rldimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc = false);
  //! `rlwnm ra, rs, rb, mb, me` (rotate-left word variable and mask).
  ASMJIT_API Error rlwnm(Gp ra, Gp rs, Gp rb, uint8_t mb, uint8_t me, bool rc = false);
  //! `rldcl ra, rs, rb, mb` (rotate-left doubleword variable and mask).
  ASMJIT_API Error rldcl(Gp ra, Gp rs, Gp rb, uint8_t mb, bool rc = false);
  //! `rldcr ra, rs, rb, me` (rotate-right doubleword variable and mask).
  ASMJIT_API Error rldcr(Gp ra, Gp rs, Gp rb, uint8_t me, bool rc = false);

  //! \}

  //! \name Mask Idioms
  //! \{

  //! `clrldi ra, rs, n` (clear left doubleword immediate).
  ASMJIT_API Error clrldi(Gp ra, Gp rs, uint8_t n);
  //! `clrrdi ra, rs, n` (clear right doubleword immediate).
  ASMJIT_API Error clrrdi(Gp ra, Gp rs, uint8_t n);
  //! `rotldi ra, rs, n` (rotate left doubleword immediate).
  ASMJIT_API Error rotldi(Gp ra, Gp rs, uint8_t n);
  //! `rotrdi ra, rs, n` (rotate right doubleword immediate).
  ASMJIT_API Error rotrdi(Gp ra, Gp rs, uint8_t n);
  //! `extldi ra, rs, n, b` (extract and left justify).
  ASMJIT_API Error extldi(Gp ra, Gp rs, uint8_t n, uint8_t b);
  //! `extrdi ra, rs, n, b` (extract and right justify).
  ASMJIT_API Error extrdi(Gp ra, Gp rs, uint8_t n, uint8_t b);
  //! `insrdi ra, rs, n, b` (insert right).
  ASMJIT_API Error insrdi(Gp ra, Gp rs, uint8_t n, uint8_t b);

  //! \}

  //! \name Barriers
  //! \{

  ASMJIT_API Error sync();
  ASMJIT_API Error lwsync();
  ASMJIT_API Error isync();
  ASMJIT_API Error eieio();
  //! `wait wc` (wait for event).
  ASMJIT_API Error wait(uint32_t wc = 0);

  //! \}

  //! \name Storage Control Instructions
  //! \{

  //! `dcbf ra, rb` (data cache block flush).
  ASMJIT_API Error dcbf(Gp ra, Gp rb);
  //! `dcbst ra, rb` (data cache block store).
  ASMJIT_API Error dcbst(Gp ra, Gp rb);
  //! `dcbt ra, rb, th` (data cache block touch).
  ASMJIT_API Error dcbt(Gp ra, Gp rb, uint8_t th = 0);
  //! `dcbtst ra, rb, th` (data cache block touch for store).
  ASMJIT_API Error dcbtst(Gp ra, Gp rb, uint8_t th = 0);
  //! `dcbz ra, rb` (data cache block zero).
  ASMJIT_API Error dcbz(Gp ra, Gp rb);
  //! `icbi ra, rb` (instruction cache block invalidate).
  ASMJIT_API Error icbi(Gp ra, Gp rb);
  //! `icbt ct, ra, rb` (instruction cache block touch).
  ASMJIT_API Error icbt(uint32_t ct, Gp ra, Gp rb);

  //! \}

  //! \name Bit Counts & Extends
  //! \{

  //! `cntlzw ra, rs`.
  ASMJIT_API Error cntlzw(Gp ra, Gp rs, bool rc = false);
  //! `cntlzd ra, rs`.
  ASMJIT_API Error cntlzd(Gp ra, Gp rs, bool rc = false);
  //! `cnttzw ra, rs` (count trailing zeros word).
  ASMJIT_API Error cnttzw(Gp ra, Gp rs, bool rc = false);
  //! `cnttzd ra, rs`.
  ASMJIT_API Error cnttzd(Gp ra, Gp rs, bool rc = false);
  //! `popcntd ra, rs`.
  ASMJIT_API Error popcntd(Gp ra, Gp rs);
  //! `popcntb ra, rs` (population count bytes).
  ASMJIT_API Error popcntb(Gp ra, Gp rs);
  //! `popcntw ra, rs` (population count words).
  ASMJIT_API Error popcntw(Gp ra, Gp rs);
  //! `prtyd ra, rs` (parity doubleword).
  ASMJIT_API Error prtyd(Gp ra, Gp rs);
  //! `prtyw ra, rs` (parity word).
  ASMJIT_API Error prtyw(Gp ra, Gp rs);
  //! `bpermd ra, rs, rb` (bit permute doubleword).
  ASMJIT_API Error bpermd(Gp ra, Gp rs, Gp rb);
  //! `extsb ra, rs`.
  ASMJIT_API Error extsb(Gp ra, Gp rs, bool rc = false);
  //! `extsh ra, rs`.
  ASMJIT_API Error extsh(Gp ra, Gp rs, bool rc = false);

  //! \}

  //! \name System Register Instructions
  //! \{

  //! `mfcr rt` (move from condition register).
  ASMJIT_API Error mfcr(Gp rt);
  //! `mtcrf fxm, rs` (move to condition register fields).
  ASMJIT_API Error mtcrf(uint32_t fxm, Gp rs);
  //! `mfocrf rt, fxm` (move from one condition register field).
  ASMJIT_API Error mfocrf(Gp rt, uint32_t fxm);
  //! `mtocrf fxm, rs` (move to one condition register field).
  ASMJIT_API Error mtocrf(uint32_t fxm, Gp rs);
  //! `mcrxrx bf` (move to CR from XER).
  ASMJIT_API Error mcrxrx(uint32_t bf);
  //! `mfspr rt, spr` (move from special purpose register).
  ASMJIT_API Error mfspr(Gp rt, uint32_t spr);
  //! `mtspr spr, rs` (move to special purpose register).
  ASMJIT_API Error mtspr(uint32_t spr, Gp rs);
  //! `mfxer rt` (move from XER).
  ASMJIT_API Error mfxer(Gp rt);
  //! `mtxer rs` (move to XER).
  ASMJIT_API Error mtxer(Gp rs);
  //! `setb rt, bfa` (set boolean).
  ASMJIT_API Error setb(Gp rt, uint32_t bfa);

  //! \}

  //! \name Condition Register Instructions
  //! \{

  //! `mcrf bf, bfa` (move condition register field).
  ASMJIT_API Error mcrf(uint32_t bf, uint32_t bfa);
  //! `crand bt, ba, bb`.
  ASMJIT_API Error crand(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crandc bt, ba, bb`.
  ASMJIT_API Error crandc(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crnor bt, ba, bb`.
  ASMJIT_API Error crnor(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `creqv bt, ba, bb`.
  ASMJIT_API Error creqv(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crnand bt, ba, bb`.
  ASMJIT_API Error crnand(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `cror bt, ba, bb`.
  ASMJIT_API Error cror(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crorc bt, ba, bb`.
  ASMJIT_API Error crorc(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crxor bt, ba, bb`.
  ASMJIT_API Error crxor(uint32_t bt, uint32_t ba, uint32_t bb);
  //! `crclr bt` (clear condition register bit).
  ASMJIT_API Error crclr(uint32_t bt);
  //! `crset bt` (set condition register bit).
  ASMJIT_API Error crset(uint32_t bt);
  //! `crmove bt, ba` (move condition register bit).
  ASMJIT_API Error crmove(uint32_t bt, uint32_t ba);
  //! `crnot bt, ba` (negate condition register bit).
  ASMJIT_API Error crnot(uint32_t bt, uint32_t ba);

  //! \}

  //! \name Miscellaneous Integer Instructions
  //! \{

  //! `sc lev` (system call).
  ASMJIT_API Error sc(uint32_t lev = 0);
  //! `scv lev` (system call vectored).
  ASMJIT_API Error scv(uint32_t lev = 0);
  //! `addg6s rt, ra, rb` (add and generate sixes).
  ASMJIT_API Error addg6s(Gp rt, Gp ra, Gp rb);
  //! `cbcdtd ra, rs` (convert binary coded decimal to decimal).
  ASMJIT_API Error cbcdtd(Gp ra, Gp rs);
  //! `cdtbcd ra, rs` (convert decimal to binary coded decimal).
  ASMJIT_API Error cdtbcd(Gp ra, Gp rs);

  //! \}

  //! \name Floating-Point Memory Instructions
  //! \{

  //! `lfs frt, d(ra)` (load floating single).
  ASMJIT_API Error lfs(Fp frt, const Mem& m);
  //! `lfsu frt, d(ra)` (load floating single with update).
  ASMJIT_API Error lfsu(Fp frt, const Mem& m);
  //! `lfsx frt, ra, rb` (load floating single indexed).
  ASMJIT_API Error lfsx(Fp frt, const Mem& m);
  //! `lfsux frt, ra, rb` (load floating single with update indexed).
  ASMJIT_API Error lfsux(Fp frt, const Mem& m);
  //! `lfd frt, d(ra)` (load floating double).
  ASMJIT_API Error lfd(Fp frt, const Mem& m);
  //! `lfdu frt, d(ra)` (load floating double with update).
  ASMJIT_API Error lfdu(Fp frt, const Mem& m);
  //! `lfdx frt, ra, rb` (load floating double indexed).
  ASMJIT_API Error lfdx(Fp frt, const Mem& m);
  //! `lfdux frt, ra, rb` (load floating double with update indexed).
  ASMJIT_API Error lfdux(Fp frt, const Mem& m);
  //! `lfiwax frt, ra, rb` (load floating-point integer word algebraic indexed).
  ASMJIT_API Error lfiwax(Fp frt, const Mem& m);
  //! `lfiwzx frt, ra, rb` (load floating-point integer word and zero indexed).
  ASMJIT_API Error lfiwzx(Fp frt, const Mem& m);
  //! `stfs frs, d(ra)` (store floating single).
  ASMJIT_API Error stfs(Fp frs, const Mem& m);
  //! `stfsu frs, d(ra)` (store floating single with update).
  ASMJIT_API Error stfsu(Fp frs, const Mem& m);
  //! `stfsx frs, ra, rb` (store floating single indexed).
  ASMJIT_API Error stfsx(Fp frs, const Mem& m);
  //! `stfsux frs, ra, rb` (store floating single with update indexed).
  ASMJIT_API Error stfsux(Fp frs, const Mem& m);
  //! `stfd frs, d(ra)` (store floating double).
  ASMJIT_API Error stfd(Fp frs, const Mem& m);
  //! `stfdu frs, d(ra)` (store floating double with update).
  ASMJIT_API Error stfdu(Fp frs, const Mem& m);
  //! `stfdx frs, ra, rb` (store floating double indexed).
  ASMJIT_API Error stfdx(Fp frs, const Mem& m);
  //! `stfdux frs, ra, rb` (store floating double with update indexed).
  ASMJIT_API Error stfdux(Fp frs, const Mem& m);
  //! `stfiwx frs, ra, rb` (store floating-point as integer word indexed).
  ASMJIT_API Error stfiwx(Fp frs, const Mem& m);

  //! \}

  //! \name Floating-Point Move Instructions
  //! \{

  //! `fmr frt, frb`.
  ASMJIT_API Error fmr(Fp frt, Fp frb, bool rc = false);
  //! `fneg frt, frb`.
  ASMJIT_API Error fneg(Fp frt, Fp frb, bool rc = false);
  //! `fabs frt, frb`.
  ASMJIT_API Error fabs(Fp frt, Fp frb, bool rc = false);
  //! `fnabs frt, frb`.
  ASMJIT_API Error fnabs(Fp frt, Fp frb, bool rc = false);
  //! `fcpsgn frt, fra, frb` (copy sign).
  ASMJIT_API Error fcpsgn(Fp frt, Fp fra, Fp frb);

  //! \}

  //! \name Floating-Point Arithmetic Instructions
  //! \{

  //! `fadd frt, fra, frb` (double).
  ASMJIT_API Error fadd(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fadds frt, fra, frb` (single).
  ASMJIT_API Error fadds(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fsub frt, fra, frb`.
  ASMJIT_API Error fsub(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fsubs frt, fra, frb`.
  ASMJIT_API Error fsubs(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fmul frt, fra, frc`.
  ASMJIT_API Error fmul(Fp frt, Fp fra, Fp frc, bool rc = false);
  //! `fmuls frt, fra, frc`.
  ASMJIT_API Error fmuls(Fp frt, Fp fra, Fp frc, bool rc = false);
  //! `fdiv frt, fra, frb`.
  ASMJIT_API Error fdiv(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fdivs frt, fra, frb`.
  ASMJIT_API Error fdivs(Fp frt, Fp fra, Fp frb, bool rc = false);
  //! `fsqrt frt, frb`.
  ASMJIT_API Error fsqrt(Fp frt, Fp frb, bool rc = false);
  //! `fsqrts frt, frb`.
  ASMJIT_API Error fsqrts(Fp frt, Fp frb, bool rc = false);
  //! `fre frt, frb` (reciprocal estimate).
  ASMJIT_API Error fre(Fp frt, Fp frb, bool rc = false);
  //! `fres frt, frb`.
  ASMJIT_API Error fres(Fp frt, Fp frb, bool rc = false);
  //! `frsqrte frt, frb` (reciprocal square root estimate).
  ASMJIT_API Error frsqrte(Fp frt, Fp frb, bool rc = false);
  //! `frsqrtes frt, frb`.
  ASMJIT_API Error frsqrtes(Fp frt, Fp frb, bool rc = false);
  //! `fmadd frt, fra, frc, frb` (frt = fra*frc + frb).
  ASMJIT_API Error fmadd(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fmadds frt, fra, frc, frb`.
  ASMJIT_API Error fmadds(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fmsub frt, fra, frc, frb`.
  ASMJIT_API Error fmsub(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fmsubs frt, fra, frc, frb`.
  ASMJIT_API Error fmsubs(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fnmadd frt, fra, frc, frb`.
  ASMJIT_API Error fnmadd(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fnmadds frt, fra, frc, frb`.
  ASMJIT_API Error fnmadds(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fnmsub frt, fra, frc, frb`.
  ASMJIT_API Error fnmsub(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);
  //! `fnmsubs frt, fra, frc, frb`.
  ASMJIT_API Error fnmsubs(Fp frt, Fp fra, Fp frc, Fp frb, bool rc = false);

  //! \}

  //! \name Floating-Point Rounding & Conversion Instructions
  //! \{

  //! `frin frt, frb` (round to nearest).
  ASMJIT_API Error frin(Fp frt, Fp frb);
  //! `friz frt, frb` (round toward zero).
  ASMJIT_API Error friz(Fp frt, Fp frb);
  //! `frip frt, frb` (round toward plus infinity).
  ASMJIT_API Error frip(Fp frt, Fp frb);
  //! `frim frt, frb` (round toward minus infinity).
  ASMJIT_API Error frim(Fp frt, Fp frb);
  //! `frsp frt, frb` (round to single precision).
  ASMJIT_API Error frsp(Fp frt, Fp frb);
  //! `fcfid frt, frb` (convert to double from signed integer doubleword).
  ASMJIT_API Error fcfid(Fp frt, Fp frb);
  //! `fcfidu frt, frb` (convert to double from unsigned integer doubleword).
  ASMJIT_API Error fcfidu(Fp frt, Fp frb);
  //! `fcfids frt, frb` (convert to single from signed integer doubleword).
  ASMJIT_API Error fcfids(Fp frt, Fp frb);
  //! `fcfidus frt, frb` (convert to single from unsigned integer doubleword).
  ASMJIT_API Error fcfidus(Fp frt, Fp frb);
  //! `fctiw frt, frb` (convert to integer word, FPSCR rounding).
  ASMJIT_API Error fctiw(Fp frt, Fp frb);
  //! `fctiwz frt, frb` (convert to integer word, truncate).
  ASMJIT_API Error fctiwz(Fp frt, Fp frb);
  //! `fctiwu frt, frb` (convert to unsigned integer word).
  ASMJIT_API Error fctiwu(Fp frt, Fp frb);
  //! `fctiwuz frt, frb` (convert to unsigned integer word, truncate).
  ASMJIT_API Error fctiwuz(Fp frt, Fp frb);
  //! `fctid frt, frb` (convert to integer doubleword).
  ASMJIT_API Error fctid(Fp frt, Fp frb);
  //! `fctidz frt, frb` (convert to integer doubleword, truncate).
  ASMJIT_API Error fctidz(Fp frt, Fp frb);
  //! `fctidu frt, frb` (convert to unsigned integer doubleword).
  ASMJIT_API Error fctidu(Fp frt, Fp frb);
  //! `fctiduz frt, frb` (convert to unsigned integer doubleword, truncate).
  ASMJIT_API Error fctiduz(Fp frt, Fp frb);

  //! \}

  //! \name Floating-Point Compare & Select Instructions
  //! \{

  //! `fcmpu bf, fra, frb` (floating compare unordered).
  ASMJIT_API Error fcmpu(uint32_t bf, Fp fra, Fp frb);
  //! `fcmpo bf, fra, frb` (floating compare ordered).
  ASMJIT_API Error fcmpo(uint32_t bf, Fp fra, Fp frb);
  //! `ftdiv bf, fra, frb` (floating test divide).
  ASMJIT_API Error ftdiv(uint32_t bf, Fp fra, Fp frb);
  //! `ftsqrt bf, frb` (floating test square root).
  ASMJIT_API Error ftsqrt(uint32_t bf, Fp frb);
  //! `fsel frt, fra, frc, frb` (floating select).
  ASMJIT_API Error fsel(Fp frt, Fp fra, Fp frc, Fp frb);

  //! \}

  //! \name Floating-Point Status & Control Register Instructions
  //! \{

  //! `mffs frt` (move from FPSCR).
  ASMJIT_API Error mffs(Fp frt, bool rc = false);
  //! `mffsce frt` (move from FPSCR and clear enables).
  ASMJIT_API Error mffsce(Fp frt);
  //! `mffsl frt` (move from FPSCR lightweight).
  ASMJIT_API Error mffsl(Fp frt);
  //! `mffscrn frt, frb` (move from FPSCR and set RN).
  ASMJIT_API Error mffscrn(Fp frt, Fp frb);
  //! `mffscrni frt, rm` (move from FPSCR and set RN immediate).
  ASMJIT_API Error mffscrni(Fp frt, uint32_t rm);
  //! `mffscdrn frt, frb` (move from FPSCR and set DRN).
  ASMJIT_API Error mffscdrn(Fp frt, Fp frb);
  //! `mffscdrni frt, drm` (move from FPSCR and set DRN immediate).
  ASMJIT_API Error mffscdrni(Fp frt, uint32_t drm);
  //! `mtfsf fxm, frb` (move to FPSCR fields).
  ASMJIT_API Error mtfsf(uint32_t fxm, Fp frb);
  //! `mtfsb0 crb` (move to FPSCR bit 0).
  ASMJIT_API Error mtfsb0(uint32_t crb);
  //! `mtfsb1 crb` (move to FPSCR bit 1).
  ASMJIT_API Error mtfsb1(uint32_t crb);
  //! `mtfsfi bf, imm` (move to FPSCR field immediate).
  ASMJIT_API Error mtfsfi(uint32_t bf, uint32_t imm);
  //! `mcrfs bf, bfa` (move to CR from FPSCR).
  ASMJIT_API Error mcrfs(uint32_t bf, uint32_t bfa);
  //! Branches to `bail` if the value in `fr` is a NaN, infinity, or denormal
  //! (zeros and normals pass). Used by JITs to send special FP values to a
  //! slow path; clobbers r0, r11, and r12, and uses the red zone at -8(r1).
  ASMJIT_API Error bailIfFPSpecial(Fp fr, const Label& bail);

  //! \}

  //! \name VSX Move Instructions
  //! \{

  //! `mfvsrd rt, xs` (move from VSR doubleword).
  ASMJIT_API Error mfvsrd(Gp rt, Vsx xs);
  //! `mfvsrld rt, xs` (move from VSR left doubleword).
  ASMJIT_API Error mfvsrld(Gp rt, Vsx xs);
  //! `mfvsrwz rt, xs` (move from VSR word and zero).
  ASMJIT_API Error mfvsrwz(Gp rt, Vsx xs);
  //! `mtvsrd xt, ra` (move to VSR doubleword).
  ASMJIT_API Error mtvsrd(Vsx xt, Gp ra);
  //! `mtvsrdd xt, ra, rb` (move to VSR double doubleword).
  ASMJIT_API Error mtvsrdd(Vsx xt, Gp ra, Gp rb);
  //! `mtvsrwa xt, ra` (move to VSR word algebraic).
  ASMJIT_API Error mtvsrwa(Vsx xt, Gp ra);
  //! `mtvsrws xt, ra` (move to VSR word signed).
  ASMJIT_API Error mtvsrws(Vsx xt, Gp ra);
  //! `mtvsrwz xt, ra` (move to VSR word and zero).
  ASMJIT_API Error mtvsrwz(Vsx xt, Gp ra);
  //! `mfvrd rt, vr` (move from vector register doubleword).
  ASMJIT_API Error mfvrd(Gp rt, Vr vr);
  //! `mfvrwz rt, vr` (move from vector register word and zero).
  ASMJIT_API Error mfvrwz(Gp rt, Vr vr);
  //! `mtvrd vr, rt` (move to vector register doubleword).
  ASMJIT_API Error mtvrd(Vr vr, Gp rt);
  //! `mtvrwa vr, rt` (move to vector register word algebraic).
  ASMJIT_API Error mtvrwa(Vr vr, Gp rt);
  //! `mtvrwz vr, rt` (move to vector register word and zero).
  ASMJIT_API Error mtvrwz(Vr vr, Gp rt);

  //! \}

  //! \name VSX Memory Instructions
  //! \{

  //! `lxsd xt, ds(ra)` (load VSX scalar doubleword; ds multiple of 4).
  ASMJIT_API Error lxsd(Vsx xt, const Mem& m);
  //! `lxssp xt, ds(ra)` (load VSX scalar single).
  ASMJIT_API Error lxssp(Vsx xt, const Mem& m);
  //! `lxsdx xt, ra, rb` (load VSX scalar doubleword indexed).
  ASMJIT_API Error lxsdx(Vsx xt, const Mem& m);
  //! `lxsspx xt, ra, rb` (load VSX scalar single indexed).
  ASMJIT_API Error lxsspx(Vsx xt, const Mem& m);
  //! `lxsiwzx xt, ra, rb` (load VSX scalar as integer word and zero indexed).
  ASMJIT_API Error lxsiwzx(Vsx xt, const Mem& m);
  //! `lxsiwax xt, ra, rb` (load VSX scalar as integer word algebraic indexed).
  ASMJIT_API Error lxsiwax(Vsx xt, const Mem& m);
  //! `lxv xt, dq(ra)` (load VSX vector; dq multiple of 16).
  ASMJIT_API Error lxv(Vsx xt, const Mem& m);
  //! `lxvx xt, ra, rb` (load VSX vector indexed).
  ASMJIT_API Error lxvx(Vsx xt, const Mem& m);
  //! `stxsd xs, ds(ra)` (store VSX scalar doubleword).
  ASMJIT_API Error stxsd(Vsx xs, const Mem& m);
  //! `stxssp xs, ds(ra)` (store VSX scalar single).
  ASMJIT_API Error stxssp(Vsx xs, const Mem& m);
  //! `stxsdx xs, ra, rb` (store VSX scalar doubleword indexed).
  ASMJIT_API Error stxsdx(Vsx xs, const Mem& m);
  //! `stxsspx xs, ra, rb` (store VSX scalar single indexed).
  ASMJIT_API Error stxsspx(Vsx xs, const Mem& m);
  //! `stxsiwx xs, ra, rb` (store VSX scalar as integer word indexed).
  ASMJIT_API Error stxsiwx(Vsx xs, const Mem& m);
  //! `stxv xs, dq(ra)` (store VSX vector).
  ASMJIT_API Error stxv(Vsx xs, const Mem& m);
  //! `stxvx xs, ra, rb` (store VSX vector indexed).
  ASMJIT_API Error stxvx(Vsx xs, const Mem& m);

  //! \}

  //! \name VSX Scalar Floating-Point Instructions
  //! \{

  //! `xsabsdp xt, xb`.
  ASMJIT_API Error xsabsdp(Vsx xt, Vsx xb);
  //! `xsadddp xt, xa, xb`.
  ASMJIT_API Error xsadddp(Vsx xt, Vsx xa, Vsx xb);
  //! `xscpsgndp xt, xa, xb` (copy sign).
  ASMJIT_API Error xscpsgndp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsdivdp xt, xa, xb`.
  ASMJIT_API Error xsdivdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmaddadp xt, xa, xb` (fused multiply-add).
  ASMJIT_API Error xsmaddadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmaddmdp xt, xa, xb`.
  ASMJIT_API Error xsmaddmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmsubadp xt, xa, xb`.
  ASMJIT_API Error xsmsubadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmsubmdp xt, xa, xb`.
  ASMJIT_API Error xsmsubmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmuldp xt, xa, xb`.
  ASMJIT_API Error xsmuldp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnegdp xt, xb`.
  ASMJIT_API Error xsnegdp(Vsx xt, Vsx xb);
  //! `xsnmsubadp xt, xa, xb`.
  ASMJIT_API Error xsnmsubadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmsubmdp xt, xa, xb`.
  ASMJIT_API Error xsnmsubmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmaddadp xt, xa, xb`.
  ASMJIT_API Error xsnmaddadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmaddmdp xt, xa, xb`.
  ASMJIT_API Error xsnmaddmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xssqrtdp xt, xb`.
  ASMJIT_API Error xssqrtdp(Vsx xt, Vsx xb);
  //! `xssubdp xt, xa, xb`.
  ASMJIT_API Error xssubdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsaddsp xt, xa, xb` (single precision).
  ASMJIT_API Error xsaddsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsdivsp xt, xa, xb`.
  ASMJIT_API Error xsdivsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmaddasp xt, xa, xb`.
  ASMJIT_API Error xsmaddasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmaddmsp xt, xa, xb`.
  ASMJIT_API Error xsmaddmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmsubasp xt, xa, xb`.
  ASMJIT_API Error xsmsubasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmsubmsp xt, xa, xb`.
  ASMJIT_API Error xsmsubmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsmulsp xt, xa, xb`.
  ASMJIT_API Error xsmulsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmsubasp xt, xa, xb`.
  ASMJIT_API Error xsnmsubasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmsubmsp xt, xa, xb`.
  ASMJIT_API Error xsnmsubmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmaddasp xt, xa, xb`.
  ASMJIT_API Error xsnmaddasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xsnmaddmsp xt, xa, xb`.
  ASMJIT_API Error xsnmaddmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xssqrtsp xt, xb`.
  ASMJIT_API Error xssqrtsp(Vsx xt, Vsx xb);
  //! `xssubsp xt, xa, xb`.
  ASMJIT_API Error xssubsp(Vsx xt, Vsx xa, Vsx xb);

  //! \}

  //! \name VSX Compare, Round & Convert Instructions
  //! \{

  //! `xscmpudp bf, xa, xb` (compare unordered).
  ASMJIT_API Error xscmpudp(uint32_t bf, Vsx xa, Vsx xb);
  //! `xscmpodp bf, xa, xb` (compare ordered).
  ASMJIT_API Error xscmpodp(uint32_t bf, Vsx xa, Vsx xb);
  //! `xscmpeqdp xt, xa, xb`.
  ASMJIT_API Error xscmpeqdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xscmpgedp xt, xa, xb`.
  ASMJIT_API Error xscmpgedp(Vsx xt, Vsx xa, Vsx xb);
  //! `xscmpgtdp xt, xa, xb`.
  ASMJIT_API Error xscmpgtdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xstdivdp bf, xa, xb` (test divide).
  ASMJIT_API Error xstdivdp(uint32_t bf, Vsx xa, Vsx xb);
  //! `xstsqrtdp bf, xb` (test square root).
  ASMJIT_API Error xstsqrtdp(uint32_t bf, Vsx xb);
  //! `xsrdpi xt, xb` (round to nearest).
  ASMJIT_API Error xsrdpi(Vsx xt, Vsx xb);
  //! `xsrdpic xt, xb` (round to nearest with current rounding).
  ASMJIT_API Error xsrdpic(Vsx xt, Vsx xb);
  //! `xsrdpiz xt, xb` (round toward zero).
  ASMJIT_API Error xsrdpiz(Vsx xt, Vsx xb);
  //! `xsrdpip xt, xb` (round toward plus infinity).
  ASMJIT_API Error xsrdpip(Vsx xt, Vsx xb);
  //! `xsrdpim xt, xb` (round toward minus infinity).
  ASMJIT_API Error xsrdpim(Vsx xt, Vsx xb);
  //! `xscvdpsp xt, xb` (convert double to single).
  ASMJIT_API Error xscvdpsp(Vsx xt, Vsx xb);
  //! `xscvspdp xt, xb` (convert single to double).
  ASMJIT_API Error xscvspdp(Vsx xt, Vsx xb);
  //! `xscvdpsxds xt, xb` (convert double to signed doubleword).
  ASMJIT_API Error xscvdpsxds(Vsx xt, Vsx xb);
  //! `xscvdpuxds xt, xb` (convert double to unsigned doubleword).
  ASMJIT_API Error xscvdpuxds(Vsx xt, Vsx xb);
  //! `xscvsxddp xt, xb` (convert signed doubleword to double).
  ASMJIT_API Error xscvsxddp(Vsx xt, Vsx xb);
  //! `xscvuxddp xt, xb` (convert unsigned doubleword to double).
  ASMJIT_API Error xscvuxddp(Vsx xt, Vsx xb);
  //! `xscvdpsxws xt, xb` (convert double to signed word).
  ASMJIT_API Error xscvdpsxws(Vsx xt, Vsx xb);
  //! `xscvdpuxws xt, xb` (convert double to unsigned word).
  ASMJIT_API Error xscvdpuxws(Vsx xt, Vsx xb);
  //! `xscvsxdsp xt, xb` (convert signed doubleword to single).
  ASMJIT_API Error xscvsxdsp(Vsx xt, Vsx xb);
  //! `xscvuxdsp xt, xb` (convert unsigned doubleword to single).
  ASMJIT_API Error xscvuxdsp(Vsx xt, Vsx xb);

  //! \name VSX Vector Floating-Point Instructions
  //! \{

  //! `xvadddp xt, xa, xb` (vector add double-precision).
  ASMJIT_API Error xvadddp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvaddsp xt, xa, xb` (vector add single-precision).
  ASMJIT_API Error xvaddsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvsubdp xt, xa, xb` (vector subtract double-precision).
  ASMJIT_API Error xvsubdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvsubsp xt, xa, xb` (vector subtract single-precision).
  ASMJIT_API Error xvsubsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmuldp xt, xa, xb` (vector multiply double-precision).
  ASMJIT_API Error xvmuldp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmulsp xt, xa, xb` (vector multiply single-precision).
  ASMJIT_API Error xvmulsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvdivdp xt, xa, xb` (vector divide double-precision).
  ASMJIT_API Error xvdivdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvdivsp xt, xa, xb` (vector divide single-precision).
  ASMJIT_API Error xvdivsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaddadp xt, xa, xb` (vector multiply-add double-precision, A form).
  ASMJIT_API Error xvmaddadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaddmdp xt, xa, xb` (vector multiply-add double-precision, M form).
  ASMJIT_API Error xvmaddmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmsubadp xt, xa, xb` (vector multiply-subtract double-precision, A form).
  ASMJIT_API Error xvmsubadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmsubmdp xt, xa, xb` (vector multiply-subtract double-precision, M form).
  ASMJIT_API Error xvmsubmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmaddadp xt, xa, xb` (vector negative multiply-add double-precision, A form).
  ASMJIT_API Error xvnmaddadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmaddmdp xt, xa, xb` (vector negative multiply-add double-precision, M form).
  ASMJIT_API Error xvnmaddmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmsubadp xt, xa, xb` (vector negative multiply-subtract double-precision, A form).
  ASMJIT_API Error xvnmsubadp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmsubmdp xt, xa, xb` (vector negative multiply-subtract double-precision, M form).
  ASMJIT_API Error xvnmsubmdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaddasp xt, xa, xb` (vector multiply-add single-precision, A form).
  ASMJIT_API Error xvmaddasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaddmsp xt, xa, xb` (vector multiply-add single-precision, M form).
  ASMJIT_API Error xvmaddmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmsubasp xt, xa, xb` (vector multiply-subtract single-precision, A form).
  ASMJIT_API Error xvmsubasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmsubmsp xt, xa, xb` (vector multiply-subtract single-precision, M form).
  ASMJIT_API Error xvmsubmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmaddasp xt, xa, xb` (vector negative multiply-add single-precision, A form).
  ASMJIT_API Error xvnmaddasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmaddmsp xt, xa, xb` (vector negative multiply-add single-precision, M form).
  ASMJIT_API Error xvnmaddmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmsubasp xt, xa, xb` (vector negative multiply-subtract single-precision, A form).
  ASMJIT_API Error xvnmsubasp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvnmsubmsp xt, xa, xb` (vector negative multiply-subtract single-precision, M form).
  ASMJIT_API Error xvnmsubmsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaxdp xt, xa, xb` (vector maximum double-precision).
  ASMJIT_API Error xvmaxdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmindp xt, xa, xb` (vector minimum double-precision).
  ASMJIT_API Error xvmindp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvmaxsp xt, xa, xb` (vector maximum single-precision).
  ASMJIT_API Error xvmaxsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvminsp xt, xa, xb` (vector minimum single-precision).
  ASMJIT_API Error xvminsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpeqdp xt, xa, xb` (vector compare equal double-precision).
  ASMJIT_API Error xvcmpeqdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpgtdp xt, xa, xb` (vector compare greater-than double-precision).
  ASMJIT_API Error xvcmpgtdp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpgedp xt, xa, xb` (vector compare greater-than-or-equal double-precision).
  ASMJIT_API Error xvcmpgedp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpeqsp xt, xa, xb` (vector compare equal single-precision).
  ASMJIT_API Error xvcmpeqsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpgtsp xt, xa, xb` (vector compare greater-than single-precision).
  ASMJIT_API Error xvcmpgtsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcmpgesp xt, xa, xb` (vector compare greater-than-or-equal single-precision).
  ASMJIT_API Error xvcmpgesp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvabsdp xt, xb` (vector absolute double-precision).
  ASMJIT_API Error xvabsdp(Vsx xt, Vsx xb);
  //! `xvabssp xt, xb` (vector absolute single-precision).
  ASMJIT_API Error xvabssp(Vsx xt, Vsx xb);
  //! `xvnabsdp xt, xb` (vector negative absolute double-precision).
  ASMJIT_API Error xvnabsdp(Vsx xt, Vsx xb);
  //! `xvnabssp xt, xb` (vector negative absolute single-precision).
  ASMJIT_API Error xvnabssp(Vsx xt, Vsx xb);
  //! `xvnegdp xt, xb` (vector negate double-precision).
  ASMJIT_API Error xvnegdp(Vsx xt, Vsx xb);
  //! `xvnegsp xt, xb` (vector negate single-precision).
  ASMJIT_API Error xvnegsp(Vsx xt, Vsx xb);
  //! `xvcpsgndp xt, xa, xb` (vector copy sign double-precision).
  ASMJIT_API Error xvcpsgndp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvcpsgnsp xt, xa, xb` (vector copy sign single-precision).
  ASMJIT_API Error xvcpsgnsp(Vsx xt, Vsx xa, Vsx xb);
  //! `xvsqrtdp xt, xb` (vector square root double-precision).
  ASMJIT_API Error xvsqrtdp(Vsx xt, Vsx xb);
  //! `xvsqrtsp xt, xb` (vector square root single-precision).
  ASMJIT_API Error xvsqrtsp(Vsx xt, Vsx xb);
  //! `xvrsqrtedp xt, xb` (vector reciprocal square root estimate double-precision).
  ASMJIT_API Error xvrsqrtedp(Vsx xt, Vsx xb);
  //! `xvrsqrtesp xt, xb` (vector reciprocal square root estimate single-precision).
  ASMJIT_API Error xvrsqrtesp(Vsx xt, Vsx xb);
  //! `xvcvsxwdp xt, xb` (convert signed word to double-precision).
  ASMJIT_API Error xvcvsxwdp(Vsx xt, Vsx xb);
  //! `xvcvuxwdp xt, xb` (convert unsigned word to double-precision).
  ASMJIT_API Error xvcvuxwdp(Vsx xt, Vsx xb);
  //! `xvcvsxwsp xt, xb` (convert signed word to single-precision).
  ASMJIT_API Error xvcvsxwsp(Vsx xt, Vsx xb);
  //! `xvcvuxwsp xt, xb` (convert unsigned word to single-precision).
  ASMJIT_API Error xvcvuxwsp(Vsx xt, Vsx xb);
  //! `xvcvdpsxws xt, xb` (convert double-precision to signed word).
  ASMJIT_API Error xvcvdpsxws(Vsx xt, Vsx xb);
  //! `xvcvdpuxws xt, xb` (convert double-precision to unsigned word).
  ASMJIT_API Error xvcvdpuxws(Vsx xt, Vsx xb);
  //! `xvcvdpsxds xt, xb` (convert double-precision to signed doubleword).
  ASMJIT_API Error xvcvdpsxds(Vsx xt, Vsx xb);
  //! `xvcvdpuxds xt, xb` (convert double-precision to unsigned doubleword).
  ASMJIT_API Error xvcvdpuxds(Vsx xt, Vsx xb);
  //! `xvcvsxddp xt, xb` (convert signed doubleword to double-precision).
  ASMJIT_API Error xvcvsxddp(Vsx xt, Vsx xb);
  //! `xvcvuxddp xt, xb` (convert unsigned doubleword to double-precision).
  ASMJIT_API Error xvcvuxddp(Vsx xt, Vsx xb);
  //! `xvcvdpsp xt, xb` (convert double-precision to single-precision).
  ASMJIT_API Error xvcvdpsp(Vsx xt, Vsx xb);
  //! `xvcvspdp xt, xb` (convert single-precision to double-precision).
  ASMJIT_API Error xvcvspdp(Vsx xt, Vsx xb);

  //! \}

  //! \name VSX Vector Logical & Permute Instructions
  //! \{

  //! `xxlxor xt, xa, xb`.
  ASMJIT_API Error xxlxor(Vsx xt, Vsx xa, Vsx xb);
  //! `xxlor xt, xa, xb`.
  ASMJIT_API Error xxlor(Vsx xt, Vsx xa, Vsx xb);
  //! `xxland xt, xa, xb`.
  ASMJIT_API Error xxland(Vsx xt, Vsx xa, Vsx xb);
  //! `xxlandc xt, xa, xb`.
  ASMJIT_API Error xxlandc(Vsx xt, Vsx xa, Vsx xb);
  //! `xxlorc xt, xa, xb`.
  ASMJIT_API Error xxlorc(Vsx xt, Vsx xa, Vsx xb);
  //! `xxlnand xt, xa, xb`.
  ASMJIT_API Error xxlnand(Vsx xt, Vsx xa, Vsx xb);
  //! `xxlnor xt, xa, xb`.
  ASMJIT_API Error xxlnor(Vsx xt, Vsx xa, Vsx xb);
  //! `xxleqv xt, xa, xb`.
  ASMJIT_API Error xxleqv(Vsx xt, Vsx xa, Vsx xb);
  //! `xxsel xt, xa, xb, xc` (select).
  ASMJIT_API Error xxsel(Vsx xt, Vsx xa, Vsx xb, Vsx xc);
  //! `xxperm xt, xa, xb` (permute).
  ASMJIT_API Error xxperm(Vsx xt, Vsx xa, Vsx xb);
  //! `xxpermdi xt, xa, xb, dm` (permute doubleword immediate).
  ASMJIT_API Error xxpermdi(Vsx xt, Vsx xa, Vsx xb, uint32_t dm);
  //! `xxmrghd xt, xa, xb` (merge high doubleword).
  ASMJIT_API Error xxmrghd(Vsx xt, Vsx xa, Vsx xb);
  //! `xxmrgld xt, xa, xb` (merge low doubleword).
  ASMJIT_API Error xxmrgld(Vsx xt, Vsx xa, Vsx xb);
  //! `xxspltd xt, xb, uim` (splat doubleword).
  ASMJIT_API Error xxspltd(Vsx xt, Vsx xb, uint32_t uim);
  //! `xxspltw xt, xb, uim` (splat word).
  ASMJIT_API Error xxspltw(Vsx xt, Vsx xb, uint32_t uim);
  //! `xxswapd xt, xa` (swap doublewords).
  ASMJIT_API Error xxswapd(Vsx xt, Vsx xa);

  //! \}

  //! \name VMX (Altivec) Memory Instructions
  //! \{

  //! `lvx vrt, ra, rb` (load vector indexed).
  ASMJIT_API Error lvx(Vr vrt, const Mem& m);
  //! `lvebx vrt, ra, rb` (load vector element byte indexed).
  ASMJIT_API Error lvebx(Vr vrt, const Mem& m);
  //! `lvehx vrt, ra, rb` (load vector element halfword indexed).
  ASMJIT_API Error lvehx(Vr vrt, const Mem& m);
  //! `lvewx vrt, ra, rb` (load vector element word indexed).
  ASMJIT_API Error lvewx(Vr vrt, const Mem& m);
  //! `lvxl vrt, ra, rb` (load vector indexed LRU).
  ASMJIT_API Error lvxl(Vr vrt, const Mem& m);
  //! `stvx vrs, ra, rb` (store vector indexed).
  ASMJIT_API Error stvx(Vr vrs, const Mem& m);
  //! `stvebx vrs, ra, rb` (store vector element byte indexed).
  ASMJIT_API Error stvebx(Vr vrs, const Mem& m);
  //! `stvehx vrs, ra, rb` (store vector element halfword indexed).
  ASMJIT_API Error stvehx(Vr vrs, const Mem& m);
  //! `stvewx vrs, ra, rb` (store vector element word indexed).
  ASMJIT_API Error stvewx(Vr vrs, const Mem& m);
  //! `stvxl vrs, ra, rb` (store vector indexed LRU).
  ASMJIT_API Error stvxl(Vr vrs, const Mem& m);

  //! \}

  //! \name VMX (Altivec) Integer Instructions
  //! \{

  //! `vaddubm vrt, vra, vrb` and the other elementary adds.
  ASMJIT_API Error vaddubm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vadduhm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vadduwm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddudm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddcuw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsububm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubuhm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubuwm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubudm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubcuw(Vr vrt, Vr vra, Vr vrb);
  //! Vector saturating add/subtract.
  ASMJIT_API Error vaddsbs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddshs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddsws(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddubs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vadduhs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vadduws(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubsbs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubshs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubsws(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsububs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubuhs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubuws(Vr vrt, Vr vra, Vr vrb);
  //! Vector 128-bit add/subtract with carry/borrow.
  ASMJIT_API Error vaddcuq(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vadduqm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vaddeuqm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vaddecuq(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vsubcuq(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubuqm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsubeuqm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vsubecuq(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  //! Vector absolute difference.
  ASMJIT_API Error vabsdub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vabsduh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vabsduw(Vr vrt, Vr vra, Vr vrb);
  //! `vand vrt, vra, vrb` and the other vector logicals.
  ASMJIT_API Error vand(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vandc(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vor(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vxor(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vnor(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error veqv(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vnand(Vr vrt, Vr vra, Vr vrb);
  //! Vector shifts.
  ASMJIT_API Error vsl(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsr(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsld(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrd(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrad(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vslw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsraw(Vr vrt, Vr vra, Vr vrb);
  //! Vector shift by byte/halfword/element/octet/variable.
  ASMJIT_API Error vslb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vslh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vslo(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsro(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vslv(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsrv(Vr vrt, Vr vra, Vr vrb);
  //! Vector rotate and rotate with mask.
  ASMJIT_API Error vrlb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrlh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrlw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrld(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrlwmi(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrlwnm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrldmi(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vrldnm(Vr vrt, Vr vra, Vr vrb);
  //! Vector compares.
  ASMJIT_API Error vcmpequb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpequh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpequw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpequd(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtsb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtsh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtsw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtsd(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtuh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtuw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpgtud(Vr vrt, Vr vra, Vr vrb);
  //! Vector compare not equal.
  ASMJIT_API Error vcmpneb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpneh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpnew(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpnezb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpnezh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vcmpnezw(Vr vrt, Vr vra, Vr vrb);
  //! Vector min/max.
  ASMJIT_API Error vminub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminuh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminuw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminsb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminsh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminsw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxuh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxuw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxsb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxsh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxsw(Vr vrt, Vr vra, Vr vrb);
  //! Vector doubleword min/max.
  ASMJIT_API Error vmaxsd(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmaxud(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminsd(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vminud(Vr vrt, Vr vra, Vr vrb);
  //! Vector average.
  ASMJIT_API Error vavgub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vavguh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vavguw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vavgsb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vavgsh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vavgsw(Vr vrt, Vr vra, Vr vrb);
  //! Vector sum across.
  ASMJIT_API Error vsum4sbs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsum4shs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsum4ubs(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsum2sws(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vsumsws(Vr vrt, Vr vra, Vr vrb);
  //! Vector pack/unpack.
  ASMJIT_API Error vpkuhum(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkuwum(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkuhus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkuwus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkshss(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkswss(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkshus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkswus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vupkhsb(Vr vrt, Vr vrb);
  ASMJIT_API Error vupkhsh(Vr vrt, Vr vrb);
  ASMJIT_API Error vupklsb(Vr vrt, Vr vrb);
  ASMJIT_API Error vupklsh(Vr vrt, Vr vrb);
  //! Vector pack/unpack doubleword and pixel.
  ASMJIT_API Error vpkudum(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpkudus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpksdss(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpksdus(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vupkhsw(Vr vrt, Vr vrb);
  ASMJIT_API Error vupklsw(Vr vrt, Vr vrb);
  ASMJIT_API Error vpkpx(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vupkhpx(Vr vrt, Vr vrb);
  ASMJIT_API Error vupklpx(Vr vrt, Vr vrb);
  //! Vector merge.
  ASMJIT_API Error vmrghb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrghh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrghw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrglb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrglh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrglw(Vr vrt, Vr vra, Vr vrb);
  //! Vector merge odd/even words.
  ASMJIT_API Error vmrgew(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmrgow(Vr vrt, Vr vra, Vr vrb);
  //! Vector splat.
  ASMJIT_API Error vspltb(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vsplth(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vspltw(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vspltisb(Vr vrt, int32_t simm);
  ASMJIT_API Error vspltish(Vr vrt, int32_t simm);
  ASMJIT_API Error vspltisw(Vr vrt, int32_t simm);
  //! Vector permute/select.
  ASMJIT_API Error vperm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vsel(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vsldoi(Vr vrt, Vr vra, Vr vrb, uint32_t shb);
  ASMJIT_API Error vpermr(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vpermxor(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  //! Vector count leading zeros / population count.
  ASMJIT_API Error vclzb(Vr vrt, Vr vrb);
  ASMJIT_API Error vclzh(Vr vrt, Vr vrb);
  ASMJIT_API Error vclzw(Vr vrt, Vr vrb);
  ASMJIT_API Error vclzd(Vr vrt, Vr vrb);
  ASMJIT_API Error vpopcntb(Vr vrt, Vr vrb);
  ASMJIT_API Error vpopcnth(Vr vrt, Vr vrb);
  ASMJIT_API Error vpopcntw(Vr vrt, Vr vrb);
  ASMJIT_API Error vpopcntd(Vr vrt, Vr vrb);
  //! Vector count trailing zeros / leading-zero byte index.
  ASMJIT_API Error vctzb(Vr vrt, Vr vrb);
  ASMJIT_API Error vctzh(Vr vrt, Vr vrb);
  ASMJIT_API Error vctzw(Vr vrt, Vr vrb);
  ASMJIT_API Error vctzd(Vr vrt, Vr vrb);
  ASMJIT_API Error vclzlsbb(Gp rt, Vr vrb);
  ASMJIT_API Error vctzlsbb(Gp rt, Vr vrb);
  //! Vector multiply.
  ASMJIT_API Error vmulouw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmuluwm(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmuleub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmuleuh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmuleuw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulesb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulesh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulesw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmuloub(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulouh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulosb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulosh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmulosw(Vr vrt, Vr vra, Vr vrb);
  //! Vector multiply by 10.
  ASMJIT_API Error vmul10cuq(Vr vrt, Vr vra);
  ASMJIT_API Error vmul10uq(Vr vrt, Vr vra);
  ASMJIT_API Error vmul10euq(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vmul10ecuq(Vr vrt, Vr vra, Vr vrb);
  //! Vector multiply-high-add / multiply-sum.
  ASMJIT_API Error vmhaddshs(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmhraddshs(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmladduhm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsummbm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumshm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumshs(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumubm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumudm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumuhm(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  ASMJIT_API Error vmsumuhs(Vr vrt, Vr vra, Vr vrb, Vr vrc);
  //! Vector polynomial multiply-sum.
  ASMJIT_API Error vpmsumb(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpmsumh(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpmsumw(Vr vrt, Vr vra, Vr vrb);
  ASMJIT_API Error vpmsumd(Vr vrt, Vr vra, Vr vrb);
  //! Vector extract / insert.
  ASMJIT_API Error vextractub(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vextractuh(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vextractuw(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vextractd(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vextublx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vextubrx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vextuhlx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vextuhrx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vextuwlx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vextuwrx(Gp rt, Gp ra, Vr vrb);
  ASMJIT_API Error vinsertb(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vinserth(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vinsertw(Vr vrt, Vr vrb, uint32_t uim);
  ASMJIT_API Error vinsertd(Vr vrt, Vr vrb, uint32_t uim);
  //! Vector sign extend / negate / parity.
  ASMJIT_API Error vextsb2w(Vr vrt, Vr vrb);
  ASMJIT_API Error vextsh2w(Vr vrt, Vr vrb);
  ASMJIT_API Error vextsw2d(Vr vrt, Vr vrb);
  ASMJIT_API Error vextsb2d(Vr vrt, Vr vrb);
  ASMJIT_API Error vextsh2d(Vr vrt, Vr vrb);
  ASMJIT_API Error vnegw(Vr vrt, Vr vrb);
  ASMJIT_API Error vnegd(Vr vrt, Vr vrb);
  ASMJIT_API Error vprtybd(Vr vrt, Vr vrb);
  ASMJIT_API Error vprtybw(Vr vrt, Vr vrb);
  ASMJIT_API Error vprtybq(Vr vrt, Vr vrb);
  //! Vector bit gather / bit permute.
  ASMJIT_API Error vgbbd(Vr vrt, Vr vrb);
  ASMJIT_API Error vbpermd(Vr vrt, Vr vra, Vr vrb);

  //! \}

  //! \name Function Prologue & Epilogue
  //! \{

  //! Aligns the current section: pads `kCode`/`kData` with no-ops and `kZero`
  //! with zero bytes up to a power-of-two `alignment` (at least 4 bytes).
  ASMJIT_API Error align(AlignMode align_mode, uint32_t alignment) override;

  //! Minimum stack frame size for the target ABI.
  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG int32_t minimum_frame_size() const noexcept {
    return environment().is_little_endian() ? 32 : 48;
  }

  //! Saves LR, allocates `frame_size` bytes with an atomic back-chain update,
  //! and returns. When any mask is nonzero the nonvolatile CR fields and the
  //! selected GPRs/FPRs/VRs are saved exactly like GCC: LR at 16(caller SP),
  //! CR (word) at 8(caller SP), FPR f14+k at -(144 - 8*k), GPR r14+k at
  //! -(fpr_bytes + 144 - 8*k), and vector v20+k at -(fpr_bytes + gpr_bytes +
  //! 192 - 16*k), all relative to the caller's SP.
  //!
  //! Mask bit `k` selects GPR r14+k / FPR f14+k (k = 0..17) or vector v20+k
  //! (k = 0..11). `frame_size` must be a multiple of 16 and cover the fixed
  //! 32-byte area plus all save areas (176 bytes when r14 is saved, 512 when
  //! r14, f14, and v20 are). Frames larger than 32 KiB use a multi-instruction
  //! allocation.
  ASMJIT_API Error prolog(int32_t frame_size, uint32_t save_mask = 0, uint32_t fpr_mask = 0,
                          uint32_t vr_mask = 0);
  //! Deallocates the frame, restores what `prolog()` saved, and returns.
  ASMJIT_API Error epilog(int32_t frame_size, uint32_t save_mask = 0, uint32_t fpr_mask = 0,
                          uint32_t vr_mask = 0);

  //! \}

private:
  enum class BranchKind : uint8_t {
    kConditional = 0,
    kUnconditional = 1
  };

  struct BranchPatch {
    uint32_t label_id;
    size_t offset;
    BranchKind kind;
  };

  std::vector<BranchPatch> _patches;

  Error emitXO(uint32_t xo, Gp rt, Gp ra, Gp rb, bool oe, bool rc);
  Error emitXLog(uint32_t xo, Gp ra, Gp rs, Gp rb, bool rc);
  Error emitXRs(uint32_t xo, Gp ra, Gp rs, bool rc);
  Error emitXSh(uint32_t xo, Gp ra, Gp rs, uint8_t sh, bool rc);
  Error emitBranch(int bo, int bi, const Label& label);
  Error patchBranch(const BranchPatch& patch, const Label& label);
};

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC

#endif // ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED
