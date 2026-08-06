// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC)

#include <asmjit/ppc/ppcfunc_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

namespace FuncInternal {

static inline bool should_treat_as_cdecl(CallConvId call_conv_id) noexcept {
  return call_conv_id == CallConvId::kCDecl ||
         call_conv_id == CallConvId::kStdCall ||
         call_conv_id == CallConvId::kFastCall ||
         call_conv_id == CallConvId::kVectorCall ||
         call_conv_id == CallConvId::kThisCall ||
         call_conv_id == CallConvId::kRegParm1 ||
         call_conv_id == CallConvId::kRegParm2 ||
         call_conv_id == CallConvId::kRegParm3;
}

ASMJIT_FAVOR_SIZE Error init_call_conv(CallConv& cc, CallConvId call_conv_id, const Environment& environment) noexcept {
  cc.set_arch(environment.arch());
  cc.set_strategy(CallConvStrategy::kDefault);

  cc.set_save_restore_reg_size(RegGroup::kGp, 8);
  cc.set_save_restore_alignment(RegGroup::kGp, 8);
  cc.set_save_restore_reg_size(RegGroup::kVec, 16);
  cc.set_save_restore_alignment(RegGroup::kVec, 16);
  cc.set_natural_stack_alignment(16);
  cc.set_red_zone_size(288);
  cc.set_flags(CallConvFlags::kVarArgCompatible);

  cc.set_passed_order(RegGroup::kGp, 3, 4, 5, 6, 7, 8, 9, 10);
  cc.set_preserved_regs(RegGroup::kGp,
    Support::bit_mask<RegMask>(14, 15, 16, 17, 18, 19, 20, 21, 22,
                               23, 24, 25, 26, 27, 28, 29, 30, 31));

  cc.set_id(should_treat_as_cdecl(call_conv_id) ? CallConvId::kCDecl : call_conv_id);
  return Error::kOk;
}

ASMJIT_FAVOR_SIZE Error init_func_detail(FuncDetail& func, const FuncSignature& signature) noexcept {
  Support::maybe_unused(signature);

  const CallConv& cc = func.call_conv();
  const bool is_little_endian = cc.arch() == Arch::kPPC64_LE;
  uint32_t stack_offset = is_little_endian ? 32 : 48;

  if (func.has_ret()) {
    for (uint32_t value_index = 0; value_index < Globals::kMaxValuePack; value_index++) {
      TypeId type_id = func._rets[value_index].type_id();
      if (type_id == TypeId::kVoid)
        break;

      if (!TypeUtils::is_int(type_id)) {
        return make_error(Error::kInvalidRegType);
      }

      func._rets[value_index].init_reg(RegType::kGp64, 3, type_id);
    }
  }

  uint32_t gp_pos = 0;
  for (uint32_t i = 0; i < func.arg_count(); i++) {
    FuncValue& arg = func._args[i][0];
    TypeId type_id = arg.type_id();

    if (!TypeUtils::is_int(type_id)) {
      return make_error(Error::kInvalidRegType);
    }

    uint32_t reg_id = Reg::kIdBad;
    if (gp_pos < CallConv::kMaxRegArgsPerGroup) {
      reg_id = cc._passed_order[RegGroup::kGp].id[gp_pos];
    }

    if (reg_id != Reg::kIdBad) {
      arg.assign_reg_data(RegType::kGp64, reg_id);
      func.add_used_regs(RegGroup::kGp, Support::bit_mask<RegMask>(reg_id));
      gp_pos++;
    }
    else {
      arg.assign_stack_offset(int32_t(stack_offset));
      stack_offset += 8;
    }
  }

  return Error::kOk;
}

} // {FuncInternal}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC
