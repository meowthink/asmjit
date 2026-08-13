// This file is part of AsmJit project <https://asmjit.com>
//
// SPDX-License-Identifier: Zlib

// Minimal PPC64 backend encoding test.

#include <asmjit/ppc.h>
#include <asmjit/ppc/ppccpuinfo_p.h>

#include <cstdint>
#include <cstring>
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

static bool testPrologLarge() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(40000);
  a.epilog(40000);

  const uint32_t expected[] = {
    0x7C0802A6u, // mflr r0
    0xF8010010u, // std r0, 16(r1)
    0x3C00FFFFu, // addis r0, r0, -1
    0x600063C0u, // ori r0, r0, 0x63C0
    0x7C21016Au, // stdux r1, r1, r0
    0xE8210000u, // ld r1, 0(r1)
    0xE8010010u, // ld r0, 16(r1)
    0x7C0803A6u, // mtlr r0
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 9);
}

static bool testPrologSaves() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(176, 0x7); // save r14, r15, r16 (ABI slots need a 176-byte frame)
  a.epilog(176, 0x7);

  const uint32_t expected[] = {
    0x7C0802A6u, // mflr r0
    0xF8010010u, // std r0, 16(r1)
    0x7D600026u, // mfcr r11
    0x91610008u, // stw r11, 8(r1)
    0xF9C1FF70u, // std r14, -144(r1)
    0xF9E1FF78u, // std r15, -136(r1)
    0xFA01FF80u, // std r16, -128(r1)
    0xF821FF51u, // stdu r1, -176(r1)
    0x382100B0u, // addi r1, r1, 176
    0xEA01FF80u, // ld r16, -128(r1)
    0xE9E1FF78u, // ld r15, -136(r1)
    0xE9C1FF70u, // ld r14, -144(r1)
    0x81610008u, // lwz r11, 8(r1)
    0x7D638120u, // mtcrf 0x38, r11
    0xE8010010u, // ld r0, 16(r1)
    0x7C0803A6u, // mtlr r0
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 17);
}

static bool testPrologLargeSaves() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(40000, 0x7);
  a.epilog(40000, 0x7);

  const uint32_t expected[] = {
    0x7C0802A6u, 0xF8010010u, 0x7D600026u, 0x91610008u,
    0xF9C1FF70u, 0xF9E1FF78u, 0xFA01FF80u,
    0x3C00FFFFu, 0x600063C0u,
    0x7C21016Au,
    0xE8210000u,
    0xEA01FF80u, 0xE9E1FF78u, 0xE9C1FF70u,
    0x81610008u, 0x7D638120u,
    0xE8010010u, 0x7C0803A6u, 0x4E800020u
  };
  return checkWords(code, expected, 19);
}

