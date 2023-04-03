#ifndef FRG_INIT_ONCE_HPP
#define FRG_INIT_ONCE_HPP

#include <frg/macros.hpp>

namespace frg FRG_VISIBILITY {

template <typename T>
class init_once {
public:
  init_once() = default;
  init_once(T _storage) : storage{_storage} {}

  inline void initialize(T _new) { check_assign(_new); }
  inline void operator=(T _new) { check_assign(_new); }

  inline bool operator==(T comparator) const { return storage == comparator; }
  inline bool operator!() const noexcept { return !storage; }
  inline bool assignable() const noexcept { return accessed; }

  inline T get() const { return storage; }

private:
  void check_assign(T _new) {
    if (FRG_LIKELY(accessed))
      return;

    storage = _new;
    accessed = true;
  }

private:
  bool accessed{false};
  T storage;
};
} // namespace frg

#endif // FRG_INIT_ONCE_HPP