// This file is part of AsmJit project <https://asmjit.com>
//
// SPDX-License-Identifier: Zlib

// Minimal PPC64 backend encoding test.

#include <asmjit/ppc.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace asmjit;

static bool checkWords(const CodeHolder& code, const uint32_t* expected, size_t count) {
  const Section* text = code.text_section();
  const uint8_t* data = text->buffer().data();
  size_t size = text->buffer().size();

  if (size != count * 4u) {
    std::printf("size mismatch: got %zu bytes, expected %zu\n", size, count * 4u);
    return false;
  }

  for (size_t i = 0; i < count; i++) {
    uint32_t word = code.environment().is_little_endian()
                      ? Support::loadu_u32_le(data + i * 4u)
                      : Support::loadu_u32_be(data + i * 4u);
    if (word != expected[i]) {
      std::printf("word[%zu] mismatch: got 0x%08X, expected 0x%08X\n", i, word, expected[i]);
      return false;
    }
  }

  return true;
}

static bool testBasic() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.li(ppc::r3, 1);
  a.addi(ppc::r3, ppc::r3, 2);
  a.blr();

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x38630002u, // addi r3, r3, 2
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 3);
}

static bool testBigEndian() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_BE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.li(ppc::r3, 1);
  a.addi(ppc::r3, ppc::r3, 2);
  a.blr();

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x38630002u, // addi r3, r3, 2
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 3);
}

static bool testBigEndianBranches() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_BE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  Label skip = a.new_label();
  a.li(ppc::r3, 1);
  a.beq(skip);
  a.li(ppc::r3, 2);
  a.bind(skip);
  a.blr();

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x41820008u, // beq +8
    0x38600002u, // li r3, 2
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 4);
}

static bool testPrologEpilog() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(32);
  a.epilog(32);

  const uint32_t expected[] = {
    0x7C0802A6u, // mflr r0
    0xF8010010u, // std r0, 16(r1)
    0xF821FFE1u, // stdu r1, -32(r1)
    0x38210020u, // addi r1, r1, 32
    0xE8010010u, // ld r0, 16(r1)
    0x7C0803A6u, // mtlr r0
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 7);
}

static bool testCallConv(Arch arch) {
  Environment env(arch, SubArch::kUnknown, Vendor::kUnknown,
                  Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT);
  CallConv cc;
  if (cc.init(CallConvId::kCDecl, env) != Error::kOk) {
    return false;
  }

  const uint8_t* order = cc.passed_order(RegGroup::kGp);
  for (uint32_t i = 0; i < 8; i++) {
    if (order[i] != i + 3) {
      std::printf("passed order[%u] = %u, expected %u\n", i, order[i], i + 3);
      return false;
    }
  }

  const RegMask preserved = cc.preserved_regs(RegGroup::kGp);
  for (uint32_t r = 14; r <= 31; r++) {
    if (!(preserved & (1u << r))) {
      std::printf("preserved r%u missing\n", r);
      return false;
    }
  }

  return cc.natural_stack_alignment() == 16;
}

static bool testFuncDetail(Arch arch) {
  Environment env(arch, SubArch::kUnknown, Vendor::kUnknown,
                  Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT);

  FuncSignature signature(CallConvId::kCDecl);
  signature.set_ret_t<uint64_t>();
  signature.add_arg_t<uint64_t>();
  signature.add_arg_t<uint64_t>();

  FuncDetail detail;
  if (detail.init(signature, env) != Error::kOk) {
    return false;
  }

  if (detail._rets[0].reg_id() != 3) {
    std::printf("ret reg = %u, expected 3\n", detail._rets[0].reg_id());
    return false;
  }
  if (detail._args[0][0].reg_id() != 3) {
    std::printf("arg0 reg = %u, expected 3\n", detail._args[0][0].reg_id());
    return false;
  }
  if (detail._args[1][0].reg_id() != 4) {
    std::printf("arg1 reg = %u, expected 4\n", detail._args[1][0].reg_id());
    return false;
  }

  return true;
}

static bool testBranches() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);

  Label skip = a.new_label();
  a.li(ppc::r3, 1);
  a.beq(skip);
  a.li(ppc::r3, 2);
  a.bind(skip);
  a.blr();

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x41820008u, // beq +8 (from offset 4 to offset 12)
    0x38600002u, // li r3, 2
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 4);
}

