// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC)

#include <asmjit/core/emitter.h>
#include <asmjit/core/string.h>
#include <asmjit/core/type.h>
#include <asmjit/support/support.h>
#include <asmjit/ppc/ppcemithelper_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

// ppc::EmitHelper - Emit Operations
// =================================

ASMJIT_FAVOR_SIZE Error EmitHelper::emit_reg_move(
  const Operand_& dst_,
  const Operand_& src_, TypeId type_id, const char* comment) {

  BaseEmitter* emitter = _emitter;

  ASMJIT_ASSERT(TypeUtils::is_valid(type_id) && !TypeUtils::is_abstract(type_id));
  emitter->set_inline_comment(comment);

  if (dst_.is_reg() && src_.is_mem()) {
    const Reg& dst = dst_.as<Reg>();
    const Mem& src = src_.as<Mem>();

    if (TypeUtils::is_int(type_id)) {
      switch (TypeUtils::size_of(type_id)) {
        case 1: return emitter->emit(Inst::kIdLbz, dst.as<Gp>(), src);
        case 2: return emitter->emit(Inst::kIdLhz, dst.as<Gp>(), src);
        case 4: return emitter->emit(Inst::kIdLwz, dst.as<Gp>(), src);
        default: return emitter->emit(Inst::kIdLd, dst.as<Gp>(), src);
      }
    }

    if (TypeUtils::is_float64(type_id) || TypeUtils::is_vec64(type_id))
      return emitter->emit(Inst::kIdLfd, dst.as<Fp>(), src);

    if (TypeUtils::is_vec128(type_id))
      return emitter->emit(Inst::kIdLxv, dst.as<Vsx>(), src);
  }

  if (dst_.is_mem() && src_.is_reg()) {
    const Mem& dst = dst_.as<Mem>();
    const Reg& src = src_.as<Reg>();

    if (TypeUtils::is_int(type_id)) {
      switch (TypeUtils::size_of(type_id)) {
        case 1: return emitter->emit(Inst::kIdStb, src.as<Gp>(), dst);
        case 2: return emitter->emit(Inst::kIdSth, src.as<Gp>(), dst);
        case 4: return emitter->emit(Inst::kIdStw, src.as<Gp>(), dst);
        default: return emitter->emit(Inst::kIdStd, src.as<Gp>(), dst);
      }
    }

    if (TypeUtils::is_float64(type_id) || TypeUtils::is_vec64(type_id))
      return emitter->emit(Inst::kIdStfd, src.as<Fp>(), dst);

    if (TypeUtils::is_vec128(type_id))
      return emitter->emit(Inst::kIdStxv, src.as<Vsx>(), dst);
  }

  if (dst_.is_reg() && src_.is_reg()) {
    const Reg& dst = dst_.as<Reg>();
    const Reg& src = src_.as<Reg>();

    if (TypeUtils::is_int(type_id))
      return emitter->emit(Inst::kIdOri, dst.as<Gp>(), src.as<Gp>(), imm(0));

    if (TypeUtils::is_float64(type_id) || TypeUtils::is_vec64(type_id))
      return emitter->emit(Inst::kIdFmr, dst.as<Fp>(), src.as<Fp>());

    if (TypeUtils::is_vec128(type_id))
      return emitter->emit(Inst::kIdXxlor, dst.as<Vsx>(), src.as<Vsx>(), src.as<Vsx>());
  }

  emitter->set_inline_comment(nullptr);
  return make_error(Error::kInvalidState);
}

Error EmitHelper::emit_reg_swap(
  const Reg& a,
  const Reg& b, const char* comment) {

  Support::maybe_unused(a, b, comment);
  return make_error(Error::kInvalidState);
}

