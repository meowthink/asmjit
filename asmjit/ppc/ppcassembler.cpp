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
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (87u << 1));
}

Error Assembler::lbzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (119u << 1));
}

Error Assembler::lhzx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (279u << 1));
}

Error Assembler::lhzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (311u << 1));
}

Error Assembler::lhax(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (343u << 1));
}

Error Assembler::lhaux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (375u << 1));
}

Error Assembler::lwzx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (23u << 1));
}

Error Assembler::lwzux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (55u << 1));
}

Error Assembler::lwax(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (341u << 1));
}

Error Assembler::ldx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (21u << 1));
}

Error Assembler::ldux(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (53u << 1));
}

Error Assembler::stbx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (215u << 1));
}

Error Assembler::stbux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (247u << 1));
}

Error Assembler::sthx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (407u << 1));
}

Error Assembler::sthux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (439u << 1));
}

Error Assembler::stwx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (151u << 1));
}

Error Assembler::stwux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (183u << 1));
}

Error Assembler::stdx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (149u << 1));
}

Error Assembler::stdux(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (181u << 1));
}

Error Assembler::lhbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (790u << 1));
}

Error Assembler::lwbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (534u << 1));
}

Error Assembler::ldbrx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (532u << 1));
}

Error Assembler::sthbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (918u << 1));
}

Error Assembler::stwbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (662u << 1));
}

Error Assembler::stdbrx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
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
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (533u << 1));
}

Error Assembler::stswi(Gp rs, Gp ra, uint8_t nb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | ((nb & 0x1Fu) << 11) | (725u << 1));
}

Error Assembler::stswx(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (661u << 1));
}

Error Assembler::lwarx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (20u << 1));
}

Error Assembler::ldarx(Gp rt, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (84u << 1));
}

Error Assembler::lbarx(Gp rt, const Mem& m, uint32_t eh) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (52u << 1) | (eh ? 1u : 0u));
}

Error Assembler::lharx(Gp rt, const Mem& m, uint32_t eh) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rt.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) |
                (372u << 1) | (eh ? 1u : 0u));
}

Error Assembler::stwcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (150u << 1) | 1u);
}

Error Assembler::stdcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (214u << 1) | 1u);
}

Error Assembler::stbcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
    return report_error(make_error(Error::kInvalidAddress));
  return emit32((31u << 26) | (rs.id() << 21) | (m.base_id() << 16) | (m.index_id() << 11) | (694u << 1) | 1u);
}

Error Assembler::sthcx_(Gp rs, const Mem& m) {
  if (ASMJIT_UNLIKELY(!m.has_base() || !m.has_index()))
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

Error Assembler::prolog(int32_t frame_size, uint32_t save_mask) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const uint32_t regs = save_mask & 0x3FFFFu; // Bits 0..17 select r14..r31.
  // ABI-fixed slots: r14+k is saved at -(144 - 8*k) relative to the caller's
  // SP, so the frame must cover the highest saved slot plus the 32-byte fixed
  // area at its bottom.
  int32_t required = minimum_frame_size();
  if (regs != 0) {
    const uint32_t lowest = regs & (~regs + 1u); // Lowest set bit (highest register).
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    required = 32 + int32_t(8 * (18 - k_min));
    if (required < minimum_frame_size())
      required = minimum_frame_size();
  }
  if (ASMJIT_UNLIKELY(frame_size < minimum_frame_size() || (frame_size & 15) != 0 ||
                      frame_size < required)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  // Save LR and CR in the caller's fixed area and nonvolatile GPRs at the top
  // of our own frame, exactly like GCC.
  ASMJIT_PROPAGATE(mflr(r0));
  ASMJIT_PROPAGATE(std(r0, ppc::ptr(r1, 16)));
  if (regs != 0) {
    ASMJIT_PROPAGATE(mfcr(r11));
    ASMJIT_PROPAGATE(stw(r11, ppc::ptr(r1, 8)));
    for (uint32_t k = 0; k < 18; k++) {
      if (regs & (1u << k)) {
        ASMJIT_PROPAGATE(std(Gp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-8 * (18 - k)))));
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

Error Assembler::epilog(int32_t frame_size, uint32_t save_mask) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }

  const uint32_t regs = save_mask & 0x3FFFFu; // Bits 0..17 select r14..r31.
  int32_t required = minimum_frame_size();
  if (regs != 0) {
    const uint32_t lowest = regs & (~regs + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    required = 32 + int32_t(8 * (18 - k_min));
    if (required < minimum_frame_size())
      required = minimum_frame_size();
  }
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

  if (regs != 0) {
    for (uint32_t k = 18; k-- > 0;) {
      if (regs & (1u << k)) {
        ASMJIT_PROPAGATE(ld(Gp { uint32_t(14 + k) }, ppc::ptr(r1, int32_t(-8 * (18 - k)))));
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
