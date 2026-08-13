// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC) && !defined(ASMJIT_NO_COMPILER)

#include <asmjit/core/cpuinfo.h>
#include <asmjit/core/formatter_p.h>
#include <asmjit/core/type.h>
#include <asmjit/support/support.h>
#include <asmjit/ppc/ppcassembler.h>
#include <asmjit/ppc/ppccompiler.h>
#include <asmjit/ppc/ppcemithelper_p.h>
#include <asmjit/ppc/ppcinstapi_p.h>
#include <asmjit/ppc/ppcrapass_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

// ppc::RACFGBuilder
// =================

class RACFGBuilder : public RACFGBuilderT<RACFGBuilder> {
public:
  Arch _arch;

  inline RACFGBuilder(PPCRAPass& pass) noexcept
    : RACFGBuilderT<RACFGBuilder>(pass),
      _arch(pass.cc().arch()) {}

  [[nodiscard]]
  inline Compiler& cc() const noexcept { return static_cast<Compiler&>(_cc); }

  [[nodiscard]]
  Error on_instruction(InstNode* inst, InstControlFlow& control_type, RAInstBuilder& ib) noexcept;

  [[nodiscard]]
  Error on_before_invoke(InvokeNode* invoke_node) noexcept;

  [[nodiscard]]
  Error on_invoke(InvokeNode* invoke_node, RAInstBuilder& ib) noexcept;

  [[nodiscard]]
  Error move_imm_to_reg_arg(InvokeNode* invoke_node, const FuncValue& arg, const Imm& imm_, Out<Reg> out) noexcept;

  [[nodiscard]]
  Error move_imm_to_stack_arg(InvokeNode* invoke_node, const FuncValue& arg, const Imm& imm_) noexcept;

  [[nodiscard]]
  Error move_reg_to_stack_arg(InvokeNode* invoke_node, const FuncValue& arg, const Reg& reg) noexcept;

  [[nodiscard]]
  Error on_before_ret(FuncRetNode* func_ret) noexcept;

  [[nodiscard]]
  Error on_ret(FuncRetNode* func_ret, RAInstBuilder& ib) noexcept;
};

// ppc::RACFGBuilder - Helpers
// ===========================

[[nodiscard]]
static inline RATiedFlags ra_use_out_flags_from_rw_flags(OpRWFlags rw_flags) noexcept {
  static constexpr RATiedFlags map[] = {
    RATiedFlags::kNone,
    RATiedFlags::kRead  | RATiedFlags::kUse, // kRead
    RATiedFlags::kWrite | RATiedFlags::kOut, // kWrite
    RATiedFlags::kRW    | RATiedFlags::kUse, // kRW
  };
  return map[uint32_t(rw_flags & OpRWFlags::kRW)];
}

[[nodiscard]]
static inline RATiedFlags ra_reg_rw_flags(OpRWFlags flags) noexcept {
  return ra_use_out_flags_from_rw_flags(flags);
}

[[nodiscard]]
static inline RATiedFlags ra_mem_base_rw_flags(OpRWFlags flags) noexcept {
  constexpr uint32_t shift = Support::ctz_const<OpRWFlags::kMemBaseRW>;
  return ra_use_out_flags_from_rw_flags(OpRWFlags(uint32_t(flags) >> shift) & OpRWFlags::kRW);
}

[[nodiscard]]
static inline RATiedFlags ra_mem_index_rw_flags(OpRWFlags flags) noexcept {
  constexpr uint32_t shift = Support::ctz_const<OpRWFlags::kMemIndexRW>;
  return ra_use_out_flags_from_rw_flags(OpRWFlags(uint32_t(flags) >> shift) & OpRWFlags::kRW);
}

[[nodiscard]]
static InstControlFlow get_control_flow_type(InstId inst_id) noexcept {
  switch (BaseInst::extract_real_id(inst_id)) {
    case Inst::kIdB:
      return InstControlFlow::kJump;

    case Inst::kIdBeq:
    case Inst::kIdBne:
    case Inst::kIdBlt:
    case Inst::kIdBge:
    case Inst::kIdBgt:
    case Inst::kIdBle:
    case Inst::kIdBc:
    case Inst::kIdBcl:
    case Inst::kIdBclr:
    case Inst::kIdBclrl:
    case Inst::kIdBcctr:
    case Inst::kIdBcctrl:
    case Inst::kIdBctr:
      return InstControlFlow::kBranch;

    case Inst::kIdBctrl:
      return InstControlFlow::kCall;

    case Inst::kIdBlr:
      return InstControlFlow::kReturn;

    default:
      return InstControlFlow::kRegular;
  }
}