ASMJIT_FAVOR_SIZE Error EmitHelper::emit_arg_move(
  const Reg& dst_, TypeId dst_type_id,
  const Operand_& src_, TypeId src_type_id, const char* comment) {

  // Deduce optional `dst_type_id`, which may be `TypeId::kVoid` in some cases.
  if (dst_type_id == TypeId::kVoid) {
    dst_type_id = RegUtils::type_id_of(dst_.reg_type());
  }

  ASMJIT_ASSERT(TypeUtils::is_valid(dst_type_id) && !TypeUtils::is_abstract(dst_type_id));
  ASMJIT_ASSERT(TypeUtils::is_valid(src_type_id) && !TypeUtils::is_abstract(src_type_id));

  const Reg& dst = dst_.as<Reg>();

  if (TypeUtils::is_int(dst_type_id)) {
    if (TypeUtils::is_int(src_type_id)) {
      _emitter->set_inline_comment(comment);

      if (src_.is_reg()) {
        return _emitter->emit(Inst::kIdOri, dst.as<Gp>(), src_.as<Reg>().as<Gp>(), imm(0));
      }
      else if (src_.is_mem()) {
        // The caller sign/zero-extends integer arguments to 64 bits, so the
        // unsigned loads are sufficient.
        switch (TypeUtils::size_of(src_type_id)) {
          case 1: return _emitter->emit(Inst::kIdLbz, dst.as<Gp>(), src_.as<Mem>());
          case 2: return _emitter->emit(Inst::kIdLhz, dst.as<Gp>(), src_.as<Mem>());
          case 4: return _emitter->emit(Inst::kIdLwz, dst.as<Gp>(), src_.as<Mem>());
          default: return _emitter->emit(Inst::kIdLd, dst.as<Gp>(), src_.as<Mem>());
        }
      }
    }
  }

  if (TypeUtils::is_float(dst_type_id) || TypeUtils::is_vec(dst_type_id)) {
    if (TypeUtils::is_float(src_type_id) || TypeUtils::is_vec(src_type_id)) {
      _emitter->set_inline_comment(comment);

      if (src_.is_reg()) {
        const Reg& src = src_.as<Reg>();
        if (dst.reg_type() == RegType::kVec64) {
          return _emitter->emit(Inst::kIdFmr, dst.as<Fp>(), src.as<Fp>());
        }
        else {
          return _emitter->emit(Inst::kIdXxlor, dst.as<Vsx>(), src.as<Vsx>(), src.as<Vsx>());
        }
      }
      else if (src_.is_mem()) {
        const Mem& src = src_.as<Mem>();
        if (dst.reg_type() == RegType::kVec64) {
          return _emitter->emit(Inst::kIdLfd, dst.as<Fp>(), src);
        }
        else {
          return _emitter->emit(Inst::kIdLxv, dst.as<Vsx>(), src);
        }
      }
    }
  }

  return make_error(Error::kInvalidState);
}

// ppc::EmitHelper - Emit Prolog & Epilog
// ======================================

//! Converts the vector-group saved register mask into the FPR and VR masks
//! used by `Assembler::prolog()/epilog()`. FPRs (ids 14..31) and VMX
//! registers (ids 20..31) share allocator ids, so saving any used id saves
//! both the FPR and the VMX register (conservative, but ABI-correct).
static inline void saved_reg_masks(const FuncFrame& frame, uint32_t* save_mask, uint32_t* fpr_mask, uint32_t* vr_mask) noexcept {
  const RegMask gp_saved = frame.saved_regs(RegGroup::kGp);
  const RegMask vec_saved = frame.saved_regs(RegGroup::kVec);

  *save_mask = (gp_saved >> 14) & 0x3FFFFu; // r14..r31 -> k = 0..17.
  *fpr_mask = (vec_saved >> 14) & 0x3FFFFu; // f14..f31 -> k = 0..17.
  *vr_mask = (vec_saved >> 20) & 0xFFFu;     // v20..v31 -> k = 0..11.
}

ASMJIT_FAVOR_SIZE Error EmitHelper::emit_prolog(const FuncFrame& frame) {
  BaseEmitter* emitter = _emitter;

  uint32_t save_mask = 0;
  uint32_t fpr_mask = 0;
  uint32_t vr_mask = 0;
  saved_reg_masks(frame, &save_mask, &fpr_mask, &vr_mask);

  // The final stack size includes the ABI header (forced into call stack size
  // by the RA pass), the call area, the local stack, and the save areas.
  int32_t frame_size = int32_t(frame.final_stack_size());

  // Save LR (in the caller's frame) and the nonvolatile CR fields (in the
  // caller's frame), then the selected nonvolatile GPRs/FPRs/VRs, exactly
  // like GCC: LR at 16(caller SP), CR at 8(caller SP), FPR f14+k at
  // -(144 - 8*k), GPR r14+k at -(fpr_bytes + 144 - 8*k), vector v20+k at
  // -(fpr_bytes + gpr_bytes + 192 - 16*k).
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdMflr, r0));
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdStd, r0, ptr(r1, 16)));

  // The nonvolatile CR fields (CR2..CR4) are saved unconditionally: the
  // register allocator cannot track which CR fields an instruction modifies.
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdMfcr, r11));
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdStw, r11, ptr(r1, 8)));

  const uint32_t regs = save_mask & 0x3FFFFu;
  const uint32_t fprs = fpr_mask & 0x3FFFFu;
  const uint32_t vrs = vr_mask & 0xFFFu;

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

  for (uint32_t k = 0; k < 18; k++) {
    if (fprs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdStfd, Fp { uint32_t(14 + k) }, ptr(r1, int32_t(-8 * (18 - k)))));
    }
  }
  for (uint32_t k = 0; k < 18; k++) {
    if (regs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdStd, Gp { uint32_t(14 + k) }, ptr(r1, int32_t(-fp_bytes - 8 * (18 - k)))));
    }
  }
  for (uint32_t k = 0; k < 12; k++) {
    if (vrs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLi, r0, imm(int16_t(-fp_bytes - gpr_bytes - 192 + 16 * k))));
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdStvx, Vr { uint32_t(20 + k) }, ptr(r1, r0)));
    }
  }

  if (frame_size <= 32764) {
    return emitter->emit(Inst::kIdStdu, r1, ptr(r1, -frame_size));
  }

  const uint32_t neg = 0u - uint32_t(frame_size);
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdAddis, r0, r0, imm(int16_t(neg >> 16))));
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdOri, r0, r0, imm(uint16_t(neg))));
  return emitter->emit(Inst::kIdStdux, r1, ptr(r1, r0));
}

