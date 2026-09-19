struct ForwardPlusComputePushConstants
{
    uint tile_columns;
    uint tile_rows;
    uint light_count;
};

struct PointLight
{
    float3 position;
    float radius;
    float intensity;
    float3 padding;
};

[[vk::binding(3, 0)]] StructuredBuffer<PointLight> point_lights;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint2> tile_headers;
[[vk::binding(5, 0)]] RWStructuredBuffer<uint> tile_light_indices;

[[vk::push_constant]]
ConstantBuffer<ForwardPlusComputePushConstants> push_constants;

[numthreads(16, 16, 1)]
[shader("compute")]
void forward_plus_light_list_main(uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
    if (group_thread_id.x != 0U || group_thread_id.y != 0U ||
        group_id.x >= push_constants.tile_columns ||
        group_id.y >= push_constants.tile_rows)
    {
        return;
    }
    const uint tile = group_id.y * push_constants.tile_columns + group_id.x;
    const uint offset = tile * 256U;
    uint count = 0U;
    for (uint light_index = 0U;
         light_index < min(push_constants.light_count, 256U);
         ++light_index)
    {
        tile_light_indices[offset + count] = light_index;
        ++count;
    }
    tile_headers[tile] = uint2(offset, count);
}