// ppc::RACFGBuilder - OnInst
// ==========================

Error RACFGBuilder::on_instruction(InstNode* inst, InstControlFlow& control_type, RAInstBuilder& ib) noexcept {
  InstRWInfo rw_info;

  if (inst->real_id() != Inst::kIdNone && inst->real_id() < Inst::_kIdCount) {
    InstId inst_id = inst->inst_id();
    Span<const Operand> operands = inst->operands();

    ASMJIT_PROPAGATE(InstAPI::query_rw_info(cc().arch(), inst->baseInst(), operands.data(), operands.size(), &rw_info));
    ib.add_inst_rw_flags(rw_info.inst_flags());

    for (size_t i = 0; i < operands.size(); i++) {
      const Operand& op = operands[i];
      const OpRWInfo& op_rw_info = rw_info.operand(i);

      if (op.is_reg()) {
        const Reg& reg = op.as<Reg>();

        RATiedFlags flags = ra_reg_rw_flags(op_rw_info.op_flags());
        uint32_t virt_index = Operand::virt_id_to_index(reg.id());

        if (virt_index < Operand::kVirtIdCount) {
          RAWorkReg* work_reg;
          ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, virt_index));

          RegGroup group = work_reg->group();
          RegMask use_regs = _pass._available_regs[group];
          RegMask out_regs = use_regs;

          // FPRs (kVec64) and VMX/VSX registers (kVec128) share the 32-register
          // vector group. VMX registers v14..v19 are volatile even though the
          // FPRs f14..f19 are nonvolatile, so VMX/VSX registers must never be
          // allocated to ids 14..19 (the allocator would treat them as
          // callee-saved and keep them live across calls).
          if (group == RegGroup::kVec && work_reg->type() == RegType::kVec128) {
            use_regs &= 0x00000FFFu | 0xFFF00000u; // ids 0..13 and 20..31.
            out_regs = use_regs;
          }

          if (Support::test(flags, RATiedFlags::kUse)) {
            ASMJIT_PROPAGATE(ib.add(work_reg, flags, use_regs, Reg::kIdBad,
                                    Support::bit_mask<uint32_t>(inst->_get_rewrite_index(&reg._base_id)),
                                    out_regs, Reg::kIdBad, 0, op_rw_info.rm_size()));
          }
          else {
            ASMJIT_PROPAGATE(ib.add(work_reg, flags, use_regs, Reg::kIdBad, 0,
                                    out_regs, Reg::kIdBad,
                                    Support::bit_mask<uint32_t>(inst->_get_rewrite_index(&reg._base_id)),
                                    op_rw_info.rm_size()));
          }
        }
      }
      else if (op.is_mem()) {
        const Mem& mem = op.as<Mem>();

        if (mem.is_reg_home()) {
          RAWorkReg* work_reg;
          ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, Operand::virt_id_to_index(mem.base_id())));
          if (ASMJIT_UNLIKELY(!_pass.get_or_create_stack_slot(work_reg))) {
            return make_error(Error::kOutOfMemory);
          }
        }
        else if (mem.has_base_reg()) {
          uint32_t virt_index = Operand::virt_id_to_index(mem.base_id());
          if (virt_index < Operand::kVirtIdCount) {
            RAWorkReg* work_reg;
            ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, virt_index));

            RATiedFlags flags = ra_mem_base_rw_flags(op_rw_info.op_flags());
            RegGroup group = work_reg->group();
            RegMask allocable = _pass._available_regs[group];

            if (Support::test(flags, RATiedFlags::kUse)) {
              ASMJIT_PROPAGATE(ib.add(work_reg, flags, allocable, Reg::kIdBad,
                                      Support::bit_mask<uint32_t>(inst->_get_rewrite_index(&mem._base_id)),
                                      allocable, Reg::kIdBad, 0));
            }
            else {
              ASMJIT_PROPAGATE(ib.add(work_reg, flags, allocable, Reg::kIdBad, 0,
                                      allocable, Reg::kIdBad,
                                      Support::bit_mask<uint32_t>(inst->_get_rewrite_index(&mem._base_id))));
            }
          }
        }

        if (mem.has_index_reg()) {
          uint32_t virt_index = Operand::virt_id_to_index(mem.index_id());
          if (virt_index < Operand::kVirtIdCount) {
            RAWorkReg* work_reg;
            ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, virt_index));

            RATiedFlags flags = ra_mem_index_rw_flags(op_rw_info.op_flags());
            RegGroup group = work_reg->group();
            RegMask allocable = _pass._available_regs[group];

            ASMJIT_PROPAGATE(ib.add(work_reg, flags, allocable, Reg::kIdBad,
                                    Support::bit_mask<uint32_t>(inst->_get_rewrite_index(&mem._data[Operand::kDataMemIndexId])),
                                    allocable, Reg::kIdBad, 0));
          }
        }
      }
    }

    control_type = get_control_flow_type(inst_id);
  }

  return Error::kOk;
}

// ppc::RACFGBuilder - OnInvoke
// ============================

Error RACFGBuilder::on_before_invoke(InvokeNode* invoke_node) noexcept {
  const FuncDetail& fd = invoke_node->detail();
  uint32_t arg_count = invoke_node->arg_count();

  cc().set_cursor(invoke_node->prev());

  // `bctrl` calls the address stored in CTR. ELFv2 also requires r12 to hold
  // the callee's entry address, so that a global entry point can initialize
  // its TOC pointer (r2). Both must be set before the invoke; the nodes
  // inserted here are processed (and register-allocated) by the CFG builder
  // restart. r12 is a volatile, call-clobbered linkage register, so writing
  // it here cannot corrupt live values.
  const Operand& target = invoke_node->operands()[0];
  if (!cc().environment().is_little_endian()) {
    // ELFv1 (big-endian): the call target is a function descriptor
    // {entry, toc, env}, so load the entry into CTR and the TOC into r2
    // before branching. r2 is never allocated by the register allocator.
    if (target.is_reg()) {
      ASMJIT_PROPAGATE(cc().mov(r11, target.as<Gp>()));
    }
    else if (target.is_imm()) {
      ASMJIT_PROPAGATE(cc().mov(r11, target.as<Imm>()));
    }
    else {
      return make_error(Error::kInvalidState);
    }
    ASMJIT_PROPAGATE(cc().ld(r12, ptr(r11, 0)));
    ASMJIT_PROPAGATE(cc().ld(r2, ptr(r11, 8)));
    ASMJIT_PROPAGATE(cc().mtctr(r12));
  }
  else {
    if (target.is_reg()) {
      ASMJIT_PROPAGATE(cc().mov(r12, target.as<Gp>()));
    }
    else if (target.is_imm()) {
      ASMJIT_PROPAGATE(cc().mov(r12, target.as<Imm>()));
    }
    else {
      return make_error(Error::kInvalidState);
    }
    ASMJIT_PROPAGATE(cc().mtctr(r12));
  }

  // `bctrl` takes no operands; drop the target so serialization emits a
  // plain call.
  invoke_node->set_op(0, Operand());
  invoke_node->set_op_count(0);

  // ELFv2 variadic calls require the bit patterns of FP arguments to be
  // mirrored into the corresponding GPR slot (r3..r10), so that `va_arg`
  // can find every argument by walking the GPR register save area.
  if (fd.has_var_args()) {
    uint32_t gp_slot = 0;
    for (uint32_t arg_index = 0; arg_index < arg_count; arg_index++) {
      const FuncValuePack& arg_pack = fd.arg_pack(arg_index);
      for (uint32_t value_index = 0; value_index < Globals::kMaxValuePack; value_index++) {
        const FuncValue& arg = arg_pack[value_index];
        if (!arg)
          break;

        if (arg.is_reg() && arg.reg_type() == RegType::kVec64 && gp_slot < 8) {
          const Operand& op = invoke_node->arg(arg_index, value_index);
          if (op.is_reg()) {
            ASMJIT_PROPAGATE(cc().mfvsrd(Gp { uint32_t(3 + gp_slot) }, op.as<Reg>().as<Vsx>()));
          }
        }

        gp_slot += arg.reg_type() == RegType::kVec128 ? 2u : 1u;
      }
    }
  }

  for (uint32_t arg_index = 0; arg_index < arg_count; arg_index++) {
    const FuncValuePack& arg_pack = fd.arg_pack(arg_index);
    for (uint32_t value_index = 0; value_index < Globals::kMaxValuePack; value_index++) {
      if (!arg_pack[value_index])
        break;

      const FuncValue& arg = arg_pack[value_index];
      const Operand& op = invoke_node->arg(arg_index, value_index);

      if (op.is_none())
        continue;

      if (op.is_reg()) {
        const Reg& reg = op.as<Reg>();
        RAWorkReg* work_reg;
        ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, Operand::virt_id_to_index(reg.id())));

        if (arg.is_reg()) {
          RegGroup reg_group = work_reg->group();
          RegGroup arg_group = RegUtils::group_of(arg.reg_type());

          if (reg_group != arg_group) {
            return make_error(Error::kInvalidAssignment);
          }
        }
        else {
          ASMJIT_PROPAGATE(move_reg_to_stack_arg(invoke_node, arg, reg));
        }
      }
      else if (op.is_imm()) {
        if (arg.is_reg()) {
          Reg reg;
          ASMJIT_PROPAGATE(move_imm_to_reg_arg(invoke_node, arg, op.as<Imm>(), Out(reg)));
          invoke_node->_args[arg_index][value_index] = reg;
        }
        else {
          ASMJIT_PROPAGATE(move_imm_to_stack_arg(invoke_node, arg, op.as<Imm>()));
        }
      }
    }
  }

  cc().set_cursor(invoke_node);

  if (fd.has_ret()) {
    for (uint32_t value_index = 0; value_index < Globals::kMaxValuePack; value_index++) {
      const FuncValue& ret = fd.ret(value_index);
      if (!ret) {
        break;
      }

      const Operand& op = invoke_node->ret(value_index);
      if (op.is_reg()) {
        const Reg& reg = op.as<Reg>();
        RAWorkReg* work_reg;
        ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, Operand::virt_id_to_index(reg.id())));

        if (ret.is_reg()) {
          RegGroup reg_group = work_reg->group();
          RegGroup ret_group = RegUtils::group_of(ret.reg_type());

          if (reg_group != ret_group) {
            return make_error(Error::kInvalidAssignment);
          }
        }
      }
    }
  }

  _cur_block->add_flags(RABlockFlags::kHasFuncCalls);
  _pass.func()->frame().add_attributes(FuncAttributes::kHasFuncCalls);
  _pass.func()->frame().update_call_stack_size(fd.arg_stack_size());

  // ELFv2 variadic callees spill the GPR argument registers into the caller's
  // parameter save area, which is 64 bytes long starting at SP+32. Make sure
  // the frame covers it.
  if (fd.has_var_args()) {
    _pass.func()->frame().update_call_stack_size(96);
  }

  return Error::kOk;
}

