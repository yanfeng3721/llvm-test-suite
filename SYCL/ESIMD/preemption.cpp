//==---------------- preemption.cpp  - DPC++ ESIMD on-device test ----------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// REQUIRES: gpu && linux
// UNSUPPORTED: cuda || hip
// TODO: remove XFAIL once GPU RT starts using "enablePreemption" by default.
// RUN: %clangxx -fsycl %s -o %t.out
// RUN: %GPU_RUN_PLACEHOLDER IGC_DumpToCustomDir=%t.dump IGC_ShaderDumpEnable=1 %t.out
// RUN: grep enablePreemption %t.dump/*.asm

// The test expects to see "enablePreemption" switch in the compilation
// switches. It fails if does not find it.

#include "esimd_test_utils.hpp"

#include <CL/sycl.hpp>
#include <iostream>
#include <sycl/ext/intel/experimental/esimd.hpp>

using namespace cl::sycl;

int main() {
  queue q;
  q.submit([&](handler &cgh) {
    cgh.parallel_for<class Test>(sycl::range<1>{1},
                                 [=](id<1> id) SYCL_ESIMD_KERNEL {});
  });
  return 0;
}
