// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCRUNTIME_H_INCLUDED
#define ASMJIT_PPC_PPCRUNTIME_H_INCLUDED

#include <asmjit/core/jitruntime.h>
#include <asmjit/ppc/ppcassembler.h>

#include <vector>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \addtogroup asmjit_ppc
//! \{

//! JIT runtime that returns callable function pointers for the target ABI.
//!
//! On ppc64le (ELFv2) this behaves like \ref JitRuntime: callables are raw code
//! addresses. On ppc64 BE (ELFv1) it wraps generated code in a function
//! descriptor so C function-pointer calls work directly.
class ASMJIT_VIRTAPI Runtime : public JitRuntime {
public:
  ASMJIT_NONCOPYABLE(Runtime)
  using Base = JitRuntime;

  ASMJIT_API Runtime() noexcept;
  ASMJIT_API ~Runtime() noexcept override;

  ASMJIT_API Error _add(void** dst, CodeHolder* code) noexcept override;
  ASMJIT_API Error _release(void* p) noexcept override;

private:
  std::vector<FunctionDescriptor*> _descriptors;
};

//! \}

ASMJIT_END_SUB_NAMESPACE

#endif // ASMJIT_PPC_PPCRUNTIME_H_INCLUDED
