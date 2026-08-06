// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>

#include <asmjit/ppc/ppcassembler.h>

#include <asmjit/core/codewriter_p.h>
#include <asmjit/core/codeholder.h>
#include <asmjit/support/support.h>

#if !defined(ASMJIT_NO_PPC)

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \addtogroup asmjit_ppc
//! \{

static inline uint32_t encode_rldi(uint32_t rs, uint32_t ra, uint8_t sh, uint8_t mb, uint32_t xo) noexcept {
  const uint32_t sh6 = sh & 0x3Fu;
  const uint32_t mb6 = mb & 0x3Fu;
  return (30u << 26) |
         (rs << 21) |
         (ra << 16) |
         ((sh6 & 0x1Fu) << 11) | (((sh6 >> 5) & 1u) << 1) |
         ((mb6 & 0x1Fu) << 6) | (((mb6 >> 5) & 1u) << 5) |
         xo;
}

// A-form floating-point: frt = f(fra, frb, frc) with optional Rc (CR1) field.
static inline uint32_t encode_fp(uint32_t op, uint32_t xo, uint32_t frt, uint32_t fra,
                                 uint32_t frb, uint32_t frc, bool rc) noexcept {
  return (op << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (frc << 6) |
         (xo << 1) | (rc ? 1u : 0u);
}

Assembler::Assembler(CodeHolder* code) noexcept
  : BaseAssembler() {
  _arch_mask = (uint64_t(1) << uint32_t(Arch::kPPC64_LE)) |
               (uint64_t(1) << uint32_t(Arch::kPPC64_BE));

  if (code) {
    code->attach(this);
  }
}

Assembler::~Assembler() noexcept {}

Error Assembler::on_attach(CodeHolder& code) noexcept {
  ASMJIT_PROPAGATE(Base::on_attach(code));
  _instruction_alignment = 4;
  return Error::kOk;
}

Error Assembler::on_detach(CodeHolder& code) noexcept {
  return Base::on_detach(code);
}

Error Assembler::bind(const Label& label) {
  ASMJIT_PROPAGATE(Base::bind(label));

  for (size_t i = 0; i < _patches.size();) {
    BranchPatch& patch = _patches[i];
    if (patch.label_id == label.id()) {
      ASMJIT_PROPAGATE(patchBranch(patch, label));
      _patches[i] = _patches.back();
      _patches.pop_back();
    }
    else {
      i++;
    }
  }

  return Error::kOk;
}

Error Assembler::emit32(uint32_t word) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  CodeWriter writer(this);
  ASMJIT_PROPAGATE(writer.ensure_space(this, 4));
  if (environment().is_little_endian())
    writer.emit32u_le(word);
  else
    writer.emit32u_be(word);
  writer.done(this);
  return Error::kOk;
}

// XO-form arithmetic: rt = f(ra, rb), with optional OE (overflow) and Rc (record) fields.
Error Assembler::emitXO(uint32_t xo, Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (xo << 1) | (oe ? (1u << 10) : 0u) | (rc ? 1u : 0u));
}

// X-form logical/shift: ra = f(rs, rb), with optional Rc (record) field.
Error Assembler::emitXLog(uint32_t xo, Gp ra, Gp rs, Gp rb, bool rc) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (xo << 1) | (rc ? 1u : 0u));
}

// X-form count/extend: ra = f(rs), with optional Rc (record) field.
Error Assembler::emitXRs(uint32_t xo, Gp ra, Gp rs, bool rc) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (xo << 1) |
                (rc ? 1u : 0u));
}

// XS-form shift: ra = f(rs, sh), with optional Rc (record) field.
Error Assembler::emitXSh(uint32_t xo, Gp ra, Gp rs, uint8_t sh, bool rc) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | ((sh & 0x1Fu) << 11) |
                (xo << 1) | (rc ? 1u : 0u));
}

Error Assembler::li(Gp rt, int16_t simm) {
  return addi(rt, r0, simm);
}

Error Assembler::addi(Gp rt, Gp ra, int16_t simm) {
  return emit32((14u << 26) | (rt.id() << 21) | (ra.id() << 16) | (uint16_t(simm)));
}

Error Assembler::addis(Gp rt, Gp ra, int16_t simm) {
  return emit32((15u << 26) | (rt.id() << 21) | (ra.id() << 16) | (uint16_t(simm)));
}

Error Assembler::ori(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((24u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

Error Assembler::oris(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((25u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

// `sldi ra, rs, sh` is canonicalized by the assembler to `rldicr ra, rs, sh, 63-sh`.
Error Assembler::sldi(Gp ra, Gp rs, uint8_t sh) {
  if (sh == 0) {
    return or_(ra, rs, rs);
  }

  return rldicr(ra, rs, sh, uint8_t(63 - sh));
}

Error Assembler::loadImm64(Gp rt, uint64_t imm) {
  // Fast paths keep common constants to one or two instructions
  // (Book II 2.1.2: 32-bit constants in oris/ori or addis/addi).
  if (imm <= 0x7FFFu || imm >= 0xFFFFFFFFFFFF8000ull) {
    return li(rt, int16_t(imm)); // sign-extended 16-bit constant
  }
  if (imm <= 0xFFFFFFFFull) {
    if (imm <= 0x7FFFFFFFull) {
      // lis+ori keeps the top bit clear, so the value is zero-extended.
      ASMJIT_PROPAGATE(addis(rt, r0, int16_t(uint32_t(imm) >> 16)));
      return ori(rt, rt, uint16_t(imm));
    }
    // Top bit set: build from zero with oris+ori (r0 is a real register in
    // logical ops, so it cannot be used as an implicit zero here).
    ASMJIT_PROPAGATE(li(rt, 0));
    ASMJIT_PROPAGATE(oris(rt, rt, uint16_t(imm >> 16)));
    return ori(rt, rt, uint16_t(imm));
  }
  if (imm >= 0xFFFFFFFF80000000ull) {
    ASMJIT_PROPAGATE(addis(rt, r0, int16_t(uint32_t(imm) >> 16)));
    return addi(rt, rt, int16_t(uint32_t(imm))); // sign-extended 32-bit constant
  }
  // General 64-bit constant.
  ASMJIT_PROPAGATE(addis(rt, r0, int16_t(imm >> 48)));
  ASMJIT_PROPAGATE(ori(rt, rt, uint16_t(imm >> 32)));
  ASMJIT_PROPAGATE(sldi(rt, rt, 32));
  ASMJIT_PROPAGATE(oris(rt, rt, uint16_t(imm >> 16)));
  return ori(rt, rt, uint16_t(imm));
}

Error Assembler::add(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(266u, rt, ra, rb, oe, rc);
}

Error Assembler::addc(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(10u, rt, ra, rb, oe, rc);
}

Error Assembler::addze(Gp rt, Gp ra, bool oe, bool rc) {
  return emitXO(202u, rt, ra, Gp { 0 }, oe, rc);
}

Error Assembler::addme(Gp rt, Gp ra, bool oe, bool rc) {
  return emitXO(234u, rt, ra, Gp { 0 }, oe, rc);
}

Error Assembler::subf(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(40u, rt, ra, rb, oe, rc);
}

Error Assembler::subfc(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(8u, rt, ra, rb, oe, rc);
}

Error Assembler::subfze(Gp rt, Gp ra, bool oe, bool rc) {
  return emitXO(200u, rt, ra, Gp { 0 }, oe, rc);
}

Error Assembler::subfme(Gp rt, Gp ra, bool oe, bool rc) {
  return emitXO(232u, rt, ra, Gp { 0 }, oe, rc);
}

Error Assembler::neg(Gp rt, Gp ra, bool oe, bool rc) {
  return emitXO(104u, rt, ra, Gp { 0 }, oe, rc);
}

Error Assembler::and_(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(28u, ra, rs, rb, rc);
}

Error Assembler::andc(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(60u, ra, rs, rb, rc);
}

Error Assembler::or_(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(444u, ra, rs, rb, rc);
}

Error Assembler::orc(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(412u, ra, rs, rb, rc);
}

Error Assembler::xor_(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(316u, ra, rs, rb, rc);
}

Error Assembler::nand(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(476u, ra, rs, rb, rc);
}

Error Assembler::nor(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(124u, ra, rs, rb, rc);
}

Error Assembler::eqv(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(284u, ra, rs, rb, rc);
}

Error Assembler::sld(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(27u, ra, rs, rb, rc);
}

Error Assembler::srd(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(539u, ra, rs, rb, rc);
}

Error Assembler::srad(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(794u, ra, rs, rb, rc);
}

Error Assembler::slw(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(24u, ra, rs, rb, rc);
}

Error Assembler::srw(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(536u, ra, rs, rb, rc);
}

Error Assembler::sraw(Gp ra, Gp rs, Gp rb, bool rc) {
  return emitXLog(792u, ra, rs, rb, rc);
}

Error Assembler::extsw(Gp ra, Gp rs, bool rc) {
  return emitXRs(986u, ra, rs, rc);
}

Error Assembler::extsb(Gp ra, Gp rs, bool rc) {
  return emitXRs(954u, ra, rs, rc);
}

Error Assembler::extsh(Gp ra, Gp rs, bool rc) {
  return emitXRs(922u, ra, rs, rc);
}

Error Assembler::extswsli(Gp ra, Gp rs, uint8_t sh, bool rc) {
  return emitXSh(890u, ra, rs, sh, rc);
}

Error Assembler::addic(Gp rt, Gp ra, int16_t simm, bool rc) {
  return emit32(((rc ? 13u : 12u) << 26) | (rt.id() << 21) | (ra.id() << 16) | uint16_t(simm));
}

Error Assembler::subfic(Gp rt, Gp ra, int16_t simm) {
  return emit32((8u << 26) | (rt.id() << 21) | (ra.id() << 16) | uint16_t(simm));
}

Error Assembler::adde(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(138u, rt, ra, rb, oe, rc);
}

Error Assembler::subfe(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(136u, rt, ra, rb, oe, rc);
}

Error Assembler::addex(Gp rt, Gp ra, Gp rb, uint32_t cy) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                ((cy & 3u) << 9) | (170u << 1));
}

Error Assembler::addpcis(Gp rt, int16_t d) {
  const uint16_t imm = uint16_t(d);
  return emit32((19u << 26) | (rt.id() << 21) | (((imm >> 1) & 0x1Fu) << 16) |
                (((imm >> 6) & 0x3FFu) << 6) | (2u << 1) | (imm & 1u));
}

Error Assembler::mulld(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(233u, rt, ra, rb, oe, rc);
}

Error Assembler::mullw(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(235u, rt, ra, rb, oe, rc);
}

Error Assembler::mulhdu(Gp rt, Gp ra, Gp rb, bool rc) {
  return emitXO(9u, rt, ra, rb, false, rc);
}

Error Assembler::mulhd(Gp rt, Gp ra, Gp rb, bool rc) {
  return emitXO(73u, rt, ra, rb, false, rc);
}

Error Assembler::mulhw(Gp rt, Gp ra, Gp rb, bool rc) {
  return emitXO(75u, rt, ra, rb, false, rc);
}

Error Assembler::mulhwu(Gp rt, Gp ra, Gp rb, bool rc) {
  return emitXO(11u, rt, ra, rb, false, rc);
}

Error Assembler::mulli(Gp rt, Gp ra, int16_t simm) {
  return emit32((7u << 26) | (rt.id() << 21) | (ra.id() << 16) | (uint16_t(simm)));
}

Error Assembler::maddhd(Gp rt, Gp ra, Gp rb, Gp rc0) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (rc0.id() << 6) | 48u);
}

Error Assembler::maddhdu(Gp rt, Gp ra, Gp rb, Gp rc0) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (rc0.id() << 6) | 49u);
}