static bool testPrologValidation() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  if (a.prolog(31) != Error::kInvalidArgument) {
    return false; // below the minimum frame size
  }

  CodeHolder code2;
  if (code2.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                             Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler b(&code2);
  if (b.prolog(160, 0x7) != Error::kInvalidArgument) {
    return false; // 18 saved GPRs require 176 bytes
  }

  CodeHolder code3;
  if (code3.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                             Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler c(&code3);
  if (c.prolog(48, 0x20000) != Error::kOk) { // r31 only needs 40 bytes (16-aligned)
    return false;
  }

  CodeHolder code4;
  if (code4.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                             Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler d(&code4);
  return d.prolog(40000, 0x3FFFF) == Error::kOk && d.epilog(40000, 0x3FFFF) == Error::kOk;
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

static bool testLoadImm64Fast() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.loadImm64(ppc::r3, 0x1234);
  a.loadImm64(ppc::r4, 0x12345678);
  a.loadImm64(ppc::r7, 0x80001234);
  a.loadImm64(ppc::r5, 0xFFFFFFFF80001234ull);
  a.loadImm64(ppc::r6, 0xFFFFFFFFFFFFFFFFull);

  const uint32_t expected[] = {
    0x38601234u, // li r3, 0x1234
    0x3C801234u, // lis r4, 0x1234
    0x60845678u, // ori r4, r4, 0x5678
    0x38E00000u, // li r7, 0
    0x64E78000u, // oris r7, r7, 0x8000
    0x60E71234u, // ori r7, r7, 0x1234
    0x3CA08000u, // addis r5, r0, 0x8000
    0x38A51234u, // addi r5, r5, 0x1234
    0x38C0FFFFu  // li r6, -1
  };
  return checkWords(code, expected, 9);
}

static bool testCallHelpers() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.call(0x1234);
  a.callDescriptor(0x1000);

  const uint32_t expected[] = {
    0x39801234u, // li r12, 0x1234
    0x7D8903A6u, // mtctr r12
    0x4E800421u, // bctrl
    0x39601000u, // li r11, 0x1000
    0xE98B0000u, // ld r12, 0(r11)
    0xE84B0008u, // ld r2, 8(r11)
    0x7D8903A6u, // mtctr r12
    0x4E800421u  // bctrl
  };
  return checkWords(code, expected, 8);
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

static bool testIntegerMath() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.neg(ppc::r3, ppc::r4);
  a.addc(ppc::r3, ppc::r4, ppc::r5);
  a.addze(ppc::r3, ppc::r4);
  a.addme(ppc::r3, ppc::r4);
  a.subfc(ppc::r3, ppc::r4, ppc::r5);
  a.subfze(ppc::r3, ppc::r4);
  a.subfme(ppc::r3, ppc::r4);
  a.divd(ppc::r3, ppc::r4, ppc::r5);
  a.divw(ppc::r3, ppc::r4, ppc::r5);
  a.divdu(ppc::r3, ppc::r4, ppc::r5);
  a.divwu(ppc::r3, ppc::r4, ppc::r5);
  a.mulli(ppc::r3, ppc::r4, -7);
  a.srawi(ppc::r3, ppc::r4, 5);
  a.srdi(ppc::r3, ppc::r4, 5);
  a.andi_(ppc::r3, ppc::r4, 123);
  a.andis_(ppc::r3, ppc::r4, 456);
  a.xori(ppc::r3, ppc::r4, 789);
  a.xoris(ppc::r3, ppc::r4, 789);
  a.mulhd(ppc::r3, ppc::r4, ppc::r5);
  a.mulhw(ppc::r3, ppc::r4, ppc::r5);
  a.mulhwu(ppc::r3, ppc::r4, ppc::r5);

  const uint32_t expected[] = {
    0x7C6400D0u, // neg r3, r4
    0x7C642814u, // addc r3, r4, r5
    0x7C640194u, // addze r3, r4
    0x7C6401D4u, // addme r3, r4
    0x7C642810u, // subfc r3, r4, r5
    0x7C640190u, // subfze r3, r4
    0x7C6401D0u, // subfme r3, r4
    0x7C642BD2u, // divd r3, r4, r5
    0x7C642BD6u, // divw r3, r4, r5
    0x7C642B92u, // divdu r3, r4, r5
    0x7C642B96u, // divwu r3, r4, r5
    0x1C64FFF9u, // mulli r3, r4, -7
    0x7C832E70u, // srawi r3, r4, 5
    0x7883D942u, // srdi r3, r4, 5
    0x7083007Bu, // andi. r3, r4, 123
    0x748301C8u, // andis. r3, r4, 456
    0x68830315u, // xori r3, r4, 789
    0x6C830315u, // xoris r3, r4, 789
    0x7C642892u, // mulhd r3, r4, r5
    0x7C642896u, // mulhw r3, r4, r5
    0x7C642816u  // mulhwu r3, r4, r5
  };
  return checkWords(code, expected, 21);
}

static bool testMaskIdioms() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.clrldi(ppc::r3, ppc::r4, 6);
  a.clrrdi(ppc::r3, ppc::r4, 6);
  a.rotldi(ppc::r3, ppc::r4, 5);
  a.rotrdi(ppc::r3, ppc::r4, 5);
  a.extldi(ppc::r3, ppc::r4, 6, 7);
  a.extrdi(ppc::r3, ppc::r4, 6, 7);
  a.insrdi(ppc::r3, ppc::r4, 6, 7);

  const uint32_t expected[] = {
    0x78830180u, // clrldi r3, r4, 6
    0x78830664u, // clrrdi r3, r4, 6
    0x78832800u, // rotldi r3, r4, 5
    0x7883D802u, // rotrdi r3, r4, 5
    0x78833944u, // extldi r3, r4, 6, 7
    0x78836EA0u, // extrdi r3, r4, 6, 7
    0x788399CEu  // insrdi r3, r4, 6, 7
  };
  return checkWords(code, expected, 7);
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

static bool testIndexedMemory() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.lbzx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lbzux(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lhzx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lhzux(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lhax(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lhaux(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lwzx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lwzux(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lwax(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.ldx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.ldux(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.stbx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stbux(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.sthx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.sthux(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stwx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stwux(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stdx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stdux(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.lhbrx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.lwbrx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.ldbrx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.sthbrx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stwbrx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.stdbrx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));
  a.lmw(ppc::r5, ppc::ptr(ppc::r1, 8));
  a.stmw(ppc::r5, ppc::ptr(ppc::r1, 8));
  a.lswi(ppc::r5, ppc::r4, 2);
  a.lswx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5));
  a.stswi(ppc::r5, ppc::r4, 2);
  a.stswx(ppc::r5, ppc::ptr(ppc::r4, ppc::r5));

  const uint32_t expected[] = {
    0x7C6428AEu, 0x7C6428EEu, 0x7C642A2Eu, 0x7C642A6Eu, 0x7C642AAEu, 0x7C642AEEu,
    0x7C64282Eu, 0x7C64286Eu, 0x7C642AAAu, 0x7C64282Au, 0x7C64286Au,
    0x7CA429AEu, 0x7CA429EEu, 0x7CA42B2Eu, 0x7CA42B6Eu, 0x7CA4292Eu, 0x7CA4296Eu,
    0x7CA4292Au, 0x7CA4296Au,
    0x7C642E2Cu, 0x7C642C2Cu, 0x7C642C28u, 0x7CA42F2Cu, 0x7CA42D2Cu, 0x7CA42D28u,
    0xB8A10008u, 0xBCA10008u, 0x7CA414AAu, 0x7C642C2Au, 0x7CA415AAu, 0x7CA42D2Au
  };
  return checkWords(code, expected, 31);
}

static bool testArithmeticExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.addic(ppc::r3, ppc::r4, 5);
  a.addic(ppc::r3, ppc::r4, 5, true);
  a.subfic(ppc::r3, ppc::r4, 5);
  a.adde(ppc::r3, ppc::r4, ppc::r5);
  a.adde(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.adde(ppc::r3, ppc::r4, ppc::r5, true);
  a.adde(ppc::r3, ppc::r4, ppc::r5, true, true);
  a.subfe(ppc::r3, ppc::r4, ppc::r5);
  a.subfe(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.subfe(ppc::r3, ppc::r4, ppc::r5, true);
  a.subfe(ppc::r3, ppc::r4, ppc::r5, true, true);
  a.addex(ppc::r3, ppc::r4, ppc::r5, 1);
  a.addpcis(ppc::r3, 1234);
  a.divde(ppc::r3, ppc::r4, ppc::r5);
  a.divde(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.divde(ppc::r3, ppc::r4, ppc::r5, true);
  a.divdeu(ppc::r3, ppc::r4, ppc::r5);
  a.divdeu(ppc::r3, ppc::r4, ppc::r5, true);
  a.divwe(ppc::r3, ppc::r4, ppc::r5);
  a.divweu(ppc::r3, ppc::r4, ppc::r5);
  a.divwe(ppc::r3, ppc::r4, ppc::r5, true);
  a.divweu(ppc::r3, ppc::r4, ppc::r5, true);
  a.modsw(ppc::r3, ppc::r4, ppc::r5);
  a.moduw(ppc::r3, ppc::r4, ppc::r5);
  a.modsd(ppc::r3, ppc::r4, ppc::r5);
  a.modud(ppc::r3, ppc::r4, ppc::r5);
  a.maddhd(ppc::r3, ppc::r4, ppc::r5, ppc::r6);
  a.maddhdu(ppc::r3, ppc::r4, ppc::r5, ppc::r6);
  a.maddld(ppc::r3, ppc::r4, ppc::r5, ppc::r6);
  a.darn(ppc::r3, 2);
  a.add(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.add(ppc::r3, ppc::r4, ppc::r5, true);
  a.add(ppc::r3, ppc::r4, ppc::r5, true, true);
  a.addc(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.addc(ppc::r3, ppc::r4, ppc::r5, true);
  a.addze(ppc::r3, ppc::r4, false, true);
  a.addze(ppc::r3, ppc::r4, true);
  a.addme(ppc::r3, ppc::r4, false, true);
  a.subf(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.subf(ppc::r3, ppc::r4, ppc::r5, true);
  a.subfc(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.subfze(ppc::r3, ppc::r4, false, true);
  a.subfme(ppc::r3, ppc::r4, false, true);
  a.neg(ppc::r3, ppc::r4, false, true);
  a.neg(ppc::r3, ppc::r4, true);
  a.mulld(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.mulld(ppc::r3, ppc::r4, ppc::r5, true);
  a.mullw(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.mullw(ppc::r3, ppc::r4, ppc::r5, true);
  a.mulhd(ppc::r3, ppc::r4, ppc::r5, true);
  a.mulhdu(ppc::r3, ppc::r4, ppc::r5, true);
  a.mulhw(ppc::r3, ppc::r4, ppc::r5, true);
  a.mulhwu(ppc::r3, ppc::r4, ppc::r5, true);
  a.divd(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.divd(ppc::r3, ppc::r4, ppc::r5, true);
  a.divw(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.divw(ppc::r3, ppc::r4, ppc::r5, true);
  a.divdu(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.divdu(ppc::r3, ppc::r4, ppc::r5, true);
  a.divwu(ppc::r3, ppc::r4, ppc::r5, false, true);
  a.divwu(ppc::r3, ppc::r4, ppc::r5, true);
  a.srawi(ppc::r3, ppc::r4, 5, true);

  const uint32_t expected[] = {
    0x30640005u, 0x34640005u, 0x20640005u,
    0x7C642914u, 0x7C642915u, 0x7C642D14u, 0x7C642D15u,
    0x7C642910u, 0x7C642911u, 0x7C642D10u, 0x7C642D11u,
    0x7C642B54u, 0x4C6904C4u,
    0x7C642B52u, 0x7C642B53u, 0x7C642F52u,
    0x7C642B12u, 0x7C642F12u,
    0x7C642B56u, 0x7C642B16u, 0x7C642F56u, 0x7C642F16u,
    0x7C642E16u, 0x7C642A16u, 0x7C642E12u, 0x7C642A12u,
    0x106429B0u, 0x106429B1u, 0x106429B3u, 0x7C6205E6u,
    0x7C642A15u, 0x7C642E14u, 0x7C642E15u,
    0x7C642815u, 0x7C642C14u,
    0x7C640195u, 0x7C640594u, 0x7C6401D5u,
    0x7C642851u, 0x7C642C50u, 0x7C642811u,
    0x7C640191u, 0x7C6401D1u,
    0x7C6400D1u, 0x7C6404D0u,
    0x7C6429D3u, 0x7C642DD2u, 0x7C6429D7u, 0x7C642DD6u,
    0x7C642893u, 0x7C642813u, 0x7C642897u, 0x7C642817u,
    0x7C642BD3u, 0x7C642FD2u, 0x7C642BD7u, 0x7C642FD6u,
    0x7C642B93u, 0x7C642F92u, 0x7C642B97u, 0x7C642F96u,
    0x7C832E71u
  };
  return checkWords(code, expected, 62);
}

static bool testCompareTrapsSelect() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.cmp(1, 0, ppc::r3, ppc::r4);
  a.cmpl(2, 0, ppc::r3, ppc::r4);
  a.cmpi(3, 0, ppc::r3, 5);
  a.cmpli(4, 0, ppc::r3, 7);
  a.cmpldi(5, ppc::r3, 7);
  a.cmpb(ppc::r3, ppc::r4, ppc::r5);
  a.cmpeqb(1, ppc::r3, ppc::r4);
  a.cmprb(2, 0, ppc::r3, ppc::r4);
  a.tw(4, ppc::r3, ppc::r4);
  a.twi(4, ppc::r3, 7);
  a.td(4, ppc::r3, ppc::r4);
  a.tdi(4, ppc::r3, 7);
  a.isel(ppc::r3, ppc::r4, ppc::r5, 2);
  a.isellt(ppc::r3, ppc::r4, ppc::r5);
  a.iselgt(ppc::r3, ppc::r4, ppc::r5);
  a.iseleq(ppc::r3, ppc::r4, ppc::r5);
  a.cmpd(ppc::r3, ppc::r4);
  a.cmpld(ppc::r3, ppc::r4);
  a.cmpdi(ppc::r3, 7);

  const uint32_t expected[] = {
    0x7C832000u, 0x7D032040u, 0x2D830005u, 0x2A030007u, 0x2AA30007u,
    0x7C832BF8u, 0x7C8321C0u, 0x7D032180u,
    0x7C832008u, 0x0C830007u, 0x7C832088u, 0x08830007u,
    0x7C64289Eu, 0x7C64281Eu, 0x7C64285Eu, 0x7C64289Eu,
    0x7C232000u, 0x7C232040u, 0x2C230007u
  };
  return checkWords(code, expected, 19);
}

static bool testLogicalShiftExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.andc(ppc::r3, ppc::r4, ppc::r5);
  a.orc(ppc::r3, ppc::r4, ppc::r5);
  a.nand(ppc::r3, ppc::r4, ppc::r5);
  a.nor(ppc::r3, ppc::r4, ppc::r5);
  a.eqv(ppc::r3, ppc::r4, ppc::r5);
  a.andc(ppc::r3, ppc::r4, ppc::r5, true);
  a.orc(ppc::r3, ppc::r4, ppc::r5, true);
  a.nand(ppc::r3, ppc::r4, ppc::r5, true);
  a.nor(ppc::r3, ppc::r4, ppc::r5, true);
  a.eqv(ppc::r3, ppc::r4, ppc::r5, true);
  a.and_(ppc::r3, ppc::r4, ppc::r5, true);
  a.or_(ppc::r3, ppc::r4, ppc::r5, true);
  a.xor_(ppc::r3, ppc::r4, ppc::r5, true);
  a.slw(ppc::r3, ppc::r4, ppc::r5);
  a.srw(ppc::r3, ppc::r4, ppc::r5);
  a.sraw(ppc::r3, ppc::r4, ppc::r5);
  a.slw(ppc::r3, ppc::r4, ppc::r5, true);
  a.srw(ppc::r3, ppc::r4, ppc::r5, true);
  a.sraw(ppc::r3, ppc::r4, ppc::r5, true);
  a.sld(ppc::r3, ppc::r4, ppc::r5, true);
  a.srd(ppc::r3, ppc::r4, ppc::r5, true);
  a.srad(ppc::r3, ppc::r4, ppc::r5, true);
  a.rlwnm(ppc::r3, ppc::r4, ppc::r5, 6, 7);
  a.rlwnm(ppc::r3, ppc::r4, ppc::r5, 6, 7, true);
  a.rldcl(ppc::r3, ppc::r4, ppc::r5, 6);
  a.rldcl(ppc::r3, ppc::r4, ppc::r5, 6, true);
  a.rldcr(ppc::r3, ppc::r4, ppc::r5, 6);
  a.rldcr(ppc::r3, ppc::r4, ppc::r5, 6, true);
  a.rlwinm(ppc::r3, ppc::r4, 5, 6, 7, true);
  a.rlwimi(ppc::r3, ppc::r4, 5, 6, 7, true);
  a.rldicl(ppc::r3, ppc::r4, 5, 6, true);
  a.rldicr(ppc::r3, ppc::r4, 5, 6, true);
  a.rldic(ppc::r3, ppc::r4, 5, 6, true);
  a.rldimi(ppc::r3, ppc::r4, 5, 6, true);
  a.extswsli(ppc::r3, ppc::r4, 5);
  a.extswsli(ppc::r3, ppc::r4, 5, true);
  a.extsb(ppc::r3, ppc::r4, true);
  a.extsh(ppc::r3, ppc::r4, true);
  a.extsw(ppc::r3, ppc::r4, true);
  a.cntlzw(ppc::r3, ppc::r4, true);
  a.cntlzd(ppc::r3, ppc::r4, true);
  a.cnttzw(ppc::r3, ppc::r4);
  a.cnttzw(ppc::r3, ppc::r4, true);
  a.cnttzd(ppc::r3, ppc::r4, true);
  a.popcntb(ppc::r3, ppc::r4);
  a.popcntw(ppc::r3, ppc::r4);
  a.popcntd(ppc::r3, ppc::r4);
  a.prtyd(ppc::r3, ppc::r4);
  a.prtyw(ppc::r3, ppc::r4);
  a.bpermd(ppc::r3, ppc::r4, ppc::r5);

  const uint32_t expected[] = {
    0x7C832878u, 0x7C832B38u, 0x7C832BB8u, 0x7C8328F8u, 0x7C832A38u,
    0x7C832879u, 0x7C832B39u, 0x7C832BB9u, 0x7C8328F9u, 0x7C832A39u,
    0x7C832839u, 0x7C832B79u, 0x7C832A79u,
    0x7C832830u, 0x7C832C30u, 0x7C832E30u,
    0x7C832831u, 0x7C832C31u, 0x7C832E31u,
    0x7C832837u, 0x7C832C37u, 0x7C832E35u,
    0x5C83298Eu, 0x5C83298Fu,
    0x78832990u, 0x78832991u, 0x78832992u, 0x78832993u,
    0x5483298Fu, 0x5083298Fu,
    0x78832981u, 0x78832985u, 0x78832989u, 0x7883298Du,
    0x7C832EF4u, 0x7C832EF5u,
    0x7C830775u, 0x7C830735u, 0x7C8307B5u,
    0x7C830035u, 0x7C830075u,
    0x7C830434u, 0x7C830435u, 0x7C830475u,
    0x7C8300F4u, 0x7C8302F4u, 0x7C8303F4u,
    0x7C830174u, 0x7C830134u, 0x7C8329F8u
  };
  return checkWords(code, expected, 50);
}

static bool testSystemRegsAndCr() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.mfcr(ppc::r3);
  a.mtcrf(0xFF, ppc::r3);
  a.mfocrf(ppc::r3, 0x80);
  a.mtocrf(0x80, ppc::r3);
  a.mcrxrx(2);
  a.mfspr(ppc::r3, 8);
  a.mtspr(8, ppc::r3);
  a.mfxer(ppc::r3);
  a.mtxer(ppc::r3);
  a.setb(ppc::r3, 2);
  a.mcrf(1, 2);
  a.crand(4, 5, 6);
  a.crandc(4, 5, 6);
  a.crnor(4, 5, 6);
  a.creqv(4, 5, 6);
  a.crnand(4, 5, 6);
  a.cror(4, 5, 6);
  a.crorc(4, 5, 6);
  a.crxor(4, 5, 6);
  a.crclr(4);
  a.crset(4);
  a.crmove(4, 5);
  a.crnot(4, 5);
  a.dcbf(ppc::r4, ppc::r5);
  a.dcbst(ppc::r4, ppc::r5);
  a.dcbt(ppc::r4, ppc::r5);
  a.dcbtst(ppc::r4, ppc::r5);
  a.dcbz(ppc::r4, ppc::r5);
  a.icbi(ppc::r4, ppc::r5);
  a.icbt(0, ppc::r4, ppc::r5);
  a.wait();
  a.sc();
  a.scv();
  a.addg6s(ppc::r3, ppc::r4, ppc::r5);
  a.cbcdtd(ppc::r3, ppc::r4);
  a.cdtbcd(ppc::r3, ppc::r4);

  const uint32_t expected[] = {
    0x7C600026u, 0x7C6FF120u, 0x7C780026u, 0x7C780120u, 0x7D000480u,
    0x7C6802A6u, 0x7C6803A6u, 0x7C6102A6u, 0x7C6103A6u,
    0x7C680100u, 0x4C880000u,
    0x4C853202u, 0x4C853102u, 0x4C853042u, 0x4C853242u, 0x4C8531C2u,
    0x4C853382u, 0x4C853342u, 0x4C853182u,
    0x4C842182u, 0x4C842242u, 0x4C852B82u, 0x4C852842u,
    0x7C0428ACu, 0x7C04286Cu, 0x7C042A2Cu, 0x7C0429ECu, 0x7C042FECu,
    0x7C042FACu, 0x7C04282Cu, 0x7C00003Cu, 0x44000002u, 0x44000001u,
    0x7C642894u, 0x7C830274u, 0x7C830234u
  };
  return checkWords(code, expected, 36);
}

static bool testBranchExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  Label skip = a.new_label();
  a.bc(12, 2, skip);
  a.bcl(20, 0, skip);
  a.bind(skip);
  a.bclr(20, 0);
  a.bclrl(20, 0);
  a.bcctr(20, 0);
  a.bcctrl(20, 0);
  a.bclr(20, 0, 1);
  a.bclr(20, 0, 2);
  a.bcctr(20, 0, 1);

  const uint32_t expected[] = {
    0x41820008u, // bc 12, 2, +8
    0x42800005u, // bcl 20, 0, +4
    0x4E800020u, // bclr 20, 0
    0x4E800021u, // bclrl 20, 0
    0x4E800420u, // bcctr 20, 0
    0x4E800421u, // bcctrl 20, 0
    0x4E800820u, // bclr 20, 0, 1
    0x4E801020u, // bclr 20, 0, 2
    0x4E800C20u  // bcctr 20, 0, 1
  };
  return checkWords(code, expected, 9);
}

static bool testFpMemory() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.lfs(ppc::f1, ppc::ptr(ppc::r3, 8));
  a.lfsu(ppc::f1, ppc::ptr(ppc::r3, 8));
  a.lfsx(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.lfsux(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.lfd(ppc::f1, ppc::ptr(ppc::r3, 8));
  a.lfdu(ppc::f1, ppc::ptr(ppc::r3, 8));
  a.lfdx(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.lfdux(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.lfiwax(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.lfiwzx(ppc::f1, ppc::ptr(ppc::r3, ppc::r4));
  a.stfs(ppc::f2, ppc::ptr(ppc::r3, 8));
  a.stfsu(ppc::f2, ppc::ptr(ppc::r3, 8));
  a.stfsx(ppc::f2, ppc::ptr(ppc::r3, ppc::r4));
  a.stfsux(ppc::f2, ppc::ptr(ppc::r3, ppc::r4));
  a.stfd(ppc::f2, ppc::ptr(ppc::r3, 8));
  a.stfdu(ppc::f2, ppc::ptr(ppc::r3, 8));
  a.stfdx(ppc::f2, ppc::ptr(ppc::r3, ppc::r4));
  a.stfdux(ppc::f2, ppc::ptr(ppc::r3, ppc::r4));
  a.stfiwx(ppc::f2, ppc::ptr(ppc::r3, ppc::r4));

  const uint32_t expected[] = {
    0xC0230008u, 0xC4230008u, 0x7C23242Eu, 0x7C23246Eu,
    0xC8230008u, 0xCC230008u, 0x7C2324AEu, 0x7C2324EEu,
    0x7C2326AEu, 0x7C2326EEu,
    0xD0430008u, 0xD4430008u, 0x7C43252Eu, 0x7C43256Eu,
    0xD8430008u, 0xDC430008u, 0x7C4325AEu, 0x7C4325EEu,
    0x7C4327AEu
  };
  return checkWords(code, expected, 19);
}

static bool testFpArithmetic() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.fmr(ppc::f1, ppc::f2);
  a.fneg(ppc::f1, ppc::f2);
  a.fabs(ppc::f1, ppc::f2);
  a.fnabs(ppc::f1, ppc::f2);
  a.fcpsgn(ppc::f1, ppc::f2, ppc::f3);
  a.fadd(ppc::f1, ppc::f2, ppc::f3);
  a.fsub(ppc::f1, ppc::f2, ppc::f3);
  a.fmul(ppc::f1, ppc::f2, ppc::f3);
  a.fdiv(ppc::f1, ppc::f2, ppc::f3);
  a.fsqrt(ppc::f1, ppc::f2);
  a.fre(ppc::f1, ppc::f2);
  a.frsqrte(ppc::f1, ppc::f2);
  a.fadds(ppc::f1, ppc::f2, ppc::f3);
  a.fsubs(ppc::f1, ppc::f2, ppc::f3);
  a.fmuls(ppc::f1, ppc::f2, ppc::f3);
  a.fdivs(ppc::f1, ppc::f2, ppc::f3);
  a.fsqrts(ppc::f1, ppc::f2);
  a.fres(ppc::f1, ppc::f2);
  a.frsqrtes(ppc::f1, ppc::f2);
  a.fmadd(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fmsub(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fnmadd(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fnmsub(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fmadds(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fmsubs(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fnmadds(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fnmsubs(ppc::f1, ppc::f2, ppc::f3, ppc::f4);
  a.fadd(ppc::f1, ppc::f2, ppc::f3, true);
  a.fsub(ppc::f1, ppc::f2, ppc::f3, true);
  a.fmadd(ppc::f1, ppc::f2, ppc::f3, ppc::f4, true);
  a.fsel(ppc::f1, ppc::f2, ppc::f3, ppc::f4);

  const uint32_t expected[] = {
    0xFC201090u, 0xFC201050u, 0xFC201210u, 0xFC201110u, 0xFC221810u,
    0xFC22182Au, 0xFC221828u, 0xFC2200F2u, 0xFC221824u,
    0xFC20102Cu, 0xFC201030u, 0xFC201034u,
    0xEC22182Au, 0xEC221828u, 0xEC2200F2u, 0xEC221824u,
    0xEC20102Cu, 0xEC201030u, 0xEC201034u,
    0xFC2220FAu, 0xFC2220F8u, 0xFC2220FEu, 0xFC2220FCu,
    0xEC2220FAu, 0xEC2220F8u, 0xEC2220FEu, 0xEC2220FCu,
    0xFC22182Bu, 0xFC221829u, 0xFC2220FBu, 0xFC2220EEu
  };
  return checkWords(code, expected, 31);
}

static bool testFpConvertFpscr() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.frin(ppc::f1, ppc::f2);
  a.friz(ppc::f1, ppc::f2);
  a.frip(ppc::f1, ppc::f2);
  a.frim(ppc::f1, ppc::f2);
  a.fcfid(ppc::f1, ppc::f2);
  a.fcfidu(ppc::f1, ppc::f2);
  a.fcfids(ppc::f1, ppc::f2);
  a.fcfidus(ppc::f1, ppc::f2);
  a.fctiw(ppc::f1, ppc::f2);
  a.fctiwz(ppc::f1, ppc::f2);
  a.fctiwu(ppc::f1, ppc::f2);
  a.fctiwuz(ppc::f1, ppc::f2);
  a.fctid(ppc::f1, ppc::f2);
  a.fctidz(ppc::f1, ppc::f2);
  a.fctidu(ppc::f1, ppc::f2);
  a.fctiduz(ppc::f1, ppc::f2);
  a.frsp(ppc::f1, ppc::f2);
  a.fcmpu(0, ppc::f1, ppc::f2);
  a.fcmpo(1, ppc::f1, ppc::f2);
  a.ftdiv(1, ppc::f2, ppc::f3);
  a.ftsqrt(1, ppc::f2);
  a.mffs(ppc::f1);
  a.mffsce(ppc::f1);
  a.mffsl(ppc::f1);
  a.mffscrn(ppc::f1, ppc::f2);
  a.mffscrni(ppc::f1, 2);
  a.mffscdrn(ppc::f1, ppc::f2);
  a.mffscdrni(ppc::f1, 2);
  a.mtfsf(0xFF, ppc::f2);
  a.mtfsf(0x80, ppc::f2);
  a.mtfsb0(2);
  a.mtfsb1(2);
  a.mtfsfi(2, 2);
  a.mcrfs(1, 2);

  const uint32_t expected[] = {
    0xFC201310u, 0xFC201350u, 0xFC201390u, 0xFC2013D0u,
    0xFC20169Cu, 0xFC20179Cu, 0xEC20169Cu, 0xEC20179Cu,
    0xFC20101Cu, 0xFC20101Eu, 0xFC20111Cu, 0xFC20111Eu,
    0xFC20165Cu, 0xFC20165Eu, 0xFC20175Cu, 0xFC20175Eu,
    0xFC201018u,
    0xFC011000u, 0xFC811040u, 0xFC821900u, 0xFC801140u,
    0xFC20048Eu, 0xFC21048Eu, 0xFC38048Eu,
    0xFC36148Eu, 0xFC37148Eu, 0xFC34148Eu, 0xFC35148Eu,
    0xFDFE158Eu, 0xFD00158Eu,
    0xFC40008Cu, 0xFC40004Cu, 0xFD00210Cu, 0xFC880080u
  };
  return checkWords(code, expected, 34);
}

static bool testBailFpSpecial() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  Label bail = a.new_label();
  Label done = a.new_label();
  a.bailIfFPSpecial(ppc::f1, bail);
  a.li(ppc::r3, 1);
  a.b(done);
  a.bind(bail);
  a.li(ppc::r3, 0);
  a.bind(done);
  a.blr();

  const uint32_t expected[] = {
    0xFC010840u, // fcmpo cr0, f1, f1
    0x4183003Cu, // bc 12, 3, +60 (bail)
    0xD821FFF8u, // stfd f1, -8(r1)
    0xE801FFF8u, // ld r0, -8(r1)
    0x78000040u, // rldicl r0, r0, 0, 1
    0x396007FFu, // li r11, 0x7FF
    0x796BA2C6u, // sldi r11, r11, 52
    0x7C205840u, // cmpld r0, r11
    0x40800020u, // bge +32 (bail)
    0x780B6520u, // srdi r11, r0, 52
    0x2C2B0000u, // cmpdi r11, 0
    0x4082000Cu, // bne +12 (skip)
    0x2C200000u, // cmpdi r0, 0
    0x4082000Cu, // bne +12 (bail)
    0x38600001u, // li r3, 1
    0x48000008u, // b +8 (done)
    0x38600000u, // li r3, 0
    0x4E800020u  // blr
  };
  return checkWords(code, expected, 18);
}

static bool testCpuFeatures() {
  using PPC = CpuFeatures::PPC;
  bool ok = true;

  auto expect = [&](const char* name, bool cond) {
    if (!cond) {
      std::printf("cpu feature check '%s' FAILED\n", name);
      ok = false;
    }
  };

  // hwcap: Altivec + ARCH_2_07 + HTM.
  {
    CpuFeatures::PPC f {};
    ppc::CpuInfoInternal::detect_features_from_hwcap(f, 0x10000000u, 0x80000000u | 0x40000000u);
    expect("hwcap altivec", f.has_altivec());
    expect("hwcap 2.07 cumulative", f.has_isa_2_07() && f.has_isa_2_06() && f.has_isa_1_1());
    expect("hwcap htm", f.has_htm());
  }

  // hwcap2: ARCH_3_00 + IEEE128/DARN/SCV.
  {
    CpuFeatures::PPC f {};
    ppc::CpuInfoInternal::detect_features_from_hwcap(f, 0, 0x00800000u | 0x00400000u | 0x00200000u | 0x00100000u);
    expect("hwcap2 3.0", f.has_isa_3_0());
    expect("hwcap2 flags", f.has_ieee128() && f.has_darn() && f.has_scv());
    expect("hwcap2 not 3.1", !f.has_isa_3_1());
  }

  // query_features.
  {
    CpuFeatures f;
    expect("query lxv isa_3_0", InstAPI::query_features(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdLxv), nullptr, 0, &f) == Error::kOk && f.ppc().has_isa_3_0());
  }
  {
    CpuFeatures f;
    expect("query vaddubm altivec", InstAPI::query_features(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdVaddubm), nullptr, 0, &f) == Error::kOk && f.ppc().has_altivec());
  }
  {
    CpuFeatures f;
    expect("query xxsel vsx", InstAPI::query_features(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXxsel), nullptr, 0, &f) == Error::kOk && f.ppc().has_vsx());
  }
  {
    CpuFeatures f;
    expect("query add empty", InstAPI::query_features(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdAdd), nullptr, 0, &f) == Error::kOk && f.is_empty());
  }
  {
    CpuFeatures f;
    expect("query bad id", InstAPI::query_features(Arch::kPPC64_LE, BaseInst(InstId(9999)), nullptr, 0, &f) != Error::kOk);
  }

  return ok;
}

static bool testInstApiRWInfo() {
  bool ok = true;

  auto expect = [&](const char* name, bool cond) {
    if (!cond) {
      std::printf("inst RW info check '%s' FAILED\n", name);
      ok = false;
    }
  };

  // add rt, ra, rb: the first GPR is written, the other two are read.
  {
    Operand_ ops[] = { ppc::r3, ppc::r4, ppc::r5 };
    InstRWInfo rw;
    expect("rw add ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdAdd), ops, 3, &rw) == Error::kOk);
    expect("rw add op0 write", rw.operand(0).is_write_only());
    expect("rw add op1 read", rw.operand(1).is_read_only());
    expect("rw add op2 read", rw.operand(2).is_read_only());
  }

  // ld rt, mem: rt is written, the memory operand is read (base read only).
  {
    Operand_ ops[] = { ppc::r3, ppc::ptr(ppc::r4) };
    InstRWInfo rw;
    expect("rw ld ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdLd), ops, 2, &rw) == Error::kOk);
    expect("rw ld op0 write", rw.operand(0).is_write_only());
    expect("rw ld mem read", rw.operand(1).is_read_only());
    expect("rw ld base read", rw.operand(1).is_mem_base_read() && !rw.operand(1).is_mem_base_write());
  }

  // std rs, mem: rs is read, the memory operand is written.
  {
    Operand_ ops[] = { ppc::r3, ppc::ptr(ppc::r4) };
    InstRWInfo rw;
    expect("rw std ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdStd), ops, 2, &rw) == Error::kOk);
    expect("rw std op0 read", rw.operand(0).is_read_only());
    expect("rw std mem write", rw.operand(1).is_write_only());
    expect("rw std base read", rw.operand(1).is_mem_base_read() && !rw.operand(1).is_mem_base_write());
  }

  // stdu rs, mem: update form - the memory base is also written (post-modify).
  {
    Operand_ ops[] = { ppc::r3, ppc::ptr(ppc::r4, -16) };
    InstRWInfo rw;
    expect("rw stdu ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdStdu), ops, 2, &rw) == Error::kOk);
    expect("rw stdu op0 read", rw.operand(0).is_read_only());
    expect("rw stdu mem write", rw.operand(1).is_write_only());
    expect("rw stdu base rw", rw.operand(1).is_mem_base_read_write());
    expect("rw stdu base post", rw.operand(1).is_mem_base_post_modify());
  }

  // lwzu rt, mem: 4-byte load with update - rt written, mem read, base written.
  {
    Operand_ ops[] = { ppc::r3, ppc::ptr(ppc::r4, 4) };
    InstRWInfo rw;
    expect("rw lwzu ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdLwzu), ops, 2, &rw) == Error::kOk);
    expect("rw lwzu op0 write", rw.operand(0).is_write_only());
    expect("rw lwzu mem read", rw.operand(1).is_read_only());
    expect("rw lwzu mem size", rw.operand(1).read_byte_mask() == 0x000000000000000Fu);
    expect("rw lwzu base rw", rw.operand(1).is_mem_base_read_write());
    expect("rw lwzu base post", rw.operand(1).is_mem_base_post_modify());
  }

  // cmpd ra, rb: compare - both GPRs are read.
  {
    Operand_ ops[] = { ppc::r3, ppc::r4 };
    InstRWInfo rw;
    expect("rw cmpd ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdCmpd), ops, 2, &rw) == Error::kOk);
    expect("rw cmpd op0 read", rw.operand(0).is_read_only());
    expect("rw cmpd op1 read", rw.operand(1).is_read_only());
  }

  // cmpb ra, rs, rb: writes its first GPR (unlike CR-setting compares).
  {
    Operand_ ops[] = { ppc::r3, ppc::r4, ppc::r5 };
    InstRWInfo rw;
    expect("rw cmpb ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdCmpb), ops, 3, &rw) == Error::kOk);
    expect("rw cmpb op0 write", rw.operand(0).is_write_only());
    expect("rw cmpb op1 read", rw.operand(1).is_read_only());
  }

  // mtctr rs: move-to-special-register - source is only read.
  {
    Operand_ ops[] = { ppc::r3 };
    InstRWInfo rw;
    expect("rw mtctr ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdMtctr), ops, 1, &rw) == Error::kOk);
    expect("rw mtctr read", rw.operand(0).is_read_only());
  }

  // Power ISA 3.0: xscmpeqdp/xscmpgedp/xscmpgtdp write an all-ones/all-zeros
  // mask to XT (only xscmpodp/xscmpudp still write a CR field).
  {
    Operand_ ops[] = { ppc::vs0, ppc::vs1, ppc::vs2 };
    InstRWInfo rw;
    expect("rw xscmpeqdp ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXscmpeqdp), ops, 3, &rw) == Error::kOk);
    expect("rw xscmpeqdp op0 write", rw.operand(0).is_write_only());
    expect("rw xscmpeqdp op1 read", rw.operand(1).is_read_only());
  }
  {
    Operand_ ops[] = { imm(0), ppc::vs0, ppc::vs1 };
    InstRWInfo rw;
    expect("rw xscmpudp ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXscmpudp), ops, 3, &rw) == Error::kOk);
    expect("rw xscmpudp op1 read", rw.operand(1).is_read_only());
    expect("rw xscmpudp op2 read", rw.operand(2).is_read_only());
  }

  // xstdivdp/xstsqrtdp: VSX scalar tests - the result goes to a CR field, so
  // the VSX source operands are only read.
  {
    Operand_ ops[] = { imm(0), ppc::vs0, ppc::vs1 };
    InstRWInfo rw;
    expect("rw xstdivdp ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXstdivdp), ops, 3, &rw) == Error::kOk);
    expect("rw xstdivdp op1 read", rw.operand(1).is_read_only());
    expect("rw xstdivdp op2 read", rw.operand(2).is_read_only());
  }
  {
    Operand_ ops[] = { imm(0), ppc::vs0 };
    InstRWInfo rw;
    expect("rw xstsqrtdp ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXstsqrtdp), ops, 2, &rw) == Error::kOk);
    expect("rw xstsqrtdp read", rw.operand(1).is_read_only());
  }

  // xvadddp xt, xa, xb: first vector is written, 16-byte mask.
  {
    Operand_ ops[] = { ppc::vs0, ppc::vs1, ppc::vs2 };
    InstRWInfo rw;
    expect("rw xvadddp ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdXvadddp), ops, 3, &rw) == Error::kOk);
    expect("rw xvadddp op0 write", rw.operand(0).is_write_only());
    expect("rw xvadddp op1 read", rw.operand(1).is_read_only());
    expect("rw xvadddp mask", rw.operand(0).write_byte_mask() == 0x000000000000FFFFu);
  }

  // vsel vrt, vra, vrb, vrc: first vector written, the rest read.
  {
    Operand_ ops[] = { ppc::v0, ppc::v1, ppc::v2, ppc::v3 };
    InstRWInfo rw;
    expect("rw vsel ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdVsel), ops, 4, &rw) == Error::kOk);
    expect("rw vsel op0 write", rw.operand(0).is_write_only());
    expect("rw vsel op1 read", rw.operand(1).is_read_only());
    expect("rw vsel op3 read", rw.operand(3).is_read_only());
  }

  // beq label: no register operands.
  {
    InstRWInfo rw;
    expect("rw beq ok", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(ppc::Inst::kIdBeq), nullptr, 0, &rw) == Error::kOk);
    expect("rw beq no ops", rw.op_count() == 0);
  }

  // Invalid instruction id.
  {
    InstRWInfo rw;
    expect("rw bad id", InstAPI::query_rw_info(Arch::kPPC64_LE, BaseInst(InstId(9999)), nullptr, 0, &rw) != Error::kOk);
  }

  return ok;
}

static bool checkEmit(const char* name,
                      void (*emitFn)(ppc::Assembler&),
                      void (*directFn)(ppc::Assembler&)) {
  CodeHolder e;
  if (e.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                          Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk)
    return false;
  CodeHolder d;
  if (d.init(e.environment()) != Error::kOk)
    return false;

  ppc::Assembler ae(&e);
  ppc::Assembler ad(&d);
  emitFn(ae);
  directFn(ad);

  const Section* es = e.text_section();
  const Section* ds = d.text_section();
  if (es->buffer().size() != ds->buffer().size()) {
    std::printf("emit '%s': size mismatch %zu vs %zu\n", name, es->buffer().size(), ds->buffer().size());
    return false;
  }
  if (std::memcmp(es->buffer().data(), ds->buffer().data(), es->buffer().size()) != 0) {
    std::printf("emit '%s': data mismatch\n", name);
    return false;
  }
  return true;
}

static bool testEmit() {
  bool ok = true;
  auto check = [&](const char* n, void (*f)(ppc::Assembler&), void (*g)(ppc::Assembler&)) {
    ok &= checkEmit(n, f, g);
  };

  // Integer.
  check("add", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdAdd, ppc::r3, ppc::r4, ppc::r5); },
               [](ppc::Assembler& a) { a.add(ppc::r3, ppc::r4, ppc::r5); });
  check("and_", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdAnd_, ppc::r3, ppc::r4, ppc::r5); },
                [](ppc::Assembler& a) { a.and_(ppc::r3, ppc::r4, ppc::r5); });
  check("subf", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdSubf, ppc::r3, ppc::r4, ppc::r5); },
                [](ppc::Assembler& a) { a.subf(ppc::r3, ppc::r4, ppc::r5); });
  check("addi", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdAddi, ppc::r3, ppc::r4, Imm(5)); },
                [](ppc::Assembler& a) { a.addi(ppc::r3, ppc::r4, 5); });
  check("ori", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdOri, ppc::r3, ppc::r4, Imm(0x1234)); },
               [](ppc::Assembler& a) { a.ori(ppc::r3, ppc::r4, 0x1234); });
  check("sldi", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdSldi, ppc::r3, ppc::r4, Imm(5)); },
                [](ppc::Assembler& a) { a.sldi(ppc::r3, ppc::r4, 5); });
  check("rlwinm", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdRlwinm, ppc::r3, ppc::r4, Imm(5), Imm(6), Imm(7)); },
                  [](ppc::Assembler& a) { a.rlwinm(ppc::r3, ppc::r4, 5, 6, 7); });
  check("li", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLi, ppc::r3, Imm(-1)); },
              [](ppc::Assembler& a) { a.li(ppc::r3, -1); });
  check("isel", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdIsel, ppc::r3, ppc::r4, ppc::r5, Imm(28)); },
                [](ppc::Assembler& a) { a.isel(ppc::r3, ppc::r4, ppc::r5, 28); });
  check("extsw", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdExtsw, ppc::r3, ppc::r4); },
                 [](ppc::Assembler& a) { a.extsw(ppc::r3, ppc::r4); });
  check("mflr", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdMflr, ppc::r3); },
                [](ppc::Assembler& a) { a.mflr(ppc::r3); });

  // Memory.
  check("ld", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLd, ppc::r3, ppc::ptr(ppc::r4, 8)); },
              [](ppc::Assembler& a) { a.ld(ppc::r3, ppc::ptr(ppc::r4, 8)); });
  check("stwx", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdStwx, ppc::r3, ppc::ptr(ppc::r4, ppc::r5)); },
                [](ppc::Assembler& a) { a.stwx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5)); });
  check("lbarx", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLbarx, ppc::r3, ppc::ptr(ppc::r4, ppc::r5), Imm(1)); },
                 [](ppc::Assembler& a) { a.lbarx(ppc::r3, ppc::ptr(ppc::r4, ppc::r5), 1); });

  // Branches.
  check("b", [](ppc::Assembler& a) { Label l = a.new_label(); a.emit(ppc::Inst::kIdB, l); a.bind(l); },
             [](ppc::Assembler& a) { Label l = a.new_label(); a.b(l); a.bind(l); });
  check("beq", [](ppc::Assembler& a) { Label l = a.new_label(); a.emit(ppc::Inst::kIdBeq, l); a.bind(l); },
               [](ppc::Assembler& a) { Label l = a.new_label(); a.beq(l); a.bind(l); });
  check("bc", [](ppc::Assembler& a) { Label l = a.new_label(); a.emit(ppc::Inst::kIdBc, Imm(12), Imm(2), l); a.bind(l); },
              [](ppc::Assembler& a) { Label l = a.new_label(); a.bc(12, 2, l); a.bind(l); });
  check("blr", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdBlr); },
               [](ppc::Assembler& a) { a.blr(); });
  check("nop", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdNop); },
               [](ppc::Assembler& a) { a.nop(); });

  // Floating-point.
  check("fadd", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdFadd, ppc::f1, ppc::f2, ppc::f3); },
                [](ppc::Assembler& a) { a.fadd(ppc::f1, ppc::f2, ppc::f3); });
  check("fsel", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdFsel, ppc::f1, ppc::f2, ppc::f3, ppc::f4); },
                [](ppc::Assembler& a) { a.fsel(ppc::f1, ppc::f2, ppc::f3, ppc::f4); });
  check("fcmpu", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdFcmpu, Imm(1), ppc::f1, ppc::f2); },
                 [](ppc::Assembler& a) { a.fcmpu(1, ppc::f1, ppc::f2); });
  check("lfd", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLfd, ppc::f1, ppc::ptr(ppc::r4, 8)); },
               [](ppc::Assembler& a) { a.lfd(ppc::f1, ppc::ptr(ppc::r4, 8)); });

  // VMX.
  check("vaddubm", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdVaddubm, ppc::v1, ppc::v2, ppc::v3); },
                   [](ppc::Assembler& a) { a.vaddubm(ppc::v1, ppc::v2, ppc::v3); });
  check("vperm", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdVperm, ppc::v1, ppc::v2, ppc::v3, ppc::v4); },
                 [](ppc::Assembler& a) { a.vperm(ppc::v1, ppc::v2, ppc::v3, ppc::v4); });
  check("vsldoi", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdVsldoi, ppc::v1, ppc::v2, ppc::v3, Imm(8)); },
                  [](ppc::Assembler& a) { a.vsldoi(ppc::v1, ppc::v2, ppc::v3, 8); });
  check("lvx", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLvx, ppc::v1, ppc::ptr(ppc::r4, ppc::r5)); },
               [](ppc::Assembler& a) { a.lvx(ppc::v1, ppc::ptr(ppc::r4, ppc::r5)); });

  // VSX.
  check("xsadddp", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdXsadddp, ppc::vs1, ppc::vs2, ppc::vs3); },
                   [](ppc::Assembler& a) { a.xsadddp(ppc::vs1, ppc::vs2, ppc::vs3); });
  check("xxsel", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdXxsel, ppc::vs1, ppc::vs2, ppc::vs3, ppc::vs4); },
                 [](ppc::Assembler& a) { a.xxsel(ppc::vs1, ppc::vs2, ppc::vs3, ppc::vs4); });
  check("xxspltd", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdXxspltd, ppc::vs1, ppc::vs2, Imm(1)); },
                   [](ppc::Assembler& a) { a.xxspltd(ppc::vs1, ppc::vs2, 1); });
  check("lxv", [](ppc::Assembler& a) { a.emit(ppc::Inst::kIdLxv, ppc::vs1, ppc::ptr(ppc::r4, 16)); },
               [](ppc::Assembler& a) { a.lxv(ppc::vs1, ppc::ptr(ppc::r4, 16)); });

  return ok;
}

