#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

#include <iostream>
using namespace std::literals;

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity)
    {}

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за
        // последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T)))
                      : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при
    // помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(),
                                             size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(),
                                  other.size_,
                                  data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::move(other.size_))
    {}

    Vector(std::initializer_list<T> list)
        : data_(list.size())
        , size_(list.size())
    {
        std::copy(list.begin(), list.end(), data_.GetAddress());
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> ||
            !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_,
                                      new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_,
                                      new_data.GetAddress());
        }

        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);

        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size,
                           size_ - new_size);
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() +
                                                 size_,
                                                 new_size - size_);
        }
        size_ = new_size;
    }

    template <typename U>
    void PushBack(U&& value) {
        EmplaceBack(std::forward<U>(value));
/*
        if (Capacity() > size_) {
            new (data_.GetAddress() + size_) T(std::forward<U>(value));
            ++size_;
            return;
        }

        RawMemory<T> new_data(std::max(Capacity() * 2, size_ + 1));
        new (new_data.GetAddress() + size_) T(std::forward<U>(value));

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> ||
                !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_,
                    new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_,
                    new_data.GetAddress());
            }
        }
        catch (...) {
            std::destroy_n(new_data.GetAddress(), size_ + 1);
            throw;
        }

        std::destroy_n(data_.GetAddress(), size_);
        ++size_;
        data_.Swap(new_data);
*/
    }

    void PopBack() /* noexcept */ {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        size_ -= 1;
    }

    template <typename ... Types>
    T& EmplaceBack(Types&&... values) {
        if (Capacity() > size_) {
            new (data_.GetAddress() + size_) T(std::forward<Types>(values)...);
            ++size_;
            return data_[size_ - 1];
        }

        RawMemory<T> new_data(std::max(Capacity() * 2, size_ + 1));
        new (new_data.GetAddress() + size_) T(std::forward<Types>(values)...);

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> ||
                !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_,
                    new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_,
                    new_data.GetAddress());
            }
        }
        catch (...) {
            std::destroy_n(new_data.GetAddress(), size_ + 1);
            throw;
        }

        std::destroy_n(data_.GetAddress(), size_);
        ++size_;
        data_.Swap(new_data);

        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        auto offset = pos - begin();
        if (Capacity() > size_) {
            if (pos != end()) {
                T value_to_emplace(std::forward<Args>(args)...);

                std::uninitialized_move_n(std::next(begin(), size_ - 1),
                                          1, end());

                std::move_backward(std::next(begin(), offset),
                                   std::next(begin(), size_ - 1),
                                   std::next(begin(), size_));

                data_[offset] = std::move(value_to_emplace);
            }
            else {
                new (end()) T(std::forward<Args>(args)...);
            }
            ++size_;
            return std::next(begin(), offset);
        }

        RawMemory<T> new_data(std::max(Capacity() * 2, size_ + 1));
        new (new_data.GetAddress() + offset) T(std::forward<Args>(args)...);

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> ||
                !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), offset,
                                          new_data.GetAddress());
                std::uninitialized_move_n(std::next(begin(), offset),
                                          size_ - offset,
                                          new_data.GetAddress() + offset + 1);
            }
            else {
                std::uninitialized_copy_n(begin(), offset,
                                          new_data.GetAddress());
                std::uninitialized_copy_n(std::next(begin(), offset),
                                          size_ - offset,
                                          new_data.GetAddress() + offset + 1);
            }
        }
        catch (...) {
            std::destroy_n(new_data.GetAddress(), size_ + 1);
            throw;
        }

        std::destroy_n(data_.GetAddress(), size_);
        ++size_;
        data_.Swap(new_data);

        return std::next(begin(), offset);
    }

    // noexcept(std::is_nothrow_move_assignable_v<T>);
    iterator Erase(const_iterator pos) {
        auto offset = pos - begin();
        if (pos != end() - 1) {
            std::move(std::next(begin(), offset + 1), end(),
                std::next(begin(), offset));
        }
        std::destroy_n(std::next(begin(), size_ - 1), 1);
        --size_;
        return std::next(begin(), offset);
    }

/*
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
*/

    template <typename U>
    iterator Insert(const_iterator pos, U&& value) {
        return Emplace(pos, std::forward<U>(value));
    }


    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Swap(Vector& other) noexcept {
        if (data_.GetAddress() != other.data_.GetAddress()) {
            data_.Swap(other.data_);
            std::swap(size_, other.size_);
        }
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (rhs.size_ > size_) {
                    std::copy(rhs.data_.GetAddress(),
                              rhs.data_.GetAddress() + size_,
                              data_.GetAddress());

                    std::uninitialized_copy_n(rhs.data_.GetAddress() +
                                              size_, rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(),
                              rhs.data_.GetAddress() + rhs.size_,
                              data_.GetAddress());

                    if (size_ > rhs.size_) {
                        std::destroy_n(data_.GetAddress() + rhs.size_,
                                       size_ - rhs.size_);
                    }
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::move(rhs.size_);
        }
        return *this;
    }

    ~Vector() {
        if (data_.GetAddress() != nullptr) {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};