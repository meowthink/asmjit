// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC)

#include <asmjit/core/cpuinfo.h>
#include <asmjit/ppc/ppcassembler.h>
#include <asmjit/ppc/ppcinstapi_p.h>
#include <asmjit/ppc/ppcinstdb_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

namespace InstInternal {

// ppc::InstInternal - QueryFeatures
// =================================

#ifndef ASMJIT_NO_INTROSPECTION
Error query_features(const BaseInst& inst, const Operand_* operands, size_t op_count, CpuFeatures* out) noexcept {
  Support::maybe_unused(operands, op_count);

  InstId inst_id = inst.inst_id();
  if (ASMJIT_UNLIKELY(inst_id >= Inst::_kIdCount))
    return make_error(Error::kInvalidInstruction);

  CpuFeatures::PPC::Id feature = Inst::inst_features[inst_id];
  out->reset();
  if (feature != CpuFeatures::PPC::kNone)
    out->ppc().add(feature);
  return Error::kOk;
}
#endif // !ASMJIT_NO_INTROSPECTION

// ppc::InstInternal - QueryRWInfo
// ===============================

#ifndef ASMJIT_NO_INTROSPECTION
//! Returns the size in bytes of a PPC register type.
static uint32_t reg_size_of(RegType reg_type) noexcept {
  switch (reg_type) {
    case RegType::kGp64 : return 8;
    case RegType::kVec64: return 8;  // FPR (f0..f31) - 64-bit half of a VSR.
    case RegType::kVec128: return 16; // VMX/VSX vector register.
    default: return 0;
  }
}

//! Returns the memory access size in bytes of `inst_id`, or `fallback` if the
//! instruction doesn't have a fixed narrow access size (GPR/FP loads and stores
//! use the full register size, vector loads and stores use 16 bytes).
static uint32_t mem_access_size(InstId inst_id, uint32_t fallback) noexcept {
  switch (inst_id) {
    // Byte accesses.
    case Inst::kIdLbz:
    case Inst::kIdLbzu:
    case Inst::kIdLbzx:
    case Inst::kIdLbzux:
    case Inst::kIdLbarx:
    case Inst::kIdLvebx:
    case Inst::kIdStb:
    case Inst::kIdStbu:
    case Inst::kIdStbx:
    case Inst::kIdStbux:
    case Inst::kIdStbcx_:
    case Inst::kIdStvebx:
      return 1;

    // Halfword accesses.
    case Inst::kIdLhz:
    case Inst::kIdLhzu:
    case Inst::kIdLhzx:
    case Inst::kIdLhzux:
    case Inst::kIdLha:
    case Inst::kIdLhau:
    case Inst::kIdLhax:
    case Inst::kIdLhaux:
    case Inst::kIdLhbrx:
    case Inst::kIdLharx:
    case Inst::kIdLvehx:
    case Inst::kIdSth:
    case Inst::kIdSthu:
    case Inst::kIdSthx:
    case Inst::kIdSthux:
    case Inst::kIdSthbrx:
    case Inst::kIdSthcx_:
    case Inst::kIdStvehx:
      return 2;

    // Word and single-precision FP accesses.
    case Inst::kIdLwz:
    case Inst::kIdLwzu:
    case Inst::kIdLwzx:
    case Inst::kIdLwzux:
    case Inst::kIdLwa:
    case Inst::kIdLwax:
    case Inst::kIdLwbrx:
    case Inst::kIdLwarx:
    case Inst::kIdLvewx:
    case Inst::kIdLfiwax:
    case Inst::kIdLfiwzx:
    case Inst::kIdLfs:
    case Inst::kIdLfsu:
    case Inst::kIdLfsx:
    case Inst::kIdLfsux:
    case Inst::kIdLxsiwzx:
    case Inst::kIdLxsiwax:
    case Inst::kIdLxssp:
    case Inst::kIdLxsspx:
    case Inst::kIdStw:
    case Inst::kIdStwu:
    case Inst::kIdStwx:
    case Inst::kIdStwux:
    case Inst::kIdStwbrx:
    case Inst::kIdStwcx_:
    case Inst::kIdStvewx:
    case Inst::kIdStfiwx:
    case Inst::kIdStfs:
    case Inst::kIdStfsu:
    case Inst::kIdStfsx:
    case Inst::kIdStfsux:
    case Inst::kIdStxsiwx:
    case Inst::kIdStxssp:
    case Inst::kIdStxsspx:
      return 4;

    // Doubleword and double-precision FP accesses.
    case Inst::kIdLd:
    case Inst::kIdLdu:
    case Inst::kIdLdx:
    case Inst::kIdLdux:
    case Inst::kIdLdarx:
    case Inst::kIdLfd:
    case Inst::kIdLfdu:
    case Inst::kIdLfdx:
    case Inst::kIdLfdux:
    case Inst::kIdLxsd:
    case Inst::kIdLxsdx:
    case Inst::kIdStd:
    case Inst::kIdStdu:
    case Inst::kIdStdx:
    case Inst::kIdStdux:
    case Inst::kIdStdcx_:
    case Inst::kIdStfd:
    case Inst::kIdStfdu:
    case Inst::kIdStfdx:
    case Inst::kIdStfdux:
    case Inst::kIdStxsd:
    case Inst::kIdStxsdx:
      return 8;

    default:
      return fallback;
  }
}

Error query_rw_info(const BaseInst& inst, const Operand_* operands, size_t op_count, InstRWInfo* out) noexcept {
  InstId inst_id = inst.inst_id();
  if (ASMJIT_UNLIKELY(inst_id >= Inst::_kIdCount))
    return make_error(Error::kInvalidInstruction);

  out->_inst_flags = InstRWFlags::kNone;
  out->_op_count = uint8_t(op_count);
  out->_rm_feature = 0;
  out->_extra_reg.reset();
  out->_read_flags = CpuRWFlags::kNone;
  out->_write_flags = CpuRWFlags::kNone;

  const Inst::RWClass rw_class = Inst::inst_rw_classes[inst_id];

  bool first_reg_written = false;
  uint32_t first_reg_size = 0;

  for (uint32_t i = 0; i < op_count; i++) {
    OpRWInfo& op = out->_operands[i];
    const Operand_& src_op = operands[i];

    if (src_op.is_reg()) {
      const Reg& reg = src_op.as<Reg>();
      uint32_t reg_size = reg_size_of(reg.reg_type());
      if (ASMJIT_UNLIKELY(reg_size == 0))
        return make_error(Error::kInvalidRegType);

      if (first_reg_size == 0)
        first_reg_size = reg_size;

      // Stores, compares, branches, and moves-to-special-registers only read
      // their register operands. Everything else writes the first register
      // operand and reads the remaining ones.
      bool is_write = false;
      switch (rw_class) {
        case Inst::RWClass::kDefault:
        case Inst::RWClass::kLoadUpdate:
          is_write = !first_reg_written;
          break;

        case Inst::RWClass::kStore:
        case Inst::RWClass::kStoreUpdate:
        case Inst::RWClass::kNoWrite:
          break;
      }

      first_reg_written = true;
      op.reset(is_write ? OpRWFlags::kWrite : OpRWFlags::kRead, reg_size);
    }
    else if (src_op.is_mem()) {
      const Mem& mem = src_op.as<Mem>();
      const bool is_store = rw_class == Inst::RWClass::kStore ||
                            rw_class == Inst::RWClass::kStoreUpdate;
      const bool is_update = rw_class == Inst::RWClass::kLoadUpdate ||
                             rw_class == Inst::RWClass::kStoreUpdate;

      uint32_t mem_size = mem_access_size(inst_id, first_reg_size != 0 ? first_reg_size : 8u);
      op.reset(is_store ? OpRWFlags::kWrite : OpRWFlags::kRead, mem_size);

      // The memory base is always read. Update-form loads and stores write the
      // base register after the address has been calculated (post-modify).
      if (mem.has_base_reg()) {
        op.add_op_flags(OpRWFlags::kMemBaseRead);
        if (is_update) {
          op.add_op_flags(OpRWFlags::kMemBaseWrite | OpRWFlags::kMemBasePostModify);
        }
      }

      if (mem.has_index_reg()) {
        op.add_op_flags(OpRWFlags::kMemIndexRead);
      }
    }
    else {
      op.reset();
    }
  }

  return Error::kOk;
}
#endif // !ASMJIT_NO_INTROSPECTION

} // {InstInternal}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC
