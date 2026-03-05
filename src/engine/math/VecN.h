#pragma once

#include <cstddef>
#include <cassert>

namespace engine::math {

// CRTP base class for vector operations
// Derived classes must provide:
//   - static constexpr size_t Size
//   - float data[Size] (or equivalent contiguous storage)
//   - float* data_ptr() and const float* data_ptr() const
/// This eliminates operator duplication across Vec2, Vec3, Vec4
template<typename Derived, size_t N>
struct VecOps {
    static constexpr size_t Size = N;

    float& operator[](size_t i) {
        assert(i < N && "VecN index out of bounds");
        return static_cast<Derived*>(this)->data_ptr()[i];
    }

    float operator[](size_t i) const {
        assert(i < N && "VecN index out of bounds");
        return static_cast<const Derived*>(this)->data_ptr()[i];
    }

    Derived operator+(const Derived& o) const {
        Derived result;
        const float* self_data = static_cast<const Derived*>(this)->data_ptr();
        const float* other_data = o.data_ptr();
        float* result_data = result.data_ptr();
        for (size_t i = 0; i < N; ++i) {
            result_data[i] = self_data[i] + other_data[i];
        }
        return result;
    }

    Derived operator-(const Derived& o) const {
        Derived result;
        const float* self_data = static_cast<const Derived*>(this)->data_ptr();
        const float* other_data = o.data_ptr();
        float* result_data = result.data_ptr();
        for (size_t i = 0; i < N; ++i) {
            result_data[i] = self_data[i] - other_data[i];
        }
        return result;
    }

    Derived operator*(float s) const {
        Derived result;
        const float* self_data = static_cast<const Derived*>(this)->data_ptr();
        float* result_data = result.data_ptr();
        for (size_t i = 0; i < N; ++i) {
            result_data[i] = self_data[i] * s;
        }
        return result;
    }

    bool operator==(const Derived& o) const {
        const float* self_data = static_cast<const Derived*>(this)->data_ptr();
        const float* other_data = o.data_ptr();
        for (size_t i = 0; i < N; ++i) {
            if (self_data[i] != other_data[i]) return false;
        }
        return true;
    }

    bool operator!=(const Derived& o) const {
        return !(*this == o);
    }
};

}