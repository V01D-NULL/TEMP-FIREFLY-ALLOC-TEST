#pragma once

#include <cstdint>
#include <frg/macros.hpp>
#include <sys/types.h> // ssize_t

namespace frg FRG_VISIBILITY {

enum class queue_result : uint8_t { Fail, Okay };

template <typename T> class intrusive_queue {
private:
  using value_type = T;
  using pointer = value_type *;
  size_t _size{};

  // [back] ||||||||| [front]
  pointer _back{nullptr}, _front{nullptr};

public:
  struct hook {
    pointer next{nullptr};
  };

  queue_result enqueue(T *item) {
    if (FRG_UNLIKELY(item == nullptr))
      return queue_result::Fail;

    if (_back == nullptr)
      _front = _back = item;

    _back->next = item;
    _back = item;
    _size++;

    return queue_result::Okay;
  }

  queue_result enqueue_head(T *item) {
    if (FRG_UNLIKELY(item == nullptr))
      return queue_result::Fail;

    if (_back == nullptr)
      _front = _back = item;

    auto &old_front = _front;
    _front = item;
    _front->next = old_front->next;

    _back = item;
    _size++;

    return queue_result::Okay;
  }

  pointer dequeue() {
    if (empty())
      return nullptr;

    auto node = _front;
    _front = _front->next;

    if (!_front)
      _back = nullptr;

    _size--;
    return node;
  }

  inline size_t size() const { return _size; }
  inline bool empty() const { return _front == nullptr; }
  inline T *front() const { return _front; }
  inline T *back() const { return _back; }
};

template <typename T> using default_queue_hook = intrusive_queue<T>::hook;

} // namespace FRG_VISIBILITY