Error RACFGBuilder::on_invoke(InvokeNode* invoke_node, RAInstBuilder& ib) noexcept {
  uint32_t arg_count = invoke_node->arg_count();
  const FuncDetail& fd = invoke_node->detail();

  for (uint32_t arg_index = 0; arg_index < arg_count; arg_index++) {
    const FuncValuePack& arg_pack = fd.arg_pack(arg_index);
    for (uint32_t value_index = 0; value_index < Globals::kMaxValuePack; value_index++) {
      if (!arg_pack[value_index]) {
        continue;
      }

      const FuncValue& arg = arg_pack[value_index];
      const Operand& op = invoke_node->arg(arg_index, value_index);

      if (op.is_none()) {
        continue;
      }

      if (op.is_reg()) {
        const Reg& reg = op.as<Reg>();
        RAWorkReg* work_reg;
        ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, Operand::virt_id_to_index(reg.id())));

        if (arg.is_indirect()) {
          RegGroup reg_group = work_reg->group();
          if (reg_group != RegGroup::kGp) {
            return make_error(Error::kInvalidState);
          }
          ASMJIT_PROPAGATE(ib.add_call_arg(work_reg, arg.reg_id()));
        }
        else if (arg.is_reg()) {
          RegGroup reg_group = work_reg->group();
          RegGroup arg_group = RegUtils::group_of(arg.reg_type());

          if (reg_group == arg_group) {
            ASMJIT_PROPAGATE(ib.add_call_arg(work_reg, arg.reg_id()));
          }
        }
      }
    }
  }

  for (uint32_t ret_index = 0; ret_index < Globals::kMaxValuePack; ret_index++) {
    const FuncValue& ret = fd.ret(ret_index);
    if (!ret) {
      break;
    }

    const Operand& op = invoke_node->ret(ret_index);
    if (op.is_reg()) {
      const Reg& reg = op.as<Reg>();
      RAWorkReg* work_reg;
      ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, Operand::virt_id_to_index(reg.id())));

      if (ret.is_reg()) {
        RegGroup reg_group = work_reg->group();
        RegGroup ret_group = RegUtils::group_of(ret.reg_type());

        if (reg_group == ret_group) {
          ASMJIT_PROPAGATE(ib.add_call_ret(work_reg, ret.reg_id()));
        }
      }
      else {
        return make_error(Error::kInvalidAssignment);
      }
    }
  }

  // Setup clobbered registers: everything that is not preserved by the
  // called function's calling convention.
  for (RegGroup group : Support::enumerate(RegGroup::kMaxVirt)) {
    ib._clobbered[size_t(group)] = Support::lsb_mask<RegMask>(_pass._phys_reg_count.get(group)) & ~fd.preserved_regs(group);
  }

  return Error::kOk;
}

