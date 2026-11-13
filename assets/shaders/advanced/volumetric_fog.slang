struct VolumetricFogParams
{
    uint3 grid_dimensions;
    float density;
    float height_falloff;
    float3 fog_color;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<float4> froxel_grid;

[[vk::push_constant]]
ConstantBuffer<VolumetricFogParams> params;

[numthreads(4, 4, 4)]
[shader("compute")]
void volumetric_fog_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.grid_dimensions.x ||
        dispatch_thread_id.y >= params.grid_dimensions.y ||
        dispatch_thread_id.z >= params.grid_dimensions.z)
    {
        return;
    }

    const uint idx = (dispatch_thread_id.z * params.grid_dimensions.y + dispatch_thread_id.y) *
                         params.grid_dimensions.x +
                     dispatch_thread_id.x;
    const float height_norm = float(dispatch_thread_id.y) / max(float(params.grid_dimensions.y), 1.0);
    const float local_density = params.density * exp(-height_norm * params.height_falloff);
    froxel_grid[idx] = float4(params.fog_color * local_density, saturate(local_density));
}
