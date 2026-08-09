// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCINSTAPI_P_H_INCLUDED
#define ASMJIT_PPC_PPCINSTAPI_P_H_INCLUDED

#include <asmjit/core/inst.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \cond INTERNAL
//! \addtogroup asmjit_ppc
//! \{

//! PowerPC64-specific instruction API.
namespace InstInternal {

//! Queries the CPU features required by `inst` (see \ref InstAPI::query_features()).
Error query_features(const BaseInst& inst, const Operand_* operands, size_t op_count, CpuFeatures* out) noexcept;

} // {InstInternal}

//! \}
//! \endcond

ASMJIT_END_SUB_NAMESPACE

#endif // ASMJIT_PPC_PPCINSTAPI_P_H_INCLUDED