// ppc::RACFGBuilder - MoveImmToRegArg
// ===================================

Error RACFGBuilder::move_imm_to_reg_arg(InvokeNode* invoke_node, const FuncValue& arg, const Imm& imm_, Out<Reg> out) noexcept {
  Support::maybe_unused(invoke_node);
  ASMJIT_ASSERT(arg.is_reg());

  Imm imm(imm_);
  TypeId type_id = TypeId::kVoid;

  switch (arg.type_id()) {
    case TypeId::kInt8  : type_id = TypeId::kUInt64; imm.sign_extend_int8(); break;
    case TypeId::kUInt8 : type_id = TypeId::kUInt64; imm.zero_extend_uint8(); break;
    case TypeId::kInt16 : type_id = TypeId::kUInt64; imm.sign_extend_int16(); break;
    case TypeId::kUInt16: type_id = TypeId::kUInt64; imm.zero_extend_uint16(); break;
    case TypeId::kInt32 : type_id = TypeId::kUInt64; imm.sign_extend_int32(); break;
    case TypeId::kUInt32: type_id = TypeId::kUInt64; imm.zero_extend_uint32(); break;
    case TypeId::kInt64 : type_id = TypeId::kUInt64; break;
    case TypeId::kUInt64: type_id = TypeId::kUInt64; break;
    case TypeId::kIntPtr : type_id = TypeId::kUInt64; break;
    case TypeId::kUIntPtr: type_id = TypeId::kUInt64; break;

    default:
      return make_error(Error::kInvalidAssignment);
  }

  ASMJIT_PROPAGATE(cc()._new_reg(out, type_id, nullptr));
  cc().virt_reg_by_id(out->id())->set_weight(BaseRAPass::kCallArgWeight);
  return cc().mov(out->as<Gp>(), imm);
}

// ppc::RACFGBuilder - MoveImmToStackArg
// =====================================

Error RACFGBuilder::move_imm_to_stack_arg(InvokeNode* invoke_node, const FuncValue& arg, const Imm& imm_) noexcept {
  Reg reg;

  ASMJIT_PROPAGATE(move_imm_to_reg_arg(invoke_node, arg, imm_, Out(reg)));
  ASMJIT_PROPAGATE(move_reg_to_stack_arg(invoke_node, arg, reg));

  return Error::kOk;
}

// ppc::RACFGBuilder - MoveRegToStackArg
// =====================================