static bool testBackwardBranch() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);

  Label loop = a.new_label();
  a.bind(loop);
  a.li(ppc::r3, 1);
  a.bne(loop);
  a.b(loop);  // unconditional backward branch (26-bit displacement field)
  a.blr();

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x4082FFFCu, // bne -4 (from offset 4 back to offset 0)
    0x4BFFFFF8u, // b -8 (from offset 8 back to offset 0)
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 4);
}

static bool testLoadImm64() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.loadImm64(ppc::r3, 0x123456789ABCDEF0ull);

  const uint32_t expected[] = {
    0x3C601234u, // lis r3, 0x1234
    0x60635678u, // ori r3, r3, 0x5678
    0x786307C6u, // sldi r3, r3, 32
    0x64639ABCu, // oris r3, r3, 0x9ABC
    0x6063DEF0u  // ori r3, r3, 0xDEF0
  };
  return checkWords(code, expected, 5);
}

static bool testMemory() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.ld(ppc::r3, ppc::ptr(ppc::r4, 8));
  a.std(ppc::r5, ppc::ptr(ppc::r4, 16));
  a.lwz(ppc::r3, ppc::ptr(ppc::r4, -4));
  a.stw(ppc::r5, ppc::ptr(ppc::r4, 8));
  a.lbz(ppc::r3, ppc::ptr(ppc::r4, 1));
  a.stb(ppc::r5, ppc::ptr(ppc::r4, 2));
  a.lhz(ppc::r3, ppc::ptr(ppc::r4, 6));
  a.sth(ppc::r5, ppc::ptr(ppc::r4, 10));

  const uint32_t expected[] = {
    0xE8640008u, // ld r3, 8(r4)
    0xF8A40010u, // std r5, 16(r4)
    0x8064FFFCu, // lwz r3, -4(r4)
    0x90A40008u, // stw r5, 8(r4)
    0x88640001u, // lbz r3, 1(r4)
    0x98A40002u, // stb r5, 2(r4)
    0xA0640006u, // lhz r3, 6(r4)
    0xB0A4000Au  // sth r5, 10(r4)
  };
  return checkWords(code, expected, 8);
}

static bool testMemoryExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.lwa(ppc::r3, ppc::ptr(ppc::r4, 8));
  a.ldu(ppc::r3, ppc::ptr(ppc::r4, 8));
  a.lwzu(ppc::r3, ppc::ptr(ppc::r4, -4));
  a.lhzu(ppc::r3, ppc::ptr(ppc::r4, 6));
  a.lha(ppc::r3, ppc::ptr(ppc::r4, 6));
  a.lhau(ppc::r3, ppc::ptr(ppc::r4, 6));
  a.lbzu(ppc::r3, ppc::ptr(ppc::r4, 1));
  a.stbu(ppc::r5, ppc::ptr(ppc::r4, 2));
  a.stwu(ppc::r5, ppc::ptr(ppc::r4, 8));
  a.sthu(ppc::r5, ppc::ptr(ppc::r4, 10));

  const uint32_t expected[] = {
    0xE864000Au, // lwa r3, 8(r4)
    0xE8640009u, // ldu r3, 8(r4)
    0x8464FFFCu, // lwzu r3, -4(r4)
    0xA4640006u, // lhzu r3, 6(r4)
    0xA8640006u, // lha r3, 6(r4)
    0xAC640006u, // lhau r3, 6(r4)
    0x8C640001u, // lbzu r3, 1(r4)
    0x9CA40002u, // stbu r5, 2(r4)
    0x94A40008u, // stwu r5, 8(r4)
    0xB4A4000Au  // sthu r5, 10(r4)
  };
  return checkWords(code, expected, 10);
}

static bool testLlSc() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.lwarx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.ldarx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.stwcx_(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stdcx_(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));

  const uint32_t expected[] = {
    0x7C642828u, // lwarx r3, r4, r5
    0x7C6428A8u, // ldarx r3, r4, r5
    0x7CA4292Du, // stwcx. r5, r4, r5
    0x7CA429ADu  // stdcx. r5, r4, r5
  };
  return checkWords(code, expected, 4);
}

