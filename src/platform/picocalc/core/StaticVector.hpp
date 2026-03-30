#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace ol {

template <typename T, size_t Capacity>
class StaticVector {
public:
    using value_type = T;
    using size_type = size_t;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr StaticVector() = default;

    constexpr size_type size() const { return size_; }
    static constexpr size_type capacity() { return Capacity; }
    constexpr bool empty() const { return size_ == 0; }
    constexpr bool full() const { return size_ == Capacity; }

    constexpr void clear() { size_ = 0; }

    constexpr T* data() { return data_.data(); }
    constexpr const T* data() const { return data_.data(); }

    constexpr T& operator[](size_type i) { return data_[i]; }
    constexpr const T& operator[](size_type i) const { return data_[i]; }

    constexpr T& front() { return data_[0]; }
    constexpr const T& front() const { return data_[0]; }

    constexpr T& back() { return data_[size_ - 1]; }
    constexpr const T& back() const { return data_[size_ - 1]; }

    constexpr iterator begin() { return data_.data(); }
    constexpr const_iterator begin() const { return data_.data(); }
    constexpr const_iterator cbegin() const { return data_.data(); }

    constexpr iterator end() { return data_.data() + size_; }
    constexpr const_iterator end() const { return data_.data() + size_; }
    constexpr const_iterator cend() const { return data_.data() + size_; }

    constexpr bool push_back(const T& value) {
        if (size_ >= Capacity) return false;
        data_[size_++] = value;
        return true;
    }

    constexpr bool push_back(T&& value) {
        if (size_ >= Capacity) return false;
        data_[size_++] = std::move(value);
        return true;
    }

    template <typename... Args>
    constexpr bool emplace_back(Args&&... args) {
        if (size_ >= Capacity) return false;
        data_[size_++] = T{std::forward<Args>(args)...};
        return true;
    }

    constexpr void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

private:
    std::array<T, Capacity> data_{};
    size_type size_ = 0;
};

} // namespace ol