Error RACFGBuilder::move_reg_to_stack_arg(InvokeNode* invoke_node, const FuncValue& arg, const Reg& reg) noexcept {
  Support::maybe_unused(invoke_node);
  Mem stack_ptr = ptr(_pass._sp.as<Gp>(), arg.stack_offset());

  if (reg.is_gp()) {
    return cc().std(reg.as<Gp>(), stack_ptr);
  }

  if (reg.reg_type() == RegType::kVec64) {
    return cc().stfd(reg.as<Fp>(), stack_ptr);
  }

  if (reg.reg_type() == RegType::kVec128) {
    return cc().stxvx(reg.as<Vsx>(), stack_ptr);
  }

  return make_error(Error::kInvalidState);
}

// ppc::RACFGBuilder - OnRet
// =========================

Error RACFGBuilder::on_before_ret(FuncRetNode* func_ret) noexcept {
  Support::maybe_unused(func_ret);
  return Error::kOk;
}

Error RACFGBuilder::on_ret(FuncRetNode* func_ret, RAInstBuilder& ib) noexcept {
  const FuncDetail& func_detail = _pass.func()->detail();
  Span<const Operand> operands = func_ret->operands();

  for (size_t i = 0; i < operands.size(); i++) {
    const Operand& op = operands[i];
    if (op.is_none()) {
      continue;
    }

    const FuncValue& ret = func_detail.ret(i);
    if (ASMJIT_UNLIKELY(!ret.is_reg())) {
      return make_error(Error::kInvalidAssignment);
    }

    if (op.is_reg()) {
      const Reg& reg = op.as<Reg>();
      uint32_t virt_index = Operand::virt_id_to_index(reg.id());

      if (virt_index < Operand::kVirtIdCount) {
        RAWorkReg* work_reg;
        ASMJIT_PROPAGATE(_pass.virt_index_as_work_reg(&work_reg, virt_index));

        RegGroup group = work_reg->group();
        RegMask allocable = _pass._available_regs[group];
        ASMJIT_PROPAGATE(ib.add(work_reg, RATiedFlags::kUse | RATiedFlags::kRead, allocable, ret.reg_id(), 0, 0, Reg::kIdBad, 0));
      }
    }
    else {
      return make_error(Error::kInvalidAssignment);
    }
  }

  return Error::kOk;
}

// ppc::PPCRAPass - Construction & Destruction
// ===========================================

PPCRAPass::PPCRAPass(BaseCompiler& cc) noexcept
  : BaseRAPass(cc) { _emit_helper_ptr = &_emit_helper; }

PPCRAPass::~PPCRAPass() noexcept {}

// ppc::PPCRAPass - OnInit / OnDone
// ================================

void PPCRAPass::on_init() noexcept {
  Arch arch = cc().arch();

  _emit_helper.reset(&_cb);
  _arch_traits = &ArchTraits::by_arch(arch);
  _phys_reg_count.set(RegGroup::kGp, 32);
  _phys_reg_count.set(RegGroup::kVec, 32);
  _phys_reg_count.set(RegGroup::kMask, 0);
  _phys_reg_count.set(RegGroup::kExtra, 0);
  _build_phys_index();

  _available_regs[RegGroup::kGp] = Support::lsb_mask<uint32_t>(32);
  _available_regs[RegGroup::kVec] = Support::lsb_mask<uint32_t>(32);
  _available_regs[RegGroup::kMask] = 0;
  _available_regs[RegGroup::kExtra] = 0;

  _scratch_reg_indexes[0] = uint8_t(11); // r11
  _scratch_reg_indexes[1] = uint8_t(12); // r12

  const FuncFrame& frame = _func->frame();

  // r0 is a literal zero in address and immediate forms, r1 is the stack
  // pointer, r2 is the TOC pointer, and r13 is reserved for the thread
  // pointer. None of them can be used as general-purpose registers.
  make_unavailable(RegGroup::kGp, 0);
  make_unavailable(RegGroup::kGp, 1);
  make_unavailable(RegGroup::kGp, 2);
  make_unavailable(RegGroup::kGp, 13);
  make_unavailable(frame._unavailable_regs);

  _sp = ppc::r1;
  _fp = ppc::r31;
}

void PPCRAPass::on_done() noexcept {}

// ppc::PPCRAPass - BuildCFG
// =========================

Error PPCRAPass::build_cfg_nodes() noexcept {
  return RACFGBuilder(*this).run();
}

