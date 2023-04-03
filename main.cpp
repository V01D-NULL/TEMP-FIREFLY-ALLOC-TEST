#include "buddy.hpp"
#include "slab.hpp"
#include <iostream>
#include <stdlib.h>

BuddyAllocator vm_buddy;

struct VmBackingAllocator {
  VirtualAddress allocate(int size) {
    auto ptr = vm_buddy.alloc(size).unpack();

    if (ptr == nullptr)
      exit(0);
    //   panic("vm_buddy returned a null-pointer, heap is OOM.");

    return VirtualAddress(ptr);
  }
};

struct LockingMechanism {
  void lock() {}
  void unlock() {}
};

using cacheType = slabCache<VmBackingAllocator, LockingMechanism>;
frg::array<cacheType, 9> kernelAllocatorCaches = {};

int main(void) {
  auto mem_base = malloc(1024 * 1024 * 1024);
  vm_buddy.init(reinterpret_cast<uint64_t *>(mem_base),
                BuddyAllocator::largest_allowed_order);

  kernelAllocatorCaches[0].initialize(8, "heap");
  kernelAllocatorCaches[1].initialize(32, "heap");
  kernelAllocatorCaches[2].initialize(64, "heap");
  kernelAllocatorCaches[3].initialize(128, "heap");
  kernelAllocatorCaches[4].initialize(256, "heap");
  kernelAllocatorCaches[5].initialize(512, "heap");
  kernelAllocatorCaches[6].initialize(1024, "heap");
  kernelAllocatorCaches[7].initialize(2048, "heap");
  kernelAllocatorCaches[8].initialize(4096, "heap");
  //   for (size_t i = 0, j = 3; i < kernelAllocatorCaches.size(); i++, j++)
  //     kernelAllocatorCaches[i].initialize(1 << j, "heap");

  const int sz = 0;
  std::cout << kernelAllocatorCaches[sz].allocate() << std::endl;
  std::cout << kernelAllocatorCaches[sz].allocate() << std::endl;
  std::cout << kernelAllocatorCaches[sz].allocate() << std::endl;
  std::cout << kernelAllocatorCaches[sz].allocate() << std::endl;
  std::cout << kernelAllocatorCaches[sz].allocate() << std::endl;
}