static bool testValidate() {
  bool ok = true;

  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk)
    return false;
  ppc::Assembler a(&code);
  a.add_diagnostic_options(DiagnosticOptions::kValidateAssembler);

  // Valid instructions pass.
  ok &= a.emit(ppc::Inst::kIdAdd, ppc::r3, ppc::r4, ppc::r5) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdSc) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdSc, Imm(1)) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdLbarx, ppc::r3, ppc::ptr(ppc::r4, ppc::r5)) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdLbarx, ppc::r3, ppc::ptr(ppc::r4, ppc::r5), Imm(1)) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdCmpd, ppc::r3, ppc::r4) == Error::kOk;
  ok &= a.emit(ppc::Inst::kIdCmpd, ppc::r3, ppc::r4, Imm(0)) == Error::kOk;

  // Invalid operand counts fail.
  ok &= a.emit(ppc::Inst::kIdAdd, ppc::r3, ppc::r4) != Error::kOk;
  ok &= a.emit(ppc::Inst::kIdSc, ppc::r3) != Error::kOk;

  // Invalid operand kinds fail.
  ok &= a.emit(ppc::Inst::kIdAddi, ppc::r3, ppc::r4, ppc::r5) != Error::kOk;
  ok &= a.emit(ppc::Inst::kIdLd, ppc::r3, ppc::r4) != Error::kOk;

  // Unknown instruction id fails.
  ok &= a.emit(ppc::Inst::Id(9999)) != Error::kOk;

  if (!ok)
    std::printf("validator tests FAILED\n");
  return ok;
}

static bool testPrologFpSaves() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(320, 0x7, 0x7); // save r14..r16 and f14..f16
  a.epilog(320, 0x7, 0x7);

  const uint32_t expected[] = {
    0x7C0802A6u, 0xF8010010u, 0x7D600026u, 0x91610008u,
    0xD9C1FF70u, 0xD9E1FF78u, 0xDA01FF80u, // stfd f14..f16
    0xF9C1FEE0u, 0xF9E1FEE8u, 0xFA01FEF0u, // std r14..r16
    0xF821FEC1u, // stdu r1, -320(r1)
    0x38210140u, // addi r1, r1, 320
    0xEA01FEF0u, 0xE9E1FEE8u, 0xE9C1FEE0u, // ld r16..r14
    0xCA01FF80u, 0xC9E1FF78u, 0xC9C1FF70u, // lfd f16..f14
    0x81610008u, 0x7D638120u,
    0xE8010010u, 0x7C0803A6u, 0x4E800020u
  };
  return checkWords(code, expected, 23);
}

static bool testVsxMoves() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.mfvsrd(ppc::r3, ppc::vs1);
  a.mfvsrld(ppc::r3, ppc::vs1);
  a.mfvsrwz(ppc::r3, ppc::vs1);
  a.mtvsrd(ppc::vs1, ppc::r3);
  a.mtvsrdd(ppc::vs1, ppc::r3, ppc::r4);
  a.mtvsrwa(ppc::vs1, ppc::r3);
  a.mtvsrws(ppc::vs1, ppc::r3);
  a.mtvsrwz(ppc::vs1, ppc::r3);
  a.mfvrd(ppc::r3, ppc::v1);
  a.mfvrwz(ppc::r3, ppc::v1);
  a.mtvrd(ppc::v1, ppc::r3);
  a.mtvrwa(ppc::v1, ppc::r3);
  a.mtvrwz(ppc::v1, ppc::r3);
  a.lxsd(ppc::vs1, ppc::ptr(ppc::r3, 8));
  a.lxssp(ppc::vs1, ppc::ptr(ppc::r3, 8));
  a.lxsdx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.lxsspx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.lxsiwzx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.lxsiwax(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.lxv(ppc::vs1, ppc::ptr(ppc::r3, 16));
  a.lxvx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.stxsd(ppc::vs1, ppc::ptr(ppc::r3, 8));
  a.stxssp(ppc::vs1, ppc::ptr(ppc::r3, 8));
  a.stxsdx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.stxsspx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.stxsiwx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));
  a.stxv(ppc::vs1, ppc::ptr(ppc::r3, 16));
  a.stxvx(ppc::vs1, ppc::ptr(ppc::r3, ppc::r4));

  const uint32_t expected[] = {
    0x7C230066u, 0x7C230266u, 0x7C2300E6u, 0x7C230166u, 0x7C232366u,
    0x7C2301A6u, 0x7C230326u, 0x7C2301E6u,
    0x7C230067u, 0x7C2300E7u, 0x7C230167u, 0x7C2301A7u, 0x7C2301E7u,
    0xE423000Au, 0xE423000Bu, 0x7C232498u, 0x7C232418u,
    0x7C232018u, 0x7C232098u, 0xF4230011u, 0x7C232218u,
    0xF423000Au, 0xF423000Bu, 0x7C232598u, 0x7C232518u,
    0x7C232118u, 0xF4230015u, 0x7C232318u
  };
  return checkWords(code, expected, 28);
}