// ppc::PPCRAPass - Rewrite
// ========================

ASMJIT_FAVOR_SPEED Error PPCRAPass::rewrite() noexcept {
  const size_t virt_count = cc()._virt_regs.size();
  return rewrite_iterate([&](BaseNode* node, BaseNode* stop, RABlock* block) noexcept -> Error {
    while (node != stop) {
      BaseNode* next = node->next();

      if (node->is_inst()) {
        InstNode* inst = node->as<InstNode>();
        RAInst* ra_inst = node->pass_data<RAInst>();

        Span<Operand> operands = inst->operands();

        if (ra_inst) {
          node->reset_pass_data();

          const RATiedReg* tied_regs = ra_inst->tied_regs();
          uint32_t tied_count = ra_inst->tied_count();

          for (uint32_t i = 0; i < tied_count; i++) {
            const RATiedReg& tied_reg = tied_regs[i];

            Support::BitWordIterator<uint32_t> use_it(tied_reg.use_rewrite_mask());
            if (use_it.has_next()) {
              uint32_t use_id = tied_reg.use_id();
              do {
                inst->_rewrite_id_at_index(use_it.next(), use_id);
              } while (use_it.has_next());
            }

            Support::BitWordIterator<uint32_t> out_it(tied_reg.out_rewrite_mask());
            if (out_it.has_next()) {
              uint32_t out_id = tied_reg.out_id();
              do {
                inst->_rewrite_id_at_index(out_it.next(), out_id);
              } while (out_it.has_next());
            }
          }

          if (ASMJIT_UNLIKELY(node->type() != NodeType::kInst)) {
            // FuncRet terminates the flow. Either remove it if the exit label
            // is next to it, or patch it to a jump to the function's exit.
            if (node->type() == NodeType::kFuncRet) {
              if (!is_next_to(node, _func->exit_node())) {
                cc().set_cursor(node->prev());
                ASMJIT_PROPAGATE(emit_jump(_func->exit_node()->label()));
              }

              BaseNode* prev = node->prev();
              cc().remove_node(node);

              if (block) {
                block->set_last(prev);
              }
            }
          }
        }

        // Rewrite stack slot addresses.
        for (Operand& op : operands) {
          if (op.is_mem()) {
            BaseMem& mem = op.as<BaseMem>();
            if (mem.is_reg_home()) {
              uint32_t virt_index = Operand::virt_id_to_index(mem.base_id());
              if (ASMJIT_UNLIKELY(virt_index >= virt_count)) {
                return make_error(Error::kInvalidVirtId);
              }

              VirtReg* virt_reg = cc().virt_reg_by_index(virt_index);
              RAWorkReg* work_reg = virt_reg->work_reg();
              ASMJIT_ASSERT(work_reg != nullptr);

              RAStackSlot* slot = work_reg->stack_slot();
              int32_t offset = slot->offset();

              mem._set_base(_sp.reg_type(), slot->base_reg_id());
              mem.clear_reg_home();
              mem.add_offset_lo32(offset);
            }
          }
        }
      }

      node = next;
    }

    return Error::kOk;
  });
}

// ppc::PPCRAPass - Prolog & Epilog
// ================================

