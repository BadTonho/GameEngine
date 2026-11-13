struct MeshletCluster
{
    float4 bounding_sphere;
    float lod_error;
    uint index_count;
    uint first_index;
    uint padding;
};

struct VirtualizedGeomParams
{
    uint cluster_count;
    float error_threshold;
    float2 padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<MeshletCluster> clusters;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> visible_cluster_indices;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> visible_cluster_counter;

[[vk::push_constant]]
ConstantBuffer<VirtualizedGeomParams> params;

[numthreads(64, 1, 1)]
[shader("compute")]
void virtualized_geometry_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint idx = dispatch_thread_id.x;
    if (idx >= params.cluster_count)
    {
        return;
    }

    const MeshletCluster cluster = clusters[idx];
    if (cluster.bounding_sphere.w <= 0.0 || cluster.lod_error > params.error_threshold)
    {
        return;
    }

    uint out_idx = 0;
    InterlockedAdd(visible_cluster_counter[0], 1U, out_idx);
    visible_cluster_indices[out_idx] = idx;
}
