struct GiProbeParams
{
    uint3 probe_grid;
    float bounce_intensity;
    float3 sky_irradiance;
    float padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float4> albedo_samples;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float4> probe_irradiance;

[[vk::push_constant]]
ConstantBuffer<GiProbeParams> params;

[numthreads(4, 4, 4)]
[shader("compute")]
void realtime_gi_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.probe_grid.x ||
        dispatch_thread_id.y >= params.probe_grid.y ||
        dispatch_thread_id.z >= params.probe_grid.z)
    {
        return;
    }

    const uint idx = (dispatch_thread_id.z * params.probe_grid.y + dispatch_thread_id.y) *
                         params.probe_grid.x +
                     dispatch_thread_id.x;
    const float4 surface_albedo = albedo_samples[idx];
    const float3 indirect = (params.sky_irradiance + surface_albedo.rgb) * params.bounce_intensity;
    probe_irradiance[idx] = float4(indirect, 1.0);
}