Error Assembler::maddld(Gp rt, Gp ra, Gp rb, Gp rc0) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (rc0.id() << 6) | 51u);
}

Error Assembler::divd(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(489u, rt, ra, rb, oe, rc);
}

Error Assembler::divw(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(491u, rt, ra, rb, oe, rc);
}

Error Assembler::divdu(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(457u, rt, ra, rb, oe, rc);
}

Error Assembler::divwu(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(459u, rt, ra, rb, oe, rc);
}

Error Assembler::divde(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(425u, rt, ra, rb, oe, rc);
}

Error Assembler::divdeu(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(393u, rt, ra, rb, oe, rc);
}

Error Assembler::divwe(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(427u, rt, ra, rb, oe, rc);
}

Error Assembler::divweu(Gp rt, Gp ra, Gp rb, bool oe, bool rc) {
  return emitXO(395u, rt, ra, rb, oe, rc);
}

Error Assembler::modsd(Gp rt, Gp ra, Gp rb) {
  return emitXO(777u, rt, ra, rb, false, false);
}

Error Assembler::modud(Gp rt, Gp ra, Gp rb) {
  return emitXO(265u, rt, ra, rb, false, false);
}

Error Assembler::modsw(Gp rt, Gp ra, Gp rb) {
  return emitXO(779u, rt, ra, rb, false, false);
}

Error Assembler::moduw(Gp rt, Gp ra, Gp rb) {
  return emitXO(267u, rt, ra, rb, false, false);
}

Error Assembler::darn(Gp rt, uint8_t l) {
  return emit32((31u << 26) | (rt.id() << 21) | ((l & 0x1Fu) << 16) | (755u << 1));
}

Error Assembler::srawi(Gp ra, Gp rs, uint8_t sh, bool rc) {
  return emitXSh(824u, ra, rs, sh, rc);
}

Error Assembler::srdi(Gp ra, Gp rs, uint8_t sh) {
  return rldicl(ra, rs, uint8_t(64 - sh), sh);
}

Error Assembler::andi_(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((28u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

Error Assembler::andis_(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((29u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

Error Assembler::xori(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((26u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

Error Assembler::xoris(Gp ra, Gp rs, uint16_t uimm) {
  return emit32((27u << 26) | (rs.id() << 21) | (ra.id() << 16) | uimm);
}

Error Assembler::cmpd(Gp ra, Gp rb, uint32_t bf) {
  return emit32((31u << 26) | (bf << 23) | (1u << 21) | (ra.id() << 16) | (rb.id() << 11));
}

Error Assembler::cmpld(Gp ra, Gp rb, uint32_t bf) {
  return emit32((31u << 26) | (bf << 23) | (1u << 21) | (ra.id() << 16) | (rb.id() << 11) | (32u << 1));
}

Error Assembler::cmpdi(Gp ra, int16_t simm, uint32_t bf) {
  return emit32((11u << 26) | (bf << 23) | (1u << 21) | (ra.id() << 16) | (uint16_t(simm)));
}

Error Assembler::cmp(uint32_t bf, uint32_t l, Gp ra, Gp rb) {
  return emit32((31u << 26) | (bf << 23) | (l << 21) | (ra.id() << 16) | (rb.id() << 11));
}

Error Assembler::cmpl(uint32_t bf, uint32_t l, Gp ra, Gp rb) {
  return emit32((31u << 26) | (bf << 23) | (l << 21) | (ra.id() << 16) | (rb.id() << 11) | (32u << 1));
}

Error Assembler::cmpi(uint32_t bf, uint32_t l, Gp ra, int16_t si) {
  return emit32((11u << 26) | (bf << 23) | (l << 21) | (ra.id() << 16) | uint16_t(si));
}

Error Assembler::cmpli(uint32_t bf, uint32_t l, Gp ra, uint16_t ui) {
  return emit32((10u << 26) | (bf << 23) | (l << 21) | (ra.id() << 16) | ui);
}

Error Assembler::cmpldi(uint32_t bf, Gp ra, uint16_t ui) {
  return emit32((10u << 26) | (bf << 23) | (1u << 21) | (ra.id() << 16) | ui);
}

Error Assembler::cmpb(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (508u << 1));
}

Error Assembler::cmpeqb(uint32_t bf, Gp ra, Gp rb) {
  return emit32((31u << 26) | (bf << 23) | (ra.id() << 16) | (rb.id() << 11) | (224u << 1));
}

Error Assembler::cmprb(uint32_t bf, uint32_t l, Gp ra, Gp rb) {
  return emit32((31u << 26) | (bf << 23) | (l << 21) | (ra.id() << 16) | (rb.id() << 11) | (192u << 1));
}

Error Assembler::tw(uint32_t to, Gp ra, Gp rb) {
  return emit32((31u << 26) | (to << 21) | (ra.id() << 16) | (rb.id() << 11) | (4u << 1));
}

Error Assembler::twi(uint32_t to, Gp ra, int16_t si) {
  return emit32((3u << 26) | (to << 21) | (ra.id() << 16) | uint16_t(si));
}

Error Assembler::td(uint32_t to, Gp ra, Gp rb) {
  return emit32((31u << 26) | (to << 21) | (ra.id() << 16) | (rb.id() << 11) | (68u << 1));
}

Error Assembler::tdi(uint32_t to, Gp ra, int16_t si) {
  return emit32((2u << 26) | (to << 21) | (ra.id() << 16) | uint16_t(si));
}

Error Assembler::isel(Gp rt, Gp ra, Gp rb, uint32_t bc) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                ((bc & 0x1Fu) << 6) | (15u << 1));
}

Error Assembler::isellt(Gp rt, Gp ra, Gp rb) {
  return isel(rt, ra, rb, 0);
}

Error Assembler::iseleq(Gp rt, Gp ra, Gp rb) {
  return isel(rt, ra, rb, 2);
}

Error Assembler::iselgt(Gp rt, Gp ra, Gp rb) {
  return isel(rt, ra, rb, 1);
}

Error Assembler::beq(const Label& label) {
  return emitBranch(12, 2, label);
}

Error Assembler::bne(const Label& label) {
  return emitBranch(4, 2, label);
}

Error Assembler::blt(const Label& label) {
  return emitBranch(12, 0, label);
}

Error Assembler::bge(const Label& label) {
  return emitBranch(4, 0, label);
}

Error Assembler::bgt(const Label& label) {
  return emitBranch(12, 1, label);
}

Error Assembler::ble(const Label& label) {
  return emitBranch(4, 1, label);
}

Error Assembler::b(const Label& label) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const size_t branch_offset = offset();
  ASMJIT_PROPAGATE(emit32((18u << 26)));

  const LabelEntry& le = _code->label_entry_of(label);
  const BranchPatch patch { label.id(), branch_offset, BranchKind::kUnconditional };
  if (le.is_bound()) {
    return patchBranch(patch, label);
  }

  _patches.push_back(patch);
  return Error::kOk;
}

Error Assembler::bc(uint32_t bo, uint32_t bi, const Label& label) {
  return emitBranch(bo, bi, label);
}

Error Assembler::bcl(uint32_t bo, uint32_t bi, const Label& label) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const size_t branch_offset = offset();
  ASMJIT_PROPAGATE(emit32((16u << 26) | (bo << 21) | (bi << 16) | 1u));

  const LabelEntry& le = _code->label_entry_of(label);
  const BranchPatch patch { label.id(), branch_offset, BranchKind::kConditional };
  if (le.is_bound()) {
    return patchBranch(patch, label);
  }

  _patches.push_back(patch);
  return Error::kOk;
}

Error Assembler::bclr(uint32_t bo, uint32_t bi, uint32_t bh) {
  return emit32((19u << 26) | (bo << 21) | (bi << 16) | ((bh & 3u) << 11) | (16u << 1));
}

Error Assembler::bclrl(uint32_t bo, uint32_t bi, uint32_t bh) {
  return emit32((19u << 26) | (bo << 21) | (bi << 16) | ((bh & 3u) << 11) | (16u << 1) | 1u);
}

Error Assembler::bcctr(uint32_t bo, uint32_t bi, uint32_t bh) {
  return emit32((19u << 26) | (bo << 21) | (bi << 16) | ((bh & 3u) << 11) | (528u << 1));
}

Error Assembler::bcctrl(uint32_t bo, uint32_t bi, uint32_t bh) {
  return emit32((19u << 26) | (bo << 21) | (bi << 16) | ((bh & 3u) << 11) | (528u << 1) | 1u);
}

Error Assembler::mtctr(Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (9u << 16) | (467u << 1));
}

Error Assembler::bctrl() {
  return emit32((19u << 26) | (20u << 21) | (528u << 1) | 1u);
}

Error Assembler::call(uint64_t address) {
  ASMJIT_PROPAGATE(loadImm64(r12, address));
  ASMJIT_PROPAGATE(mtctr(r12));
  return bctrl();
}

Error Assembler::callDescriptor(uint64_t ptr) {
  ASMJIT_PROPAGATE(loadImm64(r11, ptr));
  ASMJIT_PROPAGATE(ld(r12, ppc::ptr(r11, 0)));
  ASMJIT_PROPAGATE(ld(r2, ppc::ptr(r11, 8)));
  ASMJIT_PROPAGATE(mtctr(r12));
  return bctrl();
}

Error Assembler::callHelper(uint64_t address) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  // addpcis r12, 0 sets r12 to the address of the following instruction; the
  // inline slot below is reached with a small displacement regardless of where
  // the code is placed, and a branch skips over the data after the call.
  ASMJIT_PROPAGATE(addpcis(r12, 0));
  const size_t nia = offset();                           // NIA = address after the addpcis.
  const size_t slot_off = (nia + 16 + 7) & ~size_t(7);   // After ld+mtctr+bctrl+b, 8-aligned.
  const int64_t disp = int64_t(slot_off) - int64_t(nia);
  if (ASMJIT_UNLIKELY(disp < -32768 || disp > 32767)) {
    return report_error(make_error(Error::kTooLarge));
  }
  Label skip = new_label();
  ASMJIT_PROPAGATE(ld(r12, ppc::ptr(r12, int32_t(disp))));
  ASMJIT_PROPAGATE(mtctr(r12));
  ASMJIT_PROPAGATE(bctrl());
  ASMJIT_PROPAGATE(b(skip));
  while (offset() < slot_off) {
    ASMJIT_PROPAGATE(nop());
  }

  uint8_t bytes[8];
  if (environment().is_little_endian()) {
    Support::storeu_u64_le(bytes, address);
  }
  else {
    Support::storeu_u64_be(bytes, address);
  }
  ASMJIT_PROPAGATE(embed(bytes, 8));
  return bind(skip);
}

Error Assembler::tailCall(uint64_t address) {
  ASMJIT_PROPAGATE(loadImm64(r12, address));
  ASMJIT_PROPAGATE(mtctr(r12));
  return bctr();
}

Error Assembler::tailCallDescriptor(uint64_t ptr) {
  ASMJIT_PROPAGATE(loadImm64(r11, ptr));
  ASMJIT_PROPAGATE(ld(r12, ppc::ptr(r11, 0)));
  ASMJIT_PROPAGATE(ld(r2, ppc::ptr(r11, 8)));
  ASMJIT_PROPAGATE(mtctr(r12));
  return bctr();
}

Error Assembler::bLong(uint64_t address) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  // addpcis r12, 0; ld r12, disp(r12); mtctr r12; bctr -- tail branch to an
  // absolute 64-bit address loaded from an inline slot right after the branch
  // (the same self-contained trick as callHelper, no external relocations).
  ASMJIT_PROPAGATE(addpcis(r12, 0));
  const size_t nia = offset();                             // NIA = address after the addpcis.
  const size_t slot_off = (nia + 12 + 7) & ~size_t(7);     // After ld+mtctr+bctr, 8-aligned.
  const int64_t disp = int64_t(slot_off) - int64_t(nia);
  if (ASMJIT_UNLIKELY(disp < -32768 || disp > 32767)) {
    return report_error(make_error(Error::kTooLarge));
  }
  ASMJIT_PROPAGATE(ld(r12, ppc::ptr(r12, int32_t(disp))));
  ASMJIT_PROPAGATE(mtctr(r12));
  ASMJIT_PROPAGATE(bctr());
  while (offset() < slot_off) {
    ASMJIT_PROPAGATE(nop());
  }

  uint8_t bytes[8];
  if (environment().is_little_endian()) {
    Support::storeu_u64_le(bytes, address);
  }
  else {
    Support::storeu_u64_be(bytes, address);
  }
  return embed(bytes, 8);
}

Error Assembler::bctr() {
  return emit32((19u << 26) | (20u << 21) | (528u << 1));
}

Error Assembler::mflr(Gp rt) {
  return emit32((31u << 26) | (rt.id() << 21) | (8u << 16) | (339u << 1));
}

Error Assembler::mtlr(Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (8u << 16) | (467u << 1));
}

