// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCRAPASS_P_H_INCLUDED
#define ASMJIT_PPC_PPCRAPASS_P_H_INCLUDED

#include <asmjit/core/api-config.h>
#ifndef ASMJIT_NO_COMPILER

#include <asmjit/core/compiler.h>
#include <asmjit/core/racfgblock_p.h>
#include <asmjit/core/racfgbuilder_p.h>
#include <asmjit/core/rapass_p.h>
#include <asmjit/ppc/ppcassembler.h>
#include <asmjit/ppc/ppccompiler.h>
#include <asmjit/ppc/ppcemithelper_p.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \cond INTERNAL
//! \addtogroup asmjit_ppc
//! \{

//! PowerPC64 register allocation pass.
//!
//! The allocator exposes a single 32-register vector group (ids 0..31) shared
//! by FPRs (kVec64), VMX registers (kVec128), and VSX registers (kVec128).
//! FPRs and VMX/VSX registers are therefore conflated by the allocator even
//! though they are physically different registers; the prolog/epilog save
//! both the FPR and the VMX register of any used id, which keeps the ABI
//! nonvolatile sets (f14..f31, v20..v31) correct.
class PPCRAPass : public BaseRAPass {
public:
  ASMJIT_NONCOPYABLE(PPCRAPass)
  using Base = BaseRAPass;

  //! \name Members
  //! \{

  EmitHelper _emit_helper;

  //! \}

  //! \name Construction & Destruction
  //! \{

  PPCRAPass(BaseCompiler& cc) noexcept;
  ~PPCRAPass() noexcept override;

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG Compiler& cc() const noexcept { return static_cast<Compiler&>(_cb); }

  [[nodiscard]]
  ASMJIT_INLINE_NODEBUG EmitHelper* emit_helper() noexcept { return &_emit_helper; }

  //! \}

  //! \name Events
  //! \{

  void on_init() noexcept override;
  void on_done() noexcept override;

  //! \}

  //! \name CFG
  //! \{

  Error build_cfg_nodes() noexcept override;

  //! \}

  //! \name Rewrite
  //! \{

  Error rewrite() noexcept override;

  //! \}

  //! \name Prolog & Epilog
  //! \{

  Error update_stack_frame() noexcept override;

  //! \}

  //! \name Emit Helpers
  //! \{

  Error emit_move(RAWorkReg* work_reg, uint32_t dst_phys_id, uint32_t src_phys_id) noexcept override;
  Error emit_swap(RAWorkReg* a_reg, uint32_t a_phys_id, RAWorkReg* b_reg, uint32_t b_phys_id) noexcept override;

  Error emit_load(RAWorkReg* work_reg, uint32_t dst_phys_id) noexcept override;
  Error emit_save(RAWorkReg* work_reg, uint32_t src_phys_id) noexcept override;

  Error emit_jump(const Label& label) noexcept override;
  Error emit_pre_call(InvokeNode* invoke_node) noexcept override;

  //! \}
};

//! \}
//! \endcond

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_COMPILER
#endif // ASMJIT_PPC_PPCRAPASS_P_H_INCLUDED