static bool testRotates() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.rldicl(ppc::r3, ppc::r4, 5, 6);
  a.rldicr(ppc::r3, ppc::r4, 5, 6);
  a.rldic(ppc::r3, ppc::r4, 5, 6);
  a.rlwinm(ppc::r3, ppc::r4, 5, 6, 7);
  a.rlwimi(ppc::r3, ppc::r4, 5, 6, 7);

  const uint32_t expected[] = {
    0x78832980u, // rldicl r3, r4, 5, 6
    0x78832984u, // rldicr r3, r4, 5, 6
    0x78832988u, // rldic r3, r4, 5, 6
    0x5483298Eu, // rlwinm r3, r4, 5, 6, 7
    0x5083298Eu  // rlwimi r3, r4, 5, 6, 7
  };
  return checkWords(code, expected, 5);
}

static bool testBarriersAndCounts() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.sync();
  a.lwsync();
  a.isync();
  a.eieio();
  a.cntlzw(ppc::r3, ppc::r4);
  a.cntlzd(ppc::r3, ppc::r4);
  a.cnttzd(ppc::r3, ppc::r4);
  a.popcntd(ppc::r3, ppc::r4);
  a.extsb(ppc::r3, ppc::r4);
  a.extsh(ppc::r3, ppc::r4);
  a.extsw(ppc::r3, ppc::r4);

  const uint32_t expected[] = {
    0x7C0004ACu, // sync
    0x7C2004ACu, // lwsync
    0x4C00012Cu, // isync
    0x7C0006ACu, // eieio
    0x7C830034u, // cntlzw r3, r4
    0x7C830074u, // cntlzd r3, r4
    0x7C830474u, // cnttzd r3, r4
    0x7C8303F4u, // popcntd r3, r4
    0x7C830774u, // extsb r3, r4
    0x7C830734u, // extsh r3, r4
    0x7C8307B4u  // extsw r3, r4
  };
  return checkWords(code, expected, 11);
}

#if ASMJIT_ARCH_PPC == 64
extern "C" uint64_t ppcTestGccHelper(uint64_t a, uint64_t b) {
  return a * 3 + b;
}

static bool testExecution() {
  using Fn = uint64_t (*)(uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  Label loop = a.new_label();
  a.prolog(frame_size);
  a.li(ppc::r4, 5);
  a.bind(loop);
  a.addi(ppc::r3, ppc::r3, 1);
  a.addi(ppc::r4, ppc::r4, -1);
  a.cmpdi(ppc::r4, 0);
  a.bne(loop);
  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(37) == 42;
}

static bool testExecutionGccHelper() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  const ppc::FunctionDescriptor* helper =
    (const ppc::FunctionDescriptor*)func_as_ptr(&ppcTestGccHelper);
  const uint64_t helper_entry = (uint64_t)helper->entry;
  const uint64_t helper_toc = (uint64_t)helper->toc;
#else
  const uint64_t helper_entry = (uint64_t)func_as_ptr(&ppcTestGccHelper);
  const uint64_t helper_toc = 0;
#endif

  a.loadImm64(ppc::r12, helper_entry);
  if (helper_toc)
    a.loadImm64(ppc::r2, helper_toc);
  a.mtctr(ppc::r12);
  a.bctrl();
  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(5, 7) == 22;
}
#endif

int main() {
  bool ok = true;
  ok &= testBasic();
  ok &= testBigEndian();
  ok &= testBigEndianBranches();
  ok &= testPrologEpilog();
  ok &= testCallConv(Arch::kPPC64_LE);
  ok &= testCallConv(Arch::kPPC64_BE);
  ok &= testFuncDetail(Arch::kPPC64_LE);
  ok &= testFuncDetail(Arch::kPPC64_BE);
  ok &= testBranches();
  ok &= testBackwardBranch();
  ok &= testLoadImm64();
  ok &= testMemory();
  ok &= testMemoryExt();
  ok &= testLlSc();
  ok &= testRotates();
  ok &= testBarriersAndCounts();
#if ASMJIT_ARCH_PPC == 64
  ok &= testExecution();
  ok &= testExecutionGccHelper();
#endif

  if (!ok) {
    std::printf("ppc64 assembler tests FAILED\n");
    return 1;
  }

  std::printf("ppc64 assembler tests passed\n");
  return 0;
}
