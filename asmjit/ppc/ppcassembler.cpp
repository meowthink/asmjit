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
  ASMJIT_PROPAGATE(addis(rt, r0, int16_t(imm >> 48)));
  ASMJIT_PROPAGATE(ori(rt, rt, uint16_t(imm >> 32)));
  ASMJIT_PROPAGATE(sldi(rt, rt, 32));
  ASMJIT_PROPAGATE(oris(rt, rt, uint16_t(imm >> 16)));
  return ori(rt, rt, uint16_t(imm));
}

Error Assembler::add(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (266u << 1));
}

Error Assembler::addc(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (10u << 1));
}

Error Assembler::addze(Gp rt, Gp ra) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (202u << 1));
}

Error Assembler::addme(Gp rt, Gp ra) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (234u << 1));
}

Error Assembler::subf(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (40u << 1));
}

Error Assembler::subfc(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (8u << 1));
}

Error Assembler::subfze(Gp rt, Gp ra) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (200u << 1));
}

Error Assembler::subfme(Gp rt, Gp ra) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (232u << 1));
}

Error Assembler::neg(Gp rt, Gp ra) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (104u << 1));
}

Error Assembler::and_(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (28u << 1));
}

Error Assembler::or_(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (444u << 1));
}

Error Assembler::xor_(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (316u << 1));
}

Error Assembler::sld(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (27u << 1));
}

Error Assembler::srd(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (539u << 1));
}

Error Assembler::srad(Gp ra, Gp rs, Gp rb) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (794u << 1));
}

Error Assembler::extsw(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (986u << 1));
}

Error Assembler::extsb(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (954u << 1));
}

Error Assembler::extsh(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (922u << 1));
}

Error Assembler::mulld(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (233u << 1));
}

Error Assembler::mullw(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (235u << 1));
}

Error Assembler::mulhdu(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (9u << 1));
}

Error Assembler::mulhd(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (73u << 1));
}

Error Assembler::mulhw(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (75u << 1));
}

Error Assembler::mulhwu(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (11u << 1));
}

Error Assembler::mulli(Gp rt, Gp ra, int16_t simm) {
  return emit32((7u << 26) | (rt.id() << 21) | (ra.id() << 16) | (uint16_t(simm)));
}

Error Assembler::divd(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (489u << 1));
}

Error Assembler::divw(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (491u << 1));
}

Error Assembler::divdu(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (457u << 1));
}

Error Assembler::divwu(Gp rt, Gp ra, Gp rb) {
  return emit32((31u << 26) | (rt.id() << 21) | (ra.id() << 16) | (rb.id() << 11) | (459u << 1));
}

Error Assembler::srawi(Gp ra, Gp rs, uint8_t sh) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | ((sh & 0x1Fu) << 11) | (824u << 1));
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

Error Assembler::cmpd(Gp ra, Gp rb) {
  return emit32((31u << 26) | (1u << 21) | (ra.id() << 16) | (rb.id() << 11));
}

Error Assembler::cmpld(Gp ra, Gp rb) {
  return emit32((31u << 26) | (1u << 21) | (ra.id() << 16) | (rb.id() << 11) | (32u << 1));
}

Error Assembler::cmpdi(Gp ra, int16_t simm) {
  return emit32((11u << 26) | (1u << 21) | (ra.id() << 16) | (uint16_t(simm)));
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

Error Assembler::mtctr(Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (9u << 16) | (467u << 1));
}

Error Assembler::bctrl() {
  return emit32((19u << 26) | (20u << 21) | (528u << 1) | 1u);
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

Error Assembler::rldicl(Gp ra, Gp rs, uint8_t sh, uint8_t mb) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 0));
}

Error Assembler::rldicr(Gp ra, Gp rs, uint8_t sh, uint8_t me) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, me, 4));
}

Error Assembler::rldic(Gp ra, Gp rs, uint8_t sh, uint8_t mb) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 8));
}

Error Assembler::rldimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb) {
  return emit32(encode_rldi(rs.id(), ra.id(), sh, mb, 12));
}

Error Assembler::rlwinm(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me) {
  return emit32((21u << 26) | (rs.id() << 21) | (ra.id() << 16) |
                ((sh & 0x1Fu) << 11) | ((mb & 0x1Fu) << 6) | ((me & 0x1Fu) << 1));
}

Error Assembler::rlwimi(Gp ra, Gp rs, uint8_t sh, uint8_t mb, uint8_t me) {
  return emit32((20u << 26) | (rs.id() << 21) | (ra.id() << 16) |
                ((sh & 0x1Fu) << 11) | ((mb & 0x1Fu) << 6) | ((me & 0x1Fu) << 1));
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

Error Assembler::cntlzw(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (26u << 1));
}

Error Assembler::cntlzd(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (58u << 1));
}

Error Assembler::cnttzd(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (570u << 1));
}

Error Assembler::popcntd(Gp ra, Gp rs) {
  return emit32((31u << 26) | (rs.id() << 21) | (ra.id() << 16) | (506u << 1));
}

Error Assembler::prolog(int32_t frame_size) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }
  // DS-form stdu displacement is a 14-bit signed field shifted left by 2,
  // so a single instruction covers frames smaller than 32 KiB.
  if (ASMJIT_UNLIKELY(frame_size < minimum_frame_size() || (frame_size & 15) != 0 || frame_size > 32764)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  ASMJIT_PROPAGATE(mflr(r0));
  ASMJIT_PROPAGATE(std(r0, ppc::ptr(r1, 16)));
  return stdu(r1, ppc::ptr(r1, int32_t(-frame_size)));
}

Error Assembler::epilog(int32_t frame_size) {
  if (ASMJIT_UNLIKELY(!_code)) {
    return report_error(make_error(Error::kNotInitialized));
  }
  if (ASMJIT_UNLIKELY(frame_size < minimum_frame_size() || (frame_size & 15) != 0 || frame_size > 32764)) {
    return report_error(make_error(Error::kInvalidArgument));
  }

  ASMJIT_PROPAGATE(addi(r1, r1, int16_t(frame_size)));
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