ASMJIT_FAVOR_SIZE Error EmitHelper::emit_epilog(const FuncFrame& frame) {
  BaseEmitter* emitter = _emitter;

  uint32_t save_mask = 0;
  uint32_t fpr_mask = 0;
  uint32_t vr_mask = 0;
  saved_reg_masks(frame, &save_mask, &fpr_mask, &vr_mask);

  const uint32_t regs = save_mask & 0x3FFFFu;
  const uint32_t fprs = fpr_mask & 0x3FFFFu;
  const uint32_t vrs = vr_mask & 0xFFFu;

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

  const int32_t frame_size = int32_t(frame.final_stack_size());
  if (frame_size <= 32764) {
    ASMJIT_PROPAGATE(emitter->emit(Inst::kIdAddi, r1, r1, imm(int16_t(frame_size))));
  }
  else {
    // The back chain written by the prolog points at the caller's SP, so the
    // whole frame is released with a single load.
    ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLd, r1, ptr(r1, 0)));
  }

  for (uint32_t k = 18; k-- > 0;) {
    if (regs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLd, Gp { uint32_t(14 + k) }, ptr(r1, int32_t(-fp_bytes - 8 * (18 - k)))));
    }
  }
  for (uint32_t k = 18; k-- > 0;) {
    if (fprs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLfd, Fp { uint32_t(14 + k) }, ptr(r1, int32_t(-8 * (18 - k)))));
    }
  }
  for (uint32_t k = 12; k-- > 0;) {
    if (vrs & (1u << k)) {
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLi, r0, imm(int16_t(-fp_bytes - gpr_bytes - 192 + 16 * k))));
      ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLvx, Vr { uint32_t(20 + k) }, ptr(r1, r0)));
    }
  }
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLwz, r11, ptr(r1, 8)));
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdMtcrf, imm(0x38), r11)); // Restore CR2..CR4.

  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdLd, r0, ptr(r1, 16)));
  ASMJIT_PROPAGATE(emitter->emit(Inst::kIdMtlr, r0));
  return emitter->emit(Inst::kIdBlr);
}

// ppc::EmitHelper - Emitter Funcs
// ===============================

static Error ASMJIT_CDECL Emitter_emitProlog(BaseEmitter* emitter, const FuncFrame& frame) {
  EmitHelper emit_helper(emitter);
  return emit_helper.emit_prolog(frame);
}

static Error ASMJIT_CDECL Emitter_emitEpilog(BaseEmitter* emitter, const FuncFrame& frame) {
  EmitHelper emit_helper(emitter);
  return emit_helper.emit_epilog(frame);
}

static Error ASMJIT_CDECL Emitter_emitArgsAssignment(BaseEmitter* emitter, const FuncFrame& frame, const FuncArgsAssignment& args) {
  EmitHelper emit_helper(emitter);
  return emit_helper.emit_args_assignment(frame, args);
}

void init_emitter_funcs(BaseEmitter* emitter) {
  emitter->_funcs.emit_prolog = Emitter_emitProlog;
  emitter->_funcs.emit_epilog = Emitter_emitEpilog;
  emitter->_funcs.emit_args_assignment = Emitter_emitArgsAssignment;
}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC
