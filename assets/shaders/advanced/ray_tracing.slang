struct RayTracingParams
{
    uint2 resolution;
    float max_distance;
    float bias;
    float3 light_direction;
    float padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float4> world_positions;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float4> rt_shadow_reflection;

[[vk::push_constant]]
ConstantBuffer<RayTracingParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void ray_tracing_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.resolution.x || dispatch_thread_id.y >= params.resolution.y)
    {
        return;
    }

    const uint idx = dispatch_thread_id.y * params.resolution.x + dispatch_thread_id.x;
    const float4 pos = world_positions[idx];
    const float visibility = saturate(dot(normalize(pos.xyz + float3(0.0, 1.0, 0.0)),
                                          normalize(params.light_direction)) +
                                      params.bias);
    rt_shadow_reflection[idx] = float4(visibility, visibility, visibility, 1.0);
}
