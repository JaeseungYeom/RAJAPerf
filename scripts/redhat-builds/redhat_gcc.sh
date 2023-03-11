#!/usr/bin/env bash

###############################################################################
# Copyright (c) 2017-23, Lawrence Livermore National Security, LLC
# and RAJA project contributors. See the RAJAPerf/LICENSE file for details.
#
# SPDX-License-Identifier: (BSD-3-Clause)
###############################################################################

COMP_VER=10
shift 1

BUILD_SUFFIX=redhat-gcc-${COMP_VER}
RAJA_HOSTCONFIG=../tpl/RAJA/host-configs/ubuntu-builds/gcc_X.cmake

echo
echo "Creating build directory ${BUILD_SUFFIX} and generating configuration in it"
echo

rm -rf build_${BUILD_SUFFIX} 2>/dev/null
mkdir build_${BUILD_SUFFIX} && cd build_${BUILD_SUFFIX}

cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/gcc${COMP_VER}-gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/gcc${COMP_VER}-g++ \
  -C ${RAJA_HOSTCONFIG} \
  -DENABLE_OPENMP=On \
  -DCMAKE_INSTALL_PREFIX=../install_${BUILD_SUFFIX} \
  "$@" \
  ..

echo
echo "***********************************************************************"
echo "cd into directory ${BUILD_SUFFIX} and run make to build RAJA Perf Suite"
echo "***********************************************************************"
