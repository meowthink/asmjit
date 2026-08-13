// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCCOMPILER_H_INCLUDED
#define ASMJIT_PPC_PPCCOMPILER_H_INCLUDED

#include <asmjit/core/api-config.h>
#ifndef ASMJIT_NO_COMPILER

#include <asmjit/core/compiler.h>
#include <asmjit/core/type.h>
#include <asmjit/ppc/ppcemitter.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \addtogroup asmjit_ppc
//! \{

//! PowerPC64 compiler implementation.
//!
//! The compiler creates a function body out of virtual registers, which are
//! allocated to physical registers by the register allocation pass
//! (`PPCRAPass`) during `finalize()`.
class ASMJIT_VIRTAPI Compiler
  : public BaseCompiler,
    public EmitterExplicitT<Compiler> {
public:
  ASMJIT_NONCOPYABLE(Compiler)
  using Base = BaseCompiler;

  //! \name Construction & Destruction
  //! \{

  ASMJIT_API explicit Compiler(CodeHolder* code = nullptr) noexcept;
  ASMJIT_API ~Compiler() noexcept override;

  //! \}

  //! \name Virtual Registers
  //! \{

  //! Creates a new general-purpose register with `type_id` type and optional name passed via `args`.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp(TypeId type_id, Args&&... args) { return new_reg<Gp>(type_id, std::forward<Args>(args)...); }

  //! Creates a new floating-point register with `type_id` type and optional name passed via `args`.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Fp new_fp(TypeId type_id, Args&&... args) { return new_reg<Fp>(type_id, std::forward<Args>(args)...); }

  //! Creates a new VMX (Altivec) register with `type_id` type and optional name passed via `args`.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vr new_vec(TypeId type_id, Args&&... args) { return new_reg<Vr>(type_id, std::forward<Args>(args)...); }

  //! Creates a new VSX register with `type_id` type and optional name passed via `args`.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vsx new_vsx(TypeId type_id, Args&&... args) { return new_reg<Vsx>(type_id, std::forward<Args>(args)...); }

  //! Creates a new 64-bit general purpose register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp64(Args&&... args) { return new_reg<Gp>(TypeId::kUInt64, std::forward<Args>(args)...); }

  //! Creates a new 64-bit general purpose register (alias of `new_gp64`).
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gpx(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }

  //! Creates a new general purpose register of native register width.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gpz(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }

  //! Creates a new general purpose pointer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_gp_ptr(Args&&... args) { return new_reg<Gp>(TypeId::kUIntPtr, std::forward<Args>(args)...); }

  //! Creates a new 8-bit signed integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_int8(Args&&... args) { return new_reg<Gp>(TypeId::kInt8, std::forward<Args>(args)...); }
  //! Creates a new 16-bit signed integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_int16(Args&&... args) { return new_reg<Gp>(TypeId::kInt16, std::forward<Args>(args)...); }
  //! Creates a new 32-bit signed integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_int32(Args&&... args) { return new_reg<Gp>(TypeId::kInt32, std::forward<Args>(args)...); }
  //! Creates a new 64-bit signed integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_int64(Args&&... args) { return new_reg<Gp>(TypeId::kInt64, std::forward<Args>(args)...); }
  //! Creates a new 8-bit unsigned integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_uint8(Args&&... args) { return new_reg<Gp>(TypeId::kUInt8, std::forward<Args>(args)...); }
  //! Creates a new 16-bit unsigned integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_uint16(Args&&... args) { return new_reg<Gp>(TypeId::kUInt16, std::forward<Args>(args)...); }
  //! Creates a new 32-bit unsigned integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_uint32(Args&&... args) { return new_reg<Gp>(TypeId::kUInt32, std::forward<Args>(args)...); }
  //! Creates a new 64-bit unsigned integer register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Gp new_uint64(Args&&... args) { return new_reg<Gp>(TypeId::kUInt64, std::forward<Args>(args)...); }

  //! Creates a new single-precision floating-point register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Fp new_fp32(Args&&... args) { return new_reg<Fp>(TypeId::kFloat32, std::forward<Args>(args)...); }
  //! Creates a new double-precision floating-point register.
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Fp new_fp64(Args&&... args) { return new_reg<Fp>(TypeId::kFloat64, std::forward<Args>(args)...); }

  //! Creates a new 128-bit VMX register (Vr).
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vr new_vec128(Args&&... args) { return new_reg<Vr>(TypeId::kInt32x4, std::forward<Args>(args)...); }
  //! Creates a new 128-bit VSX register (Vsx).
  template<typename... Args>
  ASMJIT_INLINE_NODEBUG Vsx new_vsx128(Args&&... args) { return new_reg<Vsx>(TypeId::kInt32x4, std::forward<Args>(args)...); }

  //! \}

  //! \name Stack
  //! \{

  //! Creates a new stack and returns a \ref Mem operand that can be used to address it.
  ASMJIT_INLINE_NODEBUG Mem new_stack(uint32_t size, uint32_t alignment, const char* name = nullptr) {
    Mem m(Globals::NoInit);
    _new_stack(Out<BaseMem>(m), size, alignment, name);
    return m;
  }

  //! \}

  //! \name Constants
  //! \{

  //! Put data to a constant-pool and get a memory reference to it.
  ASMJIT_INLINE_NODEBUG Mem new_const(ConstPoolScope scope, const void* data, size_t size) {
    Mem m(Globals::NoInit);
    _new_const(Out<BaseMem>(m), scope, data, size);
    return m;
  }

  //! Put a BYTE `val` to a constant-pool (8 bits).
  ASMJIT_INLINE_NODEBUG Mem new_byte_const(ConstPoolScope scope, uint8_t val) noexcept { return new_const(scope, &val, 1); }
  //! Put a HWORD `val` to a constant-pool (16 bits).
  ASMJIT_INLINE_NODEBUG Mem new_half_const(ConstPoolScope scope, uint16_t val) noexcept { return new_const(scope, &val, 2); }
  //! Put a WORD `val` to a constant-pool (32 bits).
  ASMJIT_INLINE_NODEBUG Mem new_word_const(ConstPoolScope scope, uint32_t val) noexcept { return new_const(scope, &val, 4); }
  //! Put a DWORD `val` to a constant-pool (64 bits).
  ASMJIT_INLINE_NODEBUG Mem new_dword_const(ConstPoolScope scope, uint64_t val) noexcept { return new_const(scope, &val, 8); }
  //! Put a SP-FP `val` to a constant-pool.
  ASMJIT_INLINE_NODEBUG Mem new_float_const(ConstPoolScope scope, float val) noexcept { return new_const(scope, &val, 4); }
  //! Put a DP-FP `val` to a constant-pool.
  ASMJIT_INLINE_NODEBUG Mem new_double_const(ConstPoolScope scope, double val) noexcept { return new_const(scope, &val, 8); }

  //! \}

  //! \name Moves
  //! \{

  //! Moves `o1` to `o0` (general purpose registers, immediate, or memory).
  ASMJIT_API Error mov(const Gp& o0, const Gp& o1);
  ASMJIT_API Error mov(const Gp& o0, const Imm& o1);
  ASMJIT_API Error mov(const Gp& o0, const Mem& o1);
  ASMJIT_API Error mov(const Mem& o0, const Gp& o1);

  //! Moves `o1` to `o0` (floating-point registers, `fmr`).
  ASMJIT_API Error mov(const Fp& o0, const Fp& o1);
  //! Moves `o1` to `o0` (VMX registers, `vor`).
  ASMJIT_API Error mov(const Vr& o0, const Vr& o1);
  //! Moves `o1` to `o0` (VSX registers, `xxlor`).
  ASMJIT_API Error mov(const Vsx& o0, const Vsx& o1);

  //! \}

  //! \name Function Call & Ret Intrinsics
  //! \{

  //! Invoke a function call without `target` type enforcement.
  ASMJIT_INLINE_NODEBUG Error invoke_(Out<InvokeNode*> out, const Operand_& target, const FuncSignature& signature) {
    return add_invoke_node(out, Inst::kIdBctrl, target, signature);
  }

  //! Invoke a function call of the given `target` and `signature` and store the added node to `out`.
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Gp& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, const Imm& target, const FuncSignature& signature) { return invoke_(out, target, signature); }
  //! \overload
  ASMJIT_INLINE_NODEBUG Error invoke(Out<InvokeNode*> out, uint64_t target, const FuncSignature& signature) { return invoke_(out, Imm(int64_t(target)), signature); }

  //! Return from function.
  //!
  //! \note This doesn't end the function - it just emits a return.
  ASMJIT_INLINE_NODEBUG Error ret() { return add_ret(Operand(), Operand()); }

  //! Return from function - one value.
  ASMJIT_INLINE_NODEBUG Error ret(const Reg& o0) { return add_ret(o0, Operand()); }

  //! Return from function - two values / register pair.
  ASMJIT_INLINE_NODEBUG Error ret(const Reg& o0, const Reg& o1) { return add_ret(o0, o1); }

  //! \}

  //! \name Events
  //! \{

  ASMJIT_API Error on_attach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_detach(CodeHolder& code) noexcept override;
  ASMJIT_API Error on_reinit(CodeHolder& code) noexcept override;

  //! \}

  //! \name Finalize
  //! \{

  ASMJIT_API Error finalize() override;

  //! \}
};

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_COMPILER
#endif // ASMJIT_PPC_PPCCOMPILER_H_INCLUDED
