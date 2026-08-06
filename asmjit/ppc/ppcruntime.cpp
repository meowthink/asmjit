// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api-build_p.h>
#if !defined(ASMJIT_NO_PPC)

#include <asmjit/ppc/ppcruntime.h>

#include <new>

ASMJIT_BEGIN_SUB_NAMESPACE(ppc)

// PowerPC does not keep the instruction cache coherent with the data cache,
// so freshly written code must be flushed before it executes: dcbst every
// cache line (128 bytes on POWER4+), sync, icbi every line, then isync.
static void flushInstructionCache(void* begin, size_t size) noexcept {
#if defined(__powerpc__) || defined(__powerpc64__) || defined(__PPC__)
  const uintptr_t start = uintptr_t(begin);
  const uintptr_t end = start + size;

  for (uintptr_t p = start; p < end; p += 128)
    __asm__ __volatile__("dcbst 0, %0" :: "r"(p));
  __asm__ __volatile__("sync");
  for (uintptr_t p = start; p < end; p += 128)
    __asm__ __volatile__("icbi 0, %0" :: "r"(p));
  __asm__ __volatile__("isync");
#else
  // Cross-compiled backend: no flush needed when the host is not PowerPC.
  (void)begin;
  (void)size;
#endif
}

Runtime::Runtime() noexcept = default;

Runtime::~Runtime() noexcept {
  for (FunctionDescriptor* descriptor : _descriptors) {
    Base::_release(descriptor->entry);
    delete descriptor;
  }
}

Error Runtime::_add(void** dst, CodeHolder* code) noexcept {
  *dst = nullptr;

  void* raw = nullptr;
  ASMJIT_PROPAGATE(Base::_add(&raw, code));

  // The code was just written through the data path; make it executable.
  flushInstructionCache(raw, code->code_size());

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  FunctionDescriptor* descriptor = new (std::nothrow) FunctionDescriptor { raw, nullptr, nullptr };
  if (ASMJIT_UNLIKELY(!descriptor)) {
    Base::_release(raw);
    return make_error(Error::kOutOfMemory);
  }

  try {
    _descriptors.push_back(descriptor);
  }
  catch (...) {
    Base::_release(raw);
    delete descriptor;
    return make_error(Error::kOutOfMemory);
  }

  *dst = descriptor;
  return Error::kOk;
#else
  *dst = raw;
  return Error::kOk;
#endif
}

Error Runtime::_release(void* p) noexcept {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  for (auto it = _descriptors.begin(); it != _descriptors.end(); ++it) {
    if (*it == p) {
      FunctionDescriptor* descriptor = *it;
      Base::_release(descriptor->entry);
      _descriptors.erase(it);
      delete descriptor;
      return Error::kOk;
    }
  }

  return make_error(Error::kInvalidArgument);
#else
  return Base::_release(p);
#endif
}

ASMJIT_END_SUB_NAMESPACE

#endif // !ASMJIT_NO_PPC