Error PPCRAPass::update_stack_frame() noexcept {
  // The ELFv2/ELFv1 frame always starts with a 32/48-byte header (back chain,
  // CR save, LR save, TOC save) and its nonvolatile save areas live at the
  // top of the frame (FPR f14+k at -(144 - 8*k), GPR r14+k below the FPR
  // area, vector v20+k below the GPR area). Folding the header and the save
  // areas into the call stack size keeps `FuncFrame::final_stack_size()`
  // equal to the real frame size, so `sa_offset_from_sp()` addresses incoming
  // stack arguments correctly (caller SP + 32 for the first one) and the
  // prolog saves never land outside the allocated frame.
  const CallConv& call_conv = _func->detail().call_conv();

  const RegMask gp_saved = _clobbered_regs[RegGroup::kGp] & call_conv.preserved_regs(RegGroup::kGp);
  const RegMask vec_saved = _clobbered_regs[RegGroup::kVec] & call_conv.preserved_regs(RegGroup::kVec);

  // The GPR mask uses the same bit layout as the assembler's prolog helpers:
  // bit k selects r14+k. The vector group splits into FPRs (f14..f31) and
  // VMX registers (v20..v31).
  const uint32_t gpr_mask = (gp_saved >> 14) & 0x3FFFFu;
  const uint32_t fpr_mask = (vec_saved >> 14) & 0x3FFFFu;
  const uint32_t vr_mask = (vec_saved >> 20) & 0xFFFu;

  uint32_t save_area_size = cc().environment().is_little_endian() ? 32 : 48;

  if (fpr_mask != 0) {
    const uint32_t lowest = fpr_mask & (~fpr_mask + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    save_area_size += 8 * (18 - k_min);
  }
  if (gpr_mask != 0) {
    const uint32_t lowest = gpr_mask & (~gpr_mask + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    save_area_size += 8 * (18 - k_min);
  }
  if (vr_mask != 0) {
    const uint32_t lowest = vr_mask & (~vr_mask + 1u);
    const uint32_t k_min = Support::popcnt(lowest - 1u);
    save_area_size += 16 * (12 - k_min);
  }

  _func->frame().update_call_stack_size(save_area_size);

  return BaseRAPass::update_stack_frame();
}

// ppc::PPCRAPass - Emit Helpers
// =============================

Error PPCRAPass::emit_move(RAWorkReg* w_reg, uint32_t dst_phys_id, uint32_t src_phys_id) noexcept {
  Reg dst(w_reg->signature(), dst_phys_id);
  Reg src(w_reg->signature(), src_phys_id);

  const char* comment = nullptr;

#ifndef ASMJIT_NO_LOGGING
  if (has_diagnostic_option(DiagnosticOptions::kRAAnnotate)) {
    _tmp_string.clear();
    Formatter::format_virt_reg_name_with_prefix(_tmp_string, "<MOVE> ", 7u, w_reg->virt_reg());
    comment = _tmp_string.data();
  }
#endif

  return _emit_helper.emit_reg_move(dst, src, w_reg->type_id(), comment);
}

Error PPCRAPass::emit_swap(RAWorkReg* a_reg, uint32_t a_phys_id, RAWorkReg* b_reg, uint32_t b_phys_id) noexcept {
  Support::maybe_unused(a_reg, a_phys_id, b_reg, b_phys_id);
  return make_error(Error::kInvalidState);
}

Error PPCRAPass::emit_load(RAWorkReg* w_reg, uint32_t dst_phys_id) noexcept {
  Reg dst_reg(w_reg->signature(), dst_phys_id);
  BaseMem src_mem(work_reg_as_mem(w_reg));

  const char* comment = nullptr;

#ifndef ASMJIT_NO_LOGGING
  if (has_diagnostic_option(DiagnosticOptions::kRAAnnotate)) {
    _tmp_string.clear();
    Formatter::format_virt_reg_name_with_prefix(_tmp_string, "<LOAD> ", 7u, w_reg->virt_reg());
    comment = _tmp_string.data();
  }
#endif

  return _emit_helper.emit_reg_move(dst_reg, src_mem, w_reg->type_id(), comment);
}

Error PPCRAPass::emit_save(RAWorkReg* w_reg, uint32_t src_phys_id) noexcept {
  BaseMem dst_mem(work_reg_as_mem(w_reg));
  Reg src_reg(w_reg->signature(), src_phys_id);

  const char* comment = nullptr;

#ifndef ASMJIT_NO_LOGGING
  if (has_diagnostic_option(DiagnosticOptions::kRAAnnotate)) {
    _tmp_string.clear();
    Formatter::format_virt_reg_name_with_prefix(_tmp_string, "<SAVE> ", 7u, w_reg->virt_reg());
    comment = _tmp_string.data();
  }
#endif

  return _emit_helper.emit_reg_move(dst_mem, src_reg, w_reg->type_id(), comment);
}

Error PPCRAPass::emit_jump(const Label& label) noexcept {
  return cc().b(label);
}

Error PPCRAPass::emit_pre_call(InvokeNode* invoke_node) noexcept {
  Support::maybe_unused(invoke_node);
  return Error::kOk;
}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC && !ASMJIT_NO_COMPILER
