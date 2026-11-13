struct TaaParams
{
    uint2 resolution;
    float2 jitter_offset;
    float blend_factor;
    float3 padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float4> current_color;

[[vk::binding(1, 0)]]
StructuredBuffer<float4> history_color;

[[vk::binding(2, 0)]]
RWStructuredBuffer<float4> resolved_color;

[[vk::push_constant]]
ConstantBuffer<TaaParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void taa_resolve_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.resolution.x || dispatch_thread_id.y >= params.resolution.y)
    {
        return;
    }

    const uint width = params.resolution.x;
    const uint height = params.resolution.y;
    const uint center_idx = dispatch_thread_id.y * width + dispatch_thread_id.x;
    const float4 current = current_color[center_idx];

    float4 min_color = current;
    float4 max_color = current;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const uint sx = uint(clamp(int(dispatch_thread_id.x) + dx, 0, int(width) - 1));
            const uint sy = uint(clamp(int(dispatch_thread_id.y) + dy, 0, int(height) - 1));
            const float4 neighbor = current_color[sy * width + sx];
            min_color = min(min_color, neighbor);
            max_color = max(max_color, neighbor);
        }
    }

    const float4 clamped_history = clamp(history_color[center_idx], min_color, max_color);
    resolved_color[center_idx] = lerp(clamped_history, current, saturate(params.blend_factor));
}