Error Assembler::blr() {
  return emit32((19u << 26) | (20u << 21) | (16u << 1));
}

Error Assembler::nop() {
  return emit32(0x60000000u);
}

Error Assembler::ld(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((58u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::std(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((62u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::stdu(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((62u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())) | 1u);
}

Error Assembler::lwz(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((32u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::stw(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((36u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lbz(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((34u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::stb(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((38u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lhz(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((40u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::sth(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((44u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lbzu(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((35u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::stbu(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((39u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lhzu(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((41u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lha(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((42u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lhau(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((43u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::sthu(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((45u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lwzu(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((33u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::stwu(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((37u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())));
}

Error Assembler::lwa(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((58u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())) | 2u);
}

Error Assembler::ldu(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((58u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (uint16_t(m.offset())) | 1u);
}

Error Assembler::lbzx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (87u << 1));
}

Error Assembler::lbzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (119u << 1));
}

Error Assembler::lhzx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (279u << 1));
}

Error Assembler::lhzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (311u << 1));
}

Error Assembler::lhax(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (343u << 1));
}

Error Assembler::lhaux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (375u << 1));
}

Error Assembler::lwzx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (23u << 1));
}

Error Assembler::lwzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (55u << 1));
}

Error Assembler::lwax(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (341u << 1));
}

Error Assembler::ldx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (21u << 1));
}

Error Assembler::ldux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (53u << 1));
}

Error Assembler::stbx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (215u << 1));
}

Error Assembler::stbux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (247u << 1));
}

Error Assembler::sthx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (407u << 1));
}

Error Assembler::sthux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (439u << 1));
}

Error Assembler::stwx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (151u << 1));
}

Error Assembler::stwux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (183u << 1));
}

Error Assembler::stdx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (149u << 1));
}

Error Assembler::stdux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (181u << 1));
}

Error Assembler::lhbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (790u << 1));
}

Error Assembler::lwbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (534u << 1));
}

Error Assembler::ldbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (532u << 1));
}

Error Assembler::sthbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (918u << 1));
}

Error Assembler::stwbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (662u << 1));
}

Error Assembler::stdbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (660u << 1));
}

Error Assembler::lmw(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((46u << 26) | (rt.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::stmw(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((47u << 26) | (rs.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::lswi(Gp rt, Gp ra, uint8_t nb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | ((nb & 0x1Fu) << 11) | (597u << 1));
}

Error Assembler::lswx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (533u << 1));
}

Error Assembler::stswi(Gp rs, Gp ra, uint8_t nb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | ((nb & 0x1Fu) << 11) | (725u << 1));
}

Error Assembler::stswx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (661u << 1));
}

Error Assembler::lwarx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (20u << 1));
}

Error Assembler::ldarx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (84u << 1));
}

Error Assembler::lbarx(Gp rt, const Mem& m, uint32_t eh) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (52u << 1) | (eh ? 1u : 0u));
}

Error Assembler::lharx(Gp rt, const Mem& m, uint32_t eh) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (372u << 1) | (eh ? 1u : 0u));
}

Error Assembler::stwcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (150u << 1) | 1u);
}

Error Assembler::stdcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (214u << 1) | 1u);
}

Error Assembler::stbcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (694u << 1) | 1u);
}

Error Assembler::sthcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (726u << 1) | 1u);
}

Error Assembler::rldicl(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 0) | (rc ? 1u : 0u));
}

Error Assembler::rldicr(Gp ra, Gp rs, uint8_t sh, uint8_t me, bool rc) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, me, 4) | (rc ? 1u : 0u));
}

Error Assembler::rldic(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 8) | (rc ? 1u : 0u));
}

Error Assembler::rldimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb, bool rc) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 12) | (rc ? 1u : 0u));
}

Error Assembler::rlwinm(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me, bool rc) {
  return emit32((21u << 26) | (rs.id() << 21) | (ra.id() << 16) |
                ((sh & 0x1Fu) << 11) | ((mb & 0x1Fu) << 6) | ((me & 0x1Fu) << 1) | (rc ? 1u : 0u));
}

Error Assembler::rlwimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me, bool rc) {
  return emit32((20u << 26) | (rs.id() << 21) | (ra.id() << 16) |
                ((sh & 0x1Fu) << 11) | ((mb & 0x1Fu) << 6) | ((me & 0x1Fu) << 1) | (rc ? 1u : 0u));
}

