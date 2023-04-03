#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define assert_truth(x) assert(x)

enum FillMode : char {
  ZERO = 0,
  NONE = 1 // Don't fill
};

struct BuddyAllocationResult {
private:
  using Order = int;
  using AddressType = uint64_t *;

public:
  AddressType ptr{nullptr};
  Order order{0};
  int npages;

public:
  BuddyAllocationResult() = default;
  BuddyAllocationResult(const AddressType &block, Order ord, int num_pages)
      : ptr(block), order(ord), npages(num_pages) {}
  inline AddressType unpack() const { return ptr; }
};

class BuddyAllocator {
public:
  using Order = uint64_t;
  using AddressType = uint64_t *;

  Order max_order =
      0; // Represents the largest allocation and is determined at runtime.
  constexpr static Order min_order =
      9; // 4kib, this is the smallest allocation size and will never change.
  constexpr static Order largest_allowed_order =
      30; // 1GiB is the largest allocation an instance of this class may serve.
  constexpr static bool verbose{},
      sanity_checks{}; // sanity_checks ensures we don't go out-of-bounds on the
                       // freelist. Beware: These options will impact the
                       // performance of the allocator.

  BuddyAllocator() = default;
  BuddyAllocator(const BuddyAllocator &) {}
  BuddyAllocator(BuddyAllocator &&) {}
  ~BuddyAllocator() = default;

  BuddyAllocator &operator=(const BuddyAllocator &) { return *this; }

  BuddyAllocator &operator=(BuddyAllocator &&) { return *this; }

  void init(AddressType base, int target_order) {
    this->base = base;
    max_order = target_order - 3;

    // if constexpr (verbose)
    //   firefly::logLine << "min-order: " << firefly::fmt::dec << min_order
    //                    << ", max-order: " << max_order << '\n'
    //                    << firefly::fmt::endl;

    freelist.init();
    freelist.add(base, max_order - min_order);
  }

  auto alloc(uint64_t size, FillMode fill = FillMode::ZERO) {
    Order order = std::max(min_order, Order(log2(size >> 3)));

    if constexpr (sanity_checks) {
      if (order > max_order) {
        // if constexpr (verbose)
        //   firefly::logLine
        //       << "Requested order " << firefly::fmt::dec << order << "(" <<
        //       size
        //       << ")"
        //       << " is too large for this buddy instance | max-order is: "
        //       << max_order << '\n'
        //       << firefly::fmt::endl;

        return BuddyAllocationResult();
      }
    }

    // if constexpr (verbose)
    //   firefly::logLine << "Suitable order for allocation of size '"
    //                    << firefly::fmt::dec << size << "' is: " << order <<
    //                    '\n'
    //                    << firefly::fmt::endl;

    AddressType block = nullptr;
    Order ord = order;

    for (; ord <= max_order; ord++) {
      block = freelist.remove(ord - min_order);
      if (block != nullptr)
        break;
    }

    if (block == nullptr) {
      //   if constexpr (verbose)
      //     firefly::logLine << "Block is a nullptr (order: " <<
      //     firefly::fmt::dec
      //                      << order << " , size: " << size << ")\n"
      //                      << firefly::fmt::endl;

      //   lock.unlock();
      return BuddyAllocationResult();
    }

    // Split higher order blocks
    while (ord-- > order) {
      auto buddy = buddy_of(block, ord);
      freelist.add(buddy, ord - min_order);
    }

    // 'size' is not guaranteed to be a power of two. (Hence the manual pow2)
    const auto correct_size = (1 << (order + 3));

    if (fill != FillMode::NONE)
      memset(static_cast<void *>(block), fill, correct_size);

    // if constexpr (verbose)
    //   firefly::logLine << "Allocated " << firefly::fmt::hex << block
    //                    << " at order " << firefly::fmt::dec << ord
    //                    << "(max: " << max_order << " | min: " << min_order
    //                    << ")"
    //                    << " with a size of " << size << '\n'
    //                    << firefly::fmt::endl;

    // lock.unlock();
    return BuddyAllocationResult(block, ord + 1, correct_size / 4096);
  }

  void free(AddressType block, Order order) {
    // lock.lock();
    assert(order >= min_order && order <= max_order &&
           "Invalid order passed to free()");
    if (block == nullptr)
      return;

    // There are no buddies at max_order
    if (order == max_order) {
      freelist.add(block, max_order - min_order);
      return;
    }

    coalesce(block, order);
    // lock.unlock();
  }

  int log2(int size) {
    int result = 0;
    while ((1 << result) < size) {
      ++result;
    }
    return result;
  }

private:
  template <typename T, int orders> class Freelist {
  private:
    T list[orders + 1]{nullptr};

  public:
    void init() {
      for (int i = 0; i < orders + 1; i++)
        list[i] = nullptr;
    }

    void add(const T &block, Order order) {
      if constexpr (sanity_checks)
        if (order < min_order || order > largest_allowed_order)
          assert_truth(!"Order mismatch");

      if (block) {
        memset((void *)block, 0, 4096);
        *(T *)block = list[order];
      }
      list[order] = block;
    }

    T remove(Order order) {
      if constexpr (sanity_checks)
        if (order < min_order || order > largest_allowed_order)
          assert_truth(!"Order mismatch");

      T element = list[order];

      if (element == nullptr)
        return nullptr;

      list[order] = *(T *)list[order];
      list[order] = nullptr;
      return element;
    }

    T next(const T &block) const { return *reinterpret_cast<T *>(block); }

    bool find(const T &block, Order order) const {
      T element = list[order];
      while (element != nullptr && element != block) {
        element = next(element);
      }

      return element != nullptr;
    }
  };

  inline AddressType buddy_of(AddressType block, Order order) {
    return base + ((block - base) ^ (1 << order));
  }

  void coalesce(AddressType block, Order order) {
    // Description:
    // Try to merge 'block' and it's buddy into one larger block at 'order + 1'
    // If both block are free, remove them and insert the smaller of
    // the two blocks into the next highest order and repeat that process.
    if (order == max_order)
      return;

    AddressType buddy = buddy_of(block, order);
    if (buddy == nullptr)
      return;

    // Scan freelist for the buddy. (This isn't really efficient)
    // Could be optimized with a bitmap to track the state of each node.
    // The overhead of a bitmap would be minimal + the allocator itself has
    // little to no overhead so size complexity should not be of concern.
    bool is_buddy_free = freelist.find(buddy, order - min_order);

    // Buddy block is free, merge them together
    if (is_buddy_free) {
      freelist.remove(order);
      coalesce(
          std::min(block, buddy),
          order +
              1); // std::min ensures that the smaller block of memory is merged
                  // with a larger and not vice-versa (which wouldn't work)
    }

    // The buddy is not free and merging is not possible.
    // Therefore it is simply put back onto the freelist.
    freelist.add(block, order - min_order);
  }

private:
  Freelist<AddressType, largest_allowed_order - min_order> freelist;
  AddressType base{};
};
