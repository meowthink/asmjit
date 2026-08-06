// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED
#define ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED

#include <asmjit/core/assembler.h>

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
  //! and returns. When `save_mask` or `fpr_mask` is nonzero the nonvolatile
  //! CR fields and the selected GPRs/FPRs are saved exactly like GCC: LR at
  //! 16(caller SP), CR (word) at 8(caller SP), FPR f14+k at -(144 - 8*k)
  //! (caller SP), and GPR r14+k at -(fpr_bytes + 144 - 8*k)(caller SP) -- the
  //! FP save area sits above the GPR save area at the top of the frame.
  //!
  //! Mask bit `k` (k = 0..17) selects GPR r14+k / FPR f14+k. `frame_size` must
  //! be a multiple of 16 and cover the fixed 32-byte area plus the GPR and FPR
  //! save areas (e.g. 176 bytes when r14 is saved, 320 when r14 and f14 are).
  //! Frames larger than 32 KiB use a multi-instruction allocation.
  ASMJIT_API Error prolog(int32_t frame_size, uint32_t save_mask = 0, uint32_t fpr_mask = 0);
  //! Deallocates the frame, restores what `prolog()` saved, and returns.
  ASMJIT_API Error epilog(int32_t frame_size, uint32_t save_mask = 0, uint32_t fpr_mask = 0);

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