Error Assembler::rlwnm(Gp ra, Gp rs, Gp rb, uint8_t mb, uint8_t me, bool rc) {
  return emit32((23u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                ((mb & 0x1Fu) << 6) | ((me & 0x1Fu) << 1) | (rc ? 1u : 0u));
}

Error Assembler::rldcl(Gp ra, Gp rs, Gp rb, uint8_t mb, bool rc) {
  return emit32((30u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                ((mb & 0x1Fu) << 6) | (8u << 1) | (rc ? 1u : 0u));
}

Error Assembler::rldcr(Gp ra, Gp rs, Gp rb, uint8_t me, bool rc) {
  return emit32((30u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) |
                ((me & 0x1Fu) << 6) | (9u << 1) | (rc ? 1u : 0u));
}

Error Assembler::clrldi(Gp ra, Gp rs, uint8_t n) {
  return rldicl(ra, rs, 0, n);
}

Error Assembler::clrrdi(Gp ra, Gp rs, uint8_t n) {
  return rldicr(ra, rs, 0, uint8_t(63 - n));
}

Error Assembler::rotldi(Gp ra, Gp rs, uint8_t n) {
  return rldicl(ra, rs, n, 0);
}

Error Assembler::rotrdi(Gp ra, Gp rs, uint8_t n) {
  return rldicl(ra, rs, uint8_t(64 - n), 0);
}

Error Assembler::extldi(Gp ra, Gp rs, uint8_t n, uint8_t b) {
  return rldicr(ra, rs, b, uint8_t(n - 1));
}

Error Assembler::extrdi(Gp ra, Gp rs, uint8_t n, uint8_t b) {
  return rldicl(ra, rs, uint8_t(b + n), uint8_t(64 - n));
}

Error Assembler::insrdi(Gp ra, Gp rs, uint8_t n, uint8_t b) {
  return rldimi(ra, rs, uint8_t(64 - (b + n)), b);
}

Error Assembler::sync() {
  return emit32((31u << 26) | (598u << 1));
}

Error Assembler::lwsync() {
  return emit32((31u << 26) | (1u << 21) | (598u << 1));
}

Error Assembler::isync() {
  return emit32((19u << 26) | (150u << 1));
}

Error Assembler::eieio() {
  return emit32((31u << 26) | (854u << 1));
}

Error Assembler::wait(uint32_t wc) {
  return emit32((31u << 26) | ((wc & 3u) << 21) | (30u << 1));
}

Error Assembler::dcbf(Gp ra, Gp rb) {
  return emit32((31u << 26) | (ra.id() << 16) | (rb.id() << 11) | (86u << 1));
}

Error Assembler::dcbst(Gp ra, Gp rb) {
  return emit32((31u << 26) | (ra.id() << 16) | (rb.id() << 11) | (54u << 1));
}

Error Assembler::dcbt(Gp ra, Gp rb, uint8_t th) {
  return emit32((31u << 26) | ((th & 0x1Fu) << 21) | (ra.id() << 16) | (rb.id() << 11) | (278u << 1));
}

Error Assembler::dcbtst(Gp ra, Gp rb, uint8_t th) {
  return emit32((31u << 26) | ((th & 0x1Fu) << 21) | (ra.id() << 16) | (rb.id() << 11) | (246u << 1));
}

Error Assembler::dcbz(Gp ra, Gp rb) {
  return emit32((31u << 26) | (ra.id() << 16) | (rb.id() << 11) | (1014u << 1));
}

Error Assembler::icbi(Gp ra, Gp rb) {
  return emit32((31u << 26) | (ra.id() << 16) | (rb.id() << 11) | (982u << 1));
}

Error Assembler::icbt(uint32_t ct, Gp ra, Gp rb) {
  return emit32((31u << 26) | ((ct & 0xFu) << 21) | (ra.id() << 16) | (rb.id() << 11) | (22u << 1));
}

Error Assembler::cntlzw(Gp ra, Gp rs, bool rc) {
  return emitXRs(26u, ra, rs, rc);
}

Error Assembler::cntlzd(Gp ra, Gp rs, bool rc) {
  return emitXRs(58u, ra, rs, rc);
}

Error Assembler::cnttzw(Gp ra, Gp rs, bool rc) {
  return emitXRs(538u, ra, rs, rc);
}

Error Assembler::cnttzd(Gp ra, Gp rs, bool rc) {
  return emitXRs(570u, ra, rs, rc);
}

Error Assembler::popcntd(Gp ra, Gp rs) {
  return emitXRs(506u, ra, rs, false);
}

Error Assembler::popcntb(Gp ra, Gp rs) {
  return emitXRs(122u, ra, rs, false);
}

Error Assembler::popcntw(Gp ra, Gp rs) {
  return emitXRs(378u, ra, rs, false);
}

Error Assembler::prtyd(Gp ra, Gp rs) {
  return emitXRs(186u, ra, rs, false);
}

Error Assembler::prtyw(Gp ra, Gp rs) {
  return emitXRs(154u, ra, rs, false);
}

Error Assembler::bpermd(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (252u << 1));
}

Error Assembler::mfcr(Gp rt) {
  return emit32((31u << 26) | (rt.id() << 21) | (19u << 1));
}

Error Assembler::mtcrf(uint32_t fxm, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | ((fxm & 0xFFu) << 12) | (144u << 1));
}

Error Assembler::mfocrf(Gp rt, uint32_t fxm) {
  return emit32((31u << 26) | (rt.id() << 21) | ((fxm & 0xFFu) << 12) | (1u << 20) | (19u << 1));
}

Error Assembler::mtocrf(uint32_t fxm, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | ((fxm & 0xFFu) << 12) | (1u << 20) | (144u << 1));
}

Error Assembler::mcrxrx(uint32_t bf) {
  return emit32((31u << 26) | (bf << 23) | (576u << 1));
}

Error Assembler::mfspr(Gp rt, uint32_t spr) {
  return emit32((31u << 26) | (rt.id() << 21) | ((spr & 0x1Fu) << 16) |
                (((spr >> 5) & 0x1Fu) << 11) | (339u << 1));
}

Error Assembler::mtspr(uint32_t spr, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | ((spr & 0x1Fu) << 16) |
                (((spr >> 5) & 0x1Fu) << 11) | (467u << 1));
}

Error Assembler::mfxer(Gp rt) {
  return mfspr(rt, 1);
}

Error Assembler::mtxer(Gp rs) {
  return mtspr(1, rs);
}

Error Assembler::setb(Gp rt, uint32_t bfa) {
  return emit32((31u << 26) | (rt.id() << 21) | ((bfa & 7u) << 18) | (128u << 1));
}

Error Assembler::mcrf(uint32_t bf, uint32_t bfa) {
  return emit32((19u << 26) | (bf << 23) | (bfa << 18));
}

Error Assembler::crand(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (257u << 1));
}

Error Assembler::crandc(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (129u << 1));
}

Error Assembler::crnor(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (33u << 1));
}

Error Assembler::creqv(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (289u << 1));
}

Error Assembler::crnand(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (225u << 1));
}

Error Assembler::cror(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (449u << 1));
}

Error Assembler::crorc(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (417u << 1));
}

