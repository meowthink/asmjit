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
  ASMJIT_API Error add(Gp rt, Gp ra, Gp rb);
  //! `subf rt, ra, rb` (rt = rb - ra).
  ASMJIT_API Error subf(Gp rt, Gp ra, Gp rb);
  //! `and ra, rs, rb`.
  ASMJIT_API Error and_(Gp ra, Gp rs, Gp rb);
  //! `or ra, rs, rb`.
  ASMJIT_API Error or_(Gp ra, Gp rs, Gp rb);
  //! `xor ra, rs, rb`.
  ASMJIT_API Error xor_(Gp ra, Gp rs, Gp rb);
  //! `sld ra, rs, rb`.
  ASMJIT_API Error sld(Gp ra, Gp rs, Gp rb);
  //! `srd ra, rs, rb`.
  ASMJIT_API Error srd(Gp ra, Gp rs, Gp rb);
  //! `srad ra, rs, rb`.
  ASMJIT_API Error srad(Gp ra, Gp rs, Gp rb);
  //! `extsw ra, rs`.
  ASMJIT_API Error extsw(Gp ra, Gp rs);
  //! `mulld rt, ra, rb`.
  ASMJIT_API Error mulld(Gp rt, Gp ra, Gp rb);
  //! `mullw rt, ra, rb`.
  ASMJIT_API Error mullw(Gp rt, Gp ra, Gp rb);
  //! `mulhdu rt, ra, rb`.
  ASMJIT_API Error mulhdu(Gp rt, Gp ra, Gp rb);

  //! \}

  //! \name Compare & Branch Instructions
  //! \{

  //! `cmpd ra, rb` (signed compare doubleword, CR0).
  ASMJIT_API Error cmpd(Gp ra, Gp rb);
  //! `cmpld ra, rb` (unsigned compare doubleword, CR0).
  ASMJIT_API Error cmpld(Gp ra, Gp rb);
  //! `cmpdi ra, simm`.
  ASMJIT_API Error cmpdi(Gp ra, int16_t simm);

  ASMJIT_API Error beq(const Label& label);
  ASMJIT_API Error bne(const Label& label);
  ASMJIT_API Error blt(const Label& label);
  ASMJIT_API Error bge(const Label& label);
  ASMJIT_API Error bgt(const Label& label);
  ASMJIT_API Error ble(const Label& label);
  ASMJIT_API Error b(const Label& label);

  //! \}

  //! \name Control & Call Instructions
  //! \{

  //! `mtctr rs`.
  ASMJIT_API Error mtctr(Gp rs);
  //! `bctrl` (branch to CTR and link).
  ASMJIT_API Error bctrl();
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
  //! `lwz rt, d(ra)` (word load, zero-extended).
  ASMJIT_API Error lwz(Gp rt, const Mem& m);
  //! `stw rs, d(ra)`.
  ASMJIT_API Error stw(Gp rs, const Mem& m);
  //! `lbz rt, d(ra)`.
  ASMJIT_API Error lbz(Gp rt, const Mem& m);
  //! `stb rs, d(ra)`.
  ASMJIT_API Error stb(Gp rs, const Mem& m);
  //! `lhz rt, d(ra)`.
  ASMJIT_API Error lhz(Gp rt, const Mem& m);
  //! `sth rs, d(ra)`.
  ASMJIT_API Error sth(Gp rs, const Mem& m);

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

  Error emitBranch(int bo, int bi, const Label& label);
  Error patchBranch(const BranchPatch& patch, const Label& label);
};

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC

#endif // ASMJIT_PPC_PPCASSEMBLER_H_INCLUDED