static bool testVsxArith() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.xsabsdp(ppc::vs1, ppc::vs2);
  a.xsadddp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xscpsgndp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsdivdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmaddadp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmaddmdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmsubadp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmsubmdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmuldp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnegdp(ppc::vs1, ppc::vs2);
  a.xsnmsubadp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmsubmdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmaddadp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmaddmdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xssqrtdp(ppc::vs1, ppc::vs2);
  a.xssubdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsaddsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsdivsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmaddasp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmaddmsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmsubasp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmsubmsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsmulsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmsubasp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmsubmsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmaddasp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xsnmaddmsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xssqrtsp(ppc::vs1, ppc::vs2);
  a.xssubsp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xscmpudp(1, ppc::vs2, ppc::vs3);
  a.xscmpodp(1, ppc::vs2, ppc::vs3);
  a.xscmpeqdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xscmpgedp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xscmpgtdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xstdivdp(1, ppc::vs2, ppc::vs3);
  a.xstsqrtdp(1, ppc::vs2);

  const uint32_t expected[] = {
    0xF0201564u, 0xF0221900u, 0xF0221D80u, 0xF02219C0u,
    0xF0221908u, 0xF0221948u, 0xF0221988u, 0xF02219C8u,
    0xF0221980u, 0xF02015E4u, 0xF0221D88u, 0xF0221DC8u,
    0xF0221D08u, 0xF0221D48u, 0xF020112Cu, 0xF0221940u,
    0xF0221800u, 0xF02218C0u, 0xF0221808u, 0xF0221848u,
    0xF0221888u, 0xF02218C8u, 0xF0221880u, 0xF0221C88u,
    0xF0221CC8u, 0xF0221C08u, 0xF0221C48u, 0xF020102Cu, 0xF0221840u,
    0xF0821918u, 0xF0821958u, 0xF0221818u, 0xF0221898u,
    0xF0221858u, 0xF08219E8u, 0xF08011A8u
  };
  return checkWords(code, expected, 36);
}

static bool testVsxRoundLogical() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.xsrdpi(ppc::vs1, ppc::vs2);
  a.xsrdpic(ppc::vs1, ppc::vs2);
  a.xsrdpiz(ppc::vs1, ppc::vs2);
  a.xsrdpip(ppc::vs1, ppc::vs2);
  a.xsrdpim(ppc::vs1, ppc::vs2);
  a.xscvdpsp(ppc::vs1, ppc::vs2);
  a.xscvspdp(ppc::vs1, ppc::vs2);
  a.xscvdpsxds(ppc::vs1, ppc::vs2);
  a.xscvdpuxds(ppc::vs1, ppc::vs2);
  a.xscvsxddp(ppc::vs1, ppc::vs2);
  a.xscvuxddp(ppc::vs1, ppc::vs2);
  a.xscvdpsxws(ppc::vs1, ppc::vs2);
  a.xscvdpuxws(ppc::vs1, ppc::vs2);
  a.xscvsxdsp(ppc::vs1, ppc::vs2);
  a.xscvuxdsp(ppc::vs1, ppc::vs2);
  a.xxlxor(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxlor(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxland(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxlandc(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxlorc(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxlnand(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxlnor(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxleqv(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxsel(ppc::vs1, ppc::vs2, ppc::vs3, ppc::vs4);
  a.xxperm(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxpermdi(ppc::vs1, ppc::vs2, ppc::vs3, 0);
  a.xxmrghd(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxmrgld(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xxspltd(ppc::vs1, ppc::vs2, 0);
  a.xxspltw(ppc::vs1, ppc::vs2, 0);
  a.xxswapd(ppc::vs1, ppc::vs2);

  const uint32_t expected[] = {
    0xF0201124u, 0xF02011ACu, 0xF0201164u, 0xF02011A4u, 0xF02011E4u,
    0xF0201424u, 0xF0201524u, 0xF0201560u, 0xF0201520u, 0xF02015E0u,
    0xF02015A0u, 0xF0201160u, 0xF0201120u, 0xF02014E0u, 0xF02014A0u,
    0xF0221CD0u, 0xF0221C90u, 0xF0221C10u, 0xF0221C50u, 0xF0221D50u,
    0xF0221D90u, 0xF0221D10u, 0xF0221DD0u,
    0xF0221930u, 0xF02218D0u, 0xF0221850u,
    0xF0221850u, 0xF0221B50u, 0xF0221050u, 0xF0201290u, 0xF0221250u
  };
  return checkWords(code, expected, 31);
}

static bool testVsxFpArithmetic() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.xvadddp(ppc::vs0, ppc::vs1, ppc::vs2);
  a.xvsubdp(ppc::vs3, ppc::vs4, ppc::vs5);
  a.xvmuldp(ppc::vs6, ppc::vs7, ppc::vs8);
  a.xvdivdp(ppc::vs9, ppc::vs10, ppc::vs11);
  a.xvmaddadp(ppc::vs12, ppc::vs13, ppc::vs14);
  a.xvmaddmdp(ppc::vs15, ppc::vs16, ppc::vs17);
  a.xvmsubadp(ppc::vs18, ppc::vs19, ppc::vs20);
  a.xvmsubmdp(ppc::vs21, ppc::vs22, ppc::vs23);
  a.xvnmaddadp(ppc::vs24, ppc::vs25, ppc::vs26);
  a.xvnmaddmdp(ppc::vs27, ppc::vs28, ppc::vs29);
  a.xvnmsubadp(ppc::vs30, ppc::vs31, ppc::vs0);
  a.xvnmsubmdp(ppc::vs1, ppc::vs2, ppc::vs3);
  a.xvaddsp(ppc::vs4, ppc::vs5, ppc::vs6);
  a.xvsubsp(ppc::vs7, ppc::vs8, ppc::vs9);
  a.xvmulsp(ppc::vs10, ppc::vs11, ppc::vs12);
  a.xvdivsp(ppc::vs13, ppc::vs14, ppc::vs15);
  a.xvmaddasp(ppc::vs16, ppc::vs17, ppc::vs18);
  a.xvmaddmsp(ppc::vs19, ppc::vs20, ppc::vs21);
  a.xvmsubasp(ppc::vs22, ppc::vs23, ppc::vs24);
  a.xvmsubmsp(ppc::vs25, ppc::vs26, ppc::vs27);
  a.xvnmaddasp(ppc::vs28, ppc::vs29, ppc::vs30);
  a.xvnmaddmsp(ppc::vs31, ppc::vs0, ppc::vs1);
  a.xvnmsubasp(ppc::vs2, ppc::vs3, ppc::vs4);
  a.xvnmsubmsp(ppc::vs5, ppc::vs6, ppc::vs7);
  a.xvmaxdp(ppc::vs8, ppc::vs9, ppc::vs10);
  a.xvmindp(ppc::vs11, ppc::vs12, ppc::vs13);
  a.xvmaxsp(ppc::vs14, ppc::vs15, ppc::vs16);
  a.xvminsp(ppc::vs17, ppc::vs18, ppc::vs19);
  a.xvcmpeqdp(ppc::vs2, ppc::vs3, ppc::vs4);
  a.xvcmpgtdp(ppc::vs3, ppc::vs4, ppc::vs5);
  a.xvcmpgedp(ppc::vs4, ppc::vs5, ppc::vs6);
  a.xvcmpeqsp(ppc::vs5, ppc::vs6, ppc::vs7);
  a.xvcmpgtsp(ppc::vs6, ppc::vs7, ppc::vs8);
  a.xvcmpgesp(ppc::vs7, ppc::vs8, ppc::vs9);
  a.xvabsdp(ppc::vs0, ppc::vs1);
  a.xvabssp(ppc::vs6, ppc::vs7);
  a.xvnabsdp(ppc::vs2, ppc::vs3);
  a.xvnabssp(ppc::vs8, ppc::vs9);
  a.xvnegdp(ppc::vs4, ppc::vs5);
  a.xvnegsp(ppc::vs10, ppc::vs11);
  a.xvcpsgndp(ppc::vs12, ppc::vs13, ppc::vs14);
  a.xvcpsgnsp(ppc::vs15, ppc::vs16, ppc::vs17);
  a.xvsqrtdp(ppc::vs18, ppc::vs19);
  a.xvsqrtsp(ppc::vs20, ppc::vs21);
  a.xvrsqrtedp(ppc::vs22, ppc::vs23);
  a.xvrsqrtesp(ppc::vs24, ppc::vs25);
  a.xvcvsxwdp(ppc::vs30, ppc::vs31);
  a.xvcvuxwdp(ppc::vs0, ppc::vs1);
  a.xvcvsxwsp(ppc::vs2, ppc::vs3);
  a.xvcvuxwsp(ppc::vs4, ppc::vs5);
  a.xvcvdpsxws(ppc::vs6, ppc::vs7);
  a.xvcvdpuxws(ppc::vs8, ppc::vs9);
  a.xvcvdpsxds(ppc::vs10, ppc::vs11);
  a.xvcvdpuxds(ppc::vs12, ppc::vs13);
  a.xvcvsxddp(ppc::vs14, ppc::vs15);
  a.xvcvuxddp(ppc::vs16, ppc::vs17);
  a.xvcvdpsp(ppc::vs18, ppc::vs19);
  a.xvcvspdp(ppc::vs20, ppc::vs21);
  // Full vs0..vs63 register range via the XX3 extension bits.
  a.xvadddp(ppc::vs32, ppc::vs33, ppc::vs34);
  a.xvabsdp(ppc::vs32, ppc::vs33);
  a.xvcvsxwdp(ppc::vs32, ppc::vs33);
  a.xvcmpeqdp(ppc::vs32, ppc::vs33, ppc::vs34);

  const uint32_t expected[] = {
    0xF0011300u, 0xF0642B40u, 0xF0C74380u, 0xF12A5BC0u,
    0xF18D7308u, 0xF1F08B48u, 0xF253A388u, 0xF2B6BBC8u,
    0xF319D708u, 0xF37CEF48u, 0xF3DF0788u, 0xF0221FC8u,
    0xF0853200u, 0xF0E84A40u, 0xF14B6280u, 0xF1AE7AC0u,
    0xF2119208u, 0xF274AA48u, 0xF2D7C288u, 0xF33ADAC8u,
    0xF39DF608u, 0xF3E00E48u, 0xF0432688u, 0xF0A63EC8u,
    0xF1095700u, 0xF16C6F40u, 0xF1CF8600u, 0xF2329E40u,
    0xF0432318u, 0xF0642B58u, 0xF0853398u, 0xF0A63A18u,
    0xF0C74258u, 0xF0E84A98u, 0xF0000F64u, 0xF0C03E64u,
    0xF0401FA4u, 0xF1004EA4u, 0xF0802FE4u, 0xF1405EE4u,
    0xF18D7780u, 0xF1F08E80u, 0xF2409B2Cu, 0xF280AA2Cu,
    0xF2C0BB28u, 0xF300CA28u, 0xF3C0FBE0u, 0xF0000BA0u,
    0xF0401AE0u, 0xF0802AA0u, 0xF0C03B60u, 0xF1004B20u,
    0xF1405F60u, 0xF1806F20u, 0xF1C07FE0u, 0xF2008FA0u,
    0xF2409E24u, 0xF280AF24u, 0xF0011307u, 0xF0000F67u,
    0xF0000BE3u, 0xF001131Fu
  };
  return checkWords(code, expected, 62);
}

static bool testVsxMemExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  // vs32..vs63 (v0..v31): X-form puts the register high bit at bit 0, the
  // DQ-form at bit 3.
  a.lxvx(ppc::vs33, ppc::ptr(ppc::r3, ppc::r4));
  a.stxvx(ppc::vs33, ppc::ptr(ppc::r3, ppc::r4));
  a.lxv(ppc::vs33, ppc::ptr(ppc::r3, 0));
  a.stxv(ppc::vs33, ppc::ptr(ppc::r3, 0));

  const uint32_t expected[] = {
    0x7C232219u, 0x7C232319u, 0xF4230009u, 0xF423000Du
  };
  return checkWords(code, expected, 4);
}

static bool testVsxRegExt() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  // XX3/XX2 extension bits (XT[5]/XB[5]/XA[5] at bits 0/1/2): vs32..vs63.
  a.xxlxor(ppc::vs33, ppc::vs34, ppc::vs35);
  a.xxlor(ppc::vs33, ppc::vs34, ppc::vs35);
  a.xxland(ppc::vs33, ppc::vs34, ppc::vs35);
  a.xxsel(ppc::vs33, ppc::vs34, ppc::vs35, ppc::vs36);
  a.xscvdpsp(ppc::vs33, ppc::vs34);

  const uint32_t expected[] = {
    0xF0221CD7u, 0xF0221C97u, 0xF0221C17u, 0xF022193Fu, 0xF0201427u
  };
  return checkWords(code, expected, 5);
}

static bool testAlignEmbed() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.li(ppc::r3, 1);
  a.align(AlignMode::kCode, 8);
  const uint8_t bytes[8] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
  a.embed(bytes, 8);

  const uint32_t expected[] = {
    0x38600001u, // li r3, 1
    0x60000000u, // 1 no-op to align 4 -> 8
    0x05060708u, // embedded data, little-endian
    0x01020304u
  };
  return checkWords(code, expected, 4);
}

static bool testCallHelperGolden() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.callHelper(0x1122334455667788u);

  const uint32_t expected[] = {
    0x4D800004u, // lnia r12 (addpcis r12, 0)
    0xE98C0014u, // ld r12, 20(r12)
    0x7D8903A6u, // mtctr r12
    0x4E800421u, // bctrl
    0x48000010u, // b +16 (skip the inline slot)
    0x60000000u, // padding no-op
    0x55667788u, // inline slot, little-endian
    0x11223344u
  };
  return checkWords(code, expected, 8);
}

static bool testBLongGolden() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.bLong(0x1122334455667788u);

  const uint32_t expected[] = {
    0x4D800004u, // lnia r12 (addpcis r12, 0)
    0xE98C000Cu, // ld r12, 12(r12)
    0x7D8903A6u, // mtctr r12
    0x4E800420u, // bctr
    0x55667788u, // inline slot, little-endian
    0x11223344u
  };
  return checkWords(code, expected, 6);
}

static bool testVmxMemLogical() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.lvx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.lvebx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.lvehx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.lvewx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.lvxl(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.stvx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.stvebx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.stvehx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.stvewx(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.stvxl(ppc::v1, ppc::ptr(ppc::r3, ppc::r4));
  a.vand(ppc::v1, ppc::v2, ppc::v3);
  a.vandc(ppc::v1, ppc::v2, ppc::v3);
  a.vor(ppc::v1, ppc::v2, ppc::v3);
  a.vxor(ppc::v1, ppc::v2, ppc::v3);
  a.vnor(ppc::v1, ppc::v2, ppc::v3);
  a.veqv(ppc::v1, ppc::v2, ppc::v3);
  a.vnand(ppc::v1, ppc::v2, ppc::v3);
  a.vaddubm(ppc::v1, ppc::v2, ppc::v3);
  a.vadduhm(ppc::v1, ppc::v2, ppc::v3);
  a.vadduwm(ppc::v1, ppc::v2, ppc::v3);
  a.vaddudm(ppc::v1, ppc::v2, ppc::v3);
  a.vaddcuw(ppc::v1, ppc::v2, ppc::v3);
  a.vsububm(ppc::v1, ppc::v2, ppc::v3);
  a.vsubuhm(ppc::v1, ppc::v2, ppc::v3);
  a.vsubuwm(ppc::v1, ppc::v2, ppc::v3);
  a.vsubudm(ppc::v1, ppc::v2, ppc::v3);
  a.vsubcuw(ppc::v1, ppc::v2, ppc::v3);

  const uint32_t expected[] = {
    0x7C2320CEu, 0x7C23200Eu, 0x7C23204Eu, 0x7C23208Eu, 0x7C2322CEu,
    0x7C2321CEu, 0x7C23210Eu, 0x7C23214Eu, 0x7C23218Eu, 0x7C2323CEu,
    0x10221C04u, 0x10221C44u, 0x10221C84u, 0x10221CC4u, 0x10221D04u,
    0x10221E84u, 0x10221D84u,
    0x10221800u, 0x10221840u, 0x10221880u, 0x102218C0u, 0x10221980u,
    0x10221C00u, 0x10221C40u, 0x10221C80u, 0x10221CC0u, 0x10221D80u
  };
  return checkWords(code, expected, 27);
}

static bool testVmxShiftCmpMinMax() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vsl(ppc::v1, ppc::v2, ppc::v3);
  a.vsr(ppc::v1, ppc::v2, ppc::v3);
  a.vsld(ppc::v1, ppc::v2, ppc::v3);
  a.vsrd(ppc::v1, ppc::v2, ppc::v3);
  a.vsrad(ppc::v1, ppc::v2, ppc::v3);
  a.vslw(ppc::v1, ppc::v2, ppc::v3);
  a.vsrw(ppc::v1, ppc::v2, ppc::v3);
  a.vsraw(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpequb(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpequh(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpequw(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpequd(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtsb(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtsh(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtsw(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtsd(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtub(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtuh(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtuw(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpgtud(ppc::v1, ppc::v2, ppc::v3);
  a.vminub(ppc::v1, ppc::v2, ppc::v3);
  a.vminuh(ppc::v1, ppc::v2, ppc::v3);
  a.vminuw(ppc::v1, ppc::v2, ppc::v3);
  a.vminsb(ppc::v1, ppc::v2, ppc::v3);
  a.vminsh(ppc::v1, ppc::v2, ppc::v3);
  a.vminsw(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxub(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxuh(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxuw(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxsb(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxsh(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxsw(ppc::v1, ppc::v2, ppc::v3);

  const uint32_t expected[] = {
    0x102219C4u, 0x10221AC4u, 0x10221DC4u, 0x10221EC4u, 0x10221BC4u,
    0x10221984u, 0x10221A84u, 0x10221B84u,
    0x10221806u, 0x10221846u, 0x10221886u, 0x102218C7u,
    0x10221B06u, 0x10221B46u, 0x10221B86u, 0x10221BC7u,
    0x10221A06u, 0x10221A46u, 0x10221A86u, 0x10221AC7u,
    0x10221A02u, 0x10221A42u, 0x10221A82u, 0x10221B02u, 0x10221B42u, 0x10221B82u,
    0x10221802u, 0x10221842u, 0x10221882u, 0x10221902u, 0x10221942u, 0x10221982u
  };
  return checkWords(code, expected, 32);
}

static bool testVmxPackPermute() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vpkuhum(ppc::v1, ppc::v2, ppc::v3);
  a.vpkuwum(ppc::v1, ppc::v2, ppc::v3);
  a.vpkuhus(ppc::v1, ppc::v2, ppc::v3);
  a.vpkuwus(ppc::v1, ppc::v2, ppc::v3);
  a.vpkshss(ppc::v1, ppc::v2, ppc::v3);
  a.vpkswss(ppc::v1, ppc::v2, ppc::v3);
  a.vpkshus(ppc::v1, ppc::v2, ppc::v3);
  a.vpkswus(ppc::v1, ppc::v2, ppc::v3);
  a.vupkhsb(ppc::v1, ppc::v2);
  a.vupkhsh(ppc::v1, ppc::v2);
  a.vupklsb(ppc::v1, ppc::v2);
  a.vupklsh(ppc::v1, ppc::v2);
  a.vmrghb(ppc::v1, ppc::v2, ppc::v3);
  a.vmrghh(ppc::v1, ppc::v2, ppc::v3);
  a.vmrghw(ppc::v1, ppc::v2, ppc::v3);
  a.vmrglb(ppc::v1, ppc::v2, ppc::v3);
  a.vmrglh(ppc::v1, ppc::v2, ppc::v3);
  a.vmrglw(ppc::v1, ppc::v2, ppc::v3);
  a.vspltb(ppc::v1, ppc::v2, 3);
  a.vsplth(ppc::v1, ppc::v2, 3);
  a.vspltw(ppc::v1, ppc::v2, 3);
  a.vspltisb(ppc::v1, 3);
  a.vspltish(ppc::v1, 3);
  a.vspltisw(ppc::v1, 3);
  a.vperm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vsel(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vsldoi(ppc::v1, ppc::v2, ppc::v3, 4);
  a.vclzb(ppc::v1, ppc::v2);
  a.vclzh(ppc::v1, ppc::v2);
  a.vclzw(ppc::v1, ppc::v2);
  a.vclzd(ppc::v1, ppc::v2);
  a.vpopcntb(ppc::v1, ppc::v2);
  a.vpopcnth(ppc::v1, ppc::v2);
  a.vpopcntw(ppc::v1, ppc::v2);
  a.vpopcntd(ppc::v1, ppc::v2);
  a.vmulouw(ppc::v1, ppc::v2, ppc::v3);
  a.vmuluwm(ppc::v1, ppc::v2, ppc::v3);

  const uint32_t expected[] = {
    0x1022180Eu, 0x1022184Eu, 0x1022188Eu, 0x102218CEu, 0x1022198Eu, 0x102219CEu,
    0x1022190Eu, 0x1022194Eu,
    0x1020120Eu, 0x1020124Eu, 0x1020128Eu, 0x102012CEu,
    0x1022180Cu, 0x1022184Cu, 0x1022188Cu, 0x1022190Cu, 0x1022194Cu, 0x1022198Cu,
    0x1023120Cu, 0x1023124Cu, 0x1023128Cu, 0x1023030Cu, 0x1023034Cu, 0x1023038Cu,
    0x1022192Bu, 0x1022192Au, 0x1022192Cu,
    0x10201702u, 0x10201742u, 0x10201782u, 0x102017C2u,
    0x10201703u, 0x10201743u, 0x10201783u, 0x102017C3u,
    0x10221888u, 0x10221889u
  };
  return checkWords(code, expected, 37);
}

static bool testVmxSaturateAvgSum() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vaddsbs(ppc::v1, ppc::v2, ppc::v3);
  a.vaddshs(ppc::v1, ppc::v2, ppc::v3);
  a.vaddsws(ppc::v1, ppc::v2, ppc::v3);
  a.vaddubs(ppc::v1, ppc::v2, ppc::v3);
  a.vadduhs(ppc::v1, ppc::v2, ppc::v3);
  a.vadduws(ppc::v1, ppc::v2, ppc::v3);
  a.vsubsbs(ppc::v1, ppc::v2, ppc::v3);
  a.vsubshs(ppc::v1, ppc::v2, ppc::v3);
  a.vsubsws(ppc::v1, ppc::v2, ppc::v3);
  a.vsububs(ppc::v1, ppc::v2, ppc::v3);
  a.vsubuhs(ppc::v1, ppc::v2, ppc::v3);
  a.vsubuws(ppc::v1, ppc::v2, ppc::v3);
  a.vavgub(ppc::v1, ppc::v2, ppc::v3);
  a.vavguh(ppc::v1, ppc::v2, ppc::v3);
  a.vavguw(ppc::v1, ppc::v2, ppc::v3);
  a.vavgsb(ppc::v1, ppc::v2, ppc::v3);
  a.vavgsh(ppc::v1, ppc::v2, ppc::v3);
  a.vavgsw(ppc::v1, ppc::v2, ppc::v3);
  a.vsum4sbs(ppc::v1, ppc::v2, ppc::v3);
  a.vsum4shs(ppc::v1, ppc::v2, ppc::v3);
  a.vsum4ubs(ppc::v1, ppc::v2, ppc::v3);
  a.vsum2sws(ppc::v1, ppc::v2, ppc::v3);
  a.vsumsws(ppc::v1, ppc::v2, ppc::v3);
  a.vmuleub(ppc::v1, ppc::v2, ppc::v3);
  a.vmuleuh(ppc::v1, ppc::v2, ppc::v3);
  a.vmuleuw(ppc::v1, ppc::v2, ppc::v3);
  a.vmulesb(ppc::v1, ppc::v2, ppc::v3);
  a.vmulesh(ppc::v1, ppc::v2, ppc::v3);
  a.vmulesw(ppc::v1, ppc::v2, ppc::v3);
  a.vmuloub(ppc::v1, ppc::v2, ppc::v3);
  a.vmulouh(ppc::v1, ppc::v2, ppc::v3);
  a.vmulosb(ppc::v1, ppc::v2, ppc::v3);
  a.vmulosh(ppc::v1, ppc::v2, ppc::v3);
  a.vmulosw(ppc::v1, ppc::v2, ppc::v3);
  a.vslb(ppc::v1, ppc::v2, ppc::v3);
  a.vsrb(ppc::v1, ppc::v2, ppc::v3);

  const uint32_t expected[] = {
    0x10221B00u, 0x10221B40u, 0x10221B80u, 0x10221A00u, 0x10221A40u, 0x10221A80u,
    0x10221F00u, 0x10221F40u, 0x10221F80u, 0x10221E00u, 0x10221E40u, 0x10221E80u,
    0x10221C02u, 0x10221C42u, 0x10221C82u, 0x10221D02u, 0x10221D42u, 0x10221D82u,
    0x10221F08u, 0x10221E48u, 0x10221E08u, 0x10221E88u, 0x10221F88u,
    0x10221A08u, 0x10221A48u, 0x10221A88u, 0x10221B08u, 0x10221B48u, 0x10221B88u,
    0x10221808u, 0x10221848u, 0x10221908u, 0x10221948u, 0x10221988u,
    0x10221904u, 0x10221A04u
  };
  return checkWords(code, expected, 36);
}

static bool testVmxQwordRotate() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vaddcuq(ppc::v1, ppc::v2, ppc::v3);
  a.vadduqm(ppc::v1, ppc::v2, ppc::v3);
  a.vsubcuq(ppc::v1, ppc::v2, ppc::v3);
  a.vsubuqm(ppc::v1, ppc::v2, ppc::v3);
  a.vabsdub(ppc::v1, ppc::v2, ppc::v3);
  a.vabsduh(ppc::v1, ppc::v2, ppc::v3);
  a.vabsduw(ppc::v1, ppc::v2, ppc::v3);
  a.vbpermd(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpneb(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpneh(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpnew(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpnezb(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpnezh(ppc::v1, ppc::v2, ppc::v3);
  a.vcmpnezw(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxsd(ppc::v1, ppc::v2, ppc::v3);
  a.vmaxud(ppc::v1, ppc::v2, ppc::v3);
  a.vminsd(ppc::v1, ppc::v2, ppc::v3);
  a.vminud(ppc::v1, ppc::v2, ppc::v3);
  a.vmul10cuq(ppc::v1, ppc::v2);
  a.vmul10euq(ppc::v1, ppc::v2, ppc::v3);
  a.vmul10ecuq(ppc::v1, ppc::v2, ppc::v3);
  a.vmul10uq(ppc::v1, ppc::v2);
  a.vpkudum(ppc::v1, ppc::v2, ppc::v3);
  a.vpkudus(ppc::v1, ppc::v2, ppc::v3);
  a.vpksdss(ppc::v1, ppc::v2, ppc::v3);
  a.vpksdus(ppc::v1, ppc::v2, ppc::v3);
  a.vmrgew(ppc::v1, ppc::v2, ppc::v3);
  a.vmrgow(ppc::v1, ppc::v2, ppc::v3);
  a.vrlw(ppc::v1, ppc::v2, ppc::v3);
  a.vrlh(ppc::v1, ppc::v2, ppc::v3);
  a.vrlb(ppc::v1, ppc::v2, ppc::v3);
  a.vrld(ppc::v1, ppc::v2, ppc::v3);
  a.vrlwmi(ppc::v1, ppc::v2, ppc::v3);
  a.vrlwnm(ppc::v1, ppc::v2, ppc::v3);
  a.vrldmi(ppc::v1, ppc::v2, ppc::v3);
  a.vrldnm(ppc::v1, ppc::v2, ppc::v3);
  a.vslh(ppc::v1, ppc::v2, ppc::v3);
  a.vsrh(ppc::v1, ppc::v2, ppc::v3);
  a.vslo(ppc::v1, ppc::v2, ppc::v3);
  a.vsro(ppc::v1, ppc::v2, ppc::v3);
  a.vslv(ppc::v1, ppc::v2, ppc::v3);
  a.vsrv(ppc::v1, ppc::v2, ppc::v3);

  const uint32_t expected[] = {
    0x10221940u, 0x10221900u, 0x10221D40u, 0x10221D00u,
    0x10221C03u, 0x10221C43u, 0x10221C83u, 0x10221DCCu,
    0x10221807u, 0x10221847u, 0x10221887u, 0x10221907u, 0x10221947u, 0x10221987u,
    0x102219C2u, 0x102218C2u, 0x10221BC2u, 0x10221AC2u,
    0x10220001u, 0x10221A41u, 0x10221841u, 0x10220201u,
    0x10221C4Eu, 0x10221CCEu, 0x10221DCEu, 0x10221D4Eu,
    0x10221F8Cu, 0x10221E8Cu,
    0x10221884u, 0x10221844u, 0x10221804u, 0x102218C4u,
    0x10221885u, 0x10221985u, 0x102218C5u, 0x102219C5u,
    0x10221944u, 0x10221A44u, 0x10221C0Cu, 0x10221C4Cu, 0x10221F44u, 0x10221F04u
  };
  return checkWords(code, expected, 42);
}

static bool testVmxVaForm() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vaddeuqm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vaddecuq(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vsubeuqm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vsubecuq(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmladduhm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmhaddshs(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmhraddshs(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsummbm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumshm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumshs(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumubm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumudm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumuhm(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vmsumuhs(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vpermr(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vpermxor(ppc::v1, ppc::v2, ppc::v3, ppc::v4);
  a.vctzb(ppc::v1, ppc::v2);
  a.vctzh(ppc::v1, ppc::v2);
  a.vctzw(ppc::v1, ppc::v2);
  a.vctzd(ppc::v1, ppc::v2);
  a.vextractub(ppc::v1, ppc::v2, 3);
  a.vextractuh(ppc::v1, ppc::v2, 3);
  a.vextractuw(ppc::v1, ppc::v2, 3);
  a.vextractd(ppc::v1, ppc::v2, 3);
  a.vextsb2w(ppc::v1, ppc::v2);
  a.vextsh2w(ppc::v1, ppc::v2);
  a.vextsw2d(ppc::v1, ppc::v2);
  a.vextsb2d(ppc::v1, ppc::v2);
  a.vextsh2d(ppc::v1, ppc::v2);
  a.vnegw(ppc::v1, ppc::v2);
  a.vnegd(ppc::v1, ppc::v2);
  a.vprtybd(ppc::v1, ppc::v2);
  a.vprtybw(ppc::v1, ppc::v2);
  a.vprtybq(ppc::v1, ppc::v2);
  a.vupkhsw(ppc::v1, ppc::v2);
  a.vupklsw(ppc::v1, ppc::v2);
  a.vgbbd(ppc::v1, ppc::v2);
  a.vpmsumb(ppc::v1, ppc::v2, ppc::v3);
  a.vpmsumh(ppc::v1, ppc::v2, ppc::v3);
  a.vpmsumw(ppc::v1, ppc::v2, ppc::v3);
  a.vpmsumd(ppc::v1, ppc::v2, ppc::v3);
  a.vpkpx(ppc::v1, ppc::v2, ppc::v3);
  a.vupkhpx(ppc::v1, ppc::v2);
  a.vupklpx(ppc::v1, ppc::v2);

  const uint32_t expected[] = {
    0x1022193Cu, 0x1022193Du, 0x1022193Eu, 0x1022193Fu,
    0x10221922u, 0x10221920u, 0x10221921u,
    0x10221925u, 0x10221928u, 0x10221929u, 0x10221924u, 0x10221923u, 0x10221926u, 0x10221927u,
    0x1022193Bu, 0x1022192Du,
    0x103C1602u, 0x103D1602u, 0x103E1602u, 0x103F1602u,
    0x1023120Du, 0x1023124Du, 0x1023128Du, 0x102312CDu,
    0x10301602u, 0x10311602u, 0x103A1602u, 0x10381602u, 0x10391602u,
    0x10261602u, 0x10271602u, 0x10291602u, 0x10281602u, 0x102A1602u,
    0x1020164Eu, 0x102016CEu, 0x1020150Cu,
    0x10221C08u, 0x10221C48u, 0x10221C88u, 0x10221CC8u,
    0x10221B0Eu, 0x1020134Eu, 0x102013CEu
  };
  return checkWords(code, expected, 44);
}

static bool testVmxExtractInsert() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.vclzlsbb(ppc::r3, ppc::v2);
  a.vctzlsbb(ppc::r3, ppc::v2);
  a.vextublx(ppc::r1, ppc::r2, ppc::v3);
  a.vextubrx(ppc::r1, ppc::r2, ppc::v3);
  a.vextuhlx(ppc::r1, ppc::r2, ppc::v3);
  a.vextuhrx(ppc::r1, ppc::r2, ppc::v3);
  a.vextuwlx(ppc::r1, ppc::r2, ppc::v3);
  a.vextuwrx(ppc::r1, ppc::r2, ppc::v3);
  a.vinsertb(ppc::v1, ppc::v2, 3);
  a.vinserth(ppc::v1, ppc::v2, 3);
  a.vinsertw(ppc::v1, ppc::v2, 3);
  a.vinsertd(ppc::v1, ppc::v2, 3);

  const uint32_t expected[] = {
    0x10601602u, 0x10611602u,
    0x10221E0Du, 0x10221F0Du, 0x10221E4Du, 0x10221F4Du, 0x10221E8Du, 0x10221F8Du,
    0x1023130Du, 0x1023134Du, 0x1023138Du, 0x102313CDu
  };
  return checkWords(code, expected, 12);
}

static bool testPrologVrSaves() {
  CodeHolder code;
  if (code.init(Environment(Arch::kPPC64_LE, SubArch::kUnknown, Vendor::kUnknown,
                            Platform::kLinux, PlatformABI::kGNU, ObjectFormat::kJIT)) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  a.prolog(512, 0x7, 0x7, 0x3); // save r14..r16, f14..f16, v20..v21
  a.epilog(512, 0x7, 0x7, 0x3);

  const uint32_t expected[] = {
    0x7C0802A6u, 0xF8010010u, 0x7D600026u, 0x91610008u,
    0xD9C1FF70u, 0xD9E1FF78u, 0xDA01FF80u,
    0xF9C1FEE0u, 0xF9E1FEE8u, 0xFA01FEF0u,
    0x3800FE20u, 0x7E8101CEu, // li r0,-480; stvx v20, r1, r0
    0x3800FE30u, 0x7EA101CEu, // li r0,-464; stvx v21, r1, r0
    0xF821FE01u, // stdu r1, -512(r1)
    0x38210200u, // addi r1, r1, 512
    0xEA01FEF0u, 0xE9E1FEE8u, 0xE9C1FEE0u,
    0xCA01FF80u, 0xC9E1FF78u, 0xC9C1FF70u,
    0x3800FE30u, 0x7EA100CEu, // li r0,-464; lvx v21, r1, r0
    0x3800FE20u, 0x7E8100CEu, // li r0,-480; lvx v20, r1, r0
    0x81610008u, 0x7D638120u,
    0xE8010010u, 0x7C0803A6u, 0x4E800020u
  };
  return checkWords(code, expected, 31);
}

#if ASMJIT_ARCH_PPC == 64
extern "C" uint64_t ppcTestGccHelper(uint64_t a, uint64_t b) {
  return a * 3 + b;
}

extern "C" double ppcTestFpHelper(double a, double b) {
  return a * 3.0 + b;
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
  a.callDescriptor((uint64_t)func_as_ptr(&ppcTestGccHelper));
#else
  a.call((uint64_t)func_as_ptr(&ppcTestGccHelper));
#endif
  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(5, 7) == 22;
}

static bool testExecutionStep5() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);

  // r3 = a, r4 = b
  a.andc(ppc::r5, ppc::r3, ppc::r4);            // r5 = a & ~b
  a.eqv(ppc::r6, ppc::r3, ppc::r4);             // r6 = ~(a ^ b)
  a.add(ppc::r8, ppc::r3, ppc::r4);             // r8 = a + b
  a.maddld(ppc::r7, ppc::r3, ppc::r4, ppc::r8); // r7 = a*b + (a+b)
  a.modsd(ppc::r9, ppc::r3, ppc::r4);           // r9 = a % b
  a.popcntw(ppc::r10, ppc::r3);                 // r10 = popcount(a & 0xffffffff)
  a.cnttzw(ppc::r11, ppc::r4);                  // r11 = ctz(b)
  a.prtyd(ppc::r12, ppc::r3);                   // r12 = parity(a)
  a.add(ppc::r3, ppc::r5, ppc::r6);
  a.add(ppc::r3, ppc::r3, ppc::r7);
  a.add(ppc::r3, ppc::r3, ppc::r9);
  a.add(ppc::r3, ppc::r3, ppc::r10);
  a.add(ppc::r3, ppc::r3, ppc::r11);
  a.add(ppc::r3, ppc::r3, ppc::r12);
  a.cmpdi(ppc::r4, 10);
  a.isellt(ppc::r12, ppc::r4, ppc::r8);         // r12 = b if b < 10 else a+b
  a.add(ppc::r3, ppc::r3, ppc::r12);

  // byte-reversed and indexed loads from a stack scratch slot
  a.li(ppc::r11, 0);                       // zero index register (r0 is a real GPR here)
  a.addi(ppc::r9, ppc::r1, 8);
  a.loadImm64(ppc::r10, 0x1122334455667788u);
  a.stdx(ppc::r10, ppc::ptr(ppc::r9, ppc::r11));
  a.lwbrx(ppc::r12, ppc::ptr(ppc::r9, ppc::r11));
  a.add(ppc::r3, ppc::r3, ppc::r12);
  a.lhbrx(ppc::r12, ppc::ptr(ppc::r9, ppc::r11));
  a.add(ppc::r3, ppc::r3, ppc::r12);
  a.ldbrx(ppc::r12, ppc::ptr(ppc::r9, ppc::r11));
  a.add(ppc::r3, ppc::r3, ppc::r12);

  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  const uint64_t argA = 5;
  const uint64_t argB = 3;
  uint64_t expected = 0;
  expected += argA & ~argB;
  expected += ~(argA ^ argB);
  expected += argA * argB + (argA + argB);
  expected += argA % argB;
  expected += (uint64_t)__builtin_popcount((uint32_t)argA);
  expected += (uint64_t)__builtin_ctz((uint32_t)argB);
  uint64_t byteLsbParity = 0;
  for (int i = 0; i < 8; i++) byteLsbParity ^= (argA >> (8 * i)) & 1u;
  expected += byteLsbParity;
  expected += (argB < 10) ? argB : (argA + argB); // isellt semantics
  // Byte-reversed loads are equivalent to a native load followed by a byte swap,
  // so the expected values depend on the native memory layout of the stored value.
  uint64_t stored = 0x1122334455667788u;
  uint8_t bytes[8];
  std::memcpy(bytes, &stored, 8);
  uint32_t w32 = 0;
  uint16_t h16 = 0;
  uint64_t d64 = 0;
  std::memcpy(&w32, bytes, 4);
  std::memcpy(&h16, bytes, 2);
  std::memcpy(&d64, bytes, 8);
  expected += (uint64_t)__builtin_bswap32(w32); // lwbrx
  expected += (uint64_t)__builtin_bswap16(h16); // lhbrx
  expected += __builtin_bswap64(d64);           // ldbrx

  return fn(argA, argB) == expected;
}

static bool testExecutionLargeFrame() {
  using Fn = uint64_t (*)(uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = 40000; // > 32 KiB
  a.prolog(frame_size);

  // Touch storage deep inside the frame, beyond the single-instruction range.
  a.loadImm64(ppc::r9, 32784);
  a.add(ppc::r9, ppc::r1, ppc::r9);
  a.std(ppc::r3, ppc::ptr(ppc::r9, 0));
  a.ld(ppc::r3, ppc::ptr(ppc::r9, 0));
  a.li(ppc::r4, 5);
  a.add(ppc::r3, ppc::r3, ppc::r4);

  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(37) == 42;
}

static bool testExecutionNonvolatile() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = 176; // enough for the ABI-fixed r14..r16 slots
  a.prolog(frame_size, 0x7); // save r14, r15, r16 and CR

  a.li(ppc::r14, 0x1234);
  a.li(ppc::r15, 0x5678);
  a.li(ppc::r16, 0x2ABC); // keep < 0x8000: `li` sign-extends

  // Clobber the whole CR so a broken save/restore would corrupt the caller.
  a.loadImm64(ppc::r12, 0x0102030405060708u);
  a.mtcrf(0xFF, ppc::r12);

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  a.callDescriptor((uint64_t)func_as_ptr(&ppcTestGccHelper));
#else
  a.call((uint64_t)func_as_ptr(&ppcTestGccHelper));
#endif

  a.add(ppc::r3, ppc::r3, ppc::r14);
  a.add(ppc::r3, ppc::r3, ppc::r15);
  a.add(ppc::r3, ppc::r3, ppc::r16);

  a.epilog(frame_size, 0x7);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  const uint64_t argA = 5;
  const uint64_t argB = 7;
  const uint64_t keep = argA * 1009; // kept live across the call in a callee-saved reg
  const uint64_t r = fn(argA, argB);
  const uint64_t expected = 22 + 0x1234 + 0x5678 + 0x2ABC;
  return keep * 7 + r == keep * 7 + expected;
}

static bool testExecutionFp() {
  using Fn = int64_t (*)(double, double);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);

  a.fadd(ppc::f3, ppc::f1, ppc::f2);          // a + b
  a.fsub(ppc::f4, ppc::f1, ppc::f2);          // a - b
  a.fmul(ppc::f5, ppc::f3, ppc::f4);          // (a+b)(a-b)
  a.fmadd(ppc::f6, ppc::f1, ppc::f2, ppc::f1); // a*b + a
  a.fdiv(ppc::f7, ppc::f5, ppc::f6);
  a.fabs(ppc::f8, ppc::f7);
  a.fneg(ppc::f9, ppc::f8);
  a.frsp(ppc::f10, ppc::f9);
  a.fctidz(ppc::f11, ppc::f10);
  a.stfd(ppc::f11, ppc::ptr(ppc::r1, -8)); // move integer result to a GPR
  a.ld(ppc::r3, ppc::ptr(ppc::r1, -8));

  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(5.5, 2.25) == -1; // trunc(-(a+b)(a-b) / (a*b+a))
}

static bool testExecutionFpCompare() {
  using Fn = int64_t (*)(double, double);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  Label less = a.new_label();
  Label done = a.new_label();
  a.prolog(frame_size);

  a.fcmpu(1, ppc::f1, ppc::f2);
  a.bc(12, 4, less); // CR1 LT
  a.li(ppc::r3, 0);
  a.b(done);
  a.bind(less);
  a.li(ppc::r3, 1);
  a.bind(done);

  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(1.5, 2.5) == 1 && fn(3.5, 2.5) == 0;
}

static bool testExecutionFpNonvolatile() {
  using Fn = double (*)(double, double);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size() + 144 + 48; // f14..f16 area + GPR area
  a.prolog(frame_size, 0, 0x7); // save f14, f15, f16

  a.fmr(ppc::f14, ppc::f1);
  a.fmr(ppc::f15, ppc::f2);

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  a.callDescriptor((uint64_t)func_as_ptr(&ppcTestFpHelper));
#else
  a.call((uint64_t)func_as_ptr(&ppcTestFpHelper));
#endif

  a.fadd(ppc::f1, ppc::f1, ppc::f14); // (a*3+b) + a
  a.fadd(ppc::f1, ppc::f1, ppc::f15); // + b

  a.epilog(frame_size, 0, 0x7);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  const double keep = 3.75; // kept live across the call in a callee-saved FPR
  const double r = fn(1.5, 2.25);
  const double expected = 4.0 * 1.5 + 2.0 * 2.25;
  return keep * r == keep * expected && r == expected;
}

static bool testExecutionVsx() {
  // exercises GPR<->VSX bit moves only, which qemu executes correctly.
  using Fn = int64_t (*)(int64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);

  a.mtvsrd(ppc::vs1, ppc::r3);
  a.mfvsrd(ppc::r3, ppc::vs1);

  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  if (fn(0x1122334455667788ll) != 0x1122334455667788ll) {
    return false;
  }

  // Word zero-extension round-trip.
  using Fn2 = int64_t (*)(int64_t);
  CodeHolder code2;
  if (code2.init(rt.environment()) != Error::kOk) {
    return false;
  }
  ppc::Assembler b(&code2);
  b.prolog(frame_size);
  b.mtvsrwz(ppc::vs1, ppc::r3);
  b.mfvsrwz(ppc::r3, ppc::vs1);
  b.epilog(frame_size);
  Fn2 fn2 = nullptr;
  if (rt.add(&fn2, &code2) != Error::kOk) {
    return false;
  }

  return fn2(0x1122334455667788ll) == 0x55667788ll;
}

static bool testExecutionVsxFp() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);
  a.mtvsrd(ppc::vs33, ppc::r3); // v1 = a
  a.mtvsrd(ppc::vs34, ppc::r4); // v2 = b
  a.xvadddp(ppc::vs35, ppc::vs33, ppc::vs34);
  a.xvmuldp(ppc::vs36, ppc::vs35, ppc::vs33);
  a.mfvsrd(ppc::r3, ppc::vs36);
  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  double va = 1.5, vb = 2.25;
  uint64_t ab, bb;
  std::memcpy(&ab, &va, 8);
  std::memcpy(&bb, &vb, 8);
  uint64_t got = fn(ab, bb);
  double result;
  std::memcpy(&result, &got, 8);
  return result == (va + vb) * va;
}

static bool testExecutionCallHelper() {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  return true; // ELFv1 calls are exercised through callDescriptor() elsewhere.
#else
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size();
  a.prolog(frame_size);
  a.callHelper((uint64_t)func_as_ptr(&ppcTestGccHelper));
  a.epilog(frame_size);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(5, 7) == 22;
#endif
}

static bool testExecutionTailCall() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  a.tailCallDescriptor((uint64_t)func_as_ptr(&ppcTestGccHelper));
#else
  a.tailCall((uint64_t)func_as_ptr(&ppcTestGccHelper));
#endif

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  return fn(5, 7) == 22;
}

static bool testExecutionBLong() {
  using Fn = uint64_t (*)(uint64_t);

  ppc::Runtime rt;
  CodeHolder codeB;
  if (codeB.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler b(&codeB);
  b.addi(ppc::r3, ppc::r3, 100);
  b.blr();

  Fn fnB = nullptr;
  if (rt.add(&fnB, &codeB) != Error::kOk) {
    return false;
  }

  // ELFv1 (BE) descriptors store the raw entry in word 0; on LE the function
  // pointer is the code address itself. The tail branch preserves LR, so the
  // simple helper below returns straight to the test caller.
  const uint64_t target =
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    *reinterpret_cast<const uint64_t*>(func_as_ptr(fnB));
#else
    (uint64_t)(uintptr_t)func_as_ptr(fnB);
#endif

  CodeHolder codeA;
  if (codeA.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&codeA);
  a.bLong(target);

  Fn fnA = nullptr;
  if (rt.add(&fnA, &codeA) != Error::kOk) {
    return false;
  }

  return fnA(5) == 105;
}

static bool testExecutionVmx() {
  using Fn = int64_t (*)(int64_t, int64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size() + 320 + 320 + 16; // v20 save area
  a.prolog(frame_size, 0, 0, 0x1); // save v20
  a.mtvsrd(ppc::vs33, ppc::r3); // v1 = a
  a.mtvsrd(ppc::vs34, ppc::r4); // v2 = b
  a.vaddudm(ppc::v3, ppc::v1, ppc::v2);
  a.mfvsrd(ppc::r3, ppc::vs35); // v3 low doubleword
  a.mtvsrd(ppc::vs52, ppc::r3); // clobber v20 with the result
  a.epilog(frame_size, 0, 0, 0x1);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  const uint64_t pattern = 0x1122334455667788ull;
  asm volatile("mtvsrd 52,%0" : : "r"(pattern));
  const int64_t r = fn(5, 7);
  uint64_t back = 0;
  asm volatile("mfvsrd %0,52" : "=r"(back));
  return r == 12 && back == pattern;
}

static bool testExecutionVmxExt() {
  using Fn = uint64_t (*)(uint64_t, uint64_t);

  ppc::Runtime rt;
  CodeHolder code;
  if (code.init(rt.environment()) != Error::kOk) {
    return false;
  }

  ppc::Assembler a(&code);
  const int32_t frame_size = a.minimum_frame_size() + 320 + 320 + 16; // v20 save area
  a.prolog(frame_size, 0, 0, 0x1); // save v20
  a.mtvsrdd(ppc::vs33, ppc::r3, ppc::r3); // v1 = {a, a}
  a.mtvsrdd(ppc::vs34, ppc::r4, ppc::r4); // v2 = {b, b}
  a.vaddsbs(ppc::v3, ppc::v1, ppc::v2); // 0x7f+0x7f saturates to 0x7f
  a.mfvsrd(ppc::r5, ppc::vs35);
  a.vaddubs(ppc::v3, ppc::v1, ppc::v2); // 0x7f+0x7f = 0xfe
  a.mfvsrd(ppc::r6, ppc::vs35);
  a.vavgub(ppc::v3, ppc::v1, ppc::v2); // avg(0x7f, 0x7f) = 0x7f
  a.mfvsrd(ppc::r7, ppc::vs35);
  a.vctzb(ppc::v3, ppc::v1); // ctz(0x7f) = 0
  a.mfvsrd(ppc::r8, ppc::vs35);
  a.xor_(ppc::r3, ppc::r5, ppc::r6);
  a.xor_(ppc::r3, ppc::r3, ppc::r7);
  a.xor_(ppc::r3, ppc::r3, ppc::r8);
  a.mtvsrd(ppc::vs52, ppc::r3); // clobber v20 with the result
  a.epilog(frame_size, 0, 0, 0x1);

  Fn fn = nullptr;
  if (rt.add(&fn, &code) != Error::kOk) {
    return false;
  }

  const uint64_t pattern = 0x1122334455667788ull;
  asm volatile("mtvsrd 52,%0" : : "r"(pattern));
  const uint64_t r = fn(0x7F7F7F7F7F7F7F7Full, 0x7F7F7F7F7F7F7F7Full);
  uint64_t back = 0;
  asm volatile("mfvsrd %0,52" : "=r"(back));
  return r == 0xFEFEFEFEFEFEFEFEull && back == pattern;
}
#endif

int main() {
  bool ok = true;
  ok &= testBasic();
  ok &= testCpuFeatures();
  ok &= testInstApiRWInfo();
  ok &= testEmit();
  ok &= testValidate();
  ok &= testBigEndian();
  ok &= testBigEndianBranches();
  ok &= testPrologEpilog();
  ok &= testPrologLarge();
  ok &= testPrologSaves();
  ok &= testPrologLargeSaves();
  ok &= testPrologValidation();
  ok &= testCallConv(Arch::kPPC64_LE);
  ok &= testCallConv(Arch::kPPC64_BE);
  ok &= testFuncDetail(Arch::kPPC64_LE);
  ok &= testFuncDetail(Arch::kPPC64_BE);
  ok &= testBranches();
  ok &= testBackwardBranch();
  ok &= testLoadImm64();
  ok &= testLoadImm64Fast();
  ok &= testCallHelpers();
  ok &= testMemory();
  ok &= testMemoryExt();
  ok &= testLlSc();
  ok &= testRotates();
  ok &= testIntegerMath();
  ok &= testMaskIdioms();
  ok &= testBarriersAndCounts();
  ok &= testIndexedMemory();
  ok &= testArithmeticExt();
  ok &= testCompareTrapsSelect();
  ok &= testLogicalShiftExt();
  ok &= testSystemRegsAndCr();
  ok &= testBranchExt();
  ok &= testFpMemory();
  ok &= testFpArithmetic();
  ok &= testFpConvertFpscr();
  ok &= testPrologFpSaves();
  ok &= testBailFpSpecial();
  ok &= testVsxMoves();
  ok &= testVsxArith();
  ok &= testVsxRoundLogical();
  ok &= testVsxFpArithmetic();
  ok &= testVsxMemExt();
  ok &= testVsxRegExt();
  ok &= testAlignEmbed();
  ok &= testCallHelperGolden();
  ok &= testBLongGolden();
  ok &= testVmxMemLogical();
  ok &= testVmxShiftCmpMinMax();
  ok &= testVmxPackPermute();
  ok &= testVmxSaturateAvgSum();
  ok &= testVmxQwordRotate();
  ok &= testVmxVaForm();
  ok &= testVmxExtractInsert();
  ok &= testPrologVrSaves();
#if ASMJIT_ARCH_PPC == 64
  ok &= testExecution();
  ok &= testExecutionGccHelper();
  ok &= testExecutionStep5();
  ok &= testExecutionLargeFrame();
  ok &= testExecutionNonvolatile();
  ok &= testExecutionFp();
  ok &= testExecutionFpCompare();
  ok &= testExecutionFpNonvolatile();
  ok &= testExecutionVsx();
  ok &= testExecutionVsxFp();
  ok &= testExecutionCallHelper();
  ok &= testExecutionTailCall();
  ok &= testExecutionBLong();
  ok &= testExecutionVmx();
  ok &= testExecutionVmxExt();
#endif

  if (!ok) {
    std::printf("ppc64 assembler tests FAILED\n");
    return 1;
  }

  std::printf("ppc64 assembler tests passed\n");
  return 0;
}
