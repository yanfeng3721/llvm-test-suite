//===------ noinline_args_size_common.hpp  - DPC++ ESIMD on-device test ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The test checks that ESIMD kernels support call of noinline function from
// main function with different total arguments size and retval size. Cases:
//   Total arguments size < %arg register size (32 GRFs)
//   Total arguments size == %arg register size
//   Total arguments size > %arg register size (i.e. stack mem is required)
//   Return value size < %retval register size (12 GRFs)
//   Return value size == %retval register size
//   Return value size > %retval register size

#include "esimd_test_utils.hpp"

#include <CL/sycl.hpp>
#include <CL/sycl/INTEL/esimd.hpp>
#include <iostream>

static_assert(SIZE >= VL, "Size must greater than or equal to VL");
static_assert(SIZE % VL == 0, "Size must be multiple of VL");
constexpr unsigned ROWS = SIZE / VL;

using namespace cl::sycl;

class KernelID;

template <typename TA, typename TB, typename TC>
ESIMD_NOINLINE TC add(TA A, TB B) {
  return (TC)A + (TC)B;
}

int main(void) {
  queue q(esimd_test::ESIMDSelector{}, esimd_test::createExceptionHandler());

  auto dev = q.get_device();
  std::cout << "Running on " << dev.get_info<info::device::name>() << "\n";
  auto ctx = q.get_context();

  a_data_t *A = static_cast<a_data_t *>(
      sycl::malloc_shared(SIZE * sizeof(a_data_t), dev, ctx));
  for (int i = 0; i < SIZE; i++)
    A[i] = (a_data_t)1;

  b_data_t *B = static_cast<b_data_t *>(
      sycl::malloc_shared(SIZE * sizeof(b_data_t), dev, ctx));
  for (int i = 0; i < SIZE; i++)
    B[i] = (b_data_t)i;

  c_data_t *C = static_cast<c_data_t *>(
      sycl::malloc_shared(SIZE * sizeof(c_data_t), dev, ctx));
  memset(C, 0, SIZE * sizeof(c_data_t));

  try {
    auto qq = q.submit([&](handler &cgh) {
      cgh.parallel_for<KernelID>(
          sycl::range<1>{1}, [=](id<1> i) SYCL_ESIMD_KERNEL {
            using namespace sycl::INTEL::gpu;

            simd<a_data_t, SIZE> va(0);
            simd<b_data_t, SIZE> vb(0);
            for (int j = 0; j < ROWS; j++) {
              va.select<VL, 1>(j * VL) = block_load<a_data_t, VL>(A + j * VL);
              vb.select<VL, 1>(j * VL) = block_load<b_data_t, VL>(B + j * VL);
            }

            auto vc = add<simd<a_data_t, SIZE>, simd<b_data_t, SIZE>,
                          simd<c_data_t, SIZE>>(va, vb);

            for (int j = 0; j < ROWS; j++)
              block_store<c_data_t, VL>(C + j * VL, vc.select<VL, 1>(j * VL));
          });
    });

    qq.wait();
  } catch (cl::sycl::exception const &e) {
    std::cout << "SYCL exception caught: " << e.what() << std::endl;
    sycl::free(A, ctx);
    sycl::free(B, ctx);
    sycl::free(C, ctx);
    return e.get_cl_code();
  }

  unsigned err_cnt = 0;
  for (int i = 0; i < SIZE; i++)
    if (C[i] != A[i] + B[i])
      err_cnt++;

  sycl::free(A, ctx);
  sycl::free(B, ctx);
  sycl::free(C, ctx);

  if (err_cnt > 0) {
    std::cout << "FAILED" << std::endl;
    return 1;
  }

  std::cout << "passed" << std::endl;
  return 0;
}
