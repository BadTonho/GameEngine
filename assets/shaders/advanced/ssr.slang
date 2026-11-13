struct SsrParams
{
    uint2 resolution;
    uint max_steps;
    float stride;
    float max_roughness;
    float3 padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float4> scene_color;

[[vk::binding(1, 0)]]
StructuredBuffer<float4> normal_roughness;

[[vk::binding(2, 0)]]
StructuredBuffer<float> linear_depth;

[[vk::binding(3, 0)]]
RWStructuredBuffer<float4> reflection_out;

[[vk::push_constant]]
ConstantBuffer<SsrParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void ssr_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.resolution.x || dispatch_thread_id.y >= params.resolution.y)
    {
        return;
    }

    const uint width = params.resolution.x;
    const uint height = params.resolution.y;
    const uint idx = dispatch_thread_id.y * width + dispatch_thread_id.x;
    const float4 nr = normal_roughness[idx];
    const float roughness = nr.w;
    if (roughness > params.max_roughness)
    {
        reflection_out[idx] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    const float2 dir = normalize(nr.xy + float2(0.001, 0.001));
    float4 hit_color = float4(0.0, 0.0, 0.0, 0.0);
    const float base_depth = linear_depth[idx];
    for (uint step = 1; step <= params.max_steps; ++step)
    {
        const float2 p = float2(dispatch_thread_id.xy) + dir * (float(step) * params.stride);
        if (p.x < 0.0 || p.y < 0.0 || p.x >= float(width) || p.y >= float(height))
        {
            break;
        }
        const uint s_idx = uint(p.y) * width + uint(p.x);
        if (linear_depth[s_idx] <= base_depth)
        {
            const float fade = 1.0 - saturate(roughness / max(params.max_roughness, 0.01));
            hit_color = float4(scene_color[s_idx].rgb, fade);
            break;
        }
    }

    reflection_out[idx] = hit_color;
}
