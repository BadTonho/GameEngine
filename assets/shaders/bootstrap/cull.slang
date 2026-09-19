struct CullInstance
{
    float4 model_column0;
    float4 model_column1;
    float4 model_column2;
    float4 model_column3;
    float4 bounds;
};

struct VisibleInstance
{
    float4 model_column0;
    float4 model_column1;
    float4 model_column2;
    float4 model_column3;
};

struct IndirectCommand
{
    uint index_count;
    uint instance_count;
    uint first_index;
    uint vertex_offset;
    uint first_instance;
};

struct CullPushConstants
{
    float4 planes[6];
    uint instance_count;
    uint padding[3];
};

[[vk::binding(0, 0)]]
StructuredBuffer<CullInstance> source_instances;

[[vk::binding(1, 0)]]
RWStructuredBuffer<VisibleInstance> visible_instances;

[[vk::binding(2, 0)]]
RWStructuredBuffer<IndirectCommand> indirect_commands;

[[vk::push_constant]]
ConstantBuffer<CullPushConstants> push_constants;

bool sphere_visible(float3 center, float radius)
{
    for (uint index = 0; index < 6; ++index)
    {
        const float distance = dot(push_constants.planes[index].xyz, center) +
                               push_constants.planes[index].w;
        if (distance < -radius)
        {
            return false;
        }
    }
    return true;
}

[numthreads(64, 1, 1)]
[shader("compute")]
void compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint index = dispatch_thread_id.x;
    if (index >= push_constants.instance_count)
    {
        return;
    }

    const CullInstance source = source_instances[index];
    if (!sphere_visible(source.bounds.xyz, source.bounds.w))
    {
        return;
    }

    uint output_index = 0;
    InterlockedAdd(indirect_commands[0].instance_count, 1, output_index);
    visible_instances[output_index].model_column0 = source.model_column0;
    visible_instances[output_index].model_column1 = source.model_column1;
    visible_instances[output_index].model_column2 = source.model_column2;
    visible_instances[output_index].model_column3 = source.model_column3;
}
