struct ContactShadowParams
{
    uint2 resolution;
    uint step_count;
    float ray_length;
    float3 light_direction;
    float bias;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float> linear_depth;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> contact_shadow_out;

[[vk::push_constant]]
ConstantBuffer<ContactShadowParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void contact_shadows_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.resolution.x || dispatch_thread_id.y >= params.resolution.y)
    {
        return;
    }

    const uint width = params.resolution.x;
    const uint height = params.resolution.y;
    const uint idx = dispatch_thread_id.y * width + dispatch_thread_id.x;
    const float start_depth = linear_depth[idx];
    const uint steps = max(params.step_count, 1U);
    float shadow = 1.0;

    for (uint i = 1; i <= steps; ++i)
    {
        const float t = float(i) / float(steps);
        const float2 p = float2(dispatch_thread_id.xy) + params.light_direction.xy * (t * params.ray_length);
        if (p.x < 0.0 || p.y < 0.0 || p.x >= float(width) || p.y >= float(height))
        {
            break;
        }
        const uint s_idx = uint(p.y) * width + uint(p.x);
        const float ray_depth = start_depth + params.light_direction.z * (t * params.ray_length);
        if (linear_depth[s_idx] + params.bias < ray_depth)
        {
            shadow = 0.0;
            break;
        }
    }

    contact_shadow_out[idx] = shadow;
}
