#pragma once

#include <array>
#include <cmath>

#include "engine/core/types.hpp"

namespace gameengine::math {

struct Vec2 final {
    core::f32 x = 0.0F;
    core::f32 y = 0.0F;
};

struct Vec3 final {
    core::f32 x = 0.0F;
    core::f32 y = 0.0F;
    core::f32 z = 0.0F;
};

struct Mat4 final {
    std::array<core::f32, 16> values{};

    [[nodiscard]] constexpr core::f32& at(core::u32 row, core::u32 column) noexcept
    {
        return values[static_cast<core::usize>(column) * 4U + row];
    }

    [[nodiscard]] constexpr core::f32 at(core::u32 row, core::u32 column) const noexcept
    {
        return values[static_cast<core::usize>(column) * 4U + row];
    }

    [[nodiscard]] static constexpr Mat4 identity() noexcept
    {
        Mat4 result{};
        result.at(0, 0) = 1.0F;
        result.at(1, 1) = 1.0F;
        result.at(2, 2) = 1.0F;
        result.at(3, 3) = 1.0F;
        return result;
    }
};

static_assert(sizeof(Mat4) == sizeof(core::f32) * 16U);

[[nodiscard]] constexpr Mat4 multiply(const Mat4& left, const Mat4& right) noexcept
{
    Mat4 result{};
    for (core::u32 column = 0; column < 4U; ++column) {
        for (core::u32 row = 0; row < 4U; ++row) {
            for (core::u32 index = 0; index < 4U; ++index) {
                result.at(row, column) += left.at(row, index) * right.at(index, column);
            }
        }
    }
    return result;
}

[[nodiscard]] constexpr Vec3 subtract(const Vec3& left, const Vec3& right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] constexpr core::f32 dot(const Vec3& left, const Vec3& right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr Vec3 cross(const Vec3& left, const Vec3& right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

[[nodiscard]] inline Vec3 normalize(const Vec3& value) noexcept
{
    const core::f32 length_squared = dot(value, value);
    if (length_squared <= 0.0F) {
        return {};
    }
    const core::f32 inverse_length = 1.0F / std::sqrt(length_squared);
    return {value.x * inverse_length, value.y * inverse_length, value.z * inverse_length};
}

[[nodiscard]] inline Mat4 rotation_x(core::f32 radians) noexcept
{
    const core::f32 sine = std::sin(radians);
    const core::f32 cosine = std::cos(radians);
    Mat4 result = Mat4::identity();
    result.at(1, 1) = cosine;
    result.at(2, 1) = sine;
    result.at(1, 2) = -sine;
    result.at(2, 2) = cosine;
    return result;
}

[[nodiscard]] inline Mat4 rotation_y(core::f32 radians) noexcept
{
    const core::f32 sine = std::sin(radians);
    const core::f32 cosine = std::cos(radians);
    Mat4 result = Mat4::identity();
    result.at(0, 0) = cosine;
    result.at(2, 0) = -sine;
    result.at(0, 2) = sine;
    result.at(2, 2) = cosine;
    return result;
}

[[nodiscard]] inline Mat4 look_at_rh(const Vec3& eye,
                                     const Vec3& target,
                                     const Vec3& up) noexcept
{
    const Vec3 forward = normalize(subtract(target, eye));
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 corrected_up = cross(side, forward);

    Mat4 result = Mat4::identity();
    result.at(0, 0) = side.x;
    result.at(1, 0) = corrected_up.x;
    result.at(2, 0) = -forward.x;
    result.at(0, 1) = side.y;
    result.at(1, 1) = corrected_up.y;
    result.at(2, 1) = -forward.y;
    result.at(0, 2) = side.z;
    result.at(1, 2) = corrected_up.z;
    result.at(2, 2) = -forward.z;
    result.at(3, 0) = -dot(side, eye);
    result.at(3, 1) = -dot(corrected_up, eye);
    result.at(3, 2) = dot(forward, eye);
    return result;
}

[[nodiscard]] inline Mat4 perspective_rh_zo(core::f32 vertical_field_of_view_radians,
                                            core::f32 aspect_ratio,
                                            core::f32 near_plane,
                                            core::f32 far_plane) noexcept
{
    Mat4 result{};
    const core::f32 focal_length =
        1.0F / std::tan(vertical_field_of_view_radians * 0.5F);
    result.at(0, 0) = focal_length / aspect_ratio;
    result.at(1, 1) = -focal_length;
    result.at(2, 2) = far_plane / (near_plane - far_plane);
    result.at(3, 2) = (near_plane * far_plane) / (near_plane - far_plane);
    result.at(2, 3) = -1.0F;
    return result;
}

} // namespace gameengine::math
