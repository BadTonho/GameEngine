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

struct Quaternion final {
    core::f32 x = 0.0F;
    core::f32 y = 0.0F;
    core::f32 z = 0.0F;
    core::f32 w = 1.0F;

    [[nodiscard]] static constexpr Quaternion identity() noexcept { return {}; }
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
static_assert(sizeof(Quaternion) == sizeof(core::f32) * 4U);

struct Plane final {
    Vec3 normal{};
    core::f32 distance = 0.0F;
};

struct Frustum final {
    std::array<Plane, 6> planes{};
    bool valid = false;
};

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

[[nodiscard]] constexpr Vec3 add(const Vec3& left, const Vec3& right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] constexpr Vec3 multiply(const Vec3& value, core::f32 scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] inline bool is_finite(const Vec3& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] inline bool is_finite(const Mat4& value) noexcept
{
    for (const core::f32 component : value.values) {
        if (!std::isfinite(component)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr core::f32 dot(const Vec3& left, const Vec3& right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline Vec3 transform_point(const Mat4& matrix, const Vec3& point) noexcept
{
    const core::f32 x = matrix.at(0, 0) * point.x + matrix.at(0, 1) * point.y +
                        matrix.at(0, 2) * point.z + matrix.at(0, 3);
    const core::f32 y = matrix.at(1, 0) * point.x + matrix.at(1, 1) * point.y +
                        matrix.at(1, 2) * point.z + matrix.at(1, 3);
    const core::f32 z = matrix.at(2, 0) * point.x + matrix.at(2, 1) * point.y +
                        matrix.at(2, 2) * point.z + matrix.at(2, 3);
    const core::f32 w = matrix.at(3, 0) * point.x + matrix.at(3, 1) * point.y +
                        matrix.at(3, 2) * point.z + matrix.at(3, 3);
    if (std::fabs(w) <= 0.000001F || !std::isfinite(w)) {
        return {x, y, z};
    }
    return {x / w, y / w, z / w};
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

[[nodiscard]] inline Quaternion normalize(const Quaternion& value) noexcept
{
    const core::f32 length_squared = value.x * value.x + value.y * value.y + value.z * value.z +
                                     value.w * value.w;
    if (length_squared <= 0.0F) {
        return Quaternion::identity();
    }
    const core::f32 inverse_length = 1.0F / std::sqrt(length_squared);
    return {value.x * inverse_length,
            value.y * inverse_length,
            value.z * inverse_length,
            value.w * inverse_length};
}

[[nodiscard]] constexpr Quaternion multiply(const Quaternion& left,
                                            const Quaternion& right) noexcept
{
    return {
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
    };
}

[[nodiscard]] inline Quaternion quaternion_from_axis_angle(const Vec3& axis,
                                                           core::f32 radians) noexcept
{
    const Vec3 normalized_axis = normalize(axis);
    const core::f32 half_angle = radians * 0.5F;
    const core::f32 sine = std::sin(half_angle);
    return normalize({normalized_axis.x * sine,
                      normalized_axis.y * sine,
                      normalized_axis.z * sine,
                      std::cos(half_angle)});
}

[[nodiscard]] constexpr Mat4 translation(const Vec3& value) noexcept
{
    Mat4 result = Mat4::identity();
    result.at(0, 3) = value.x;
    result.at(1, 3) = value.y;
    result.at(2, 3) = value.z;
    return result;
}

[[nodiscard]] constexpr Mat4 scale_matrix(const Vec3& value) noexcept
{
    Mat4 result = Mat4::identity();
    result.at(0, 0) = value.x;
    result.at(1, 1) = value.y;
    result.at(2, 2) = value.z;
    return result;
}

[[nodiscard]] constexpr Mat4 rotation(const Quaternion& input) noexcept
{
    const Quaternion value = input;
    const core::f32 xx = value.x * value.x;
    const core::f32 yy = value.y * value.y;
    const core::f32 zz = value.z * value.z;
    const core::f32 xy = value.x * value.y;
    const core::f32 xz = value.x * value.z;
    const core::f32 yz = value.y * value.z;
    const core::f32 wx = value.w * value.x;
    const core::f32 wy = value.w * value.y;
    const core::f32 wz = value.w * value.z;

    Mat4 result = Mat4::identity();
    result.at(0, 0) = 1.0F - 2.0F * (yy + zz);
    result.at(1, 0) = 2.0F * (xy + wz);
    result.at(2, 0) = 2.0F * (xz - wy);
    result.at(0, 1) = 2.0F * (xy - wz);
    result.at(1, 1) = 1.0F - 2.0F * (xx + zz);
    result.at(2, 1) = 2.0F * (yz + wx);
    result.at(0, 2) = 2.0F * (xz + wy);
    result.at(1, 2) = 2.0F * (yz - wx);
    result.at(2, 2) = 1.0F - 2.0F * (xx + yy);
    return result;
}

[[nodiscard]] constexpr Mat4 compose_transform(const Vec3& position,
                                               const Quaternion& rotation_value,
                                               const Vec3& scale_value) noexcept
{
    return multiply(translation(position),
                    multiply(rotation(rotation_value), scale_matrix(scale_value)));
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
    result.at(0, 3) = -dot(side, eye);
    result.at(1, 3) = -dot(corrected_up, eye);
    result.at(2, 3) = dot(forward, eye);
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
    result.at(2, 3) = (near_plane * far_plane) / (near_plane - far_plane);
    result.at(3, 2) = -1.0F;
    return result;
}

[[nodiscard]] inline bool extract_frustum_rh_zo(const Mat4& view_projection,
                                                Frustum& result) noexcept
{
    result = {};
    if (!is_finite(view_projection)) {
        return false;
    }

    const auto make_plane = [](core::f32 a,
                               core::f32 b,
                               core::f32 c,
                               core::f32 d,
                               Plane& plane) noexcept {
        const core::f32 length_squared = a * a + b * b + c * c;
        if (!std::isfinite(length_squared) || length_squared <= 0.00000001F) {
            return false;
        }
        const core::f32 inverse_length = 1.0F / std::sqrt(length_squared);
        plane.normal = {a * inverse_length, b * inverse_length, c * inverse_length};
        plane.distance = d * inverse_length;
        return std::isfinite(plane.distance) && is_finite(plane.normal);
    };

    const auto row_value = [&view_projection](core::u32 row, core::u32 column) noexcept {
        return view_projection.at(row, column);
    };
    const auto make_sum_plane = [&make_plane, &row_value](core::u32 row,
                                                           core::f32 sign,
                                                           Plane& plane) noexcept {
        return make_plane(row_value(0, 3) + sign * row_value(0, row),
                          row_value(1, 3) + sign * row_value(1, row),
                          row_value(2, 3) + sign * row_value(2, row),
                          row_value(3, 3) + sign * row_value(3, row),
                          plane);
    };

    const bool valid = make_sum_plane(0, 1.0F, result.planes[0]) &&
                       make_sum_plane(0, -1.0F, result.planes[1]) &&
                       make_sum_plane(1, 1.0F, result.planes[2]) &&
                       make_sum_plane(1, -1.0F, result.planes[3]) &&
                       make_plane(row_value(0, 2),
                                  row_value(1, 2),
                                  row_value(2, 2),
                                  row_value(3, 2),
                                  result.planes[4]) &&
                       make_sum_plane(2, -1.0F, result.planes[5]);
    result.valid = valid;
    return valid;
}

[[nodiscard]] inline bool sphere_visible(const Frustum& frustum,
                                         const Vec3& center,
                                         core::f32 radius) noexcept
{
    if (!frustum.valid || !is_finite(center) || !std::isfinite(radius) || radius < 0.0F) {
        return true;
    }
    for (const Plane& plane : frustum.planes) {
        if (dot(plane.normal, center) + plane.distance < -radius) {
            return false;
        }
    }
    return true;
}

} // namespace gameengine::math
