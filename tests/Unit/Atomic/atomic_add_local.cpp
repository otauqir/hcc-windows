// RUN: %amp_device -D__GPU__ %s -m32 -emit-llvm -c -S -O3 -o %t.ll && mkdir -p %t
// RUN: %llc -march=c -o %t/kernel_.cl < %t.ll
// RUN: cat %opencl_math_dir/opencl_math.cl %t/kernel_.cl > %t/kernel.cl
// RUN: pushd %t && objcopy -B i386:x86-64 -I binary -O elf64-x86-64 kernel.cl %t/kernel.o && popd
// RUN: %cxxamp %link %t/kernel.o %s -o %t.out && %t.out
#include <amp.h>
#include <stdlib.h>
#include <iostream>

using namespace concurrency;

int main(void) {
  const int vecSize = 100;
  const int tile_size = 10;

  // Alloc & init input data
  extent<2> e_a(vecSize, vecSize);
  std::vector<int> va(vecSize * vecSize);
  for(unsigned i = 0; i < vecSize * vecSize; i++) {
    va[i] = 0;
  }
  array_view<int, 2> av_a(e_a, va); 

  extent<2> compute_domain(e_a);
  parallel_for_each(compute_domain.tile<tile_size, tile_size>(), [=] (tiled_index<tile_size, tile_size> tidx) restrict(amp) {
    index<2> localIdx = tidx.local;
    index<2> globalIdx = tidx.global;

    tile_static unsigned localA[tile_size][tile_size];
    localA[localIdx[0]][localIdx[1]] = 0;
    tidx.barrier.wait();

    for(int i = 0; i < tile_size; i++) {
      for(int j = 0; j < tile_size; j++) {
        atomic_fetch_add(&(localA[i][j]), 1);
      }
    }
  tidx.barrier.wait();
  av_a[globalIdx[0]][globalIdx[1]] = localA[localIdx[0]][localIdx[1]];
  });

  for(unsigned i = 0; i < vecSize; i++) {
    for(unsigned j = 0; j < vecSize; j++) {
      if(av_a(i, j) != tile_size * tile_size) {
        return 1;
      }
    }
  }

  return 0;
}