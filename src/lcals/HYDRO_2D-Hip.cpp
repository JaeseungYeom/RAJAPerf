//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2017-23, Lawrence Livermore National Security, LLC
// and RAJA Performance Suite project contributors.
// See the RAJAPerf/LICENSE file for details.
//
// SPDX-License-Identifier: (BSD-3-Clause)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "HYDRO_2D.hpp"

#include "RAJA/RAJA.hpp"

#if defined(RAJA_ENABLE_HIP)

#include "common/HipDataUtils.hpp"

#include <iostream>

namespace rajaperf
{
namespace lcals
{

  //
  // Define thread block shape for Hip execution
  //
#define j_block_sz (32)
#define k_block_sz (block_size / j_block_sz)

#define HYDRO_2D_THREADS_PER_BLOCK_TEMPLATE_PARAMS_HIP \
  j_block_sz, k_block_sz

#define HYDRO_2D_THREADS_PER_BLOCK_HIP \
  dim3 nthreads_per_block(HYDRO_2D_THREADS_PER_BLOCK_TEMPLATE_PARAMS_HIP, 1); \
  static_assert(j_block_sz*k_block_sz == block_size, "Invalid block_size");

#define HYDRO_2D_NBLOCKS_HIP \
  dim3 nblocks(static_cast<size_t>(RAJA_DIVIDE_CEILING_INT(jn-2, j_block_sz)), \
               static_cast<size_t>(RAJA_DIVIDE_CEILING_INT(kn-2, k_block_sz)), \
               static_cast<size_t>(1));


template < size_t j_block_size, size_t k_block_size >
__launch_bounds__(j_block_size*k_block_size)
__global__ void hydro_2d1(Real_ptr zadat, Real_ptr zbdat,
                          Real_ptr zpdat, Real_ptr zqdat,
                          Real_ptr zrdat, Real_ptr zmdat,
                          Index_type jn, Index_type kn)
{
   Index_type k = 1 + blockIdx.y * k_block_size + threadIdx.y;
   Index_type j = 1 + blockIdx.x * j_block_size + threadIdx.x;

   if (k < kn-1 && j < jn-1) {
     HYDRO_2D_BODY1;
   }
}

template < size_t j_block_size, size_t k_block_size >
__launch_bounds__(j_block_size*k_block_size)
__global__ void hydro_2d2(Real_ptr zudat, Real_ptr zvdat,
                          Real_ptr zadat, Real_ptr zbdat,
                          Real_ptr zzdat, Real_ptr zrdat,
                          Real_type s,
                          Index_type jn, Index_type kn)
{
   Index_type k = 1 + blockIdx.y * k_block_size + threadIdx.y;
   Index_type j = 1 + blockIdx.x * j_block_size + threadIdx.x;

   if (k < kn-1 && j < jn-1) {
     HYDRO_2D_BODY2;
   }
}

template < size_t j_block_size, size_t k_block_size >
__launch_bounds__(j_block_size*k_block_size)
__global__ void hydro_2d3(Real_ptr zroutdat, Real_ptr zzoutdat,
                          Real_ptr zrdat, Real_ptr zudat,
                          Real_ptr zzdat, Real_ptr zvdat,
                          Real_type t,
                          Index_type jn, Index_type kn)
{
   Index_type k = 1 + blockIdx.y * k_block_size + threadIdx.y;
   Index_type j = 1 + blockIdx.x * j_block_size + threadIdx.x;

   if (k < kn-1 && j < jn-1) {
     HYDRO_2D_BODY3;
   }
}


template < size_t block_size >
void HYDRO_2D::runHipVariantImpl(VariantID vid)
{
  const Index_type run_reps = getRunReps();
  const Index_type kbeg = 1;
  const Index_type kend = m_kn - 1;
  const Index_type jbeg = 1;
  const Index_type jend = m_jn - 1;

  HYDRO_2D_DATA_SETUP;

  if ( vid == Base_HIP ) {

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

      HYDRO_2D_THREADS_PER_BLOCK_HIP;
      HYDRO_2D_NBLOCKS_HIP;

      hipLaunchKernelGGL((hydro_2d1<HYDRO_2D_THREADS_PER_BLOCK_TEMPLATE_PARAMS_HIP>),
                         dim3(nblocks), dim3(nthreads_per_block), 0, 0,
                         zadat, zbdat,
                         zpdat, zqdat, zrdat, zmdat,
                         jn, kn);
       hipErrchk( hipGetLastError() );

       hipLaunchKernelGGL((hydro_2d2<HYDRO_2D_THREADS_PER_BLOCK_TEMPLATE_PARAMS_HIP>),
                          dim3(nblocks), dim3(nthreads_per_block), 0, 0,
                          zudat, zvdat,
                          zadat, zbdat, zzdat, zrdat,
                          s,
                          jn, kn);
       hipErrchk( hipGetLastError() );

       hipLaunchKernelGGL((hydro_2d3<HYDRO_2D_THREADS_PER_BLOCK_TEMPLATE_PARAMS_HIP>),
                          dim3(nblocks), dim3(nthreads_per_block), 0, 0,
                          zroutdat, zzoutdat,
                          zrdat, zudat, zzdat, zvdat,
                          t,
                          jn, kn);
       hipErrchk( hipGetLastError() );

    }
    stopTimer();

  } else if ( vid == RAJA_HIP ) {

    HYDRO_2D_VIEWS_RAJA;

    using EXECPOL =
      RAJA::KernelPolicy<
        RAJA::statement::HipKernelFixedAsync<j_block_sz * k_block_sz,
          RAJA::statement::Tile<0, RAJA::tile_fixed<k_block_sz>,
                                   RAJA::hip_block_y_direct,
            RAJA::statement::Tile<1, RAJA::tile_fixed<j_block_sz>,
                                   RAJA::hip_block_x_direct,
              RAJA::statement::For<0, RAJA::hip_thread_y_direct,   // k
                RAJA::statement::For<1, RAJA::hip_thread_x_direct, // j
                  RAJA::statement::Lambda<0>
                >
              >
            >
          >
        >
      >;

    startTimer();
    for (RepIndex_type irep = 0; irep < run_reps; ++irep) {

      RAJA::kernel<EXECPOL>(
        RAJA::make_tuple( RAJA::RangeSegment(kbeg, kend),
                          RAJA::RangeSegment(jbeg, jend)),
        [=] __device__ (Index_type k, Index_type j) {
        HYDRO_2D_BODY1_RAJA;
      });

      RAJA::kernel<EXECPOL>(
        RAJA::make_tuple( RAJA::RangeSegment(kbeg, kend),
                          RAJA::RangeSegment(jbeg, jend)),
        [=] __device__ (Index_type k, Index_type j) {
        HYDRO_2D_BODY2_RAJA;
      });

      RAJA::kernel<EXECPOL>(
        RAJA::make_tuple( RAJA::RangeSegment(kbeg, kend),
                          RAJA::RangeSegment(jbeg, jend)),
        [=] __device__ (Index_type k, Index_type j) {
        HYDRO_2D_BODY3_RAJA;
      });

    }
    stopTimer();

  } else {
     getCout() << "\n  HYDRO_2D : Unknown Hip variant id = " << vid << std::endl;
  }
}

RAJAPERF_GPU_BLOCK_SIZE_TUNING_DEFINE_BIOLERPLATE(HYDRO_2D, Hip)

} // end namespace lcals
} // end namespace rajaperf

#endif  // RAJA_ENABLE_HIP