Error Assembler::crxor(uint32_t bt, uint32_t ba, uint32_t bb) {
  return emit32((19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (193u << 1));
}

Error Assembler::crclr(uint32_t bt) {
  return crxor(bt, bt, bt);
}

Error Assembler::crset(uint32_t bt) {
  return creqv(bt, bt, bt);
}

Error Assembler::crmove(uint32_t bt, uint32_t ba) {
  return cror(bt, ba, ba);
}

Error Assembler::crnot(uint32_t bt, uint32_t ba) {
  return crnor(bt, ba, ba);
}

Error Assembler::sc(uint32_t lev) {
  return emit32((17u << 26) | ((lev & 0x7FFFu) << 21) | 2u);
}

Error Assembler::scv(uint32_t lev) {
  return emit32((17u << 26) | ((lev & 0x7FFFu) << 21) | 1u);
}

Error Assembler::addg6s(Gp rt, Gp ra, Gp rb) {
  return emitXO(74u, rt, ra, rb, false, false);
}

Error Assembler::cbcdtd(Gp ra, Gp rs) {
  return emitXRs(314u, ra, rs, false);
}

Error Assembler::cdtbcd(Gp ra, Gp rs) {
  return emitXRs(282u, ra, rs, false);
}

Error Assembler::lfs(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((48u << 26) | (frt.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::lfsu(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((49u << 26) | (frt.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::lfsx(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (535u << 1));
}

Error Assembler::lfsux(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (567u << 1));
}

Error Assembler::lfd(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((50u << 26) | (frt.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::lfdu(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((51u << 26) | (frt.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::lfdx(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (599u << 1));
}

Error Assembler::lfdux(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (631u << 1));
}

Error Assembler::lfiwax(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (855u << 1));
}

Error Assembler::lfiwzx(Fp frt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (887u << 1));
}

Error Assembler::stfs(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((52u << 26) | (frs.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::stfsu(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 0u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((53u << 26) | (frs.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::stfsx(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (663u << 1));
}

Error Assembler::stfsux(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (695u << 1));
}

Error Assembler::stfd(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((54u << 26) | (frs.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::stfdu(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((55u << 26) | (frs.id() << 21) | (m.base_id() << 16) | uint16_t(m.offset()));
}

Error Assembler::stfdx(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (727u << 1));
}

Error Assembler::stfdux(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (759u << 1));
}

Error Assembler::stfiwx(Fp frs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (frs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (983u << 1));
}

Error Assembler::fmr(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 72u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fneg(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 40u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fabs(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 264u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fnabs(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 136u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fcpsgn(Fp frt, Fp fra, Fp frb) {
  return emit32(encode_fp(63u, 8u, frt.id(), fra.id(), frb.id(), 0, false));
}

Error Assembler::fadd(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 21u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fadds(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 21u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fsub(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 20u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fsubs(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 20u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fmul(Fp frt, Fp fra, Fp frc, bool rc) {
  return emit32(encode_fp(63u, 25u, frt.id(), fra.id(), 0, frc.id(), rc));
}

Error Assembler::fmuls(Fp frt, Fp fra, Fp frc, bool rc) {
  return emit32(encode_fp(59u, 25u, frt.id(), fra.id(), 0, frc.id(), rc));
}

Error Assembler::fdiv(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 18u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fdivs(Fp frt, Fp fra, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 18u, frt.id(), fra.id(), frb.id(), 0, rc));
}

Error Assembler::fsqrt(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 22u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fsqrts(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 22u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fre(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 24u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fres(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 24u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::frsqrte(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 26u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::frsqrtes(Fp frt, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 26u, frt.id(), 0, frb.id(), 0, rc));
}

Error Assembler::fmadd(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 29u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fmadds(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 29u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fmsub(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 28u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fmsubs(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 28u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fnmadd(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 31u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fnmadds(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 31u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fnmsub(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(63u, 30u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::fnmsubs(Fp frt, Fp fra, Fp frc, Fp frb, bool rc) {
  return emit32(encode_fp(59u, 30u, frt.id(), fra.id(), frb.id(), frc.id(), rc));
}

Error Assembler::frin(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 392u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::friz(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 424u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::frip(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 456u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::frim(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 488u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::frsp(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 12u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fcfid(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 846u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fcfidu(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 974u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fcfids(Fp frt, Fp frb) {
  return emit32(encode_fp(59u, 846u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fcfidus(Fp frt, Fp frb) {
  return emit32(encode_fp(59u, 974u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctiw(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 14u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctiwz(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 15u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctiwu(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 142u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctiwuz(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 143u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctid(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 814u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctidz(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 815u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctidu(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 942u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fctiduz(Fp frt, Fp frb) {
  return emit32(encode_fp(63u, 943u, frt.id(), 0, frb.id(), 0, false));
}

Error Assembler::fcmpu(uint32_t bf, Fp fra, Fp frb) {
  return emit32((63u << 26) | (bf << 23) | (fra.id() << 16) | (frb.id() << 11));
}

Error Assembler::fcmpo(uint32_t bf, Fp fra, Fp frb) {
  return emit32((63u << 26) | (bf << 23) | (fra.id() << 16) | (frb.id() << 11) | (32u << 1));
}

Error Assembler::ftdiv(uint32_t bf, Fp fra, Fp frb) {
  return emit32((63u << 26) | (bf << 23) | (fra.id() << 16) | (frb.id() << 11) | (128u << 1));
}

Error Assembler::ftsqrt(uint32_t bf, Fp frb) {
  return emit32((63u << 26) | (bf << 23) | (frb.id() << 11) | (160u << 1));
}

Error Assembler::fsel(Fp frt, Fp fra, Fp frc, Fp frb) {
  return emit32(encode_fp(63u, 23u, frt.id(), fra.id(), frb.id(), frc.id(), false));
}

Error Assembler::mffs(Fp frt, bool rc) {
  return emit32((63u << 26) | (frt.id() << 21) | (583u << 1) | (rc ? 1u : 0u));
}

Error Assembler::mffsce(Fp frt) {
  return emit32((63u << 26) | (frt.id() << 21) | (1u << 16) | (583u << 1));
}

Error Assembler::mffsl(Fp frt) {
  return emit32((63u << 26) | (frt.id() << 21) | (24u << 16) | (583u << 1));
}

Error Assembler::mffscrn(Fp frt, Fp frb) {
  return emit32((63u << 26) | (frt.id() << 21) | (22u << 16) | (frb.id() << 11) | (583u << 1));
}

Error Assembler::mffscrni(Fp frt, uint32_t rm) {
  return emit32((63u << 26) | (frt.id() << 21) | (23u << 16) | ((rm & 3u) << 11) | (583u << 1));
}

Error Assembler::mffscdrn(Fp frt, Fp frb) {
  return emit32((63u << 26) | (frt.id() << 21) | (20u << 16) | (frb.id() << 11) | (583u << 1));
}

Error Assembler::mffscdrni(Fp frt, uint32_t drm) {
  return emit32((63u << 26) | (frt.id() << 21) | (21u << 16) | ((drm & 3u) << 11) | (583u << 1));
}

Error Assembler::mtfsf(uint32_t fxm, Fp frb) {
  return emit32((63u << 26) | ((fxm & 0xFFu) << 17) | (frb.id() << 11) | (711u << 1));
}

Error Assembler::mtfsb0(uint32_t crb) {
  return emit32((63u << 26) | (crb << 21) | (70u << 1));
}

Error Assembler::mtfsb1(uint32_t crb) {
  return emit32((63u << 26) | (crb << 21) | (38u << 1));
}

Error Assembler::mtfsfi(uint32_t bf, uint32_t imm) {
  return emit32((63u << 26) | (bf << 23) | ((imm & 0xFu) << 12) | (134u << 1));
}

Error Assembler::mcrfs(uint32_t bf, uint32_t bfa) {
  return emit32((63u << 26) | (bf << 23) | (bfa << 18) | (64u << 1));
}

Error Assembler::bailIfFPSpecial(Fp fr, const Label& bail) {
  ASMJIT_PROPAGATE(fcmpo(0, fr, fr));
  ASMJIT_PROPAGATE(bc(12, 3, bail));        // unordered (NaN)
  ASMJIT_PROPAGATE(stfd(fr, ppc::ptr(r1, -8)));       // move the bits to a GPR
  ASMJIT_PROPAGATE(ld(r0, ppc::ptr(r1, -8)));
  ASMJIT_PROPAGATE(rldicl(r0, r0, 0, 1));   // drop the sign bit
  ASMJIT_PROPAGATE(li(r11, 0x7FF));
  ASMJIT_PROPAGATE(sldi(r11, r11, 52));     // r11 = 0x7FF0000000000000
  ASMJIT_PROPAGATE(cmpld(r0, r11));
  ASMJIT_PROPAGATE(bge(bail));              // infinity (NaN already handled)
  ASMJIT_PROPAGATE(srdi(r11, r0, 52));      // exponent
  ASMJIT_PROPAGATE(cmpdi(r11, 0));
  Label skip = new_label();
  ASMJIT_PROPAGATE(bne(skip));              // nonzero exponent -> normal
  ASMJIT_PROPAGATE(cmpdi(r0, 0));
  ASMJIT_PROPAGATE(bne(bail));              // zero exponent, nonzero mantissa -> denormal
  return bind(skip);
}

Error Assembler::align(AlignMode align_mode, uint32_t alignment) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }
  if (ASMJIT_UNLIKELY(alignment < 4 || (alignment & (alignment - 1)) != 0)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  const size_t aligned = (offset() + alignment - 1) & ~size_t(alignment - 1);
  while (offset() < aligned) {
    if (align_mode == AlignMode::kZero) {
      const uint8_t zeros[4] = { 0, 0, 0, 0 };
      ASMJIT_PROPAGATE(embed(zeros, 4));
    }
    else {
      ASMJIT_PROPAGATE(nop());
    }
  }
  return Error::kOk;
}

// XX3/XX2-form VSX: xt = f(xa, xb).  The VSX register high bits (XT[5] ->
// bit 0, XB[5] -> bit 1, XA[5] -> bit 2) are OR'd in so the full vs0..vs63
// range works; for registers 0..31 the extension bits are zero and the
// encoding is unchanged.
static inline uint32_t encode_xx(uint32_t xo, uint32_t xt, uint32_t xa, uint32_t xb) noexcept {
  return (60u << 26) | ((xt & 0x1Fu) << 21) | ((xa & 0x1Fu) << 16) | ((xb & 0x1Fu) << 11) |
         (xo << 1) | (xt >> 5) | ((xb >> 5) << 1) | ((xa >> 5) << 2);
}

// XX3/XX2-form VSX with the ISA 3.0 register-extension bits: `lo` is the
// low 11 bits of the instruction (XO at bits 10-3 plus any constants at
// 2-0); the VSX register high bits (XT[5] -> bit 0, XB[5] -> bit 1, XA[5] ->
// bit 2) are OR'd in, which allows the full vs0..vs63 range.  Three-operand
// forms have bits 2-0 of `lo` zero; two-operand forms pass `xa = 0` and keep
// their constant bits.
static inline uint32_t encode_vsx(uint32_t lo, uint32_t xt, uint32_t xa, uint32_t xb) noexcept {
  return (60u << 26) | ((xt & 0x1Fu) << 21) | ((xa & 0x1Fu) << 16) | ((xb & 0x1Fu) << 11) |
         ((lo & 0x7FFu) | (xt >> 5) | ((xb >> 5) << 1) | ((xa >> 5) << 2));
}

Error Assembler::mfvsrd(Gp rt, Vsx xs) {
  return emit32((31u << 26) | ((xs.id() & 0x1Fu) << 21) | (rt.id() << 16) | (51u << 1) | (xs.id() >> 5));
}

Error Assembler::mfvsrld(Gp rt, Vsx xs) {
  return emit32((31u << 26) | ((xs.id() & 0x1Fu) << 21) | (rt.id() << 16) | (307u << 1) | (xs.id() >> 5));
}

Error Assembler::mfvsrwz(Gp rt, Vsx xs) {
  return emit32((31u << 26) | ((xs.id() & 0x1Fu) << 21) | (rt.id() << 16) | (115u << 1) | (xs.id() >> 5));
}

Error Assembler::mtvsrd(Vsx xt, Gp ra) {
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (ra.id() << 16) | (179u << 1) | (xt.id() >> 5));
}

Error Assembler::mtvsrdd(Vsx xt, Gp ra, Gp rb) {
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (ra.id() << 16) | (rb.id() << 11) |
                (435u << 1) | (xt.id() >> 5));
}

Error Assembler::mtvsrwa(Vsx xt, Gp ra) {
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (ra.id() << 16) | (211u << 1) | (xt.id() >> 5));
}

Error Assembler::mtvsrws(Vsx xt, Gp ra) {
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (ra.id() << 16) | (403u << 1) | (xt.id() >> 5));
}

Error Assembler::mtvsrwz(Vsx xt, Gp ra) {
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (ra.id() << 16) | (243u << 1) | (xt.id() >> 5));
}

Error Assembler::mfvrd(Gp rt, Vr vr) {
  return emit32((31u << 26) | (vr.id() << 21) | (rt.id() << 16) | (51u << 1) | 1u);
}

Error Assembler::mfvrwz(Gp rt, Vr vr) {
  return emit32((31u << 26) | (vr.id() << 21) | (rt.id() << 16) | (115u << 1) | 1u);
}

Error Assembler::mtvrd(Vr vr, Gp rt) {
  return emit32((31u << 26) | (vr.id() << 21) | (rt.id() << 16) | (179u << 1) | 1u);
}

Error Assembler::mtvrwa(Vr vr, Gp rt) {
  return emit32((31u << 26) | (vr.id() << 21) | (rt.id() << 16) | (211u << 1) | 1u);
}

Error Assembler::mtvrwz(Vr vr, Gp rt) {
  return emit32((31u << 26) | (vr.id() << 21) | (rt.id() << 16) | (243u << 1) | 1u);
}

Error Assembler::lxsd(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((57u << 26) | (xt.id() << 21) | (m.base_id() << 16) |
                (((uint16_t(m.offset()) >> 2) & 0x3FFFu) << 2) | 2u);
}

Error Assembler::lxssp(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((57u << 26) | (xt.id() << 21) | (m.base_id() << 16) |
                (((uint16_t(m.offset()) >> 2) & 0x3FFFu) << 2) | 3u);
}

Error Assembler::lxsdx(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (588u << 1));
}

Error Assembler::lxsspx(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (524u << 1));
}

Error Assembler::lxsiwzx(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (12u << 1));
}

Error Assembler::lxsiwax(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(xt.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (76u << 1));
}

Error Assembler::lxv(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 15u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((61u << 26) | ((xt.id() & 0x1Fu) << 21) | (m.base_id() << 16) |
                ((xt.id() >> 5) << 3) |
                (((uint16_t(m.offset()) >> 4) & 0xFFFu) << 4) | 1u);
}

Error Assembler::lxvx(Vsx xt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | ((xt.id() & 0x1Fu) << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (268u << 1) | (xt.id() >> 5));
}

Error Assembler::stxsd(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(xs.id() >= 32u || !m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((61u << 26) | (xs.id() << 21) | (m.base_id() << 16) |
                (((uint16_t(m.offset()) >> 2) & 0x3FFFu) << 2) | 2u);
}

Error Assembler::stxssp(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(xs.id() >= 32u || !m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 3u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((61u << 26) | (xs.id() << 21) | (m.base_id() << 16) |
                (((uint16_t(m.offset()) >> 2) & 0x3FFFu) << 2) | 3u);
}

Error Assembler::stxsdx(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(xs.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (716u << 1));
}

Error Assembler::stxsspx(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(xs.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (652u << 1));
}

Error Assembler::stxsiwx(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(xs.id() >= 32u || !m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (xs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (140u << 1));
}

Error Assembler::stxv(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || m.has_index() || m.offset() < -32768 || m.offset() > 32767 || (m.offset() & 15u)))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((61u << 26) | ((xs.id() & 0x1Fu) << 21) | (m.base_id() << 16) |
                ((xs.id() >> 5) << 3) |
                (((uint16_t(m.offset()) >> 4) & 0xFFFu) << 4) | 5u);
}

Error Assembler::stxvx(Vsx xs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | ((xs.id() & 0x1Fu) << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (396u << 1) | (xs.id() >> 5));
}

Error Assembler::xsabsdp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(690u, xt.id(), 0, xb.id()));
}

Error Assembler::xsadddp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(128u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xscpsgndp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(704u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsdivdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(224u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmaddadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(132u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmaddmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(164u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmsubadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(196u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmsubmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(228u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmuldp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(192u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnegdp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(754u, xt.id(), 0, xb.id()));
}

Error Assembler::xsnmsubadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(708u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmsubmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(740u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmaddadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(644u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmaddmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(676u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xssqrtdp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(150u, xt.id(), 0, xb.id()));
}

Error Assembler::xssubdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(160u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsaddsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(0u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsdivsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(96u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmaddasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(4u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmaddmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(36u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmsubasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(68u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmsubmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(100u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsmulsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(64u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmsubasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(580u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmsubmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(612u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmaddasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(516u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xsnmaddmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(548u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xssqrtsp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(22u, xt.id(), 0, xb.id()));
}

Error Assembler::xssubsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(32u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xscmpudp(uint32_t bf, Vsx xa, Vsx xb) {
  return emit32((60u << 26) | (bf << 23) | (xa.id() << 16) | (xb.id() << 11) | (140u << 1));
}

Error Assembler::xscmpodp(uint32_t bf, Vsx xa, Vsx xb) {
  return emit32((60u << 26) | (bf << 23) | (xa.id() << 16) | (xb.id() << 11) | (172u << 1));
}

Error Assembler::xscmpeqdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(12u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xscmpgedp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(76u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xscmpgtdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(44u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xstdivdp(uint32_t bf, Vsx xa, Vsx xb) {
  return emit32((60u << 26) | (bf << 23) | (xa.id() << 16) | (xb.id() << 11) | (244u << 1));
}

Error Assembler::xstsqrtdp(uint32_t bf, Vsx xb) {
  return emit32((60u << 26) | (bf << 23) | (xb.id() << 11) | (212u << 1));
}

Error Assembler::xsrdpi(Vsx xt, Vsx xb) {
  return emit32(encode_xx(146u, xt.id(), 0, xb.id()));
}

Error Assembler::xsrdpic(Vsx xt, Vsx xb) {
  return emit32(encode_xx(214u, xt.id(), 0, xb.id()));
}

Error Assembler::xsrdpiz(Vsx xt, Vsx xb) {
  return emit32(encode_xx(178u, xt.id(), 0, xb.id()));
}

Error Assembler::xsrdpip(Vsx xt, Vsx xb) {
  return emit32(encode_xx(210u, xt.id(), 0, xb.id()));
}

Error Assembler::xsrdpim(Vsx xt, Vsx xb) {
  return emit32(encode_xx(242u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvdpsp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(530u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvspdp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(658u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvdpsxds(Vsx xt, Vsx xb) {
  return emit32(encode_xx(688u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvdpuxds(Vsx xt, Vsx xb) {
  return emit32(encode_xx(656u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvsxddp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(752u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvuxddp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(720u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvdpsxws(Vsx xt, Vsx xb) {
  return emit32(encode_xx(176u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvdpuxws(Vsx xt, Vsx xb) {
  return emit32(encode_xx(144u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvsxdsp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(624u, xt.id(), 0, xb.id()));
}

Error Assembler::xscvuxdsp(Vsx xt, Vsx xb) {
  return emit32(encode_xx(592u, xt.id(), 0, xb.id()));
}

Error Assembler::xvadddp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x300u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvaddsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x200u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvsubdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x340u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvsubsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x240u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmuldp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x380u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmulsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x280u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvdivdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x3C0u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvdivsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x2C0u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaddadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x308u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaddmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x348u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmsubadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x388u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmsubmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x3C8u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmaddadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x708u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmaddmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x748u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmsubadp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x788u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmsubmdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x7C8u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaddasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x208u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaddmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x248u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmsubasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x288u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmsubmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x2C8u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmaddasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x608u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmaddmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x648u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmsubasp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x688u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvnmsubmsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x6C8u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaxdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x700u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmindp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x740u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvmaxsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x600u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvminsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x640u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpeqdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x318u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpgtdp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x358u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpgedp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x398u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpeqsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x218u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpgtsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x258u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcmpgesp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x298u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvabsdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x764u, xt.id(), 0, xb.id()));
}

Error Assembler::xvabssp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x664u, xt.id(), 0, xb.id()));
}

Error Assembler::xvnabsdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x7A4u, xt.id(), 0, xb.id()));
}

Error Assembler::xvnabssp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x6A4u, xt.id(), 0, xb.id()));
}

Error Assembler::xvnegdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x7E4u, xt.id(), 0, xb.id()));
}

Error Assembler::xvnegsp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x6E4u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcpsgndp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x780u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvcpsgnsp(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_vsx(0x680u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xvsqrtdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x32Cu, xt.id(), 0, xb.id()));
}

Error Assembler::xvsqrtsp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x22Cu, xt.id(), 0, xb.id()));
}

Error Assembler::xvrsqrtedp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x328u, xt.id(), 0, xb.id()));
}

Error Assembler::xvrsqrtesp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x228u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvsxwdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x3E0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvuxwdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x3A0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvsxwsp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x2E0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvuxwsp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x2A0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvdpsxws(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x360u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvdpuxws(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x320u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvdpsxds(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x760u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvdpuxds(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x720u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvsxddp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x7E0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvuxddp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x7A0u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvdpsp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x624u, xt.id(), 0, xb.id()));
}

Error Assembler::xvcvspdp(Vsx xt, Vsx xb) {
  return emit32(encode_vsx(0x724u, xt.id(), 0, xb.id()));
}

Error Assembler::xxlxor(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(616u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxlor(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(584u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxland(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(520u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxlandc(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(552u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxlorc(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(680u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxlnand(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(712u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxlnor(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(648u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxleqv(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(744u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxsel(Vsx xt, Vsx xa, Vsx xb, Vsx xc) {
  return emit32((60u << 26) | ((xt.id() & 0x1Fu) << 21) | ((xa.id() & 0x1Fu) << 16) |
                ((xb.id() & 0x1Fu) << 11) | ((xc.id() & 0x1Fu) << 6) | (152u << 1) |
                (xt.id() >> 5) | ((xb.id() >> 5) << 1) | ((xa.id() >> 5) << 2) |
                ((xc.id() >> 5) << 3));
}

Error Assembler::xxperm(Vsx xt, Vsx xa, Vsx xb) {
  return emit32(encode_xx(104u, xt.id(), xa.id(), xb.id()));
}

Error Assembler::xxpermdi(Vsx xt, Vsx xa, Vsx xb, uint32_t dm) {
  return emit32((60u << 26) | ((xt.id() & 0x1Fu) << 21) | ((xa.id() & 0x1Fu) << 16) |
                ((xb.id() & 0x1Fu) << 11) | ((dm & 3u) << 8) | (40u << 1) |
                (xt.id() >> 5) | ((xb.id() >> 5) << 1) | ((xa.id() >> 5) << 2));
}

Error Assembler::xxmrghd(Vsx xt, Vsx xa, Vsx xb) {
  return xxpermdi(xt, xa, xb, 0);
}

Error Assembler::xxmrgld(Vsx xt, Vsx xa, Vsx xb) {
  return xxpermdi(xt, xa, xb, 3);
}

Error Assembler::xxspltd(Vsx xt, Vsx xb, uint32_t uim) {
  return xxpermdi(xt, xb, xb, uim & 1u ? 3u : 0u);
}

Error Assembler::xxspltw(Vsx xt, Vsx xb, uint32_t uim) {
  return emit32((60u << 26) | (xt.id() << 21) | ((uim & 3u) << 16) | (xb.id() << 11) | (328u << 1));
}

Error Assembler::xxswapd(Vsx xt, Vsx xa) {
  return xxpermdi(xt, xa, xa, 2);
}

// VX-form VMX: vrt = f(vra, vrb), XO split across vrc (bits 21-25) and xo (26-30).
static inline uint32_t encode_vx(uint32_t vrc, uint32_t xo, uint32_t vrt, uint32_t vra,
                                 uint32_t vrb, bool rc = false) noexcept {
  return (4u << 26) | (vrt << 21) | (vra << 16) | (vrb << 11) | (vrc << 6) | (xo << 1) |
         (rc ? 1u : 0u);
}

// VA-form VMX: vrt = f(vra, vrb, vrc), 5-bit XO in the low field.
static inline uint32_t encode_va(uint32_t xo, uint32_t vrt, uint32_t vra, uint32_t vrb,
                                 uint32_t vrc, bool rc = false) noexcept {
  return (4u << 26) | (vrt << 21) | (vra << 16) | (vrb << 11) | (vrc << 6) | (xo << 1) |
         (rc ? 1u : 0u);
}

Error Assembler::lvx(Vr vrt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (103u << 1));
}

Error Assembler::lvebx(Vr vrt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (7u << 1));
}

Error Assembler::lvehx(Vr vrt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (39u << 1));
}

Error Assembler::lvewx(Vr vrt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (71u << 1));
}

Error Assembler::lvxl(Vr vrt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (359u << 1));
}

Error Assembler::stvx(Vr vrs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (231u << 1));
}

Error Assembler::stvebx(Vr vrs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (135u << 1));
}

Error Assembler::stvehx(Vr vrs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (167u << 1));
}

Error Assembler::stvewx(Vr vrs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (199u << 1));
}

Error Assembler::stvxl(Vr vrs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index() || m.has_offset()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (vrs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (487u << 1));
}

Error Assembler::vand(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vandc(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vor(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(18u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vxor(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(19u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vnor(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(20u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::veqv(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(26u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vnand(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(22u, 2u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::vaddubm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vadduhm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vadduwm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddudm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddcuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsububm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubuhm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubuwm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(18u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubudm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(19u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubcuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(22u, 0u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::vsl(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(7u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsr(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(11u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsld(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(23u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(27u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrad(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(15u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vslw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(10u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsraw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(14u, 2u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::vcmpequb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpequh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpequw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpequd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpgtsb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(12u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtsh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(13u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtsw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(14u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtsd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(15u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpgtub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(8u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtuh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(10u, 3u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vcmpgtud(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(11u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }

Error Assembler::vminub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(8u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminuh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(10u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminsb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(12u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminsh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(13u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminsw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(14u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxuh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxsb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxsh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxsw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 1u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::vpkuhum(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkuwum(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkuhus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkuwus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkshss(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkswss(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(7u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkshus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkswus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vupkhsb(Vr vrt, Vr vrb) { return emit32(encode_vx(8u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vupkhsh(Vr vrt, Vr vrb) { return emit32(encode_vx(9u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vupklsb(Vr vrt, Vr vrb) { return emit32(encode_vx(10u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vupklsh(Vr vrt, Vr vrb) { return emit32(encode_vx(11u, 7u, vrt.id(), 0, vrb.id())); }

Error Assembler::vmrghb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrghh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrghw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrglb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrglh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrglw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 6u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::vspltb(Vr vrt, Vr vrb, uint32_t uim) {
  return emit32(encode_vx(8u, 6u, vrt.id(), uim & 0xFu, vrb.id()));
}
Error Assembler::vsplth(Vr vrt, Vr vrb, uint32_t uim) {
  return emit32(encode_vx(9u, 6u, vrt.id(), uim & 0x7u, vrb.id()));
}
Error Assembler::vspltw(Vr vrt, Vr vrb, uint32_t uim) {
  return emit32(encode_vx(10u, 6u, vrt.id(), uim & 0x3u, vrb.id()));
}
Error Assembler::vspltisb(Vr vrt, int32_t simm) {
  return emit32(encode_vx(12u, 6u, vrt.id(), uint32_t(simm) & 0x1Fu, 0));
}
Error Assembler::vspltish(Vr vrt, int32_t simm) {
  return emit32(encode_vx(13u, 6u, vrt.id(), uint32_t(simm) & 0x1Fu, 0));
}
Error Assembler::vspltisw(Vr vrt, int32_t simm) {
  return emit32(encode_vx(14u, 6u, vrt.id(), uint32_t(simm) & 0x1Fu, 0));
}

Error Assembler::vperm(Vr vrt, Vr vra, Vr vrb, Vr vrc) {
  return emit32((4u << 26) | (vrt.id() << 21) | (vra.id() << 16) | (vrb.id() << 11) |
                (vrc.id() << 6) | (21u << 1) | 1u);
}
Error Assembler::vsel(Vr vrt, Vr vra, Vr vrb, Vr vrc) {
  return emit32((4u << 26) | (vrt.id() << 21) | (vra.id() << 16) | (vrb.id() << 11) |
                (vrc.id() << 6) | (21u << 1));
}
Error Assembler::vsldoi(Vr vrt, Vr vra, Vr vrb, uint32_t shb) {
  return emit32((4u << 26) | (vrt.id() << 21) | (vra.id() << 16) | (vrb.id() << 11) |
                ((shb & 0xFu) << 6) | (22u << 1));
}

Error Assembler::vclzb(Vr vrt, Vr vrb) { return emit32(encode_vx(28u, 1u, vrt.id(), 0, vrb.id())); }
Error Assembler::vclzh(Vr vrt, Vr vrb) { return emit32(encode_vx(29u, 1u, vrt.id(), 0, vrb.id())); }
Error Assembler::vclzw(Vr vrt, Vr vrb) { return emit32(encode_vx(30u, 1u, vrt.id(), 0, vrb.id())); }
Error Assembler::vclzd(Vr vrt, Vr vrb) { return emit32(encode_vx(31u, 1u, vrt.id(), 0, vrb.id())); }
Error Assembler::vpopcntb(Vr vrt, Vr vrb) { return emit32(encode_vx(28u, 1u, vrt.id(), 0, vrb.id(), true)); }
Error Assembler::vpopcnth(Vr vrt, Vr vrb) { return emit32(encode_vx(29u, 1u, vrt.id(), 0, vrb.id(), true)); }
Error Assembler::vpopcntw(Vr vrt, Vr vrb) { return emit32(encode_vx(30u, 1u, vrt.id(), 0, vrb.id(), true)); }
Error Assembler::vpopcntd(Vr vrt, Vr vrb) { return emit32(encode_vx(31u, 1u, vrt.id(), 0, vrb.id(), true)); }

Error Assembler::vmulouw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmuluwm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 4u, vrt.id(), vra.id(), vrb.id(), true)); }

// Saturating adds.
Error Assembler::vaddsbs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(12u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddshs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(13u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddsws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(14u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddubs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(8u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vadduhs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vadduws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(10u, 0u, vrt.id(), vra.id(), vrb.id())); }
// Saturating subtracts.
Error Assembler::vsubsbs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(28u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubshs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(29u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubsws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(30u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsububs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(24u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubuhs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(25u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubuws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(26u, 0u, vrt.id(), vra.id(), vrb.id())); }
// 128-bit add/subtract with carry/borrow.
Error Assembler::vaddcuq(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vadduqm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vaddeuqm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(30u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vaddecuq(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(30u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vsubcuq(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(21u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubuqm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(20u, 0u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsubeuqm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(31u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vsubecuq(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(31u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
// Absolute difference.
Error Assembler::vabsdub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 1u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vabsduh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 1u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vabsduw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(18u, 1u, vrt.id(), vra.id(), vrb.id(), true)); }

// Byte/halfword shifts.
Error Assembler::vslb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vslh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(8u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 2u, vrt.id(), vra.id(), vrb.id())); }
// Shift by octet / variable.
Error Assembler::vslo(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsro(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vslv(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(29u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsrv(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(28u, 2u, vrt.id(), vra.id(), vrb.id())); }
// Rotates.
Error Assembler::vrlb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vrlh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vrlw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vrld(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 2u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vrlwmi(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 2u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vrlwnm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 2u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vrldmi(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 2u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vrldnm(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(7u, 2u, vrt.id(), vra.id(), vrb.id(), true)); }

// Compare not equal.
Error Assembler::vcmpneb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpneh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpnew(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(2u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpnezb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpnezh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vcmpnezw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 3u, vrt.id(), vra.id(), vrb.id(), true)); }

// Doubleword min/max.
Error Assembler::vmaxsd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(7u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmaxud(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(3u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminsd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(15u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vminud(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(11u, 1u, vrt.id(), vra.id(), vrb.id())); }
// Averages.
Error Assembler::vavgub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vavguh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vavguw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(18u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vavgsb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(20u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vavgsh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(21u, 1u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vavgsw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(22u, 1u, vrt.id(), vra.id(), vrb.id())); }
// Sum across.
Error Assembler::vsum4sbs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(28u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsum4shs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(25u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsum4ubs(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(24u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsum2sws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(26u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vsumsws(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(30u, 4u, vrt.id(), vra.id(), vrb.id())); }

// Pack/unpack doubleword and pixel.
Error Assembler::vpkudum(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpkudus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(19u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpksdss(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(23u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpksdus(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(21u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vupkhsw(Vr vrt, Vr vrb) { return emit32(encode_vx(25u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vupklsw(Vr vrt, Vr vrb) { return emit32(encode_vx(27u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vpkpx(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(12u, 7u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vupkhpx(Vr vrt, Vr vrb) { return emit32(encode_vx(13u, 7u, vrt.id(), 0, vrb.id())); }
Error Assembler::vupklpx(Vr vrt, Vr vrb) { return emit32(encode_vx(15u, 7u, vrt.id(), 0, vrb.id())); }
// Merge odd/even words.
Error Assembler::vmrgew(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(30u, 6u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmrgow(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(26u, 6u, vrt.id(), vra.id(), vrb.id())); }

// Permute right-indexed / permute xor.
Error Assembler::vpermr(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(29u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vpermxor(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(22u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }

// Count trailing zeros (VRA field holds a fixed element-size code).
Error Assembler::vctzb(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 28u, vrb.id())); }
Error Assembler::vctzh(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 29u, vrb.id())); }
Error Assembler::vctzw(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 30u, vrb.id())); }
Error Assembler::vctzd(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 31u, vrb.id())); }
Error Assembler::vclzlsbb(Gp rt, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (0u << 16) | (vrb.id() << 11) | (24u << 6) | (1u << 1));
}
Error Assembler::vctzlsbb(Gp rt, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (1u << 16) | (vrb.id() << 11) | (24u << 6) | (1u << 1));
}

// Multiply even/odd.
Error Assembler::vmuleub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(8u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmuleuh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmuleuw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(10u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulesb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(12u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulesh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(13u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulesw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(14u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmuloub(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(0u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulouh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulosb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(4u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulosh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(5u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vmulosw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(6u, 4u, vrt.id(), vra.id(), vrb.id())); }
// Multiply by 10.
Error Assembler::vmul10cuq(Vr vrt, Vr vra) { return emit32(encode_vx(0u, 0u, vrt.id(), vra.id(), 0, true)); }
Error Assembler::vmul10uq(Vr vrt, Vr vra) { return emit32(encode_vx(8u, 0u, vrt.id(), vra.id(), 0, true)); }
Error Assembler::vmul10euq(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(9u, 0u, vrt.id(), vra.id(), vrb.id(), true)); }
Error Assembler::vmul10ecuq(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(1u, 0u, vrt.id(), vra.id(), vrb.id(), true)); }
// Multiply-high-add / multiply-sum.
Error Assembler::vmhaddshs(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(16u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vmhraddshs(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(16u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vmladduhm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(17u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vmsummbm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(18u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vmsumshm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(20u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vmsumshs(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(20u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vmsumubm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(18u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vmsumudm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(17u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
Error Assembler::vmsumuhm(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(19u, vrt.id(), vra.id(), vrb.id(), vrc.id())); }
Error Assembler::vmsumuhs(Vr vrt, Vr vra, Vr vrb, Vr vrc) { return emit32(encode_va(19u, vrt.id(), vra.id(), vrb.id(), vrc.id(), true)); }
// Polynomial multiply-sum.
Error Assembler::vpmsumb(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(16u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpmsumh(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(17u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpmsumw(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(18u, 4u, vrt.id(), vra.id(), vrb.id())); }
Error Assembler::vpmsumd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(19u, 4u, vrt.id(), vra.id(), vrb.id())); }

// Extract element (UIM in the VRA field).
Error Assembler::vextractub(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(8u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vextractuh(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(9u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vextractuw(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(10u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vextractd(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(11u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
// Extract element indexed (GPR address).
Error Assembler::vextublx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (24u << 6) | (6u << 1) | 1u);
}
Error Assembler::vextubrx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (28u << 6) | (6u << 1) | 1u);
}
Error Assembler::vextuhlx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (25u << 6) | (6u << 1) | 1u);
}
Error Assembler::vextuhrx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (29u << 6) | (6u << 1) | 1u);
}
Error Assembler::vextuwlx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (26u << 6) | (6u << 1) | 1u);
}
Error Assembler::vextuwrx(Gp rt, Gp ra, Vr vrb) {
  return emit32((4u << 26) | (rt.id() << 21) | (ra.id() << 16) | (vrb.id() << 11) | (30u << 6) | (6u << 1) | 1u);
}
// Insert element (UIM in the VRA field).
Error Assembler::vinsertb(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(12u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vinserth(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(13u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vinsertw(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(14u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }
Error Assembler::vinsertd(Vr vrt, Vr vrb, uint32_t uim) { return emit32(encode_vx(15u, 6u, vrt.id(), uim & 0xFu, vrb.id(), true)); }

// Sign extend (VRA field holds a fixed source-size code).
Error Assembler::vextsb2w(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 16u, vrb.id())); }
Error Assembler::vextsh2w(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 17u, vrb.id())); }
Error Assembler::vextsw2d(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 26u, vrb.id())); }
Error Assembler::vextsb2d(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 24u, vrb.id())); }
Error Assembler::vextsh2d(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 25u, vrb.id())); }
Error Assembler::vnegw(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 6u, vrb.id())); }
Error Assembler::vnegd(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 7u, vrb.id())); }
Error Assembler::vprtybd(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 9u, vrb.id())); }
Error Assembler::vprtybw(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 8u, vrb.id())); }
Error Assembler::vprtybq(Vr vrt, Vr vrb) { return emit32(encode_vx(24u, 1u, vrt.id(), 10u, vrb.id())); }
// Gather bits / bit permute.
Error Assembler::vgbbd(Vr vrt, Vr vrb) { return emit32(encode_vx(20u, 6u, vrt.id(), 0, vrb.id())); }
Error Assembler::vbpermd(Vr vrt, Vr vra, Vr vrb) { return emit32(encode_vx(23u, 6u, vrt.id(), vra.id(), vrb.id())); }

Error Assembler::prolog(int32_t frame_size, uint32_t save_mask, uint32_t fpr_mask, uint32_t vr_mask) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const uint32_t regs = save_mask & 0x3FFFFu; // Bits 0..17 select r14..r31.
  const uint32_t fprs = fpr_mask & 0x3FFFFu;  // Bits 0..17 select f14..f31.
  const uint32_t vrs = vr_mask & 0xFFFu;      // Bits 0..11 select v20..v31.
  // ABI-fixed slots: FPR f14+k at -(144 - 8*k), GPR r14+k below the FP area
  // at -(fpr_bytes + 144 - 8*k), and vector v20+k below the GPR area at
  // -(fpr_bytes + gpr_bytes + 192 - 16*k), relative to the caller's SP.
  int32_t required = minimum_frame_size();
  int32_t fp_bytes = 0;
  if (fprs != 0) {
    const uint32_t lowest = fprs & (~fprs + 1u); // Lowest set bit (highest register).
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    fp_bytes = int32_t(8 * (18 - k_min));
  }
  int32_t gpr_bytes = 0;
  if (regs != 0) {
    const uint32_t lowest = regs & (~regs + 1u); // Lowest set bit (highest register).
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    gpr_bytes = int32_t(8 * (18 - k_min));
  }
  int32_t vr_bytes = 0;
  if (vrs != 0) {
    const uint32_t lowest = vrs & (~vrs + 1u); // Lowest set bit (highest register).
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    vr_bytes = int32_t(16 * (12 - k_min));
  }
  required = 32 + fp_bytes + gpr_bytes + vr_bytes;
  if (required < minimum_frame_size())
    required = minimum_frame_size();
  if (ASMJIT_UNLIKELY(frame_size < minimum_frame_size() || (frame_size & 15) != 0 ||
                      frame_size < required)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  // Save LR and CR in the caller's fixed area and nonvolatile GPRs/FPRs at the
  // top of our own frame, exactly like GCC.
  ASMJIT_PROPAGATE(mflr(r0));
  ASMJIT_PROPAGATE(std(r0, ppc::ptr(r1, 16)));
  if (regs != 0 || fprs != 0 || vrs != 0) {
    ASMJIT_PROPAGATE(mfcr(r11));
    ASMJIT_PROPAGATE(stw(r11, ppc::ptr(r1, 8)));
    for (uint32_t k = 0; k < 18; k++) {
      if (fprs & (1u << k)) {
        ASMJIT_PROPAGATE(stfd(Fp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-8 * (18 - k)))));
      }
    }
    for (uint32_t k = 0; k < 18; k++) {
      if (regs & (1u << k)) {
        ASMJIT_PROPAGATE(std(Gp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-fp_bytes - 8 * (18 - k)))));
      }
    }
    for (uint32_t k = 0; k < 12; k++) {
      if (vrs & (1u << k)) {
        ASMJIT_PROPAGATE(li(r0, int16_t(-fp_bytes - gpr_bytes - 192 + 16 * k)));
        ASMJIT_PROPAGATE(stvx(Vr { uint32_t(20 + k) }, ppc::ptr(r1, r0)));
      }
    }
  }

  if (frame_size <= 32764) {
    // DS-form stdu displacement is a 14-bit signed field shifted left by 2,
    // so a single instruction covers frames smaller than 32 KiB.
    return stdu(r1, ppc::ptr(r1, int32_t(-frame_size)));
  }

  // Large frames: a frame size fits in 32 bits, so the negative size is two
  // instructions (addis+ori), and an indexed store-with-update writes the back
  // chain once at the final frame (same sequence as GCC).
  const uint32_t neg = 0u - uint32_t(frame_size);
  ASMJIT_PROPAGATE(addis(r0, r0, int16_t(neg >> 16)));
  ASMJIT_PROPAGATE(ori(r0, r0, uint16_t(neg)));
  return stdux(r1, ppc::ptr(r1, r0));
}

Error Assembler::epilog(int32_t frame_size, uint32_t save_mask, uint32_t fpr_mask, uint32_t vr_mask) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const uint32_t regs = save_mask & 0x3FFFFu; // Bits 0..17 select r14..r31.
  const uint32_t fprs = fpr_mask & 0x3FFFFu;  // Bits 0..17 select f14..f31.
  const uint32_t vrs = vr_mask & 0xFFFu;      // Bits 0..11 select v20..v31.
  int32_t required = minimum_frame_size();
  int32_t fp_bytes = 0;
  if (fprs != 0) {
    const uint32_t lowest = fprs & (~fprs + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    fp_bytes = int32_t(8 * (18 - k_min));
  }
  int32_t gpr_bytes = 0;
  if (regs != 0) {
    const uint32_t lowest = regs & (~regs + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    gpr_bytes = int32_t(8 * (18 - k_min));
  }
  int32_t vr_bytes = 0;
  if (vrs != 0) {
    const uint32_t lowest = vrs & (~vrs + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    vr_bytes = int32_t(16 * (12 - k_min));
  }
  required = 32 + fp_bytes + gpr_bytes + vr_bytes;
  if (required < minimum_frame_size())
    required = minimum_frame_size();
  if (ASMJIT_UNLIKELY(frame_size < minimum_frame_size() || (frame_size & 15) != 0 ||
                      frame_size < required)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  if (frame_size <= 32764) {
    ASMJIT_PROPAGATE(addi(r1, r1, int16_t(frame_size)));
  }
  else {
    // The back chain written by the prolog points at the caller's SP, so the
    // whole frame is released with a single load (same as GCC).
    ASMJIT_PROPAGATE(ld(r1, ppc::ptr(r1, 0)));
  }

  if (regs != 0 || fprs != 0 || vrs != 0) {
    for (uint32_t k = 18; k-- > 0;) {
      if (regs & (1u << k)) {
        ASMJIT_PROPAGATE(ld(Gp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-fp_bytes - 8 * (18 - k)))));
      }
    }
    for (uint32_t k = 18; k-- > 0;) {
      if (fprs & (1u << k)) {
        ASMJIT_PROPAGATE(lfd(Fp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-8 * (18 - k)))));
      }
    }
    for (uint32_t k = 12; k-- > 0;) {
      if (vrs & (1u << k)) {
        ASMJIT_PROPAGATE(li(r0, int16_t(-fp_bytes - gpr_bytes - 192 + 16 * k)));
        ASMJIT_PROPAGATE(lvx(Vr { uint32_t(20 + k) }, ppc::ptr(r1, r0)));
      }
    }
    ASMJIT_PROPAGATE(lwz(r11, ppc::ptr(r1, 8)));
    ASMJIT_PROPAGATE(mtcrf(0x38, r11)); // Restore nonvolatile CR fields CR2..CR4.
  }

  ASMJIT_PROPAGATE(ld(r0, ppc::ptr(r1, 16)));
  ASMJIT_PROPAGATE(mtlr(r0));
  return blr();
}

Error Assembler::emitBranch(int bo, int bi, const Label& label) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const size_t branch_offset = offset();
  ASMJIT_PROPAGATE(emit32((16u << 26) | (uint32_t(bo) << 21) | (uint32_t(bi) << 16)));

  const LabelEntry& le = _code->label_entry_of(label);
  const BranchPatch patch { label.id(), branch_offset, BranchKind::kConditional };
  if (le.is_bound()) {
    return patchBranch(patch, label);
  }

  _patches.push_back(patch);
  return Error::kOk;
}

Error Assembler::patchBranch(const BranchPatch& patch, const Label& label) {
  const LabelEntry& le = _code->label_entry_of(label);
  if (ASMJIT_UNLIKELY(!le.is_bound())) {
    return report_error(make_error(Error::kInvalidLabel));
  }

  const int64_t target = int64_t(le.offset());
  const int64_t source = int64_t(patch.offset);
  const int64_t displacement = target - source;

  if (ASMJIT_UNLIKELY((displacement & 3) != 0)) {
    return report_error(make_error(Error::kInvalidState));
  }

  uint8_t* p = _section->_buffer.data() + patch.offset;
  uint32_t word = environment().is_little_endian() ? Support::loadu_u32_le(p)
                                                   : Support::loadu_u32_be(p);

  if (patch.kind == BranchKind::kConditional) {
    if (ASMJIT_UNLIKELY(displacement < -32768 || displacement > 32767)) {
      return report_error(make_error(Error::kTooLarge));
    }
    const uint32_t bd = uint32_t(displacement / 4) & 0x3FFFu;
    word = (word & ~0x0000FFFCu) | (bd << 2);
  }
  else {
    if (ASMJIT_UNLIKELY(displacement < -(1LL << 25) || displacement >= (1LL << 25))) {
      return report_error(make_error(Error::kTooLarge));
    }
    const uint32_t li = uint32_t(displacement / 4) & 0x03FFFFFFu;
    word = (word & ~0x03FFFFFCu) | ((li << 2) & 0x03FFFFFCu);
  }

  if (environment().is_little_endian())
    Support::storeu_u32_le(p, word);
  else
    Support::storeu_u32_be(p, word);
  return Error::kOk;
}

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC
