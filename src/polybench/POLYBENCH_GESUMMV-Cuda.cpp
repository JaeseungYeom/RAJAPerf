//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-23, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "POLYBENCH_GESUMMV.hpp"

#include "RAJA/RAJA.hpp"

#if defined(RAJA_ENABLE_CUDA)

#include "common/CudaDataUtils.hpp"

#include <iostream>

namespace rajaperf
{
namespace polybench
{

template < size_t block_size >
__launch_bounds__(block_size)
__global__ void poly_gesummv(Real_ptr x, Real_ptr y,
                             Real_ptr A, Real_ptr B,
                             Real_type alpha, Real_type beta,
                             Index_type N)
{
   Index_type i = blockIdx.x * block_size + threadIdx.x;

   if (i < N) {
     POLYBENCH_GESUMMV_BODY1;
     for (Index_type j = 0; j < N; ++j ) {
       POLYBENCH_GESUMMV_BODY2;
     }
     POLYBENCH_GESUMMV_BODY3;
   }
}


template < size_t block_size >
void POLYBENCH_GESUMMV::runCudaVariantImpl(VariantID vid)
{
  const Index_type run_reps = getRunReps();

  POLYBENCH_GESUMMV_DATA_SETUP;

  if ( vid == Base_CUDA ) {

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

      const size_t grid_size = RAJA_DIVIDE_CEILING_INT(N, block_size);

      poly_gesummv<block_size><<<grid_size, block_size>>>(x, y,
                                              A, B,
                                              alpha, beta,
                                              N);
      cudaErrchk( cudaGetLastError() );

    }
    stopTimer();

  } else if (vid == RAJA_CUDA) {

    POLYBENCH_GESUMMV_VIEWS_RAJA;

    using EXEC_POL =
      RAJA::KernelPolicy<
        RAJA::statement::CudaKernelFixedAsync<block_size,
          RAJA::statement::Tile<0, RAJA::tile_fixed<block_size>,
                                   RAJA::cuda_block_x_direct,
            RAJA::statement::For<0, RAJA::cuda_thread_x_direct,  // i
              RAJA::statement::Lambda<0, RAJA::Params<0,1>>,
              RAJA::statement::For<1, RAJA::seq_exec,            // j
                RAJA::statement::Lambda<1, RAJA::Segs<0,1>, RAJA::Params<0,1>>
              >,
              RAJA::statement::Lambda<2, RAJA::Segs<0>, RAJA::Params<0,1>>
            >
          >
        >
      >;

      startTimer();
      for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

        RAJA::kernel_param<EXEC_POL>(
          RAJA::make_tuple( RAJA::RangeSegment{0, N},
                            RAJA::RangeSegment{0, N} ),
          RAJA::make_tuple(static_cast<Real_type>(0.0),
                           static_cast<Real_type>(0.0)),

          [=] __device__ (Real_type& tmpdot,
                          Real_type& ydot) {
            POLYBENCH_GESUMMV_BODY1_RAJA;
          },
          [=] __device__ (Index_type i, Index_type j, Real_type& tmpdot,
                                                      Real_type& ydot) {
            POLYBENCH_GESUMMV_BODY2_RAJA;
          },
          [=] __device__ (Index_type i, Real_type& tmpdot,
                                        Real_type& ydot) {
            POLYBENCH_GESUMMV_BODY3_RAJA;
          }
        );

      }
      stopTimer();

  } else {
      getCout() << "\n  POLYBENCH_GESUMMV : Unknown Cuda variant id = " << vid << std::endl;
  }
}

RAJAPERF_GPU_BLOCK_SIZE_TUNING_DEFINE_BIOLERPLATE(POLYBENCH_GESUMMV, Cuda)

} // end namespace polybench
} // end namespace rajaperf

#endif  // RAJA_ENABLE_CUDA

