// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef ASMJIT_PPC_PPCCPUINFO_P_H_INCLUDED
#define ASMJIT_PPC_PPCCPUINFO_P_H_INCLUDED

#include <asmjit/core/cpuinfo.h>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

//! \cond INTERNAL
//! \addtogroup asmjit_ppc
//! \{

//! PPC CPU detection helpers (pure, testable on any host).
namespace CpuInfoInternal {

//! Adds the ISA level `level` and every lower ISA level to `features`.
static ASMJIT_INLINE void add_isa_level(CpuFeatures::PPC& features, CpuFeatures::PPC::Id level) noexcept {
  for (uint32_t id = uint32_t(CpuFeatures::PPC::kISA_1_1); id <= uint32_t(level); id++)
    features.add(CpuFeatures::PPC::Id(id));
}

//! Maps `AT_HWCAP`/`AT_HWCAP2` bits to PPC features.
static ASMJIT_INLINE void detect_features_from_hwcap(CpuFeatures::PPC& features, uint32_t hwcap, uint32_t hwcap2) noexcept {
  using PPC = CpuFeatures::PPC;

  if (hwcap & 0x10000000u) features.add(PPC::kAltivec);  // PPC_FEATURE_HAS_ALTIVEC.
  if (hwcap & 0x00800000u) features.add(PPC::kSPE);      // PPC_FEATURE_HAS_SPE.
  if (hwcap2 & 0x80000000u) add_isa_level(features, PPC::kISA_2_07); // PPC_FEATURE2_ARCH_2_07.
  if (hwcap2 & 0x40000000u) features.add(PPC::kHTM);     // PPC_FEATURE2_HTM.
  if (hwcap2 & 0x00800000u) add_isa_level(features, PPC::kISA_3_0);  // PPC_FEATURE2_ARCH_3_00.
  if (hwcap2 & 0x00400000u) features.add(PPC::kIEEE128); // PPC_FEATURE2_HAS_IEEE128.
  if (hwcap2 & 0x00200000u) features.add(PPC::kDARN);    // PPC_FEATURE2_DARN.
  if (hwcap2 & 0x00100000u) features.add(PPC::kSCV);     // PPC_FEATURE2_SCV.
  if (hwcap2 & 0x00040000u) add_isa_level(features, PPC::kISA_3_1);  // PPC_FEATURE2_ARCH_3_1.
  if (hwcap2 & 0x00020000u) features.add(PPC::kMMA);     // PPC_FEATURE2_MMA.
}

} // {CpuInfoInternal}

//! \}
//! \endcond

ASMJIT_END_SUB_NAMESPACE

#endif // ASMJIT_PPC_PPCCPUINFO_P_H_INCLUDED
