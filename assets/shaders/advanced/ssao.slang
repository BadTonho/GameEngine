struct SsaoParams
{
    uint2 resolution;
    float radius;
    float bias;
    uint sample_count;
    float3 padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float> linear_depth;

[[vk::binding(1, 0)]]
StructuredBuffer<float4> world_normals;

[[vk::binding(2, 0)]]
RWStructuredBuffer<float> occlusion_out;

[[vk::push_constant]]
ConstantBuffer<SsaoParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void ssao_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.resolution.x || dispatch_thread_id.y >= params.resolution.y)
    {
        return;
    }

    const uint width = params.resolution.x;
    const uint height = params.resolution.y;
    const uint idx = dispatch_thread_id.y * width + dispatch_thread_id.x;
    const float center_depth = linear_depth[idx];
    const float3 normal = normalize(world_normals[idx].xyz);

    float occlusion = 0.0;
    const uint samples = max(params.sample_count, 1U);
    for (uint i = 0; i < samples; ++i)
    {
        const float angle = float(i) * 2.3999632;
        const int ox = int(cos(angle) * params.radius);
        const int oy = int(sin(angle) * params.radius);
        const uint sx = uint(clamp(int(dispatch_thread_id.x) + ox, 0, int(width) - 1));
        const uint sy = uint(clamp(int(dispatch_thread_id.y) + oy, 0, int(height) - 1));
        const float sample_depth = linear_depth[sy * width + sx];
        const float delta = center_depth - sample_depth - params.bias;
        const float weight = saturate(dot(normal, float3(0.0, 1.0, 0.0)) * 0.5 + 0.5);
        occlusion += (delta > 0.0) ? weight : 0.0;
    }

    occlusion_out[idx] = saturate(1.0 - occlusion / float(samples));
}
